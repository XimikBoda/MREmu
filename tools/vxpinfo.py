#!/usr/bin/env python3
"""vxpinfo - inspect a MediaTek MRE .vxp package.

Prints the file envelope, MRE tag table, (for compressed ADS packages) the
compressed-region descriptor, and (for uncompressed ELF packages) the ELF
header and section table.  Output is human-readable by default; pass --json for
machine-readable output.
"""

import argparse
import glob
import hashlib
import json
import os
import struct
import sys


# Tag IDs follow the MediaTek MRE SDK (vmcert.h), which is what MREmu uses.
TAG_NAMES = {
    0x00: "VM_CE_INFO_END",
    0x01: "VM_CE_INFO_DEV",
    0x02: "VM_CE_INFO_APP_ID",
    0x03: "VM_CE_INFO_CERT_ID",
    0x04: "VM_CE_INFO_NAME",
    0x05: "VM_CE_INFO_VER",
    0x06: "VM_CE_INFO_ISSUE",
    0x07: "VM_CE_INFO_EXPIRED",
    0x08: "VM_CE_INFO_PAY",
    0x09: "VM_CE_INFO_PAY_NODE",
    0x0A: "VM_CE_INFO_PRICE",
    0x0B: "VM_CE_INFO_PAY_MODE",
    0x0C: "VM_CE_INFO_PAY_PARAM",
    0x0D: "VM_CE_INFO_APP_USE",
    0x0E: "VM_CE_INFO_PAY_CHANNEL",
    0x0F: "VM_CE_INFO_MEM_REQ",
    0x10: "VM_CE_INFO_RESOLUTION_REQ",
    0x11: "VM_CE_INFO_PLAT_REQ",
    0x12: "VM_CE_INFO_IMSI",
    0x13: "VM_CE_INFO_PERMISSION",
    0x14: "VM_CE_INFO_TRIAL",
    0x15: "VM_CE_INFO_COMPILER",
    0x16: "VM_CE_INFO_INPUT_MODE",
    0x17: "VM_CE_INFO_DESC",
    0x18: "VM_CE_INFO_SUPPORT_BG_RUN",
    0x19: "VM_CE_INFO_NAME_LIST",
    0x1A: "VM_CE_INFO_DESC_LIST",
    0x1B: "VM_CE_INFO_ZIMO_LIST",
    0x1C: "VM_CE_INFO_ROTATE",
    0x1D: "VM_CE_INFO_SM",
    0x1E: "VM_CE_INFO_SM_TYPE",
    0x1F: "VM_CE_INFO_SM_PRIVATE",
    0x20: "VM_CE_INFO_SM_CRYPTEXT",
    0x21: "VM_CE_INFO_FILE_TYPE",
    0x22: "VM_CE_INFO_RO_RW_ZIP",
    0x23: "VM_CE_INFO_CHARSET",
    0x24: "VM_CE_INFO_SUPPORT_VP",
    0x25: "VM_CE_INFO_SYS_FILE_SIZE",
    0x26: "VM_CE_INFO_MULTI_NAME_LIST",
    0x27: "VM_CE_INFO_URL",
    0x28: "VM_CE_INFO_UPDATE_INFO",
    0x29: "VM_CE_INFO_NO_SCREEN",
    0x2A: "VM_CE_INFO_VPP_TYPE",
    0x2C: "VM_CE_INFO_PUSH",
    0x2D: "VM_CE_INFO_PUSH_APPID",
    0x2E: "VM_CE_INFO_PUSH_SENDERID",
    0x2F: "VM_CE_INFO_RES_AB2",
    0x30: "VM_CE_INFO_BUILD_ID",
    0x31: "VM_CE_INFO_ADV_ICON",
    0x32: "VM_CE_INFO_AUTO_START",
}

# Tags whose payload is a (possibly NUL-terminated) text string.
STRING_TAGS = {0x01, 0x04, 0x17, 0x27, 0x28}
# Tags whose payload is a multi-language (lang_id, size, bytes) list.
MLIST_TAGS = {0x19, 0x1A, 0x1B, 0x26}

