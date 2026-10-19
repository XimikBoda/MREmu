# vxpicon

Extract the app icon from a `.vxp` package. Mirrors `MREngine::extract_vxp_icon()`
and the Android `VxpIconLoader`: scans for `VREAPPLOGO` blobs first, then falls
back to a ranked walk of the `.vm_res` resource directory.

## Location

`tools/vxpicon.py`

## Usage

```text
python tools/vxpicon.py <file.vxp> [-o OUTPUT]
```

### Options

- `path`  -  path to a `.vxp` file (required).
- `-o`, `--output`  -  output image path. Default: `<vxp basename>.icon<ext>` where
  `<ext>` is detected from the image signature (`.gif`, `.png`, `.jpg`, `.bmp`).

## Example

```text
python tools/vxpicon.py "Opera Mini (6.1).vxp" -o opera.icon.png
```

Output:

```text
Extracted 1776 bytes -> opera.icon.png (.gif)
```

## Notes

- VRE icon blobs are located by the stable `VREAPPLOGO` prefix; the image
  signature (`GIF8`, PNG, JPEG, `BM`) is found within the first 64 bytes of the
  payload, ignoring the variant tag.
- The declared payload size bounds the scan so a stub icon cannot borrow an image
  from a following resource.
- Packages without a usable VRE blob (e.g. `GravityDefied (1.0.0).vxp`) fall back to the
  `.vm_res` resource directory, ranked by name (`AppLogo.img`, `*applogo*`, etc.).
