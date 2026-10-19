#!/usr/bin/env python3
"""vxpdisasm - Disassemble ARM/Thumb machine code in MediaTek MRE .vxp packages.

Supports both uncompressed ELF packages and compressed ADS packages (with
automatic zlib decompression of RO/RW segments), as well as raw ELF/binaries.
Provides flexible addressing, context surrounding crash PCs, PC-relative literal
pool annotation, auto-detection of ARM vs Thumb modes, and instruction count controls.
"""

import argparse
import os
import re
import struct
import sys
import zlib

try:
    from capstone import (
        Cs,
        CS_ARCH_ARM,
        CS_MODE_ARM,
        CS_MODE_THUMB,
    )
    HAVE_CAPSTONE = True
except ImportError:
    HAVE_CAPSTONE = False


def read_le32(data, off):
    if off + 4 > len(data):
        return 0
    return struct.unpack_from("<I", data, off)[0]


def parse_trailer(data):
    if len(data) < 12:
        return None
    tags_offset, reserved0, reserved1 = struct.unpack_from("<III", data, len(data) - 12)
    return {
        "tags_offset": tags_offset,
        "reserved0": reserved0,
        "reserved1": reserved1,
    }


def parse_ads_descriptor(data, trailer):
    if trailer is None:
        return None
    to = trailer["tags_offset"]
    if to > len(data) or to < 4 + 36:
        return None
    elf_info_size = struct.unpack_from("<I", data, to - 4)[0]
    if elf_info_size != 36:
        return None
    off = to - 4 - 36
    if off < 0 or off + 36 > len(data):
        return None
    fields = struct.unpack_from("<9I", data, off)
    names = [
        "ro_offset", "ro_size", "org_ro_size",
        "rw_offset", "rw_size", "org_rw_size",
        "zi_size", "res_offset", "res_size",
    ]
    return dict(zip(names, fields))


def parse_elf_sections(data):
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        return None, 0
    entry = read_le32(data, 0x18)
    shoff = read_le32(data, 0x20)
    shentsize = struct.unpack_from("<H", data, 0x2E)[0]
    shnum = struct.unpack_from("<H", data, 0x30)[0]
    shstrndx = struct.unpack_from("<H", data, 0x32)[0]
    if shentsize < 40 or shnum == 0 or shstrndx >= shnum:
        return None, entry
    str_entry = shoff + shstrndx * shentsize
    if str_entry + 24 > len(data):
        return None, entry
    stroff = read_le32(data, str_entry + 16)
    strsize = read_le32(data, str_entry + 20)
    if strsize == 0 or stroff + strsize > len(data):
        return None, entry

    sections = []
    for i in range(shnum):
        o = shoff + i * shentsize
        if o + 40 > len(data):
            break
        name_idx = read_le32(data, o)
        sh_type = read_le32(data, o + 4)
        sh_flags = read_le32(data, o + 8)
        sh_addr = read_le32(data, o + 12)
        sh_offset = read_le32(data, o + 16)
        sh_size = read_le32(data, o + 20)
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
    return sections, entry


class BinaryImage:
    """Represents the loaded memory image of an MRE package (ELF or ADS)."""

    def __init__(self, raw_data):
        self.raw_data = raw_data
        self.segments = []  # list of (vaddr, size, bytes, name)
        self.entry_point = 0
        self.sections = []
        self.is_ads = False
        self._load()

    def _load(self):
        trailer = parse_trailer(self.raw_data)
        ads_info = parse_ads_descriptor(self.raw_data, trailer)

        if ads_info:
            self.is_ads = True
            # Decompress RO segment
            ro_data = b""
            if ads_info["ro_size"] > 0:
                try:
                    cdata = self.raw_data[ads_info["ro_offset"]:ads_info["ro_offset"] + ads_info["ro_size"]]
                    ro_data = zlib.decompress(cdata)
                except Exception as e:
                    print(f"[warning] Failed to decompress ADS RO segment: {e}", file=sys.stderr)
            # Decompress RW segment
            rw_data = b""
            if ads_info["rw_size"] > 0:
                try:
                    cdata = self.raw_data[ads_info["rw_offset"]:ads_info["rw_offset"] + ads_info["rw_size"]]
                    rw_data = zlib.decompress(cdata)
                except Exception as e:
                    print(f"[warning] Failed to decompress ADS RW segment: {e}", file=sys.stderr)

            # In ADS mode, RO is mapped at 0x00000000, RW is mapped right after org_ro_size
            self.entry_point = 0
            if ro_data:
                self.segments.append((0x00000000, len(ro_data), ro_data, "RO"))
            if rw_data:
                self.segments.append((ads_info["org_ro_size"], len(rw_data), rw_data, "RW"))
            return

        # ELF uncompressed
        sections, entry = parse_elf_sections(self.raw_data)
        self.entry_point = entry
        if sections:
            self.sections = sections
            for s in sections:
                if s["size"] > 0 and s["offset"] + s["size"] <= len(self.raw_data):
                    sbytes = self.raw_data[s["offset"]:s["offset"] + s["size"]]
                    self.segments.append((s["addr"], s["size"], sbytes, s["name"]))
            return

        # Raw binary fallback
        self.segments.append((0x00000000, len(self.raw_data), self.raw_data, "RAW"))

    def read_bytes(self, vaddr, length):
        for base, size, sbytes, name in self.segments:
            if base <= vaddr < base + size:
                rel = vaddr - base
                avail = min(length, size - rel)
                return sbytes[rel:rel + avail]
        return b""

    def vaddr_to_segment(self, vaddr):
        for base, size, sbytes, name in self.segments:
            if base <= vaddr < base + size:
                return base, size, sbytes, name
        return None

    def read_u32(self, vaddr):
        b = self.read_bytes(vaddr, 4)
        if len(b) == 4:
            return struct.unpack("<I", b)[0]
        return None