FILE_TYPE_NAMES = {
    0: "ADS",
    1: "ADS",
    5: "Simple ADS",
}

# SDK placeholder titles left in VM_CE_INFO_NAME when the real title was only
# given per language (the three PeanutVXP packages ship "App" this way).
GENERIC_NAMES = {
    "", "app", "application", "apps", "app name", "application name", "name",
    "title", "mre app", "mre application", "default", "untitled", "new app",
    "n/a", "na", "none", "null", "nil", "-", "--", "---", "?",
}

# First two bytes of a zlib/deflate stream. A compressed ADS package starts with
# one of these, so it is a usable fallback when the descriptor cannot be read.
ZLIB_MAGICS = (b"\x78\x01", b"\x78\x9c", b"\x78\xda", b"\x78\x5e")

ELF_MACHINE_NAMES = {
    0x28: "ARM",
    0x2A: "Thumb/ARM? (0x2A)",
    0xB7: "AArch64",
}

ELF_SECTION_TYPE_NAMES = {
    0: "NULL",
    1: "PROGBITS",
    2: "SYMTAB",
    3: "STRTAB",
    4: "RELA",
    5: "HASH",
    6: "DYNAMIC",
    7: "NOTE",
    8: "NOBITS",
    9: "REL",
    11: "DYNSYM",
    14: "INIT_ARRAY",
    15: "FINI_ARRAY",
}


def read_file(path):
    with open(path, "rb") as f:
        return f.read()


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def parse_trailer(data):
    if len(data) < 12:
        return None
    tags_offset, reserved0, reserved1 = struct.unpack_from("<III", data, len(data) - 12)
    return {
        "tags_offset": tags_offset,
        "reserved0": reserved0,
        "reserved1": reserved1,
    }


def decode_string(payload, ucs2):
    if ucs2:
        # UTF-16 LE; drop a single trailing NUL word if present for display.
        raw = payload
        if len(raw) >= 2 and raw[-2:] == b"\x00\x00":
            raw = raw[:-2]
        try:
            return raw.decode("utf-16-le")
        except UnicodeDecodeError:
            return raw.decode("utf-16-le", errors="replace")
    # ASCII / unknown: try utf-8, then latin-1 as a lossless fallback.
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError:
        text = payload.decode("latin-1")
    if text.endswith("\x00"):
        text = text[:-1]
    return text


def parse_name_list(payload, ucs2):
    out = []
    off = 0
    n = len(payload)
    while off + 8 <= n:
        lang_id, size = struct.unpack_from("<II", payload, off)
        off += 8
        if size > n - off:
            size = n - off
        raw = payload[off:off + size]
        off += size
        out.append({"lang_id": lang_id, "size": size, "name": decode_string(raw, ucs2)})
    return out


def decode_version(raw):
    """VM_CE_INFO_VER (0x05) packs a dotted version into one u32.

    Observed layouts:
      - 4-byte dotted: (b0<<24) | (b1<<16) | (b2<<8) | b3 (e.g. 0x02000100 -> 2.0.1.0)
      - 3-byte dotted: major<<16 | minor<<8 | patch (e.g. 0x00040400 -> 4.4.0, 0x00010000 -> 1.0.0)
      - Small integers: e.g. 111 (reported as '111')
    """
    b0 = (raw >> 24) & 0xFF
    b1 = (raw >> 16) & 0xFF
    b2 = (raw >> 8) & 0xFF
    b3 = raw & 0xFF

    if b0 > 0:
        text = "%d.%d.%d.%d" % (b0, b1, b2, b3) if b3 != 0 else "%d.%d.%d" % (b0, b1, b2)
    elif b1 == 0 and b2 == 0:
        text = str(raw)
    else:
        text = "%d.%d.%d" % (b1, b2, b3)

    return {
        "raw": raw,
        "hex": "0x%08X" % raw,
        "major": b1 if b0 == 0 else b0,
        "minor": b2 if b0 == 0 else b1,
        "patch": b3 if b0 == 0 else b2,
        "high_byte": b0,
        "text": text,
    }


