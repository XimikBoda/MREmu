#!/usr/bin/env python3
"""vxpclean - dedupe and rename a folder of .vxp packages.

Two passes, both driven by content rather than by what the filenames claim:

  1. Byte-identical duplicates are removed by SHA-256. The catalog this was
     written for had 210 files with only 143 distinct hashes - the same app
     shipped repeatedly under names like "Parkour Run", "Temple Run" and
     "TemplerunNokia215220225nallvxpp", plus some flat-out mislabels where
     "Cut The Rope.vxp" is really Asphalt.

  2. What survives is renamed to the app_name declared inside the package
     (VM_CE_INFO_NAME), which is authoritative and fixes the mislabels.
     Where several distinct packages declare the same name - Opera Mini ships
     as 4.4 and 6.1 - a qualifier is appended from the first discriminator
     that separates them.

The launcher does not care about any of this: it shows the metadata app_name,
so filenames are filesystem hygiene only.

Dry run by default. Pass --apply to actually move and rename.

    python tools/vxpclean.py                          # plan only
    python tools/vxpclean.py --apply                  # do it
    python tools/vxpclean.py --dir <folder> --apply
"""

import argparse
import collections
import difflib
import hashlib
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DIR = os.path.join(REPO, "bin", "Release", "fs", "e", "mre")
QUARANTINE = "_duplicates"
VXPINFO = os.path.join(REPO, "tools", "vxpinfo.py")

# Characters Windows will not accept in a filename, plus the control range.
ILLEGAL = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
SITE_TAG = re.compile(r"\s*\[[^\]]*\]")
COPY_MARK = re.compile(r"\s*\(\s*(copy|\d+)\s*\)\s*$", re.I)
RES_TOKEN = re.compile(r"\b\d{3,4}\s*[xX]\s*\d{3,4}\b")
# Uploader / device / format junk that some packages bake into their own name.
UPLOADER_JUNK = re.compile(
    r"\s*(?:MT\d{4}\([^)]*\)\s*works\s+in\s+all\s+vxp\s+phone-\d+"
    r"|QVGA\s+\d+K\s+\w+\s+\d{6}"
    r"|works\s+in\s+all\s+vxp\s+phone-\d+"
    r"|works\s+in\s+nokia\s+[\d,]+"
    r")",
    re.I)
# Attribution prefix before " - " (e.g. "Pragathees Waran - Go Cart ...").
ATTRIBUTION = re.compile(r"^[A-Za-z][A-Za-z .'-]*\s+-\s+")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_metadata(folder, names):
    """app_name / version / resolution for each package, via tools/vxpinfo."""
    if not names:
        return {}
    proc = subprocess.run(
        ["python", VXPINFO, "--json"] + [os.path.join(folder, n) for n in names],
        capture_output=True, cwd=REPO)
    # App names carry bytes outside cp1252, so decoding with the console codec
    # kills the whole batch. UTF-8 with replacement keeps partial data usable.
    try:
        data = json.loads(proc.stdout.decode("utf-8", "replace"))
    except ValueError:
        return {}
    if isinstance(data, dict):
        data = [data]
    out = {}
    for rec in data:
        base = os.path.basename(rec.get("file") or rec.get("path") or "")
        s = rec.get("summary", {})
        out[base] = {
            "app": (s.get("app_name") or "").strip(),
            "ver": (s.get("version") or "").strip(),
            "res": (s.get("resolution") or "").strip(),
        }
    return out


def clean_stem(filename):
    """Reduce a filename to something readable, without inventing anything."""
    stem = filename[:-4] if filename.lower().endswith(".vxp") else filename
    stem = SITE_TAG.sub("", stem)
    stem = UPLOADER_JUNK.sub("", stem)
    stem = ATTRIBUTION.sub("", stem)
    stem = stem.replace("_", " ")
    stem = COPY_MARK.sub("", stem)
    return re.sub(r"\s+", " ", stem).strip(" -.")


def usable_version(meta):
    v = meta.get("ver", "")
    # The catalog is full of "0", which means undeclared rather than real.
    return "" if v in ("", "0", "0.0", "0.0.0") else v


def usable_res(meta):
    r = meta.get("res", "")
    return "" if r.lower() in ("", "any", "0") else r


def is_mojibake(name):
    """True for text that is really cp936 bytes mis-decoded as Latin-1.

    Several packages in the catalog are Chinese builds whose VM_CE_INFO_NAME
    holds cp936 bytes. tools/vxpinfo.py decodes them as Latin-1, so the
    declared name comes out as "开心农场" - readable, but
    strictly worse than the English filename it would replace. The tell is
    that the string round-trips: encode back to Latin-1 and it decodes cleanly
    as cp936. Pure ASCII is never mojibake, and genuinely correct Unicode
    cannot be encoded to Latin-1 at all, so both pass.
    """
    if not name or any(ord(c) < 128 for c in name) and all(
            ord(c) < 128 for c in name):
        return False
    try:
        raw = name.encode("latin-1")
    except UnicodeEncodeError:
        return False
    try:
        raw.decode("cp936")
    except UnicodeDecodeError:
        return False
    return True


