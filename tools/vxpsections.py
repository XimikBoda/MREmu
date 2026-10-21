#!/usr/bin/env python3
"""vxpsections - dump ELF sections of a .vxp and optionally disassemble.

Targets the 32-bit little-endian ARM ELFs used by MRE.  With --disasm it
decodes a section (ER_RO by default) using capstone if it is installed.
"""

import argparse
import os
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_ARM
    HAVE_CAPSTONE = True
except Exception:
    HAVE_CAPSTONE = False


def read_le32(data, size, off):
    if off + 4 > size:
        return 0
    return struct.unpack_from("<I", data, off)[0]


def parse_elf(data):
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
    sections = []
    for i in range(shnum):
        o = shoff + i * shentsize
        if o + 40 > len(data):
            break
        name_idx = read_le32(data, len(data), o)
        sh_type = read_le32(data, len(data), o + 4)
        sh_flags = read_le32(data, len(data), o + 8)
        sh_addr = read_le32(data, len(data), o + 12)
        sh_offset = read_le32(data, len(data), o + 16)
        sh_size = read_le32(data, len(data), o + 20)
        name = ""
        if name_idx < strsize:
            base = stroff + name_idx
            end = data.find(b"\x00", base, stroff + strsize)
            if end < 0:
                end = stroff + strsize
            name = data[base:end].decode("utf-8", errors="replace")
        sections.append({
            "index": i, "name": name, "type": sh_type, "flags": sh_flags,
            "addr": sh_addr, "offset": sh_offset, "size": sh_size,
        })
    entry = read_le32(data, len(data), 0x18)
    return {"entry": entry, "sections": sections}


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(description="Dump ELF sections of a .vxp and optionally disassemble.")
    ap.add_argument("path", help="path to .vxp file")
    ap.add_argument("--disasm", action="store_true", help="disassemble a section")
    ap.add_argument("--section", help="section name to disassemble (default ER_RO)")
    ap.add_argument("--offset", type=lambda x: int(x, 0), default=0, help="file offset if no section (default 0)")
    ap.add_argument("--size", type=int, default=256, help="bytes to disassemble (default 256)")
    ap.add_argument("--max-insns", type=int, default=0, help="max instructions (0 = all in range)")
    ap.add_argument("--mode", choices=["thumb", "arm"], default="thumb", help="disassembly mode (default thumb)")
    args = ap.parse_args()

    if not os.path.isfile(args.path):
        print("vxpsections: file not found: %s" % args.path, file=sys.stderr)
        return 2
    data = open(args.path, "rb").read()
    info = parse_elf(data)
    if not info:
        print("vxpsections: not an ELF32 LE VXP", file=sys.stderr)
        return 1

    print("Entry: 0x%08X" % info["entry"])
    print("Idx  %-18s %10s %10s %10s" % ("Name", "Offset", "Size", "Addr"))
    for s in info["sections"]:
        print("%-4d %-18s 0x%08X %10d 0x%08X" % (s["index"], s["name"] or "(noname)", s["offset"], s["size"], s["addr"]))

    if not args.disasm:
        return 0

    if not HAVE_CAPSTONE:
        print("vxpsections: capstone not installed; cannot disassemble", file=sys.stderr)
        return 1

    # pick slice
    sec = None
    if args.section:
        for s in info["sections"]:
            if s["name"] == args.section:
                sec = s
                break
        if sec is None:
            print("vxpsections: section not found: %s" % args.section, file=sys.stderr)
            return 1
    elif not args.offset and not args.section:
        for s in info["sections"]:
            if s["name"] == "ER_RO":
                sec = s
                break

    if sec is not None:
        off = sec["offset"]
        sz = sec["size"]
        base_addr = sec["addr"]
        label = sec["name"]
    else:
        off = args.offset
        sz = args.size
        base_addr = args.offset
        label = "offset 0x%X" % off

    if args.size and args.size < sz and not args.section:
        sz = args.size
    n = min(sz, len(data) - off)
    if n <= 0:
        print("vxpsections: nothing to disassemble", file=sys.stderr)
        return 1
    if args.size and sec is not None:
        n = min(n, args.size)

    mode = CS_MODE_THUMB if args.mode == "thumb" else CS_MODE_ARM
    md = Cs(CS_ARCH_ARM, mode)
    md.detail = False
    print("\nDisassembly of %s (%d bytes):" % (label, n))
    count = 0
    for insn in md.disasm(data[off:off + n], base_addr):
        print("0x%08X:  %-24s %s %s" % (insn.address, insn.bytes.hex(" "), insn.mnemonic, insn.op_str))
        count += 1
        if args.max_insns and count >= args.max_insns:
            break
    if count == 0:
        print("(no instructions decoded)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