def decode_resolution(raw):
    """VM_CE_INFO_RESOLUTION_REQ (0x10) specifies required screen resolution."""
    if raw == 0:
        return {"raw": 0, "width": 0, "height": 0, "text": "any"}
    w = (raw >> 16) & 0xFFFF
    h = raw & 0xFFFF
    text = "%dx%d" % (w, h) if (w > 0 and h > 0) else str(raw)
    return {"raw": raw, "width": w, "height": h, "text": text}



def is_generic_name(name):
    return name.strip().lower() in GENERIC_NAMES


def resolve_app_name(tags, ucs2):
    """Pick the app title across VM_CE_INFO_NAME (0x04), VM_CE_INFO_NAME_LIST
    (0x19) and VM_CE_INFO_MULTI_NAME_LIST (0x26).

    The single-string tag wins whenever it holds a real title.  Packages built
    from the SDK placeholder leave it as "App" while putting the real title in
    the per-language list, so a generic single-string name loses to a concrete
    list entry instead of shadowing it.
    """
    single = None
    if 0x04 in tags:
        single = decode_string(tags[0x04], ucs2).strip() or None

    listed = []
    for tid in (0x19, 0x26):
        if tid not in tags:
            continue
        for entry in parse_name_list(tags[tid], ucs2):
            name = entry["name"].strip()
            if name:
                listed.append((tid, entry["lang_id"], name))

    # A real title in the list beats a placeholder single name.
    if single and not is_generic_name(single):
        chosen, source = single, "VM_CE_INFO_NAME (0x04)"
    elif listed:
        tid, lang, name = listed[0]
        chosen, source = name, "VM_CE_INFO_NAME_LIST (0x%02X) lang %d" % (tid, lang)
    elif single:
        chosen, source = single, "VM_CE_INFO_NAME (0x04)"
    else:
        chosen, source = None, None

    return {
        "name": chosen,
        "source": source,
        "single": single,
        "single_is_generic": bool(single and is_generic_name(single)),
        "list_entries": [{"tag": "0x%02X" % t, "lang_id": l, "name": n} for t, l, n in listed],
    }


def resolve_description(tags, ucs2):
    """VM_CE_INFO_DESC (0x17) is free text, but MRE Studio also packs
    'KEY:value;' segments into it (VREDESC / VREPHONE / VRERANK / Branding), so
    those are split out as well.  VM_CE_INFO_DESC_LIST (0x1A) is the per-language
    equivalent."""
    text = None
    if 0x17 in tags:
        text = decode_string(tags[0x17], ucs2).strip() or None
    listed = []
    if 0x1A in tags:
        listed = [e["name"].strip() for e in parse_name_list(tags[0x1A], ucs2) if e["name"].strip()]
    if text is None and listed:
        text = listed[0]

    fields = {}
    if text:
        for chunk in text.split(";"):
            if ":" not in chunk:
                continue
            key, _, val = chunk.partition(":")
            key = key.strip()
            # MRE Studio packs KEY:value segments (VREDESC/VREPHONE/VRERANK/...),
            # so only treat a segment as a field when it looks like such a key.
            if key and (key.isupper() or "_" in key):
                fields[key] = val.strip()
    return {"text": text, "fields": fields, "list_entries": listed}


def parse_tags(data, trailer):
    tags = {}
    order = []
    if trailer is None:
        return tags, order, "tags_offset missing"
    pos = trailer["tags_offset"]
    n = len(data)
    if pos > n:
        return tags, order, "tags_offset out of range"
    err = None
    while True:
        if pos + 8 > n:
            err = "truncated tag header"
            break
        tid, tsize = struct.unpack_from("<II", data, pos)
        pos += 8
        if pos + tsize > n:
            err = "truncated tag payload"
            break
        payload = data[pos:pos + tsize]
        pos += tsize
        tags[tid] = payload
        order.append(tid)
        if tid == 0:
            break
    return tags, order, err


