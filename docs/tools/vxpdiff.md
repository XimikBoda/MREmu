# vxpdiff

Compare two `.vxp` packages and report differences in size, container type,
charset, summary metadata, MRE tags, ELF sections, and icon hash.

Summary comparison covers `app_name`, `app_name_source`, `dev_name`, `version`,
`version_raw`, `description`, `resolution`, `ram_kb`, `is_ads`, `is_vre_ads` and
`is_zipped`, so a version or description change shows up without reading the raw
tag table.

## Location

`tools/vxpdiff.py`

## Usage

```text
python tools/vxpdiff.py <a.vxp> <b.vxp> [--json]
```

### Options

- `a`, `b`  -  paths to the two `.vxp` files (required).
- `--json`  -  emit a machine-readable JSON diff.

## Example

```text
python tools/vxpdiff.py "Opera Mini 4.4.vxp" "Opera Mini 6.1.vxp"
```

Output (abridged):

```text
A: Opera Mini 4.4.vxp
B: Opera Mini 6.1.vxp
Size: 637265 -> 2041317 (delta 1404052)
Summary changes:
  app_name_source: 'VM_CE_INFO_NAME (0x04)' -> 'VM_CE_INFO_NAME_LIST (0x19) lang 1'
  dev_name: 'Opera Software' -> 'Opera'
  version: '4.4.0' -> '0.0.0'
  version_raw: 263168 -> 0
  description: 'VREDESC:Branding=oem_tct;VREPHONE:;VRERANK:' -> 'VREDESC:Opera Mini v6;VREPHONE:;VRERANK:'
  ram_kb: 1500 -> 4500
Tags added:
  0x24 VM_CE_INFO_SUPPORT_VP
Tags removed:
  0x04 VM_CE_INFO_NAME
Tags changed:
  0x0F VM_CE_INFO_MEM_REQ: 1500 -> 4500
Sections changed:
  ER_RO: 405952 -> 465612 (delta 59660)
  .vm_res: 217148 -> 1562858 (delta 1345710)
Icon SHA256: f640... -> f640... (same)
```

## Notes

- Tags are compared by ID; added/removed/changed tags are listed separately.
- Sections are compared by name; size changes are reported as deltas.
- The icon is hashed via `vxpicon`'s extraction, so identical icons compare equal
  even if stored differently.
