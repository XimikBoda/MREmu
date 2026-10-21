#!/usr/bin/env python3
"""vxpdiff - compare two .vxp packages (tags, sections, metadata, icon)."""

import argparse
import hashlib
import json
import os
import sys

try:
    import vxpinfo
except Exception:
    vxpinfo = None
try:
    import vxpicon
except Exception:
    vxpicon = None


def load(path):
    if vxpinfo is None:
        raise RuntimeError("vxpinfo module not available")
    info = vxpinfo.inspect(path)
    data = open(path, "rb").read()
    icon_hash = None
    if vxpicon is not None:
        img = vxpicon.extract_icon(data)
        if img:
            icon_hash = hashlib.sha256(img).hexdigest()
    info["_icon_sha256"] = icon_hash
    info["_size"] = len(data)
    return info


def tag_map(info):
    return {t["id"]: t for t in info["tags"]}


def section_map(info):
    if not info.get("elf") or "sections" not in info["elf"]:
        return {}
    return {s["name"]: s for s in info["elf"]["sections"]}


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(description="Compare two .vxp packages.")
    ap.add_argument("a", help="first .vxp")
    ap.add_argument("b", help="second .vxp")
    ap.add_argument("--json", action="store_true", help="emit JSON diff")
    args = ap.parse_args()

    for p in (args.a, args.b):
        if not os.path.isfile(p):
            print("vxpdiff: file not found: %s" % p, file=sys.stderr)
            return 2

    ia = load(args.a)
    ib = load(args.b)

    diff = {
        "file_a": ia["file"],
        "file_b": ib["file"],
        "size": {"a": ia["_size"], "b": ib["_size"], "delta": ib["_size"] - ia["_size"]},
        "container": {"a": ia["container"], "b": ib["container"]},
        "charset_ucs2": {"a": ia["charset_ucs2"], "b": ib["charset_ucs2"]},
        "summary": {},
        "tags": {"added": [], "removed": [], "changed": []},
        "sections": {"added": [], "removed": [], "changed": []},
        "icon_sha256": {"a": ia["_icon_sha256"], "b": ib["_icon_sha256"]},
    }

    # summary fields
    for key in ("app_name", "app_name_source", "dev_name", "version", "version_raw",
                "description", "resolution", "ram_kb", "is_ads", "is_vre_ads", "is_zipped"):
        va = ia["summary"].get(key)
        vb = ib["summary"].get(key)
        if va != vb:
            diff["summary"][key] = {"a": va, "b": vb}

    # tags
    ma = tag_map(ia)
    mb = tag_map(ib)
    for tid in sorted(set(ma) | set(mb)):
        if tid in ma and tid not in mb:
            diff["tags"]["removed"].append(ma[tid])
        elif tid in mb and tid not in ma:
            diff["tags"]["added"].append(mb[tid])
        elif ma[tid].get("value") != mb[tid].get("value"):
            diff["tags"]["changed"].append({"id": tid, "a": ma[tid], "b": mb[tid]})

    # sections
    sa = section_map(ia)
    sb = section_map(ib)
    for name in sorted(set(sa) | set(sb)):
        if name in sa and name not in sb:
            diff["sections"]["removed"].append(sa[name])
        elif name in sb and name not in sa:
            diff["sections"]["added"].append(sb[name])
        elif sa[name].get("size") != sb[name].get("size"):
            diff["sections"]["changed"].append({
                "name": name,
                "size_a": sa[name].get("size"),
                "size_b": sb[name].get("size"),
                "delta": (sb[name].get("size") or 0) - (sa[name].get("size") or 0),
            })

    if args.json:
        print(json.dumps(diff, indent=2, ensure_ascii=False))
        return 0

    print("A: %s" % ia["file"])
    print("B: %s" % ib["file"])
    print("Size: %d -> %d (delta %d)" % (ia["_size"], ib["_size"], ib["_size"] - ia["_size"]))
    print("Container: %s -> %s" % (ia["container"], ib["container"]))
    print("Charset UCS2: %s -> %s" % (ia["charset_ucs2"], ib["charset_ucs2"]))
    if diff["summary"]:
        print("Summary changes:")
        for k, v in diff["summary"].items():
            print("  %s: %r -> %r" % (k, v["a"], v["b"]))
    if diff["tags"]["added"]:
        print("Tags added:")
        for t in diff["tags"]["added"]:
            print("  0x%02X %s" % (t["id"], t["name"]))
    if diff["tags"]["removed"]:
        print("Tags removed:")
        for t in diff["tags"]["removed"]:
            print("  0x%02X %s" % (t["id"], t["name"]))
    if diff["tags"]["changed"]:
        print("Tags changed:")
        for c in diff["tags"]["changed"]:
            print("  0x%02X %s: %r -> %r" % (c["id"], c["a"]["name"], c["a"].get("value"), c["b"].get("value")))
    if diff["sections"]["added"]:
        print("Sections added:")
        for s in diff["sections"]["added"]:
            print("  %s (%d bytes)" % (s["name"], s["size"]))
    if diff["sections"]["removed"]:
        print("Sections removed:")
        for s in diff["sections"]["removed"]:
            print("  %s (%d bytes)" % (s["name"], s["size"]))
    if diff["sections"]["changed"]:
        print("Sections changed:")
        for c in diff["sections"]["changed"]:
            print("  %s: %d -> %d (delta %d)" % (c["name"], c["size_a"], c["size_b"], c["delta"]))
    if ia["_icon_sha256"] or ib["_icon_sha256"]:
        print("Icon SHA256: %s -> %s%s" % (
            ia["_icon_sha256"], ib["_icon_sha256"],
            " (changed)" if ia["_icon_sha256"] != ib["_icon_sha256"] else " (same)"))
    if (not diff["summary"] and not diff["tags"]["added"] and not diff["tags"]["removed"] and
            not diff["tags"]["changed"] and not diff["sections"]["added"] and
            not diff["sections"]["removed"] and not diff["sections"]["changed"] and
            ia["_icon_sha256"] == ib["_icon_sha256"] and ia["_size"] == ib["_size"]):
        print("No differences found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
