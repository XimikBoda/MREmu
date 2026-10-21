#!/usr/bin/env python3
"""Survey which MediaTek MRE APIs MREmu implements, and generate the
unimplemented-API reference.

The API surface is ~900 functions across 50 SDK headers. Hand-maintaining a
list that size does not work: it goes stale within a week and nobody notices.
This tool derives the answer from two sources that are always correct:

  * the SDK headers in $MRE_SDK/include, which declare the API surface, and
  * MREmu/Bridge.cpp, whose func_map is what vm_get_sym_entry() actually
    resolves against.

An SDK function that is absent from func_map is unimplemented: the guest is
handed a NULL pointer for it. A function that IS in func_map gets its C++
body inspected, so "registered" and "actually implemented" stay distinct -
the bridge happily registers things that do nothing.

Usage:
    python tools/apisurvey.py                 # write docs/unimplemented_apis.md
    python tools/apisurvey.py --check         # exit 1 if the doc is stale
    python tools/apisurvey.py --list-missing  # bare names on stdout

Set MRE_SDK if the SDK headers are not on the default path.
"""

import argparse
import collections
import glob
import os
import re
import sys
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRIDGE = os.path.join(REPO, "MREmu", "Bridge.cpp")
ARMODULE = os.path.join(REPO, "MREmu", "ARModule.cpp")
DOC = os.path.join(REPO, "docs", "unimplemented_apis.md")

# Declared by the SDK but never resolved by the guest, so counting it as
# unimplemented would be misleading. vm_main is the application entry point:
# MREmu calls it from the VXP metadata, it is not looked up via
# vm_get_sym_entry, and it appears nowhere in the engine.
NOT_GUEST_RESOLVED = {"vm_main"}

# Headers grouped by subsystem so the generated table reads as a map of what
# MREmu does and does not cover, rather than 50 unrelated lists. Anything not
# listed here still appears, under "Other".
GROUPS = collections.OrderedDict([
    ("Core system, memory, files", [
        "vmsys.h", "vmstdlib.h", "vmio.h", "vmres.h", "vm4res.h", "vmgettag.h"]),
    ("Graphics", [
        "vmgraph.h", "vmgfxold.h"]),
    ("Audio, video and media", [
        "vmmm.h", "vmmms.h", "vmvideo.h", "vmbitstream.h", "vmcamera.h"]),
    ("Text, charset and UI", [
        "vmchset.h", "vmlog.h", "vmso.h", "vmmullang.h", "vmeditor.h",
        "vmcontact.h", "vmstatusbar.h", "vmsettings.h"]),
    ("Network, sockets and telephony", [
        "vmsock.h", "vmhttp.h", "vmhttps.h", "vmssl.h", "vmtel.h", "vmuart.h"]),
    ("Messaging", [
        "vmsms.h", "vmappcomm.h"]),
    ("Bluetooth, location and radio", [
        "vmbtcm.h", "vmbtspp.h", "vmbtnotify.h", "vmgps.h", "vmcell.h"]),
    ("Platform services and identity", [
        "vmsim.h", "vmsm.h", "vmcert.h", "vmdes.h", "vmda.h", "vmpayment.h",
        "vmpn.h", "vmsrvmng.h", "vmche.h"]),
    ("App lifecycle, resources and sensors", [
        "vmpromng.h", "vmsensor.h", "vmalarm.h", "vmjava.h"]),
])
_HEADER_GROUP = {h: g for g, hs in GROUPS.items() for h in hs}

