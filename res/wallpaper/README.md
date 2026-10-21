# App selector wallpaper

The image drawn behind the native app selector, in both its layouts
(see [../../docs/app_selector.md](../../docs/app_selector.md)).

## Provenance

`source/app_selector.jpg` is a star-field / galaxy wallpaper, already at the
display's native 240x320.

The dim applied on top of it in the app is correspondingly light — it is already
a dark night sky, so pulling it much of the way toward black just erases it.

## Files

| File | What it is |
| --- | --- |
| `source/app_selector.jpg` | The master image. This is the file to replace. |
| `app_selector.png` | Generated: the master resized to the display and written as a PNG. Shareable on its own. |
| `make_wallpaper.py` | Generates the PNG and the embedded header together. |

The generated PNG is also emitted as a C array in
`MREmu/NativeApps/Menu/WallpaperData.h`, which is what the selector actually
reads. That is not a stylistic choice: the selector is a VXP native app compiled
into the shared library for Android, and there is no asset directory beside the
binary there for it to load a file from.

## Changing it

1. Replace `source/app_selector.jpg`.
2. `python res/wallpaper/make_wallpaper.py`
3. `cmake --build build --config Release`

Step 2 is not optional. Editing the PNG alone leaves the running build on the
previous header, which is exactly the kind of drift you find out about weeks
later.

## Why the script writes the PNG with Pillow

It is tempting to hand-roll the PNG — a palette-indexed one is about a third the
size of truecolour and the source is flat-shaded, so it compresses beautifully.
That version was written and it did not work: every reader accepted it except the
one that matters, `stb_image` inside SFML behind `vm_graphic_load_image_FIX`,
which returned failure and left the background black.

Pillow's writer costs about 100 KB and is known to be correct for the one decoder
in the chain. Do not "optimise" this back into a hand-rolled writer.

## Sizing

The master is prepared at 240x320, the emulated display's native size, and
stretched if the window is stepped up past it. That is the right way round for
this image: the triangles are large and flat-shaded, so a blocky upscale in a
dimmed background is invisible, whereas a larger master is paid for in the
embedded header on every build.

At 5/6/5-bit the colours are quantised on the way to the display anyway, so a
larger master would not buy back any of the detail it costs.