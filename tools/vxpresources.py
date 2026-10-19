#!/usr/bin/env python3
"""vxpresources - list and extract entries from a .vxp .vm_res resource block."""

import argparse
import os
import struct
import sys

try:
    import vxpicon
except Exception:
    vxpicon = None


def read_le32(data, size, off):
    if off + 4 > size:
        return 0
    return struct.unpack_from("<I", data, off)[0]


def tags_offset_of(data):
    if len(data) < 12:
        return 0
    return read_le32(data, len(data), len(data) - 12)


def vm_res_block(data):
    is_elf = data[:4] == b"\x7fELF"
    to = tags_offset_of(data)
    if is_elf:
        if vxpicon is not None:
            r = vxpicon.elf_res_section(data)
        else:
            r = None
    else:
        if vxpicon is not None:
            r = vxpicon.ads_res_section(data, to)
        else:
            r = None
    return r


def resource_dir(data):
    r = vm_res_block(data)
    if not r:
        return None, []
    off, sz = r
    if off + sz > len(data):
        return r, []
    if vxpicon is not None:
        res = vxpicon.read_resource_dir(data, off, sz)
    else:
        res = []
    return r, res


def guess_type(blob):
    if blob[:4] == b"GIF8":
        return "gif"
    if blob[:8] == b"\x89PNG\r\n\x1a\n":
        return "png"
    if blob[:3] == b"\xff\xd8\xff":
        return "jpg"
    if blob[:2] == b"BM":
        return "bmp"
    # heuristic: all printable?
    if all(32 <= b < 127 or b in (9, 10, 13) for b in blob[:64]):
        return "text"
    return "bin"


def safe_name(name):
    return "".join(c if c.isalnum() or c in "._-" else "_" for c in name)


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(description="List/extract resources from a .vxp .vm_res block.")
    ap.add_argument("path", help="path to .vxp file")
    ap.add_argument("--list", action="store_true", help="list resources (default)")
    ap.add_argument("--extract", metavar="NAME", help="extract one resource by name")
    ap.add_argument("--extract-all", action="store_true", help="extract all resources")
    ap.add_argument("-o", "--outdir", default=".", help="output directory for extraction")
    args = ap.parse_args()

    if not os.path.isfile(args.path):
        print("vxpresources: file not found: %s" % args.path, file=sys.stderr)
        return 2
    data = open(args.path, "rb").read()
    r, resources = resource_dir(data)
    if r is None:
        print("vxpresources: no .vm_res block found", file=sys.stderr)
        return 1
    block_off, block_sz = r
    print("vm_res block: offset=0x%X size=%d resources=%d" % (block_off, block_sz, len(resources)))

    if args.extract or args.extract_all:
        os.makedirs(args.outdir, exist_ok=True)

    if args.extract:
        matches = [x for x in resources if x[0] == args.extract]
        if not matches:
            print("vxpresources: resource not found: %s" % args.extract, file=sys.stderr)
            return 1
        for name, off, sz in matches:
            blob = data[off:off + sz]
            out = os.path.join(args.outdir, safe_name(name))
            with open(out, "wb") as f:
                f.write(blob)
            print("extracted %s (%d bytes) -> %s" % (name, sz, out))

    if args.extract_all:
        for name, off, sz in resources:
            blob = data[off:off + sz]
            out = os.path.join(args.outdir, safe_name(name) or "resource_%d.bin" % off)
            with open(out, "wb") as f:
                f.write(blob)
            print("extracted %s (%d bytes, %s) -> %s" % (name, sz, guess_type(blob), out))

    if not args.extract and not args.extract_all:
        print("%-4s %-28s %10s %10s  %s" % ("Idx", "Name", "Offset", "Size", "Type"))
        for i, (name, off, sz) in enumerate(resources):
            blob = data[off:off + sz]
            print("%-4d %-28s 0x%08X %10d  %s" % (i, name, off, sz, guess_type(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