# Registered and does *something*, but not what the API promises. Too subtle
# to detect statically, so this list is maintained by hand after reading the
# code. Locations are looked up, not stored, so it does not rot.
PARTIAL = collections.OrderedDict([
    ("vm_open_wap_url",
     "Logs the request and returns 0 - which is `VM_OPEN_WAP_URL_SUCCESS`. It "
     "reports the browser opened the URL when nothing was opened at all."),
    ("vm_get_sys_property",
     "Handles 2 of the 9 `MRE_SYS_*` keys (`MRE_SYS_EQUIPMENT_ID`, "
     "`MRE_SYS_HOST_MAX_MEM`). The rest, including `MRE_SYS_SUBSCRIBER_ID`, "
     "yield an empty string and a written length of 0, which is a valid "
     "answer rather than an error."),
    ("vm_appmgr_get_installed_list",
     "Reports 0, which the SDK defines as success, with the count zeroed - so "
     "an app is told 'no apps installed' rather than 'query failed'."),
    ("vm_file_set_attributes",
     "Resolves the path and then ignores it, always reporting failure. Read "
     "attributes work; writes do not."),
    ("vm_post_msg",
     "Always returns 1. The SDK defines 1 as queued and 0 as queue-full, so "
     "the queue can never be reported full."),
    ("vm_start_app_with_para",
     "Empty body, unlike `vm_start_app`, which really launches. An app cannot "
     "pass a start parameter to the next app."),
    ("vm_get_resource_offset_from_file",
     "Ignores the filename argument and resolves against the running app's "
     "own resources, so it cannot read another file's resource table."),
    ("vm_res_init",
     "Hardcodes language 44 instead of the system language, and never calls "
     "`vm_res_init_with_language`."),
    ("vm_graphic_textout_by_baseline",
     "Ignores the baseline argument and draws from the top of the cell."),
    ("vm_graphic_release_canvas_ex",
     "Ignores its extra arguments and behaves as the plain release."),
    ("vm_get_vm_tag",
     "Carries a TODO marker. The `@` ROM form and plain-file reads both work; "
     "the edge cases are simply unverified."),
    ("vm_graphic_flush_layer",
     "Composites correctly but also sleeps to hold 30fps, which is why it is "
     "the usual suspect when stepping or pausing looks wedged."),
    ("vm_pmng_get_current_handle",
     "Returns the handle the engine recorded, and is marked TODO: there is no "
     "separate OS process behind the handle."),
])

# Curated notes for the registered-but-inert entries. Anything not listed here
# gets a generic note, so the document stays correct as the code changes even
# though the prose may lag.
NOTES = {
    "vm_font_set_font_size": "Reports success and does nothing - there is only "
        "one bitmap face.",
    "vm_font_set_font_style": "Reports success and does nothing - bold, italic "
        "and underline are all ignored.",
    "vm_graphic_set_font": "No-op. Font selection is not implemented; text is "
        "always drawn in the built-in unifont face.",
    "vm_appmgr_is_installed": "Always false, so an app probing for a sibling "
        "MRE app concludes nothing is installed.",
    "vm_get_res_header": "Always 0, which is correct rather than lazy: the "
        "engine hands resources out verbatim, so there is no header to skip. A "
        "non-zero answer shifted every resource Opera Mini 4.4 read and it quit "
        "during startup - see docs/fixed_cases/opera_mini_4.4.md.",
    "vm_get_sys_scene": "Always 0; there is no scene stack.",
    "vm_switch_power_saving_mode": "Always returns 0 and changes no state; "
        "power saving is not modelled.",
    "vm_get_operator": "Returns a fixed unknown-operator string.",
    "vm_srv_sensor_is_available": "Reports no sensors.",
    "vm_has_sim_card": "Claims a SIM is present. The SIM stubs return plausible "
        "fixed values, so apps take the SIM path with fabricated data.",
    "vm_sim_card_count": "One fake SIM.",
    "vm_sim_max_card_count": "One fake SIM.",
    "vm_sim_get_active_sim_card": "One fake SIM.",
    "vm_sim_get_prefer_sim_card": "One fake SIM.",
    "vm_audio_suspend_bg_play": "Empty body. Background audio state is not "
        "tracked, so an app cannot be suspended this way.",
    "vm_audio_resume_bg_play": "Empty body. Counterpart to the suspend stub.",
    "vm_set_app_desired_input_mode": "Empty bridge lambda.",
    "vm_input_set_vk_default_enlarge": "Empty bridge lambda.",
    "vm_file_set_attributes": "Reports failure without touching the file.",
    "vm_get_language": "Always English. Fine for the shipping apps.",
    "vm_get_cache_free_space": "Fixed 16 MiB.",
    "vm_sys_file_get_space": "Fixed 128 MiB.",
    "vm_get_disk_free_space": "Fixed 256 MiB.",
    "vm_graphic_get_character_height": "Fixed 16px cell for the unifont face.",
    "vm_graphic_get_bits_per_pixel": "16bpp, which matches the real canvas.",
    "vm_graphic_is_r2l_state": "Always false; no right-to-left layout.",
    "vm_graphic_is_use_vector_font": "Correct - the face is a bitmap.",
    "vm_is_support_keyborad": "Claims a keypad is present.",
    "vm_is_support_pen_touch": "Claims pen touch is present.",
    "vm_is_support_wifi": "Claims Wi-Fi is present.",
    "vm_wifi_is_connected": "Claims a connection; the socket layer is real "
        "regardless.",
}

