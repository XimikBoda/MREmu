#!/usr/bin/env python3
"""vxpicon - extract the app icon from a .vxp package.

Mirrors MREngine::extract_vxp_icon() (MREmu/MREngine/VxpIcon.cpp) and the
Kotlin VxpIconLoader: VREAPPLOGO blob scan first, then a ranked walk of the
.vm_res resource directory.  Writes the extracted image bytes to a file.
"""

import argparse
import os
import struct
import sys

VRE_PREFIX = b"VREAPPLOGO"
MAGIC_SEARCH_LIMIT = 64
ADS_INFO_SIZE = 36
MAX_RESOURCES = 4096
MAX_ICON_BYTES = 512 * 1024


def read_le32(data, size, off):
    if off + 4 > size:
        return 0
    return struct.unpack_from("<I", data, off)[0]


def image_signature_len(data, size):
    if size >= 4 and data[0:4] == b"GIF8":
        return 4
    if (size >= 8 and data[0] == 0x89 and data[1:4] == b"PNG" and
            data[4:8] == b"\x0D\x0A\x1A\x0A"):
        return 8
    if size >= 3 and data[0] == 0xFF and data[1] == 0xD8 and data[2] == 0xFF:
        return 3
    if size >= 2 and data[0:2] == b"BM":
        return 2
    return 0


def image_span(data, size):
    """Return (start_offset, length) of the image inside data, or None."""
    if size < 4:
        return None
    sig = image_signature_len(data, size)
    if sig:
        return 0, size
    if data[:len(VRE_PREFIX)] != VRE_PREFIX:
        return None
    if size <= 23:
        return None
    declared = read_le32(data, size, 19)
    if declared > 0 and declared <= size - 23:
        body_end = 23 + declared
    else:
        body_end = size
    body = data[23:body_end]
    body_size = len(body)
    limit = min(MAGIC_SEARCH_LIMIT, body_size)
    for off in range(limit):
        if image_signature_len(body[off:], body_size - off):
            return 23 + off, body_size - off
    return None


def icon_score(name):
    n = name.lower()
    if n == "applogo.img":
        return 100
    if "applogo" in n:
        return 90
    if n.startswith("_vreapplogo"):
        return 85
    def ends_with(s):
        return n.endswith(s)
    looks = (ends_with(".img") or ends_with(".png") or ends_with(".gif") or
             ends_with(".jpg") or ends_with(".jpeg") or ends_with(".bmp"))
    if not looks:
        return 0
    if n in ("icon.png", "icon.gif", "icon.jpg", "icon.img"):
        return 60
    if "logo" in n:
        return 50
    if "icon" in n:
        return 40
    return 0


def elf_res_section(data):
    if len(data) < 0x34:
        return None
    if data[0:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
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
        if (max_len >= 7 and data[base:base + 7] == b".vm_res" and
                (max_len == 7 or data[base + 7:base + 8] == b"\x00")):
            off = read_le32(data, len(data), o + 16)
            sz = read_le32(data, len(data), o + 20)
            return off, sz
    return None


def ads_res_section(data, tags_offset):
    if tags_offset < 4:
        return None
    if read_le32(data, len(data), tags_offset - 4) != ADS_INFO_SIZE:
        return None
    if tags_offset - 4 < ADS_INFO_SIZE:
        return None
    base = tags_offset - 4 - ADS_INFO_SIZE
    if base + ADS_INFO_SIZE > len(data):
        return None
    off = read_le32(data, len(data), base + 28)
    sz = read_le32(data, len(data), base + 32)
    return off, sz


def read_resource_dir(data, res_off, res_size):
    out = []
    if res_size == 0 or res_off + res_size > len(data):
        return out
    pos = res_off
    limit = res_off + res_size
    while pos < limit:
        if data[pos] == 0:
            break
        end = pos
        while end < limit and data[end] != 0:
            end += 1
        if end >= limit:
            break
        name = data[pos:end].decode("latin-1")
        pos = end + 1
        if pos + 8 > limit:
            break
        off = read_le32(data, len(data), pos)
        pos += 4
        sz = read_le32(data, len(data), pos)
        pos += 4
        if off == 0 or sz == 0:
            continue
        if off + sz > len(data):
            continue
        out.append((name, off, sz))
        if len(out) >= MAX_RESOURCES:
            break
    return out


def icon_from_resources(data, tags_offset):
    is_elf = data[:4] == b"\x7fELF"
    if is_elf:
        r = elf_res_section(data)
    else:
        r = ads_res_section(data, tags_offset)
    if not r:
        return b""
    res_off, res_size = r
    resources = read_resource_dir(data, res_off, res_size)
    resources.sort(key=lambda x: icon_score(x[0]), reverse=True)
    for name, off, sz in resources:
        if sz > MAX_ICON_BYTES:
            continue
        if icon_score(name) == 0:
            break
        span = image_span(data[off:off + sz], sz)
        if not span:
            continue
        s, ln = span
        img = data[off + s:off + s + min(ln, sz - s)]
        if img:
            return img
    return b""


def extract_icon(data):
    if len(data) < 32:
        return b""
    # 1. VRE icon blobs, in file order.
    i = 0
    n = len(data)
    while True:
        idx = data.find(VRE_PREFIX, i)
        if idx < 0:
            break
        if idx + 4 >= n:
            break
        span = image_span(data[idx:], n - idx)
        if span:
            s, ln = span
            img = data[idx + s:idx + s + min(ln, n - idx - s)]
            if img:
                return img
        i = idx + 1
    # 2. Resource directory fallback.
    tags_offset = read_le32(data, n, n - 12)
    if tags_offset and tags_offset + 12 <= n:
        return icon_from_resources(data, tags_offset)
    return b""


def image_ext(data):
    if data[:4] == b"GIF8":
        return ".gif"
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return ".png"
    if data[:3] == b"\xff\xd8\xff":
        return ".jpg"
    if data[:2] == b"BM":
        return ".bmp"
    return ".bin"


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    ap = argparse.ArgumentParser(description="Extract the app icon from a .vxp package.")
    ap.add_argument("path", help="path to .vxp file")
    ap.add_argument("-o", "--output", help="output image path (default: <vxp>.icon<ext>)")
    args = ap.parse_args()

    if not os.path.isfile(args.path):
        print("vxpicon: file not found: %s" % args.path, file=sys.stderr)
        return 2
    data = open(args.path, "rb").read()
    img = extract_icon(data)
    if not img:
        print("vxpicon: no icon found in %s" % args.path, file=sys.stderr)
        return 1
    ext = image_ext(img)
    out = args.output
    if not out:
        base = os.path.splitext(args.path)[0]
        out = base + ".icon" + ext
    with open(out, "wb") as f:
        f.write(img)
    print("Extracted %d bytes -> %s (%s)" % (len(img), out, ext))
    return 0


if __name__ == "__main__":
    sys.exit(main())
