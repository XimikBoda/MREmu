# Merging upstream/master into master — notes for future agents

Date: 2026-10-20  
Branch: `master`  
Merge: `d54e04f` (merge `upstream/master` `1d14940` into `master` `047e100`)

## Why this merge existed

`upstream/master` (`github.com/XimikBoda/MREmu`) is the original upstream repo.  
`master` (this fork) carries the MCP server, `--inspect` headless mode, DragAndDrop,
the Android Compose options sheet, the grid-view app selector, the VXP catalog
cleanup tooling, and key-repeat/vibration work that upstream does not have.

Upstream had advanced the **front end** into a `Device/` module (keyboard, touch,
menubar, screen corner) and added **Emscripten** build support. The merge brought
those in while keeping everything master-only.

## The big picture

- Merge base: `e9ead91` ("Some new api")
- master: 88 commits ahead of base
- upstream/master: 13 commits ahead of base
- Total merge conflicts: **12 files**

## Structural changes upstream made (and how they collided)

### 1. `MREmu.cpp` was nearly rewritten (1122 → 206 lines)

Upstream deleted the entire desktop main loop, the MCP server, DragAndDrop,
SystemTextBox, the Android Compose UI, and the Options button, replacing them
with a `Device` class that owns the window, keypad, and menubar.

**Collision:** master's `MREmu.cpp` carried the MCP server, `--inspect`, and the
Android options dialog. Resolved by keeping master's version of `MREmu.cpp` and
adopting upstream's `Device` class as the widget/layout layer inside it.

### 2. Files moved: `Keyboard.{cpp,h}`, `Touch.{cpp,h}` → `MREmu/Device/`

Upstream also split `KeyboardControl` out into `Device/KeyboardControl.{cpp,h}`.

**Collision:** `MREmu/Keyboard.h` was deleted in upstream but modified in master
(my key-repeat work). Git flagged it as a modify/delete conflict. Resolved by
deleting the old file, keeping the moved version in `Device/`, and re-applying
master's key-repeat changes on top of the new layout.

### 3. `KeyboardControl::update()` is inert in upstream

Upstream's `update()` just bumps `key_status` from `DOWN` to `REPEAT` after 500ms
and never actually sends `VM_KEY_EVENT_REPEAT`. Worse, upstream's `Device`
**never calls `update()` at all**.

**Collision:** master's accelerating key-repeat curve (500ms → 50ms) lived in
`KeyboardControl::update()` which master called every frame. Resolved by:
- Keeping master's accelerating curve (changed to steady 350ms/120ms in commit
  `047e100` for rotating-mass motor reasons)
- Adding `device.update()` calls in both the main loop and the MCP window loop
  so repeats actually fire

## String type changes broke the guest-facing boundary

Upstream changed:
- `Log::set_module(const std::string)` → `set_module(const std::string_view)`
- `App::app_name`: `std::string` → `std::u8string`

**Collision:** `MREmu.cpp` and `McpHost.cpp` passed `app_name` (now `u8string`)
into `S()` and `std::ostringstream` (both `std::string`). Resolved by adding a
`u8_to_utf8()` helper in `MREngine/CharSet` and converting at the two call sites.

## API removals by upstream that master had implemented

Upstream deleted several Tier-1 APIs that master had implemented:

- `vm_file_rmdir`, `vm_get_default_folder_path`, `vm_get_multi_tags`,
  `vm_kbd_set_mode`, `vm_sys_file_{open,seek,read,write,delete,get_space}`,
  `vm_get_disk_{free_space,info}`

**Resolution:** Kept master's implementations. Upstream's `IO.cpp`/`Resources.cpp`
had deleted them; the merge took master's versions.

## IMGUI/Android build breakage

Upstream's `MREmu.cpp` unconditionally `#include <imgui.h>`, but upstream builds
Android without `DEBUG_WINDOW`, so imgui isn't compiled. This breaks Android.

**Resolution:** Guarded the imgui includes with `#ifdef DEBUG_WINDOW` everywhere
(`MREmu.cpp`, `IO.cpp`, `Canvas.cpp`, `Graphic.cpp`, `Cpu.cpp`).

## `.gitignore` and `README.md` were trivial but noisy

Both files had edits on both sides. Resolved by taking master's plus upstream's
Emscripten credits in `README.md` and the `em/` directory ignore in `.gitignore`.

## The "other agent's" uncommitted work

Before merging, there was uncommitted work on:
`IO.cpp`, `IO.h`, `Sock.h`, `Resources.cpp`, `System.cpp`, `Textout.cpp`,
`McpHost.cpp`, `AppSelector.cpp`, `MainActivity.kt`, `Keyboard.cpp/h`,
`MREmu.cpp`, `System.cpp`.

This was stashed before the merge, committed as `047e100`, then re-applied. The
key-repeat commit (`047e100`) intentionally **reversed** upstream's flat 500ms
repeat in favor of a steady 350ms initial / 120ms repeat, because the faster
rate retriggered faster than a rotating-mass motor could spin up.

## What to watch for next time upstream advances

| Area | Risk |
|------|------|
| `MREmu.cpp` | Upstream keeps rewriting the main loop. Any future merge will re-conflict with master's MCP/inspect/DragAndDrop code. |
| `Device/` | Upstream owns this directory now. Master's key-repeat lives in `Device/KeyboardControl.cpp`. |
| `AppSelector` / `BottomSheet` | Upstream deleted both. Master's grid view lives in `AppSelector.cpp`. |
| String types | Upstream is moving toward `std::u8string` for `app_name`. More conversion points will appear. |
| Emscripten | Upstream has working Emscripten support; master's desktop build doesn't conflict but the CMake files diverge. |

## Recommended merge procedure

1. **Stash or commit all working-tree changes first.** The other agent's
   uncommitted work on `IO.cpp`/`System.cpp` etc. would have been silently
   overwritten by the merge otherwise.
2. **Fetch both remotes** before merging: `git fetch upstream && git fetch origin`.
3. **Merge with `--no-commit`** so you can review all conflict resolutions
   before committing.
4. **Build both desktop and Android** after resolving — the Android imgui
   breakage is not caught by desktop builds.
5. **Check `KeyboardControl::update()` is actually called** — upstream's `Device`
   doesn't call it, so key repeat silently stops working.
6. **Verify `vm_file_rmdir` and the other Tier-1 APIs survive** — upstream has
   deleted them before and may again.