def tag_value_repr(tid, payload, ucs2):
    # Returns a JSON-friendly value plus a display string.
    if tid == 0:
        return None, "(end of tag stream)"
    if tid in MLIST_TAGS:
        nl = parse_name_list(payload, ucs2)
        return nl, None
    if tid in STRING_TAGS:
        s = decode_string(payload, ucs2)
        return s, repr(s)
    if len(payload) == 4:
        v = struct.unpack_from("<I", payload, 0)[0]
        extra = ""
        if tid == 0x21:
            extra = " (%s)" % FILE_TYPE_NAMES.get(v, "unknown")
        if tid == 0x15:
            extra = " (VRE/ADS)" if v == 1 else ""
        if tid == 0x23:
            extra = " (UCS-2 LE)" if v == 1 else " (ASCII)" if v == 0 else ""
        if tid == 0x22:
            extra = " (ADS)" if v else ""
        if tid == 0x0F:
            extra = " KB"
        if tid == 0x05:
            ver = decode_version(v)
            extra = " (%s) = %s" % (ver["hex"], ver["text"])
        if tid == 0x10:
            res = decode_resolution(v)
            extra = " (%s)" % res["text"]
        return v, "%d%s" % (v, extra)
    # Unknown: hex + printable ascii if any.
    hexs = payload.hex(" ")
    printable = "".join(chr(b) if 32 <= b < 127 else "." for b in payload)
    return {"hex": hexs, "ascii": printable}, hexs


def parse_ads_descriptor(data, trailer):
    if trailer is None:
        return None
    to = trailer["tags_offset"]
    # Layout: ... compress_ads_elf_info(36) | uint32 elf_info_size(4) | tags
    # The trailer's offset is untrusted input, so both ends are bounded before
    # anything is read through it.
    if to > len(data) or to < 4 + 36:
        return None
    elf_info_size = struct.unpack_from("<I", data, to - 4)[0]
    if elf_info_size != 36:
        return None
    off = to - 4 - 36
    if off < 0 or off + 36 > len(data):
        return None
    fields = struct.unpack_from("<9I", data, off)
    names = ["ro_offset", "ro_size", "org_ro_size", "rw_offset", "rw_size",
             "org_rw_size", "zi_size", "res_offset", "res_size"]
    return {"elf_info_size": elf_info_size, "descriptor_offset": off,
            **dict(zip(names, fields))}


def detect_container(data, trailer):
    """Classify the container from the file magic, not from the tag table.

    The old code called anything that was not ELF 'ads_compressed', which
    mislabelled non-VXP payloads such as the MSVC DLL that ships as
    GravityDefied.vc.vxp (magic 'MZ').
    """
    if data[:4] == b"\x7fELF":
        return "elf_uncompressed", None
    if data[:2] == b"MZ":
        return "pe_dll", "Windows PE/DLL image, not a VXP container"
    if parse_ads_descriptor(data, trailer) is not None:
        return "ads_compressed", None
    if data[:2] in ZLIB_MAGICS:
        return "ads_compressed", "ADS descriptor not found; container inferred from the zlib stream magic"
    if len(data) < 12:
        return "unknown", "file is too small to carry a VXP trailer"
    return "unknown", "magic is neither ELF, PE nor a valid ADS descriptor"