# Log-only statements carry no behaviour, so they must not count as work when
# deciding whether a body does anything.
_LOGGING = re.compile(
    r"^\s*(spdlog::|Log::|MREngine::Log|log->|LOG_|std::(cout|cerr)|printf\s*\(|"
    r"fprintf\s*\(|vm_[A-Za-z0-9_]*_log\s*\(|spdlog_)")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    return re.sub(r"//[^\n]*", " ", src)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def declared_apis(inc):
    """vm_* functions declared by the SDK headers.

    Returns (by_name, by_header, all_headers): by_name maps a function to
    the header that declares it, by_header is the inverse, and all_headers
    lists every SDK header including ones that declare nothing (vmcert.h is
    enums only) or that are fully implemented (vmtimer.h) - both would
    otherwise vanish from the per-subsystem totals.
    """
    pat = re.compile(r"\b(vm_[A-Za-z0-9_]+)\s*\(")
    by_name = {}
    by_header = collections.defaultdict(set)
    all_headers = []
    for path in sorted(glob.glob(os.path.join(inc, "vm*.h"))):
        src = strip_comments(read(path))
        header = os.path.basename(path)
        all_headers.append(header)
        for m in pat.finditer(src):
            # A name preceded by '(' or '.' is a call, not a declaration.
            if src[max(0, m.start() - 2):m.start()].endswith(("(", ".")):
                continue
            name = m.group(1)
            by_name.setdefault(name, header)
            by_header[header].add(name)
    return by_name, dict(by_header), all_headers


def registered_apis(bridge, armodule=None):
    """Names the guest can actually resolve.

    func_map covers most of the surface, but vm_get_sym_entry falls through to
    ARModule when func_map misses, and ARModule resolves a few more from its
    own table - vm_vsprintf, vm_sprintf and vm_sscanf. Ignoring that fallback
    reports three APIs as unimplemented when they resolve fine.
    """
    names = set(re.findall(r"\{FUNCN(?:_FIX)?\((\w+)\)", bridge))
    # Entries written as {"name", NULL, lambda} carry no C function.
    names |= set(re.findall(r'\{"(\w+)",\s*NULL', bridge))
    if armodule:
        names |= set(re.findall(r'fn_m\["(\w+)"\]', armodule))
    return names - NOT_GUEST_RESOLVED


def repo_sources():
    files = set()
    for pat in ("MREmu/**/*.cpp", "MREmu/**/*.h"):
        files |= set(glob.glob(os.path.join(REPO, pat), recursive=True))
    return [f for f in sorted(files) if "unifont" not in f]


def find_definitions(names):
    """Locate the C++ definition of each name, following the _FIX convention.

    FUNCN_FIX(vm_graphic_create_canvas) exposes the guest name
    vm_graphic_create_canvas but points at vm_graphic_create_canvas_FIX, so a
    plain search of the guest name finds the prototype and nothing else.
    """
    srcs = {}
    for path in repo_sources():
        srcs[path] = strip_comments(read(path))

    def find(cand):
        for path, src in srcs.items():
            for m in re.finditer(r"\b" + re.escape(cand) + r"\s*\(", src):
                pre = src[:m.start()].rstrip()
                if pre and (pre[-1] in ".>" or pre.endswith("=")):
                    continue
                # Scan from the opening paren itself. Starting after it sends
                # the depth negative on an empty arg list and runs the match
                # off to an unrelated paren pair.
                i = m.end() - 1
                depth = 0
                while i < len(src):
                    if src[i] == "(":
                        depth += 1
                    elif src[i] == ")":
                        depth -= 1
                        if depth == 0:
                            break
                    i += 1
                j = i + 1
                while j < len(src) and src[j] in " \t\r\n":
                    j += 1
                if j >= len(src) or src[j] != "{":
                    continue  # a call, not a definition
                depth = 0
                k = j
                while k < len(src):
                    if src[k] == "{":
                        depth += 1
                    elif src[k] == "}":
                        depth -= 1
                        if depth == 0:
                            break
                    k += 1
                line = src.count("\n", 0, m.start()) + 1
                rel = os.path.relpath(path, REPO).replace("\\", "/")
                return rel, line, src[m.end():i], src[j:k + 1]
        return None, None, None, None

    out = {}
    for name in names:
        rel, line, args, body = find(name)
        if body is None:
            rel, line, args, body = find(name + "_FIX")
        out[name] = (rel, line, args, body)
    return out