def detect_mode(image, vaddr, explicit_mode):
    """Determine whether target code is ARM or Thumb."""
    if explicit_mode == "thumb":
        return True
    if explicit_mode == "arm":
        return False

    # Auto detection:
    # 1. If low bit is 1, it's explicitly a Thumb address (ARM BX/BLX interworking convention).
    if vaddr & 1:
        return True

    # 2. If address is not 4-byte aligned, it MUST be Thumb (ARM instructions must be 4-byte aligned).
    if vaddr % 4 != 0:
        return True

    # 3. Check the 32-bit opcode at vaddr.
    # In standard ARM code, most instructions are unconditional AL condition (0xE...):
    # e.g., 0xE1A0... (MOV), 0xE28F... (ADD), 0xE59... (LDR), 0xE92D... (PUSH/STMDB), 0xEB... (BL)
    w0 = image.read_u32(vaddr)
    w1 = image.read_u32(vaddr + 4)
    if w0 is not None:
        cond0 = (w0 >> 28) & 0xF
        cond1 = ((w1 >> 28) & 0xF) if w1 is not None else 0xE
        # Standard ARM unconditional or common branch condition
        if cond0 == 0xE or (cond0 in (0x0, 0x1, 0xA, 0xB) and cond1 == 0xE):
            return False

    # Default to Thumb for general MRE code if ambiguous
    return True


def annotate_insn(image, insn, is_thumb):
    """Annotate literal pool loads (e.g. ldr rX, [pc, #...])."""
    op_str = insn.op_str
    if insn.mnemonic == "ldr" and "[pc" in op_str:
        pc_base = (insn.address + 4) & ~3 if is_thumb else (insn.address + 8)
        m = re.search(r"\[pc,\s*#?(-?0x[0-9a-fA-F]+|-?\d+)\]", op_str)
        if m:
            val_str = m.group(1)
            offset = int(val_str, 0)
            target = pc_base + offset
            val = image.read_u32(target)
            if val is not None:
                # Also check if value might be an ASCII string
                str_candidate = image.read_bytes(val, 32)
                str_anno = ""
                if str_candidate and str_candidate[0:1].isascii() and str_candidate[0] >= 32:
                    end = str_candidate.find(b"\x00")
                    if end > 0:
                        try:
                            s = str_candidate[:end].decode("ascii", errors="replace")
                            str_anno = f' -> "{s}"'
                        except Exception:
                            pass
                return f"; [0x{target:08X}] = 0x{val:08X}{str_anno}"
    return ""


