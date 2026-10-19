# The native app selector

`MREmu/NativeApps/Menu/AppSelector.cpp` is the menu you land in when you start the
emulator with no VXP on the command line (`MREmu.exe --mcp`). It is a VXP native
app: it draws onto an emulated canvas with the `vm_graphic_*` API, so it runs
identically on desktop and on Android.

ImGui is not involved. It is linked into the emulator and used only for the
developer panels — *Screen*, *Layers*, *CPU REG* and *KeyBoard*.

## Two layouts

The selector has a list layout and a grid layout, and the choice is remembered
between launches.

| | List | Grid |
| --- | --- | --- |
| Item | one row per app, 30px icon | 4x4 icons per page |
| Selection | grey row highlight | blue ring around the tile |
| Name | shown on the row | shown in the title bar |
| Keys | Up/Down, T9 letters | arrows, T9 letters |
| Scrolling | follows the finger, rubber-banded | whole pages only |

Both layouts share one selection index. `m_i` addresses `vxps` in both of them,
plus a **trailing switch item** at index `vxps.size()`:

- in the list it is a centred row at the bottom reading *Switch to Grid View*,
- in the grid it is the last tile, showing a glyph of the layout it switches to.

Because that index has no `vxp` behind it, anything reading `vxps[m_i]` has to
check `is_switch_item(m_i)` first — it is a control, not an app, so it has no
icon, no details sheet and is not remembered as the last selection.

## Remembering the layout

The emulator exposes no settings store to VXP apps, and `DragAndDrop`'s
last-app memory is in-process only outside Windows, so the choice is kept in a
one-byte file:

```
fs/e/mre/mremu_view.cfg      1 = grid, absent or anything else = list
```

`scan()` only accepts `.vxp` entries, so the file never appears in the app list.

## Paging

The grid is **always exactly 4x4 icons**, and its scroll offset is always a whole
number of pages:

```cpp
int grid_total_rows() { return (count() + grid_cols - 1) / grid_cols; }
int grid_pages()      { return (rows + grid_rows_per_page - 1) / grid_rows_per_page; }
```

A partially scrolled grid looks like a broken one — a half row of icons under the
title bar reads as a clipping bug rather than as content continuing past the fold.
So `scroll_to_selection()` snaps to the page holding the selection rather than
aligning it to an edge, and a drag is converted into whole page steps: the view
moves once the finger has covered half a page, and not before.

That is why `draw_grid()` iterates rows from a page index rather than subtracting
a free-running offset — the page boundary is structural, not something the
drawing has to be careful never to land between.

## Wallpaper

Both layouts sit on the same image, `res/wallpaper/app_selector.png`, pushed
further back in the list than in the grid. It is a star field, and it is already
dark, so the dim applied to it is much lighter than a bright wallpaper would
need. See [res/wallpaper/README.md](../res/wallpaper/README.md) for its
provenance and for how to change it.

Two details are easy to get wrong:

- **It is embedded, not loaded.** `WallpaperData.h` holds the PNG as a byte array,
  because Android has no asset directory beside the binary for a VXP native app to
  read from. Replacing the PNG without regenerating the header leaves the build on
  the old image.
- **The dim is baked in at start-up, not applied per frame.** `build_wallpaper()`
  decodes once, scales to the display and blends toward `kDimTint`, producing two
  ready-to-blit buffers. Drawing the wallpaper is then a single `memcpy`, which is
  what keeps it from costing a full-screen resample on every key press, tap and
  marquee tick.

The dim pulls toward the UI's own dark blue rather than scaling to black. On
5/6/5-bit data a straight multiply to black crushes the channels to zero and takes
the hue with it, so the wallpaper goes grey.

## Tuning

Everything that decides how the grid looks is a named constant in one block at
the top of `AppSelector.cpp`, under the `Grid appearance` heading — so it can be
adjusted without reading the drawing code.

The suffix tells you what kind of number it is: **`Pct` is a percentage**, `Px`
is already in pixels, and `Div` is an integer divisor. That distinction is not
pedantry — several of the original expressions were integer divisions that a
round-looking percentage does **not** reproduce. `size / 16` against
`size * 6 / 100` gives 3 where it should give 2, and the switch tile's `size * 5 / 8`
glyph box becomes `size * 62 / 100`. Those are `kFolderGlyphYDiv` and the
`kGlyphBoxNum` / `kGlyphBoxDen` pair for exactly that reason.

The ones worth reaching for:

| Constant | Default | What it moves |
| --- | --- | --- |
| `kColGapPct`, `kRowGapPct` | 20 | Gap between tiles, as a percentage of the cell pitch |
| `kCornerRadiusPct`, `kSquircleN` | 44, 3.0 | Tile corner roundness — a pair, not independent knobs |
| `kFocusRimPx` | 2 | Thickness of the outline around the selected tile |
| `kFocusAlpha` | 235 | How solid that outline is |
| `kAccentR/G/B` | 126, 178, 242 | The outline's colour, shared with the title text |
| `kShadowAlpha`, `kShadowOffsetX/Y` | 55, −1, +1 | The drop shadow under each tile |
| `kMaxCellPx` | 64 | Ceiling on the cell pitch, so tiles stay tile-sized |
| `kGridDim`, `kListDim` | 64, 70 | How far each layout pushes the wallpaper back |
| `kPressR/G/B` | 232, 236, 243 | The block behind a held softkey |