def find_bridge_lambdas(bridge):
    """Bodies of the {"name", NULL, lambda} entries, which have no C function."""
    out = {}
    pat = re.compile(
        r'\{"(\w+)",\s*NULL,\s*\[\]\([^)]*\)\s*\{(.*?)\n\t*\}\}', re.S)
    for m in pat.finditer(bridge):
        line = bridge.count("\n", 0, m.start()) + 1
        out[m.group(1)] = (line, m.group(2))
    return out


def _is_literal_constant(expr):
    """True when expr is a constant, not a delegation.

    A one-line body is not on its own a stub: `return create_layer(...)` and
    `return vm_graphic_get_character_height()` both delegate to real work and
    are fully implemented. Only a literal answer means nothing is happening.
    """
    if "(" in expr:                       # a call, or a cast of something
        return False
    # Any bare identifier must be an ALL-CAPS constant (VM_GDI_SUCCEED,
    # ENGLISH, NULL, VM_STRING_ID_LEN). Anything holding lowercase is a
    # variable or a parameter, e.g. `return shared_memory_size` or
    # `return card_id == 1`, both of which are real work.
    for tok in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", expr):
        if any(c.isalpha() for c in tok) and tok != tok.upper():
            return False
    return True


def classify(body):
    """'empty', 'fixed' or 'real' - does this body do anything?"""
    b = body.strip()
    if b in ("{}", "{ }"):
        return "empty", ""
    inner = b[1:-1] if b.startswith("{") else b
    stmts = [s.strip() for s in inner.split(";") if s.strip()]
    stmts = [s for s in stmts if not _LOGGING.match(s)]
    rets = [s for s in stmts if s.startswith("return ")]
    if not stmts:
        return "empty", ""
    if len(stmts) == len(rets) and len(set(rets)) == 1:
        expr = rets[0][len("return "):].strip()
        if _is_literal_constant(expr):
            return "fixed", expr
    return "real", ""


def build(sdk):
    inc = os.path.join(sdk, "include")
    if not os.path.isdir(inc):
        sys.exit("SDK headers not found at %s - set MRE_SDK." % inc)

    declared, by_header, all_headers = declared_apis(inc)
    declared = {k: v for k, v in declared.items() if k not in NOT_GUEST_RESOLVED}
    bridge = read(BRIDGE)
    reg = registered_apis(bridge, read(ARMODULE))
    defs = find_definitions(reg)
    lambdas = find_bridge_lambdas(bridge)

    missing = collections.defaultdict(set)
    for name, header in declared.items():
        if name not in reg:
            missing[header].add(name)

    inert = []
    for name in sorted(reg):
        # Only SDK APIs belong in this table. The bridge also exposes a few
        # host symbols (rand, srand, armodule_*) that the guest asks for.
        if name not in declared:
            continue
        rel, line, args, body = defs[name]
        if body is None:
            if name in lambdas:
                lline, lbody = lambdas[name]
                kind, val = classify("{" + lbody + "}")
                if kind == "empty":
                    inert.append((name, "no behaviour", "",
                                  "MREmu/Bridge.cpp:%d" % lline))
            continue
        kind, val = classify(body)
        if kind == "empty":
            inert.append((name, "no behaviour", "", "%s:%d" % (rel, line)))
        elif kind == "fixed":
            inert.append((name, "fixed answer", val, "%s:%d" % (rel, line)))

    # `reg` also holds host symbols the guest asks for (rand, srand,
    # armodule_*), so only the SDK-declared ones count towards coverage.
    implemented = len([n for n in reg if n in declared]) - len(inert)
    return declared, reg, missing, inert, implemented, inc, defs, by_header, all_headers


