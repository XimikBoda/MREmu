# The grid's own glyphs

The glyphs the grid draws for itself, where there is no package artwork to
show. Both are embedded as **8-bit coverage masks**, not as PNGs.

| File | What it is |
| --- | --- |
| `source/list-ul.png` | The list glyph, on the "Switch to List View" tile. |
| `source/gamepad.png` | The placeholder glyph, for packages with no extractable icon. |
| `make_icons.py` | Generates `MREmu/NativeApps/Menu/IconsData.h` from both. |
| `MREmu/NativeApps/Menu/IconsData.h` | Generated: two 64×64 byte arrays. |

## Changing one

1. Replace the matching file in `source/`.
2. `python res/icons/make_icons.py`
3. `cmake --build build --config Release`

Step 2 is not optional. Editing a PNG alone leaves the build on the previous
mask. Adding a third glyph is a one-line addition to the `ICONS` table at the top
of the script; the header, the crop and the fitting all follow from it.

## Why a coverage mask and not an embedded PNG

`MREmu/MREngine/Image.cpp`'s `load_image()` sets `has_transparency = 0` and
converts straight to 16-bit, so **alpha does not survive the image loader**. An
embedded PNG with a transparent background arrives as a black square — which is
why the wallpaper is embedded as a PNG (it has no alpha) and these are not.

Both glyphs are solid black on transparency, so the only information they carry
is per-pixel opacity; the colour is a constant the tile supplies. The generator
extracts the alpha channel, crops to it, fits it into a square at its own aspect
and emits that as a byte array. 4 KB each, no decoder at run time, and exact.

If you replace a source with a multi-coloured icon this will not look right: only
the coverage is kept, and everything is drawn in the tile's ink colour
(`kInkR/G/B` for the list glyph, `kPlaceholderInkR/G/B` for the gamepad).

## Cropping and fitting

The sources are 512×512 icon canvases whose drawings occupy only the middle —
342×256 and 456×342. The generator crops to the alpha bounding box before
fitting, otherwise the glyph would be scaled to a fraction of the tile it is
meant to fill.

Each mask is square with zero coverage around the glyph, so `blend_mask_glyph()`
treats the whole mask as the box it scales onto the tile and the padding needs no
special case. How big that box is on the tile is a constant, not a property of
the mask: `kGlyphBoxNum`/`kGlyphBoxDen` for the list glyph,
`kPlaceholderGlyphNum`/`kPlaceholderGlyphDen` for the gamepad. The gamepad gets a
larger box because it is the placeholder tile's only content, where a mark sized
to sit alongside artwork looks lost.