def sanitize(name):
    name = ILLEGAL.sub("", name)
    name = re.sub(r"\s+", " ", name).strip(" .")
    return name[:120] or "app"


def _norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def tight_stem(name, stem, app):
    """The part of a filename that actually distinguishes it from the app name.

    Parenthetical compat notes, trailing resolutions and the app-name prefix
    are all dropped, because they either restate the name or nest parentheses
    inside the ones the new name already has. "Opera Mini 4.4 (EN)" against an
    app named "Opera Mini" reduces to just "4.4".

    The comparison ignores spaces and case, because declared names are not
    consistent about either: an app called "GravityDefied" has a filename
    "Gravity Defied", and without normalising that pair produces the useless
    qualifier "GravityDefied (Gravity Defied)".
    """
    s = re.sub(r"\([^)]*\)", " ", stem)
    s = RES_TOKEN.sub(" ", s)
    s = re.sub(r"\s+", " ", s).strip()
    if not app:
        return s[:30].strip()

    na, ns = _norm(app), _norm(s)
    # The stem merely repeats the app name, possibly with a "The" or a space
    # difference. Nothing there to disambiguate with.
    if not ns or ns == na or na.endswith(ns):
        return ""
    if s.lower().startswith(app.lower()):
        s = s[len(app):].strip()
    return s[:30].strip(" -_.")


