#!/usr/bin/env python3
"""vxpstrings - extract printable strings from a .vxp package.

Scans the whole file (or just the .vm_res resource block with --vm-res) for
ASCII and/or UTF-16LE (UCS-2) runs of at least --min characters.
"""

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


def elf_vm_res(data):
    if vxpicon is not None:
        r = vxpicon.elf_res_section(data)
        if r:
            return r
    # fallback minimal
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        return None
    shoff = read_le32(data, len(data), 0x20)
    shentsize = struct.unpack_from("<H", data, 0x2E)[0]
    shnum = struct.unpack_from("<H", data, 0x30)[0]
    shstrndx = struct.unpack_from("<H", data, 0x32)[0]
    if shentsize < 40 or shnum == 0 or shstrndx >= shnum:
        return None
    str_entry = shoff + shstrndx * shentsize
    if str_entry + 24 > len(data):
        return None
    stroff = read_le32(data, len(data), str_entry + 16)
    strsize = read_le32(data, len(data), str_entry + 20)
    if strsize == 0 or stroff + strsize > len(data):
        return None
    for i in range(shnum):
        o = shoff + i * shentsize
        if o + 24 > len(data):
            continue
        name_idx = read_le32(data, len(data), o)
        if name_idx >= strsize:
            continue
        base = stroff + name_idx
        max_len = strsize - name_idx
        if max_len >= 7 and data[base:base + 7] == b".vm_res" and (max_len == 7 or data[base + 7:base + 8] == b"\x00"):
            return read_le32(data, len(data), o + 16), read_le32(data, len(data), o + 20)
    return None


def ads_vm_res(data, tags_offset):
    if vxpicon is not None:
        r = vxpicon.ads_res_section(data, tags_offset)
        if r:
            return r
    if tags_offset < 4 or read_le32(data, len(data), tags_offset - 4) != 36:
        return None
    if tags_offset - 4 < 36:
        return None
    base = tags_offset - 4 - 36
    if base + 36 > len(data):
        return None
    off = read_le32(data, len(data), base + 28)
    sz = read_le32(data, len(data), base + 32)
    return off, sz


def ascii_strings(data, min_len):
    out = []
    start = None
    for i in range(len(data) + 1):
        b = data[i] if i < len(data) else 0
        if 32 <= b < 127:
            if start is None:
                start = i
        else:
            if start is not None and i - start >= min_len:
                out.append((start, data[start:i].decode("ascii")))
            start = None
    return out


def ucs2_strings(data, min_len):
    out = []
    start = None
    count = 0
    i = 0
    n = len(data)
    while i < n:
        if i + 1 < n and data[i + 1] == 0 and 32 <= data[i] < 127:
            if start is None:
                start = i
            count += 1
            i += 2
            continue
        if start is not None and count >= min_len:
            raw = data[start:start + count * 2]
            try:
                out.append((start, raw.decode("utf-16-le")))
            except UnicodeDecodeError:
                pass
        start = None
        count = 0
        i += 1
    if start is not None and count >= min_len:
        raw = data[start:start + count * 2]
        try:
            out.append((start, raw.decode("utf-16-le")))
        except UnicodeDecodeError:
            pass
    return out


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(description="Extract printable strings from a .vxp package.")
    ap.add_argument("path", help="path to .vxp file")
    ap.add_argument("--min", type=int, default=4, help="minimum string length (default 4)")
    ap.add_argument("--encoding", choices=["ascii", "ucs2", "all"], default="all")
    ap.add_argument("--vm-res", action="store_true", help="scan only the .vm_res resource block")
    ap.add_argument("--limit", type=int, default=0, help="max strings to print (0 = all)")
    args = ap.parse_args()

    if not os.path.isfile(args.path):
        print("vxpstrings: file not found: %s" % args.path, file=sys.stderr)
        return 2
    data = open(args.path, "rb").read()

    scan_data = data
    base_off = 0
    if args.vm_res:
        tags_offset = struct.unpack_from("<I", data, len(data) - 12)[0] if len(data) >= 12 else 0
        is_elf = data[:4] == b"\x7fELF"
        r = elf_vm_res(data) if is_elf else ads_vm_res(data, tags_offset)
        if not r:
            print("vxpstrings: no .vm_res block found", file=sys.stderr)
            return 1
        off, sz = r
        if off + sz > len(data):
            print("vxpstrings: invalid .vm_res bounds", file=sys.stderr)
            return 1
        scan_data = data[off:off + sz]
        base_off = off

    results = []
    if args.encoding in ("ascii", "all"):
        for off, s in ascii_strings(scan_data, args.min):
            results.append((base_off + off, "ascii", s))
    if args.encoding in ("ucs2", "all"):
        for off, s in ucs2_strings(scan_data, args.min):
            results.append((base_off + off, "ucs2", s))

    results.sort(key=lambda x: x[0])
    if args.limit and len(results) > args.limit:
        results = results[:args.limit]

    for off, enc, s in results:
        print("0x%08X  %-5s  %s" % (off, enc, s))
    print("# %d strings" % len(results), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
