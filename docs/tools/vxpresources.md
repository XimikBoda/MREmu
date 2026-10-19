# vxpresources

List and extract entries from the `.vm_res` resource block of a `.vxp`
package. Resource offsets are resolved for both ELF and compressed ADS containers.

## Location

`tools/vxpresources.py`

## Usage

```text
python tools/vxpresources.py <file.vxp> [--list] [--extract NAME] [--extract-all] [-o DIR]
```

### Options

- `path`  -  path to a `.vxp` file (required).
- `--list`  -  list resources (default action).
- `--extract NAME`  -  extract a single resource by name.
- `--extract-all`  -  extract every resource.
- `-o`, `--outdir`  -  output directory for extraction (default `.`).

## Example

```text
python tools/vxpresources.py "Opera Mini 6.1.vxp"
```

Output:

```text
vm_res block: offset=0x74914 size=1562858 resources=2
Idx  Name                             Offset       Size  Type
0    AppLogo.img                  0x00074941       2315  gif
1    mre-2.0                      0x0007524C        112  bin
```

Extract all resources:

```text
python tools/vxpresources.py "Opera Mini 6.1.vxp" --extract-all -o out/res
```

## Notes

- Resource names are sanitized for use as filenames during extraction.
- Type is guessed from the leading bytes (`gif`, `png`, `jpg`, `bmp`, `text`, `bin`).
```
