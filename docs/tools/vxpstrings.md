# vxpstrings

Extract printable strings from a `.vxp` package. Scans for ASCII and/or UTF-16LE
(UCS-2) runs of at least `--min` characters, across the whole file or only the
`.vm_res` resource block.

## Location

`tools/vxpstrings.py`

## Usage

```text
python tools/vxpstrings.py <file.vxp> [--min N] [--encoding {ascii,ucs2,all}] [--vm-res] [--limit N]
```

### Options

- `path`  -  path to a `.vxp` file (required).
- `--min`  -  minimum string length (default `4`).
- `--encoding`  -  `ascii`, `ucs2`, or `all` (default `all`).
- `--vm-res`  -  scan only the `.vm_res` resource block.
- `--limit`  -  maximum number of strings to print (`0` = all).

## Example

```text
python tools/vxpstrings.py "Opera Mini (6.1).vxp" --vm-res --encoding ucs2 --limit 5
```

Output:

```text
0x001BE33D  ucs2   ++/+
0x001BEFA3  ucs2   0</+)=
...
# 5 strings
```

## Notes

- Each line is `OFFSET  ENCODING  STRING`.
- The count summary is written to stderr.
- Resource-block offsets are resolved for both ELF and compressed ADS containers.
```