One geometry note on `kRowGapPct`: the tile is square and the cell is square, so
the tile is sized from the **larger** of the two gaps and the smaller is
absorbed by the centring. They are separate constants because row and column
spacing are separate design decisions, but giving them different values has no
effect until the cell pitch itself is made rectangular.

Changing any of this was verified to be pixel-identical at the defaults — see
[Checking it](#checking-it).

## Tile shape

The tiles are **squircles**, not rounded rectangles. Their corners are continuous
curves: the outline leaves the straight edge already turning, so there is no point
where a circular arc begins. That is the superellipse `|x|^n + |y|^n = 1`.

`n` and the radius are a pair, not independent knobs — raising `n` fills the
corner in, so a big radius at `n = 4` still leaves the tip looking square. The
code uses `n = 3` with the radius at 44% of the tile side. `n = 3` is also what
makes the mask cheap: `|u|^3` and `|u|^2` are two and one multiplications, where
the general exponent would need a `pow()` per axis per pixel.

Coverage is antialiased analytically rather than by supersampling. A 2x2 sample
can only produce five distinct coverage values, and at this radius the corner is
most of the tile, so the edge came out visibly stepped. The curve's first-order
distance to the boundary is just `F/|grad F|`, which gives a full 8-bit ramp
across the single pixel the edge passes through and needs no supersampling at all.

The focus ring is drawn as a slightly larger squircle *underneath* the tile rather
than as a frame on top of it: `vm_graphic_roundrect` has circular corners, so a
frame from it sits on a squircle as a visibly different shape.

## Icons

The grid needs the same picture twice — the list draws icons at `img_wh` and the
grid at `grid_icon` — so each entry carries both a `VMINT_CANVAS` and the source
bytes. Only the list-sized canvas is built during `scan()`; the grid-sized one is
decoded on the first grid draw. Building both up front would double the cold-start
decode cost for a layout the user may never open.

Packages with no extractable icon (see [vxp_icons.md](vxp_icons.md)) get a
procedural placeholder tile rather than a hole.

## Switch tile glyph

The trailing tile carries a list icon, embedded as a **coverage mask** rather
than a PNG. `Image.cpp`'s `load_image()` sets `has_transparency = 0` and
converts straight to 16-bit, so an embedded transparent PNG arrives as a black
square — which is fine for the wallpaper, which has no alpha, and not fine here.
The glyph is a solid shape on transparency, so only the coverage matters; the
generator extracts the alpha channel, crops to it and emits 4 KB of it.

Downsampling onto the tile is a box average, not a point sample: the mask is
64×64 and the tile is about 48, and point sampling breaks the bars into dashes.
The result is multiplied by the tile's own squircle coverage so the glyph is
clipped by the tile instead of sitting over its corners.

See [res/icons/README.md](../res/icons/README.md).

## Font limits

`vm_graphic_set_font` is a no-op and `vm_font_set_font_style` returns success
without doing anything, so there is no bold and no larger face to ask for. The
only weight and size available is drawing the 15px bitmap glyphs again at small
offsets:

- the centre softkey (*Choose*) is drawn four times at ±1px, which reads as a
  larger label rather than a fatter one — the bar is ~22px tall, so a real 2x
  scale would not fit;
- there is no second pass for *Options* and *Back*.

## Input

Both layouts route through one set of helpers, none of which draw:

```cpp
enter_dir()                 open_parent_dir()    open_details_for_selection()
activate()                   jump_by_letter()     move_selection_grid()
```

`vm_graphic_flush_layer()` paces itself to ~30fps with a sleep, so a helper that
drew as well as its caller would cost a second frame of latency on every key
press and tap. Every caller draws exactly once.

| | List | Grid |
| --- | --- | --- |
| Left softkey / *Options* | app or folder details | same |
| Right softkey / *Back* | parent folder | same |
| OK / *Choose* | open folder, launch, switch layout | same |
| Long tap | details sheet | same |

Tapping the grid's softkey bar runs the same three actions, with the pressed label
inverted over a block sized to the text.

## Checking it

The menu is driveable without a human: `--mcp-window` opens a real window, so
posted keys and clicks go through the same SFML events a person's would, and
`emu_screen` reads back what the window is showing. The window is
`graphic.width x (graphic.height + 208)` at scale 1 with the sprite at the origin,
so client coordinates in the top 320 rows equal emulated screen coordinates.

Two things about that harness are worth knowing before trusting a failure:

- **Settle ~300ms between keys.** Every key the app handles ends in a flush that
  sleeps a frame, and a press arriving while the previous one is still being
  serviced is dropped. At a 50ms gap, pressing DOWN moved the selection about a
  third as far as it should have.
- **Colours are not what you wrote.** The capture goes through the engine's 5/6/5-bit
  channel extraction, so `0xFFFF` lands as `(248, 252, 248)`, not white. The focus
  ring is `accent_blue` at 235 alpha, so it blends with the wallpaper and matches
  no single colour — it has to be found by shape.