# vxpsections

Dump the ELF section table of a `.vxp` package and optionally disassemble a
section. Targets the 32-bit little-endian ARM ELFs used by MRE. Disassembly
requires the Python `capstone` package.

## Location

`tools/vxpsections.py`

## Usage

```text
python tools/vxpsections.py <file.vxp> [--disasm] [--section NAME] [--offset N] [--size N] [--max-insns N] [--mode {thumb,arm}]
```

### Options

- `path`  -  path to a `.vxp` file (required).
- `--disasm`  -  disassemble a section.
- `--section`  -  section name to disassemble (default `ER_RO` when present).
- `--offset`  -  file offset to disassemble when no section is selected (default `0`).
- `--size`  -  number of bytes to disassemble (default `256`).
- `--max-insns`  -  maximum instructions to print (`0` = all in range).
- `--mode`  -  `thumb` (default) or `arm`.

## Example

```text
python tools/vxpsections.py "Opera Mini 6.1.vxp" --disasm --max-insns 8
```

Output (abridged):

```text
Entry: 0x00008000
Idx  Name                   Offset       Size       Addr
0    (noname)           0x00000000          0 0x00000000
1    ER_RO              0x00000034     465612 0x00008000
...
Disassembly of ER_RO (256 bytes):
0x00008000:  c8 80                    strh r0, [r1, #6]
0x00008002:  8f e2                    b #0x8524
```

## Notes

- Non-ELF (compressed ADS) packages are rejected.
- Thumb mode is the appropriate default for MRE entry points.
```
