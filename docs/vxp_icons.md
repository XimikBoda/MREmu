# VXP Icon Extraction

## What this does

Two front ends show app icons for a `.vxp`:

- **Android** - `VxpIconLoader.kt` powers the VXP library list.
- **Native list browser** - `MREngine::extract_vxp_icon()` (`MREngine/VxpIcon.cpp`) powers
  the in-emulator `AppSelector` menu.

Both implement the same discovery and accept the same packages.

## Important: the symptom was in the native list, not Android

An earlier revision of this work concluded that *no* icons rendered anywhere. That was
wrong, and it was checked against the wrong surface.

The Android library list really did show a hard-coded glyph for every row. But the
emulator's own `AppSelector` menu - the list you see when MREmu boots with no VXP - has
always tried to load icons (`AppSelector.cpp`, the `VREAPPLOGO09BVRE` search). That is
the "listview" in question, and there *some* icons did work.

Reproducing its exact algorithm against the library shows the failure set matches the
reported symptom one for one:

| Worked | Failed |
|---|---|
| App Store, Facebook Messenger, Gameloft, Opera Mini 4.4 (both), Spider Man | Calculator, Facebook 2.5.1, Niblp, Textep, gpt, peanut x3, GravityDefied x2, Mrecp, Show_picture |

### Why it failed

The old loader trusted the VRE header's variant tag and declared size:

```cpp
size_t pos = data_view.find("VREAPPLOGO09BVRE");
int img_size = *(int*)(data.data() + pos + 16 + 3);
VMUINT8* img_data = (data.data() + pos + 16 + 3 + 4);
if (data_view.substr(pos + 16, 3) == "PNG")
    img_data += 8;
```

Only packages whose payload happened to begin with an image worked. The ones that failed
needed the 8-byte skip but were tagged `JPG`, not `PNG`, so the payload pointer landed on
8 bytes of padding and the decoder was handed garbage.

Three further problems: the search was for a full 16-byte magic that one variant
(`VREAPPLOGO09AVRE`) does not contain; `img_size` was read with an unaligned `int*` cast;
and there was no `.vm_res` fallback for packages with no VRE blob at all.

## The header varies; do not trust it

Observed shapes, all starting with the stable prefix `VREAPPLOGO`:

```text
"VREAPPLOGO09BVRE" + tag[3] + u32 size + image     <- most packages
"VREAPPLOGO09AVRE" + fmt[3] + u32 size + image     <- Opera Mini 6.1 (no "BVRE")
```

And the tag contradicts the payload: packages tagged `JPG` hold a **PNG**, packages
tagged `PIC` hold a **GIF**. So both implementations ignore the tag and find a real image
signature (`GIF8`, PNG, JPEG `FF D8 FF`, `BM`) within the first 64 bytes of the payload.

The declared size is trusted for one thing only: bounding the scan. Without it a stub
icon borrows an image from whatever resource follows it in the file - a 31-byte stub in
Mrecp would otherwise pick up the neighbouring GIF. It is never used to skip bytes.

Extracted images are **clamped to the declared payload**. Without that clamp one
1,776-byte icon returned 1.5 MB, because the slice ran to end-of-file and swept up the
rest of the package.

## Coverage

Both implementations verified over all 19 packages in `bin/Release/fs/e/mre`. The C++
result was produced by compiling `VxpIcon.cpp` into a standalone probe, not by mirroring
the algorithm.

| Result | Count | Notes |
|---|---|---|
| Icon extracted | 16 | up from 5 in the native list |
| Stub, no image | 2 | `Mrecp.vxp`, `Show_picture.vxp` |
| Not a VXP container | 1 | `GravityDefied.vc.vxp` is a Windows DLL (`MZ`) |

`GravityDefied.vxp` is the case that needs the `.vm_res` fallback: its `Applogo.img` is a
stub but it ships a real `logo.png`. Formats handled: GIF (8), PNG (8).

## Android list details

- The whole library is loaded as **one unit**: the directory scan and every icon decode
  complete before anything is shown, under a single centred spinner in place of the list.
  Resolving icons per row instead meant scrolling back and forth re-decoded them, because
  a row that scrolled out of view lost its bitmap.
- The icon slot is a **fixed 40dp box** whether or not a package has an icon, so rows keep
  the same text alignment either way. Sizing it to content made each row's text shift
  sideways as icons arrived.
- Icons are cached in two tiers: an in-memory `LruCache`, and PNGs written under
  `cacheDir/vxp_icons/`. Without the disk tier every cold start re-parsed and re-decoded
  every VXP before the first row could paint. Keys include path, mtime and size.

## Notes for the next agent

- Keep the two implementations in step. The scoring table (`iconScore` / `icon_score`) and
  the magic list are duplicated on purpose; there is no shared test, so a change to one
  should be made in the other.
- `icon_score()` returns 0 for anything unrecognised and both walks stop on 0, so an
  unknown naming convention yields no icon rather than a random sprite sheet. That is
  deliberate - prefer a missing icon to a splash screen.
- Icon discovery never runs guest code, and every offset is bounds-checked against the
  real file length, since a VXP is untrusted input whose header fields the producer does
  not validate.