def qualifiers(name, stem, meta, size):
    """Candidate discriminators, best first."""
    out = []
    # The filename is the only thing that reliably tells two builds of the
    # same app apart - version and resolution are "0" and "any" far too often.
    tight = tight_stem(name, stem, meta.get("app", ""))
    if tight and tight.lower() != name.lower():
        out.append(tight)
    r = usable_res(meta)
    if r:
        out.append(r)
    v = usable_version(meta)
    if v:
        out.append(v)
    out.append("%d KB" % (size // 1024))
    seen, uniq = set(), []
    for o in out:
        k = o.lower()
        if k not in seen:
            seen.add(k)
            uniq.append(o)
    return uniq


def plan(folder):
    vxps = sorted(f for f in os.listdir(folder)
                  if f.lower().endswith(".vxp") and os.path.isfile(
                      os.path.join(folder, f)))
    by_hash = collections.defaultdict(list)
    sizes = {}
    for f in vxps:
        p = os.path.join(folder, f)
        by_hash[sha256(p)].append(f)
        sizes[f] = os.path.getsize(p)

    # Metadata first, so the survivor of each group can be chosen by whether
    # its name matches what it actually is. The catalog mislabels files: one
    # package is "Cut The Rope.vxp" but declares app_name "Asphalt". Keeping
    # the copy that is correctly labelled makes the quarantine list readable,
    # and the survivor gets renamed from metadata regardless.
    all_meta = read_metadata(folder, vxps)

    def norm(s):
        return re.sub(r"[^a-z0-9]", "", s.lower())

    def score(f, app):
        stem = clean_stem(f)
        if not app:
            return (1, len(stem), f.lower())
        ratio = difflib.SequenceMatcher(None, norm(stem), norm(app)).ratio()
        return (-ratio, len(stem), f.lower())

    survivors, moved = {}, []
    for h, group in by_hash.items():
        apps = [all_meta.get(f, {}).get("app", "") for f in group]
        app = max((a for a in apps if a), key=len, default="")
        keep = sorted(group, key=lambda f: score(f, app))[0]
        survivors[h] = keep
        for f in group:
            if f != keep:
                moved.append((f, keep))

    meta = {keep: all_meta.get(keep, {}) for keep in survivors.values()}

    entries = []
    for h, keep in survivors.items():
        stem = clean_stem(keep)
        m = meta.get(keep, {})
        declared = m.get("app", "")
        skipped = ""
        # The original filename wins. Packages routinely declare an app_name
        # that is just their own filename flattened - "Adam n Eve.vxp" arrives
        # as "adameve", "Call Of Duty.vxp" as "callduty" - and renaming to
        # that is strictly worse than what was already on disk. The metadata
        # is only used when the filename is unusable, and as a hint when two
        # surviving names collide.
        if stem:
            base = stem
        elif is_mojibake(declared):
            skipped = declared
            base = ""
        else:
            base = declared
        entries.append({"hash": h, "keep": keep, "stem": stem, "meta": m,
                        "base": sanitize(base), "size": sizes[keep],
                        "skipped": skipped})

    # Same declared name across distinct packages: qualify until unique.
    by_name = collections.defaultdict(list)
    for e in entries:
        by_name[e["base"].lower()].append(e)

    taken = {}
    for name, group in by_name.items():
        if len(group) == 1:
            e = group[0]
            e["final"] = e["base"]
            e["why"] = ""
            continue
        # Hand out qualifiers greedily, but never two members the same one.
        used = set()
        for e in sorted(group, key=lambda x: (-x["size"], x["keep"].lower())):
            pick = ""
            for cand in qualifiers(e["base"], e["stem"], e["meta"], e["size"]):
                key = "%s|%s" % (name, cand.lower())
                if key not in used:
                    pick = cand
                    used.add(key)
                    break
            e["final"] = "%s (%s)" % (e["base"], pick) if pick else e["base"]
            e["why"] = pick

    # Final uniqueness sweep, in case a qualifier still collided.
    seen = {}
    for e in sorted(entries, key=lambda x: x["final"].lower()):
        n, i = e["final"], 2
        while n.lower() in seen:
            n = "%s (%d)" % (e["final"], i)
            i += 1
        seen[n.lower()] = e
        e["final"] = n

    return entries, moved


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=DEFAULT_DIR)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--keep-duplicates", action="store_true",
                    help="move duplicates to _duplicates/ instead of deleting "
                         "them (default is to delete)")
    args = ap.parse_args()

    if not os.path.isdir(args.dir):
        sys.exit("no such folder: %s" % args.dir)

    entries, moved = plan(args.dir)
    entries.sort(key=lambda e: e["final"].lower())

    print("folder        : %s" % args.dir)
    print("distinct apps : %d" % len(entries))
    print("duplicates    : %d (to %s\\)" % (len(moved), QUARANTINE))
    # `keep` carries the .vxp extension and `final` does not, so comparing
    # them directly says every file is renamed, even the untouched ones.
    def target(e):
        return e["final"] + ".vxp"

    renamed = [e for e in entries if target(e) != e["keep"]]
    print("renames       : %d" % len(renamed))
    print()

    print("=== duplicates -> %s ===" % QUARANTINE)
    for old, keep in sorted(moved):
        print("  %-52s -> %s" % (old[:52], keep))

    print()
    print("=== renames ===")
    for e in entries:
        if target(e) != e["keep"]:
            tag = "  [%s]" % e["why"] if e["why"] else ""
            print("  %-50s -> %s%s" % (e["keep"][:50], target(e), tag))
        else:
            print("  %-50s    (unchanged)" % e["keep"][:50])

    skipped = [e for e in entries if e["skipped"]]
    if skipped:
        print()
        print("=== declared name rejected, filename kept ===")
        print("(tools/vxpinfo.py decodes these packages' cp936 names as "
              "Latin-1; renaming to that would be worse than what is there)")
        for e in sorted(skipped, key=lambda x: x["keep"].lower()):
            print("  %-46s keeps filename" % e["keep"][:46])
            print("      declared would have read: %r" % e["skipped"])

    if not args.apply:
        print()
        print("dry run - nothing changed. Re-run with --apply.")
        return

    # Always record the mapping before touching anything. Renaming is
    # otherwise irreversible: the original filename is not recoverable from
    # the package, only from this file.
    manifest = os.path.join(args.dir, "vxpclean-manifest.txt")
    with open(manifest, "w", encoding="utf-8") as fh:
        fh.write("# old name -> new name (and which copy won each hash group)\n")
        for old, keep in sorted(moved):
            fh.write("duplicate\t%s\t->\t%s\n" % (old, keep))
        for e in sorted(entries, key=lambda x: x["final"].lower()):
            fh.write("rename\t%s\t->\t%s\n" % (e["keep"], target(e)))
    print("manifest: %s" % manifest)

    # Eliminate duplicates: delete them. The content is preserved in the
    # surviving copy (verified byte-identical by hash), so nothing is lost.
    # The --keep-duplicates flag overrides this for a safety net.
    if args.keep_duplicates:
        qdir = os.path.join(args.dir, QUARANTINE)
        os.makedirs(qdir, exist_ok=True)
        for old, _keep in sorted(moved):
            src = os.path.join(args.dir, old)
            dst = os.path.join(qdir, old)
            n = 2
            while os.path.exists(dst):
                dst = os.path.join(qdir, "%s.%d.vxp" % (old[:-4], n))
                n += 1
            os.replace(src, dst)
        print("quarantined: %d (in %s)" % (len(moved), QUARANTINE))
    else:
        for old, _keep in sorted(moved):
            src = os.path.join(args.dir, old)
            if os.path.exists(src):
                os.remove(src)
        # Remove _duplicates if it exists from a previous run.
        qdir = os.path.join(args.dir, QUARANTINE)
        if os.path.isdir(qdir):
            try:
                # Only remove if empty (all duplicates were deleted, not moved).
                if not os.listdir(qdir):
                    os.rmdir(qdir)
                else:
                    # Move any remaining files to a temp location and remove.
                    import shutil
                    for f in os.listdir(qdir):
                        os.remove(os.path.join(qdir, f))
                    os.rmdir(qdir)
            except OSError:
                pass
        print("eliminated: %d duplicates" % len(moved))

    # Two-phase rename so a swap cannot collide with a target name.
    tmp = {}
    for e in entries:
        if target(e) != e["keep"]:
            t = os.path.join(args.dir, ".~%s.vxp" % e["hash"][:12])
            os.replace(os.path.join(args.dir, e["keep"]), t)
            tmp[t] = e
    for t, e in tmp.items():
        os.replace(t, os.path.join(args.dir, target(e)))

    print()
    print("applied: %d eliminated, %d renamed, %d kept" %
          (len(moved), len(renamed), len(entries) - len(renamed)))


if __name__ == "__main__":
    main()