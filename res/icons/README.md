# List-switch tile glyph

The glyph on the grid's trailing "Switch to List View" tile.

| File | What it is |
| --- | --- |
| `source/list-ul.png` | The master image. This is the file to replace. |
| `make_icon.py` | Generates `ListIconData.h` from it. |
| `MREmu/NativeApps/Menu/ListIconData.h` | Generated: a 64×64, 8-bit coverage mask. |

## Changing it

1. Replace `source/list-ul.png` — any RGBA image works; black-on-transparent is
   what it was drawn for, but see below.
2. `python res/icons/make_icon.py`
3. `cmake --build build --config Release`

Step 2 is not optional. Editing the PNG alone leaves the build on the previous
mask.

## Why a coverage mask and not an embedded PNG

`MREmu/MREngine/Image.cpp`'s `load_image()` sets `has_transparency = 0` and
converts straight to 16-bit, so **alpha does not survive the image loader**. An
embedded PNG with a transparent background arrives as a black square — which is
why the wallpaper is embedded as a PNG (it has no alpha) and this is not.

The glyph is a solid shape on a transparent background, so the only information
it carries is per-pixel opacity; the colour is a constant the tile supplies. So
the generator extracts the alpha channel, crops to it, fits it into a square at
its own aspect and emits that as a 64×64 byte array. 4 KB, no decoder at run
time, and exact.

If you replace the source with a multi-coloured icon this will not look right:
only the coverage is kept, and everything is drawn in the tile's ink colour.

## Cropping

The source is a 512×512 icon canvas whose drawing occupies only the middle
342×256. The generator crops to the alpha bounding box before fitting, otherwise
the glyph would be scaled to a fraction of the tile it is meant to fill.

## Padding

The mask is square with zero coverage around the glyph, so `draw_list_glyph()`
treats the whole mask as the box it scales onto the tile and the padding needs no
special case.