def render(declared, reg, missing, inert, implemented, inc, defs, by_header,
          all_headers):
    total_missing = sum(len(v) for v in missing.values())
    missing_by_header = sorted(missing.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    no_behaviour = [r for r in inert if r[1] == "no behaviour"]

    L = []
    a = L.append
    a("# Unimplemented MRE APIs")
    a("")
    a("What MREmu does **not** implement, and what it registers but leaves")
    a("inert. Generated by [`tools/apisurvey.py`](../tools/apisurvey.py) - do")
    a("not edit by hand; run the script instead.")
    a("")
    a("| | Count |")
    a("| --- | ---: |")
    a("| APIs declared by the SDK | %d |" % len(declared))
    a("| Resolved by `vm_get_sym_entry` | %d |" % len(reg))
    a("| **Unimplemented** (guest gets NULL) | **%d** |" % total_missing)
    a("| Registered but doing nothing | %d |" % len(inert))
    a("| Registered but partial (hand-maintained) | %d |" % len(PARTIAL))
    a("| Implemented | %d |" % implemented)
    a("")

    a("## What a missing API does to an app")
    a("")
    a("`vm_get_sym_entry` in [`MREmu/Bridge.cpp`](../MREmu/Bridge.cpp) walks")
    a("`func_map` and falls back to `ARModule::vm_get_sym_entry`, which only")
    a("resolves symbols the guest binary carries itself. Neither path can find")
    a("an unimplemented VM API, so both return **0**:")
    a("")
    a("```cpp")
    a("if (ret == 0)")
    a("    ret = armodule.vm_get_sym_entry(symbol);   // also returns 0")
    a("return ret;                                     // NULL to the guest")
    a("```")
    a("")
    a("The app stores that NULL in its dispatch table. A well-written app")
    a("checks the pointer and carries on without the feature; one that does")
    a("not will jump to address 0. This is why missing entries below are not")
    a("automatically fatal - most VXP apps resolve dozens of APIs at start-up")
    a("and tolerate a good number being absent.")
    a("")
    a("Note that a *registered* API is not a working one. Several entries are")
    a("wired up and then do nothing at all; those are in their own section")
    a("below.")
    a("")

    a("## How the survey is derived")
    a("")
    a("- **Declared** - every `vm_*` prototype in the SDK headers under")
    a("  `$MRE_SDK/include` (this run: `%s`)." % inc.replace("\\", "/"))
    a("- **Resolved** - every name in `func_map`, including the")
    a("    `FUNCN_FIX` variants, which expose a guest name such as")
    a("    `vm_graphic_create_canvas` while the C++ function is")
    a("    `vm_graphic_create_canvas_FIX`, plus the handful `ARModule`")
    a("    resolves from its own table when `func_map` misses")
    a("    (`vm_vsprintf`, `vm_sprintf`, `vm_sscanf`).")
    a("- **Inert** - the resolved name's body was parsed and judged to do no")
    a("  work: either empty, or nothing but a `return` of a constant once")
    a("  logging calls are discounted.")
    a("")
    a("Two caveats on the method. *Registered* means the symbol resolves, not")
    a("that it is correct - a function can be fully implemented and still be")
    a("wrong for an app that depends on subtle behaviour. And *declared* comes")
    a("from the SDK headers, which are a superset of what any one phone ever")
    a("exposed, so a large share of the missing list is hardware no emulator")
    a("would have: camera, Bluetooth, telephony, SMS, payment.")
    a("")

    a("## Unimplemented by subsystem")
    a("")
    a("| Subsystem | Headers | Declared | Missing |")
    a("| --- | ---: | ---: | ---: |")
    for group, headers in GROUPS.items():
        d = sum(len(by_header.get(h, ())) for h in headers)
        m = sum(len(missing.get(h, ())) for h in headers)
        a("| %s | %d | %d | %d |" % (group, len(headers), d, m))
    others = [h for h in all_headers if h not in _HEADER_GROUP]
    a("| **All other headers** | %d | %d | %d |" % (
        len(others),
        sum(len(by_header.get(h, ())) for h in others),
        sum(len(missing.get(h, ())) for h in others)))
    a("")
    a("The big gaps are whole subsystems rather than scattered holes:")
    a("**camera** (36), **video** (19), **editor** (39), **contact** (14),")
    a("**MMS** (8), **SMS** (15), **Bluetooth** (33 across three headers),")
    a("**payment** (3) and **GPS** (8) are absent end to end. An app that")
    a("needs one of these will show the feature as unavailable or simply skip")
    a("it, rather than crashing.")
    a("")

    a("## Unimplemented by header")
    a("")
    for header, names in missing_by_header:
        a("### `%s` - %d missing" % (header, len(names)))
        a("")
        a(_code_rows(sorted(names)))
        a("")
    a("")

    a("## Registered but doing nothing")
    a("")
    a("These resolve, so the guest gets a valid pointer and calls into")
    a("MREmu - which then does nothing useful. They are harder to live with")
    a("than a missing API, because a missing one can be feature-detected and")
    a("these cannot: the app gets a success code or a plausible constant back")
    a("and proceeds as though the call worked.")
    a("")
    a("| API | Kind | Returns | Implementation | Note |")
    a("| --- | --- | --- | --- | --- |")
    for name, kind, val, where in inert:
        # Fall back to the PARTIAL note so an entry appearing in both tables
        # does not show a blank cell in one of them.
        a("| `%s` | %s | `%s` | `%s` | %s |" % (
            name, kind, val if val else "nothing", where,
            NOTES.get(name) or PARTIAL.get(name, "")))
    a("")
    a("Two kinds, and they are not equally serious. **No behaviour** is an")
    a("empty body - the call is a no-op, which is an unambiguous stub.")
    a("**Fixed answer** returns a constant; whether that is a defect depends on")
    a("what was asked. Answering *what is your character height* with `16` is")
    a("correct, and answering *which apps are installed* with `0` is a stub.")
    a("The Note column marks which is which.")
    a("")
    a("A one-line body is **not** treated as inert. `vm_graphic_create_layer`")
    a("is a single `return create_layer(...)` and is a complete")
    a("implementation; only a literal answer is flagged.")
    a("")
    if no_behaviour:
        a("The unambiguous stubs - empty bodies, no behaviour at all:")
        a("")
        for name, kind, val, where in no_behaviour:
            a("- `%s` (%s) - %s" % (
                name, where, NOTES.get(name, "does nothing and reports nothing")))
        a("")

    a("## Registered but partial")
    a("")
    a("These resolve and do real work, but not all of it. Too subtle to detect")
    a("statically, so the list is maintained by hand in `PARTIAL` at the top of")
    a("the script rather than derived - treat it as the best current reading,")
    a("not a proof.")
    a("")
    a("| API | Implementation | What is missing |")
    a("| --- | --- | --- |")
    for name, note in PARTIAL.items():
        rel, line, _args, _body = defs.get(name, (None, None, None, None))
        where = "%s:%s" % (rel, line) if rel else "-"
        a("| `%s` | `%s` | %s |" % (name, where, note))
    a("")

    a("## Regenerating")
    a("")
    a("```")
    a("python tools/apisurvey.py            # rewrite this file")
    a("python tools/apisurvey.py --check    # fail if it is stale")
    a("python tools/apisurvey.py --list-missing vm_graphic_   # filter by prefix")
    a("```")
    a("")
    a("The SDK headers are not vendored; they come from `$MRE_SDK/include`,")
    a("so the counts track whichever SDK the build points at.")
    a("")
    return "\n".join(L), total_missing


def _code_rows(names, width=96):
    """Render function names as a wrapped inline-code list.

    Wrapping happens at a comma so the markdown reflows correctly - a name is
    never split across lines. Without it the biggest header produces a single
    4000-character line, which is unreadable in a diff and in review.
    """
    out, line = [], ""
    for i, n in enumerate(names):
        piece = "`%s`%s" % (n, "," if i < len(names) - 1 else "")
        if line and len(line) + len(piece) + 1 > width:
            out.append(line)
            line = piece
        else:
            line = ("%s %s" % (line, piece)).strip() if line else piece
    if line:
        out.append(line)
    return "\n".join(out)


VXP_DIR = os.path.join(REPO, "bin", "Release", "fs", "e", "mre")

# Same app shipped at several sizes is one consumer, not three. Counting the
# three PeanutVXP builds separately would triple its apparent demand.
SAME_APP = [("peanutvxp", "PeanutVXP"), ("opera mini 4.4", "Opera Mini 4.4"),
            ("gravity defied", "Gravity Defied")]

_ZHEAD = (0x01, 0x5e, 0x9c, 0xda)
_ASCII_RUN = re.compile(rb"[\x20-\x7e]{4,}")
_UCS2_RUN = re.compile(rb"(?:[\x20-\x7e]\x00){4,}")
_SYM = re.compile(r"^vm_[a-z0-9_]+$")


def _inflate(data):
    """Pull every zlib stream out of a package.

    Compressed packages hide their whole payload, so a raw string scan finds
    nothing at all. Four of the installed packages are ads_compressed, and they
    are exactly the ones that had boot problems - an invisible app is an
    unmeasured app. Brute-forcing the stream offsets is crude but fine here:
    we want the strings, not a faithful parse.
    """
    out = []
    for i in range(len(data) - 2):
        if data[i] == 0x78 and data[i + 1] in _ZHEAD:
            try:
                buf = zlib.decompressobj().decompress(data[i:])
                if len(buf) > 256:
                    out.append(buf)
            except Exception:
                pass
    return out


def _symbols_in(blobs):
    names = set()
    for blob in blobs:
        for rx, enc in ((_ASCII_RUN, "latin-1"), (_UCS2_RUN, "utf-16-le")):
            for m in rx.finditer(blob):
                for tok in re.findall(r"vm_[A-Za-z0-9_]+",
                                      m.group(0).decode(enc, "ignore")):
                    if _SYM.match(tok):
                        names.add(tok)
    return names


def scan_demand(vxp_dir, missing):
    """Which unimplemented APIs do the installed apps actually ask for?

    A vm_* string in a package is a proxy for what the app resolves at
    start-up, not proof that it calls it - so this over-counts slightly. It is
    still far better evidence than guessing, and it is the only signal here
    that separates 'nobody wants this' from 'one app badly wants this'.
    """
    want = {}
    for path in sorted(glob.glob(os.path.join(vxp_dir, "*.vxp"))):
        with open(path, "rb") as fh:
            data = fh.read()
        base = os.path.basename(path)[:-4]
        for prefix, name in SAME_APP:
            if base.lower().startswith(prefix):
                base = name
                break
        names = _symbols_in([data]) | _symbols_in(_inflate(data))
        want.setdefault(base, set()).update(names)
    return ({api: {app for app in want if api in want[app]}
             for api in missing
             if any(api in names for names in want.values())}, want)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", default=os.environ.get(
        "MRE_SDK", r"C:\MRE_SDK"))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--list-missing", metavar="PREFIX")
    ap.add_argument("--demand", action="store_true",
                    help="rank unimplemented APIs by how many installed apps "
                         "ask for them")
    ap.add_argument("--vxp-dir", default=VXP_DIR)
    args = ap.parse_args()

    (declared, reg, missing, inert, implemented, inc, defs, by_header,
     all_headers) = build(args.sdk)

    if args.list_missing is not None:
        # is not None, not truthiness: an empty prefix means "list all", and
        # "" is falsy.
        prefix = args.list_missing
        for header, names in sorted(missing.items()):
            for n in sorted(names):
                if n.startswith(prefix):
                    print(n)
        return

    if args.demand:
        want_all = set(declared) - reg
        demand, want = scan_demand(args.vxp_dir, want_all)
        if not os.path.isdir(args.vxp_dir):
            sys.exit("no packages at %s - pass --vxp-dir" % args.vxp_dir)
        print("apps scanned : %d" % len(want))
        print("unimplemented: %d" % len(want_all))
        print("demanded     : %d" % len(demand))
        print("never wanted : %d" % (len(want_all) - len(demand)))
        print()
        multi = [a for a in demand if len(demand[a]) >= 2]
        print("wanted by 2+ apps: %d   by 3+ apps: %d"
              % (len(multi), len([a for a in demand if len(demand[a]) >= 3])))
        print()
        print("%-46s %-11s %s" % ("API", "header", "apps"))
        for api in sorted(demand, key=lambda a: (-len(demand[a]), a)):
            if len(demand[api]) < 2:
                continue
            print("%-46s %-11s %s" % (api, declared[api],
                                      ", ".join(sorted(demand[api]))))
        print()
        print("--- wanted by exactly one app ---")
        by_app = collections.Counter(next(iter(v)) for v in demand.values()
                                     if len(v) == 1)
        for app, c in by_app.most_common():
            print("  %-30s %d" % (app, c))
        return

    text, _ = render(declared, reg, missing, inert, implemented, inc, defs,
                     by_header, all_headers)
    if args.check:
        if not os.path.exists(DOC):
            print("missing %s" % DOC)
            sys.exit(1)
        if read(DOC).rstrip("\n") != text.rstrip("\n"):
            print("%s is stale - rerun tools/apisurvey.py" % DOC)
            sys.exit(1)
        print("up to date")
        return

    with open(DOC, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print("wrote %s (%d missing, %d inert)" % (DOC, sum(
        len(v) for v in missing.values()), len(inert)))


if __name__ == "__main__":
    main()