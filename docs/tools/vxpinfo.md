# vxpinfo

Inspect MediaTek MRE `.vxp` packages and print the file envelope, the MRE tag
table, the compressed-ADS descriptor (when applicable), the ELF header and
section table (when applicable), icon presence, and a summary of app metadata.

## Location

`tools/vxpinfo.py` (Windows wrapper: `tools/vxpinfo.bat`)

## Usage

```text
python tools/vxpinfo.py <path> [<path> ...] [--summary] [--recursive] [--json]
```

### Inputs

`path` accepts:

- a single `.vxp` file
- several files at once
- a **directory** (every `*.vxp` inside it, matched case-insensitively, so
  `Gameloft_Live_240x320.Vxp` is found too)
- a **wildcard**, e.g. `bin/Release/fs/e/mre/Peanut*.vxp`

### Options

| Option | Effect |
| :--- | :--- |
| `--summary` | One compact line per package instead of a full report. |
| `--recursive` | Recurse into subdirectories when a directory is given. |
| `--json` | Machine-readable output. One input emits an object; several emit a list. |

## Full report

```text
python tools/vxpinfo.py "Opera Mini 6.1.vxp"
```

```text
File:        ...\Opera Mini 6.1.vxp
Size:        2041317 bytes
SHA256:      53fb5b91b24a5c655bce6979341c22566ff1eed9bddbe10c0b7b73356c9edf26
Magic:       7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
Container:   elf_uncompressed
Trailer:
  tags_offset: 0x001F2372 (2040690)
Charset:     UCS-2 LE
Summary:
  app_name:   Opera Mini
    from:     VM_CE_INFO_NAME_LIST (0x19) lang 1
  dev_name:   Opera
  version:    0.0.0
    raw:      0 (0x00000000)
  ram_kb:     4500
  resolution: 0
  is_ads:     True
  description: VREDESC:Opera Mini v6;VREPHONE:;VRERANK:
    VREDESC:  Opera Mini v6
    VREPHONE: (empty)
    VRERANK:  (empty)
Icon:        GIF, 1776 bytes (sha256 f64051f5dfcf649d...)
...
Tags (17):
  0x01 VM_CE_INFO_DEV           size=12   'Opera'
  0x0F VM_CE_INFO_MEM_REQ       size=4    4500 KB
  0x19 VM_CE_INFO_NAME_LIST     size=98
       lang=1: 'Opera Mini'
```

## Summary mode (batches)

```text
python tools/vxpinfo.py bin/Release/fs/e/mre --summary
```

```text
NAME                     VERSION    RAM(KB)  CONTAINER        ICON       SIZE  FILE
Calculator               0.0.111       1024  elf_uncompressed png      132033  Calculator.vxp
Facebook                 16.1.0         570  elf_uncompressed png      227657  Facebook 2.5.1.vxp
Messenger                0.1.0          800  ads_compressed   gif      409416  Facebook Messenger.vxp
GravityDefied            1.0.0          900  pe_dll           -        924220  Gravity Defied (x86).vxp
Mrecp                    1.0.0         1500  elf_uncompressed -        232915  Mrecp.vxp
Opera Mini               4.4.0         1500  elf_uncompressed gif      637265  Opera Mini 4.4.vxp
```

## Behaviour notes

### App name resolution

The title is taken from `VM_CE_INFO_NAME` (0x04) unless that tag holds a generic
SDK placeholder such as `"App"`. Placeholder names lose to a concrete entry from
`VM_CE_INFO_NAME_LIST` (0x19) or `VM_CE_INFO_MULTI_NAME_LIST` (0x26), because the
three PeanutVXP packages ship `0x04 = "App"` while the list carries `"PeanutVXP"`.
The tag the name came from is printed under `from:`, and JSON callers can read
`app_name_resolution` (`name`, `source`, `single`, `single_is_generic`,
`list_entries`).

### Version tag

`VM_CE_INFO_VER` (0x05) is a packed u32, rendered as `X.Y.Z` using
`major = (v >> 16) & 0xFF`, `minor = (v >> 8) & 0xFF`, `patch = v & 0xFF`. That is
what makes Opera Mini 4.4's `0x00040400` read as `4.4.0` and the fifteen packages
storing `0x00010000` read as `1.0.0`. The unused top byte is reported as
`high_byte` in JSON. The raw value is always kept (`version_raw`), because a few
packages store something that is not a dotted version at all (`Calculator.vxp`
has `111`).

### Container detection

The container is decided from the file magic, not from the tag table:

| `container` | Meaning |
| :--- | :--- |
| `elf_uncompressed` | Magic `7F 45 4C 46`. |
| `ads_compressed` | A valid 36-byte `compress_ads_elf_info` descriptor precedes the tags, or the file starts with a zlib stream. |
| `pe_dll` | Magic `MZ` - a Windows PE/DLL image, not a VXP (`Gravity Defied (x86).vxp`). |
| `unknown` | Nothing recognisable; `container_note` explains why. |

`VM_CE_INFO_RO_RW_ZIP` is deliberately not used here: it is set on plain
uncompressed ELFs too. See [vxp_icons.md](../vxp_icons.md).

### Icon reporting

`Icon:` reports presence, format and size by delegating to
[`vxpicon`](vxpicon.md), so both front ends agree on which packages have a
usable icon. `-` in summary mode means "no icon found" (`Mrecp.vxp` and
`Show Picture.vxp` carry stub icons; this matches the table in
[vxp_icons.md](../vxp_icons.md)).

### Other summary fields

`description` reads `VM_CE_INFO_DESC` (0x17), falling back to
`VM_CE_INFO_DESC_LIST` (0x1A). Its `KEY:value;` segments are split out into
`description_fields` (`VREDESC`, `VREPHONE`, `VRERANK`, ...).
`resolution` is `VM_CE_INFO_RESOLUTION_REQ` (0x10).

### Malformed input

A `.vxp` is untrusted input, so every offset is bounded before it is dereferenced
and no file crashes the tool: truncated files, garbage `tags_offset` values and
mutated headers are reported as `container: unknown` or with a tag-parse error.