def disassemble(image, target_vaddr, count=40, mode_name="auto", before=0, no_bytes=False, annotate=True):
    if not HAVE_CAPSTONE:
        print("vxpdisasm: capstone library is required. Install via: pip install capstone", file=sys.stderr)
        return 1

    is_thumb = detect_mode(image, target_vaddr, mode_name)
    vaddr = target_vaddr & ~1

    mode = CS_MODE_THUMB if is_thumb else CS_MODE_ARM
    cs = Cs(CS_ARCH_ARM, mode)
    cs.detail = False
    cs.skipdata = True  # don't abort on literal data words embedded between functions

    seg = image.vaddr_to_segment(vaddr)
    if not seg:
        print(f"vxpdisasm: address 0x{vaddr:08X} not found in any mapped segment", file=sys.stderr)
        return 1

    base, size, sbytes, seg_name = seg

    # Calculate starting address with context before
    if before > 0:
        if not is_thumb:
            start_vaddr = max(base, vaddr - before * 4)
            start_vaddr &= ~3
        else:
            # Thumb has 2 or 4 byte instructions. Step back and ensure synchronization with vaddr.
            start_vaddr = max(base, vaddr - before * 4)
            start_vaddr &= ~1
    else:
        start_vaddr = vaddr

    # Calculate buffer to disassemble
    max_insns = before + count + 10
    read_len = max_insns * 4
    code_slice = image.read_bytes(start_vaddr, read_len)
    if not code_slice:
        print(f"vxpdisasm: no code bytes at 0x{start_vaddr:08X}", file=sys.stderr)
        return 1

    all_insns = list(cs.disasm(code_slice, start_vaddr))
    if not all_insns:
        print(f"vxpdisasm: unable to decode instructions at 0x{start_vaddr:08X}", file=sys.stderr)
        return 1

    # Find the target instruction index
    target_idx = None
    for idx, insn in enumerate(all_insns):
        if insn.address == vaddr:
            target_idx = idx
            break

    if target_idx is not None:
        start_idx = max(0, target_idx - before)
        end_idx = min(len(all_insns), target_idx + count)
        display_insns = all_insns[start_idx:end_idx]
    else:
        display_insns = all_insns[:count]

    mode_str = "Thumb" if is_thumb else "ARM"
    print(f"Disassembly of [{seg_name}] at 0x{vaddr:08X} ({mode_str} mode):")

    for insn in display_insns:
        is_target = (insn.address == vaddr)
        marker = "=>" if is_target else "  "
        hex_str = insn.bytes.hex(" ") if not no_bytes else ""
        annotation = annotate_insn(image, insn, is_thumb) if annotate else ""
        if no_bytes:
            print(f"{marker} 0x{insn.address:08X}:  {insn.mnemonic:<8} {insn.op_str:<24} {annotation}")
        else:
            print(f"{marker} 0x{insn.address:08X}:  {hex_str:<18} {insn.mnemonic:<8} {insn.op_str:<24} {annotation}")

    return 0


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

    ap = argparse.ArgumentParser(
        description="Disassemble ARM/Thumb code from MRE .vxp packages or raw binaries with Capstone."
    )
    ap.add_argument("path", help="Path to .vxp, .elf, or raw .bin file")
    ap.add_argument("-a", "--addr", type=lambda x: int(x, 0), default=None,
                    help="Target virtual address to disassemble (hex or decimal, default: entry point)")
    ap.add_argument("-n", "--count", type=int, default=40,
                    help="Number of instructions to disassemble (default: 40)")
    ap.add_argument("-m", "--mode", choices=["auto", "thumb", "arm"], default="auto",
                    help="Instruction set mode: auto, thumb, or arm (default: auto)")
    ap.add_argument("-B", "-C", "--before", type=int, default=0,
                    help="Number of context instructions to show before target address")
    ap.add_argument("-o", "--offset", type=lambda x: int(x, 0), default=None,
                    help="Direct file offset to disassemble")
    ap.add_argument("-s", "--section", help="Disassemble by ELF section name (e.g. ER_RO, .text)")
    ap.add_argument("--no-bytes", action="store_true", help="Hide instruction opcode bytes")
    ap.add_argument("--no-annotate", action="store_true", help="Disable PC-relative constant annotation")
    ap.add_argument("--list-segments", action="store_true", help="List all mapped executable segments and exit")

    args = ap.parse_args()

    if not os.path.isfile(args.path):
        print(f"vxpdisasm: file not found: {args.path}", file=sys.stderr)
        return 2

    raw_data = open(args.path, "rb").read()
    image = BinaryImage(raw_data)

    if args.list_segments:
        print(f"File: {os.path.basename(args.path)} ({'ADS Compressed' if image.is_ads else 'ELF / Flat'})")
        print(f"Entry point: 0x{image.entry_point:08X}")
        print(f"{'Idx':<4} {'Segment / Section':<20} {'VAddr':<12} {'Size':<10}")
        for i, (base, size, _, name) in enumerate(image.segments):
            print(f"{i:<4} {name:<20} 0x{base:08X}   {size:<10} ({size / 1024:.1f} KB)")
        return 0

    target_addr = args.addr
    if args.offset is not None:
        target_addr = args.offset
    elif args.section:
        found = False
        for base, size, _, name in image.segments:
            if name.lower() == args.section.lower():
                target_addr = base
                found = True
                break
        if not found:
            print(f"vxpdisasm: section '{args.section}' not found in package", file=sys.stderr)
            return 1
    elif target_addr is None:
        target_addr = image.entry_point

    return disassemble(
        image=image,
        target_vaddr=target_addr,
        count=args.count,
        mode_name=args.mode,
        before=args.before,
        no_bytes=args.no_bytes,
        annotate=not args.no_annotate,
    )


if __name__ == "__main__":
    sys.exit(main())