def probe_icon(data):
    """Report whether an app icon is embedded, and in what format.

    Delegates to vxpicon so there is one implementation of the discovery rules
    (VREAPPLOGO scan, then the ranked .vm_res walk); falls back to a prefix-only
    probe when vxpinfo is copied away from its siblings.
    """
    try:
        import vxpicon
    except Exception:
        vxpicon = None
    if vxpicon is not None:
        img = vxpicon.extract_icon(data)
        if not img:
            return {"present": False, "format": None, "size": 0}
        return {
            "present": True,
            "format": vxpicon.image_ext(img).lstrip("."),
            "size": len(img),
            "sha256": hashlib.sha256(img).hexdigest(),
            "source": "vxpicon",
        }
    idx = data.find(b"VREAPPLOGO")
    if idx < 0:
        return {"present": False, "format": None, "size": 0,
                "note": "vxpicon.py unavailable; only the VREAPPLOGO marker was searched"}
    return {"present": None, "format": None, "size": 0,
            "note": "vxpicon.py unavailable; VREAPPLOGO marker found at 0x%X" % idx}


def parse_elf(data):
    if data[:4] != b"\x7fELF":
        return None
    ei_class = data[4]
    ei_data = data[5]
    endian = "<" if ei_data == 1 else ">"
    out = {"class": "ELF32" if ei_class == 1 else "ELF64" if ei_class == 2 else str(ei_class),
           "data_encoding": "little" if ei_data == 1 else "big" if ei_data == 2 else str(ei_data)}
    if ei_class != 1:
        out["note"] = "only ELF32 is fully parsed"
        return out
    (e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags,
     e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = \
        struct.unpack_from(endian + "HHIIIIIHHHHHH", data, 16)
    out.update({
        "type": e_type,
        "machine": e_machine,
        "machine_name": ELF_MACHINE_NAMES.get(e_machine, "unknown"),
        "version": e_version,
        "entry": e_entry,
        "phoff": e_phoff,
        "shoff": e_shoff,
        "flags": e_flags,
        "ehsize": e_ehsize,
        "phentsize": e_phentsize,
        "phnum": e_phnum,
        "shentsize": e_shentsize,
        "shnum": e_shnum,
        "shstrndx": e_shstrndx,
    })
    # section headers
    sections = []
    shstrtab = b""
    if 0 <= e_shstrndx < e_shnum:
        idx_off = e_shoff + e_shstrndx * e_shentsize
        if idx_off + e_shentsize <= len(data):
            sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize = \
                struct.unpack_from(endian + "IIIIIIIIII", data, idx_off)
            if sh_offset + sh_size <= len(data):
                shstrtab = data[sh_offset:sh_offset + sh_size]
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        if off + e_shentsize > len(data):
            break
        (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize) = \
            struct.unpack_from(endian + "IIIIIIIIII", data, off)
        name = ""
        if shstrtab and sh_name < len(shstrtab):
            end = shstrtab.find(b"\x00", sh_name)
            if end < 0:
                end = len(shstrtab)
            name = shstrtab[sh_name:end].decode("utf-8", errors="replace")
        sections.append({
            "index": i,
            "name": name,
            "type": sh_type,
            "type_name": ELF_SECTION_TYPE_NAMES.get(sh_type, "0x%x" % sh_type),
            "flags": sh_flags,
            "addr": sh_addr,
            "offset": sh_offset,
            "size": sh_size,
            "link": sh_link,
            "info": sh_info,
            "addralign": sh_addralign,
            "entsize": sh_entsize,
        })
    out["sections"] = sections
    # program headers (brief)
    phdrs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        if off + e_phentsize > len(data):
            break
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = \
            struct.unpack_from(endian + "IIIIIIII", data, off)
        phdrs.append({
            "index": i,
            "type": p_type,
            "offset": p_offset,
            "vaddr": p_vaddr,
            "paddr": p_paddr,
            "filesz": p_filesz,
            "memsz": p_memsz,
            "flags": p_flags,
            "align": p_align,
        })
    out["program_headers"] = phdrs
    return out


def inspect(path):
    data = read_file(path)
    trailer = parse_trailer(data)
    tags, order, tag_err = parse_tags(data, trailer)
    ucs2 = False
    if 0x23 in tags and len(tags[0x23]) == 4:
        ucs2 = struct.unpack_from("<I", tags[0x23], 0)[0] == 1

    # Build tag list preserving order, de-duping the trailing END? keep all.
    tag_items = []
    for tid in order:
        payload = tags[tid]
        value, display = tag_value_repr(tid, payload, ucs2)
        item = {
            "id": tid,
            "id_hex": "0x%02X" % tid,
            "name": TAG_NAMES.get(tid, "UNKNOWN"),
            "size": len(payload),
            "value": value,
        }
        if display is not None:
            item["display"] = display
        tag_items.append(item)

    is_elf = data[:4] == b"\x7fELF"
    container, container_note = detect_container(data, trailer)

    info = {
        "file": os.path.abspath(path),
        "size": len(data),
        "sha256": sha256(data),
        "magic": data[:16].hex(" "),
        "container": container,
        "container_note": container_note,
        "trailer": trailer,
        "tag_error": tag_err,
        "charset_ucs2": ucs2,
        "tags": tag_items,
    }

    if container == "ads_compressed":
        info["ads_descriptor"] = parse_ads_descriptor(data, trailer)
    else:
        info["ads_descriptor"] = None

    elf_info = parse_elf(data) if is_elf else None
    info["elf"] = elf_info

    # Resolved metadata. Name and description fall back across the tag variants
    # because packages disagree about which one carries the real value.
    name_info = resolve_app_name(tags, ucs2)
    desc_info = resolve_description(tags, ucs2)
    app_name = name_info["name"]
    dev_name = None
    ram_kb = None
    if 0x01 in tags:
        dev_name = decode_string(tags[0x01], ucs2)
    if 0x0F in tags and len(tags[0x0F]) == 4:
        ram_kb = struct.unpack_from("<I", tags[0x0F], 0)[0]
    version = decode_version(struct.unpack_from("<I", tags[0x05], 0)[0]) \
        if 0x05 in tags and len(tags[0x05]) == 4 else None
    resolution = struct.unpack_from("<I", tags[0x10], 0)[0] \
        if 0x10 in tags and len(tags[0x10]) == 4 else None
    info["summary"] = {
        "app_name": app_name,
        "app_name_source": name_info["source"],
        "dev_name": dev_name,
        "ram_kb": ram_kb,
        "version": version["text"] if version else None,
        "version_raw": version["raw"] if version else None,
        "description": desc_info["text"],
        "description_fields": desc_info["fields"],
        "resolution": decode_resolution(resolution)["text"] if resolution is not None else None,
        "resolution_raw": resolution,
        "is_ads": (0x21 in tags and struct.unpack_from("<I", tags[0x21], 0)[0] in (0, 1, 5)) if (0x21 in tags and len(tags[0x21]) == 4) else None,
        "is_vre_ads": (0x15 in tags and struct.unpack_from("<I", tags[0x15], 0)[0] == 1) if (0x15 in tags and len(tags[0x15]) == 4) else None,
        "is_zipped": (0x22 in tags and struct.unpack_from("<I", tags[0x22], 0)[0] == 1) if (0x22 in tags and len(tags[0x22]) == 4) else None,
    }
    info["app_name_resolution"] = name_info
    info["description_resolution"] = desc_info
    info["icon"] = probe_icon(data)

    # vm_res location hint.
    vm_res = None
    if is_elf and elf_info and "sections" in elf_info:
        for s in elf_info["sections"]:
            if s["name"] == ".vm_res":
                vm_res = {"offset": s["offset"], "size": s["size"]}
    if vm_res is None and info["ads_descriptor"]:
        d = info["ads_descriptor"]
        vm_res = {"offset": d["res_offset"], "size": d["res_size"]}
    info["vm_res"] = vm_res

    return info


def human_print(info):
    print("File:        %s" % info["file"])
    print("Size:        %d bytes" % info["size"])
    print("SHA256:      %s" % info["sha256"])
    print("Magic:       %s" % info["magic"])
    print("Container:   %s" % info["container"])
    if info.get("container_note"):
        print("Note:        %s" % info["container_note"])
    t = info["trailer"]
    if t:
        print("Trailer:")
        print("  tags_offset: 0x%08X (%d)" % (t["tags_offset"], t["tags_offset"]))
        print("  reserved0:   0x%08X" % t["reserved0"])
        print("  reserved1:   0x%08X" % t["reserved1"])
    if info["tag_error"]:
        print("Tag parse:   %s" % info["tag_error"])
    print("Charset:     %s" % ("UCS-2 LE" if info["charset_ucs2"] else "ASCII"))
    s = info["summary"]
    print("Summary:")
    print("  app_name:   %s" % (s["app_name"] if s["app_name"] is not None else "(none)"))
    if s.get("app_name_source"):
        placeholder = ""
        if info["app_name_resolution"]["single_is_generic"]:
            placeholder = "  (0x04 held the generic placeholder %r)" % info["app_name_resolution"]["single"]
        print("    from:     %s%s" % (s["app_name_source"], placeholder))
    print("  dev_name:   %s" % (s["dev_name"] if s["dev_name"] is not None else "(none)"))
    print("  version:    %s" % (s["version"] if s["version"] is not None else "(none)"))
    if s.get("version_raw") is not None:
        print("    raw:      %d (0x%08X)" % (s["version_raw"], s["version_raw"]))
    print("  ram_kb:     %s" % (s["ram_kb"] if s["ram_kb"] is not None else "(none)"))
    print("  resolution: %s" % (s["resolution"] if s.get("resolution") is not None else "(none)"))
    print("  is_ads:     %s" % s["is_ads"])
    print("  is_vre_ads: %s" % s["is_vre_ads"])
    print("  is_zipped:  %s" % s["is_zipped"])
    if s.get("description"):
        print("  description: %s" % s["description"])
        for k, val in (s.get("description_fields") or {}).items():
            print("    %-9s %s" % (k + ":", val if val else "(empty)"))
    icon = info.get("icon") or {}
    if icon.get("present") is True:
        print("Icon:        %s, %d bytes (sha256 %s...)" % (icon["format"].upper(), icon["size"], icon["sha256"][:16]))
    elif icon.get("present") is False:
        print("Icon:        none")
    else:
        print("Icon:        unknown (%s)" % icon.get("note", "not probed"))
    if info["ads_descriptor"]:
        d = info["ads_descriptor"]
        print("ADS descriptor (offset 0x%X):" % d["descriptor_offset"])
        for k in ["ro_offset", "ro_size", "org_ro_size", "rw_offset", "rw_size",
                  "org_rw_size", "zi_size", "res_offset", "res_size"]:
            print("  %-12s %d (0x%X)" % (k, d[k], d[k]))
    if info["elf"]:
        e = info["elf"]
        print("ELF header:")
        print("  class:      %s" % e.get("class"))
        print("  machine:    %s (0x%X)" % (e.get("machine_name"), e.get("machine", 0)))
        print("  type:       %d" % e.get("type", 0))
        print("  entry:      0x%08X" % e.get("entry", 0))
        print("  phoff:      0x%08X (%d entries)" % (e.get("phoff", 0), e.get("phnum", 0)))
        print("  shoff:      0x%08X (%d entries)" % (e.get("shoff", 0), e.get("shnum", 0)))
        print("  flags:      0x%08X" % e.get("flags", 0))
        if "sections" in e:
            print("Sections:")
            print("  idx  %-18s %-10s %10s %10s %10s" % ("name", "type", "offset", "size", "addr"))
            for sec in e["sections"]:
                print("  %-4d %-18s %-10s 0x%08X %10d 0x%08X" % (
                    sec["index"], sec["name"] or "(noname)", sec["type_name"],
                    sec["offset"], sec["size"], sec["addr"]))
    if info["vm_res"]:
        vr = info["vm_res"]
        print("vm_res:      offset=0x%X size=%d" % (vr["offset"], vr["size"]))
    print("Tags (%d):" % len(info["tags"]))
    for item in info["tags"]:
        tid = item["id"]
        name = item["name"]
        size = item["size"]
        value = item["value"]
        if tid in MLIST_TAGS and isinstance(value, list):
            print("  0x%02X %-24s size=%-4d" % (tid, name, size))
            for nl in value:
                print("       lang=%d: %r" % (nl["lang_id"], nl["name"]))
        elif isinstance(value, str):
            print("  0x%02X %-24s size=%-4d %r" % (tid, name, size, value))
        elif isinstance(value, int):
            disp = item.get("display", str(value))
            print("  0x%02X %-24s size=%-4d %s" % (tid, name, size, disp))
        else:
            print("  0x%02X %-24s size=%-4d %s" % (tid, name, size, item.get("display", value)))


def expand_inputs(inputs, recursive=False):
    """Turn files, directories and wildcards into a de-duplicated file list.

    Directories are scanned with os.scandir/os.walk rather than glob because
    glob is case sensitive on POSIX, and packages in the wild ship as both
    '.vxp' and '.Vxp' (Gameloft_Live_240x320.Vxp).
    """
    out = []
    for item in inputs:
        if os.path.isdir(item):
            if recursive:
                walker = os.walk(item)
            else:
                walker = [(item, [], os.listdir(item))]
            for root, _dirs, files in walker:
                for name in files:
                    if name.lower().endswith(".vxp"):
                        out.append(os.path.join(root, name))
        elif any(ch in item for ch in "*?["):
            out.extend(glob.glob(item, recursive=recursive))
        else:
            out.append(item)

    seen = set()
    unique = []
    for path in sorted(out):
        key = os.path.normcase(os.path.abspath(path))
        if key not in seen:
            seen.add(key)
            unique.append(path)
    return unique


def summary_line(info):
    s = info["summary"]
    icon = info.get("icon") or {}
    icon_txt = icon["format"] if icon.get("present") else ("-" if icon.get("present") is False else "?")
    return "%-24s %-9s %8s  %-16s %-5s %9d  %s" % (
        (s["app_name"] or "-")[:24],
        s["version"] or "-",
        s["ram_kb"] if s["ram_kb"] is not None else "-",
        info["container"],
        icon_txt,
        info["size"],
        os.path.basename(info["file"]),
    )


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(
        description="Inspect one or more .vxp packages.",
        epilog="PATH accepts files, directories and wildcards, e.g. "
               "vxpinfo.py bin/Release/fs/e/mre --summary",
    )
    ap.add_argument("paths", nargs="+", metavar="path",
                    help="a .vxp file, a directory of them, or a wildcard")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of text")
    ap.add_argument("--summary", action="store_true",
                    help="one compact line per package instead of a full report")
    ap.add_argument("--recursive", action="store_true",
                    help="recurse into subdirectories")
    args = ap.parse_args()

    files = expand_inputs(args.paths, args.recursive)
    if not files:
        print("vxpinfo: no .vxp files matched", file=sys.stderr)
        return 2

    reports = []
    failed = 0
    for path in files:
        if not os.path.isfile(path):
            print("vxpinfo: file not found: %s" % path, file=sys.stderr)
            failed += 1
            continue
        try:
            reports.append(inspect(path))
        except Exception as e:
            print("vxpinfo: failed to parse %s: %s" % (path, e), file=sys.stderr)
            failed += 1

    if not reports:
        return 2

    if args.json:
        # A single input keeps the historical object shape; batches become a list.
        print(json.dumps(reports[0] if len(reports) == 1 else reports,
                         indent=2, ensure_ascii=False))
    elif args.summary:
        print("%-24s %-9s %8s  %-16s %-5s %9s  %s"
              % ("NAME", "VERSION", "RAM(KB)", "CONTAINER", "ICON", "SIZE", "FILE"))
        for info in reports:
            print(summary_line(info))
    else:
        for i, info in enumerate(reports):
            if i:
                print()
            human_print(info)

    return 1 if failed and not reports else 0


if __name__ == "__main__":
    sys.exit(main())
