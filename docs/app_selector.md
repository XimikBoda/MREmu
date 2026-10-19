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

## Loading

`scan()` is synchronous and never yields — it walks the directory, reads every
`.vxp` in full, runs the icon extractor over each one and decodes the list-sized
icon. Nothing in that gives the host a chance to present a frame, so until it
finishes the window still shows whatever was last flushed, which on a cold start
is nothing at all. A slow folder is therefore indistinguishable from a hang.

So `scan()` presents a **"Loading..."** centred on both axes, over the same
wallpaper the layout will use.

The frame does not need an event to be drawn: the app runs on `AppManager`'s
loop thread while the window is presented from the main one, and
`update_screen()` is unthrottled, so the presenting thread picks it up on its
next pass. The flush's own ~33 ms sleep is what gives it the window to do that
in.

**But nothing waits for that pass to happen.** On a scan fast enough to finish
inside the presenting loop's own cadence, the next flush overwrites the frame
unseen — which is the right outcome, since there was no wait worth explaining,
but it does mean the frame is only visible when the scan genuinely *is* slow.

That is also why **`--mcp-window` cannot show it at all**: the launch is queued
at `MREmu.cpp` before `run_mcp_window` creates the window, so the entire load
happens with nothing presenting. Verify this one in the normal window.

**It is not shown unconditionally.** Over a warm folder `scan()` measures about
15 ms — nineteen packages, 6.9 MB, page cache warm — and there a loading frame is
just a flicker that explains nothing. It appears when the *previous* scan was
slow, above `kLoadingThresholdMs`, which is what the case it exists for looks
like: a cold cache, a slow device, a large folder. The first scan of a session
always shows it, since `last_scan_ms` starts at -1 and a cold start is precisely
the moment the screen would otherwise be blank. Changing directory clears the
measurement too, because a different folder is a different cost and the old one
predicts nothing.

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
| `kIconOverdrawNum/Den` | 11, 10 | Icon scale before the clip — trims the frames packages bake in |
| `kMaxCellPx` | 64 | Ceiling on the cell pitch, so tiles stay tile-sized |
| `kGridDim`, `kListDim` | 64, 70 | How far each layout pushes the wallpaper back |
| `kPressR/G/B` | 232, 236, 243 | The block behind a held softkey |
| `kGlyphBoxNum/Den` | 5, 8 | Size of the list glyph on the switch tile |
| `kPlaceholderGlyphNum/Den` | 3, 4 | Size of the gamepad on a placeholder tile |

One geometry note on `kRowGapPct`: the tile is square and the cell is square, so
the tile is sized from the **larger** of the two gaps and the smaller is
absorbed by the centring. They are separate constants because row and column
spacing are separate design decisions, but giving them different values has no
effect until the cell pitch itself is made rectangular.

### Icon overdraw

Package artwork is drawn at `kIconOverdrawNum/Den` of the tile (110%) and
centred, so the squircle clip throws away its outermost band.

This is not for looks. A VXP icon is its **own** rounded-square artwork, and many
of them have a pale frame drawn around the outside — Gameloft Live's is a 45×45
GIF with a light border a couple of pixels in. Drawn edge to edge, that frame
showed on all four sides and every tile looked outlined in grey, while the clip
only cut it at the corners where the two shapes happened to overlap. Scaling past
the clip puts the frame outside the tile, and the tile's edge becomes purely the
squircle.

The icon is also decoded at the overdraw size rather than at `grid_icon`, so the
source is resampled once at the resolution it is actually drawn at instead of
being stretched after decoding.

### No drop shadow

The tiles deliberately have nothing under them. A dark squircle offset down and
right reads as a **rim** rather than as depth at this size: the tile's own edge is
antialiased, so a partially covered pixel already blends toward whatever is
underneath, and putting a dark shape under a light tile doubles that fringe
instead of softening it. The icons came out looking outlined in grey. The
selection ring is the only outline on a tile, and it is there to mean something.

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
placeholder tile rather than a hole.

## Glyphs the grid draws for itself

Where there is no package artwork, the grid draws its own: a list icon on the
trailing switch tile, and a gamepad on any package whose icon could not be
extracted. Both are embedded as **coverage masks** rather than PNGs.
`Image.cpp`'s `load_image()` sets `has_transparency = 0` and converts straight to
16-bit, so a transparent PNG arrives as a black square — fine for the wallpaper,
which has no alpha, and not fine here. A glyph is a solid shape on transparency,
so only the coverage carries information; the generator extracts the alpha
channel, crops to it and emits 4 KB of it per glyph.

`blend_mask_glyph()` is shared by both, so the two tiles differ only in which
mask they pass and how big a box they ask for. Downsampling onto the tile is a
box average, not a point sample: the masks are 64×64 and the tile is about 48,
and point sampling breaks the strokes into dashes. The result is multiplied by
the tile's own squircle coverage so a glyph is clipped by the tile instead of
sitting over its corners.

The gamepad gets a larger box than the list glyph, because it is the
placeholder tile's only content — a mark sized to sit alongside artwork looks
lost on its own.

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
| Right softkey / *Back* | parent folder | parent folder, else **list view** |
| OK / *Choose* | open folder, launch, switch layout | same |
| Long tap | details sheet | same |

In the grid, *Back* is also the way **out**. Reaching list view otherwise meant
scrolling to the very last item on the very last page and choosing it, and at
the MRE root there was no *Back* at all — `open_parent_dir()` just returned
false. So it falls back to the layout switch when there is no parent left.

`do_back()` holds that rule for both the key handler and the pen, so the two
cannot drift apart, and `toggle_view()` holds the layout flip shared by it and
the trailing switch item.

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