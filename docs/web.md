# Web (Emscripten) Build

MREmu also compiles to WebAssembly, which runs the emulator in a browser tab
and launches `.vxp` files uploaded through the page. This page covers building
it, running it, and the parts that behave differently from the desktop build.

## Building

The build lives behind `launch.bat` menu option `[8]`:

```
launch.bat            # interactive menu, then [8] Web (Emscripten)
launch.bat 8 1        # non-interactive: configure + build Release
launch.bat 8 2        # configure + build Debug
launch.bat 8 3        # serve the Release build and open it in a browser
launch.bat 8 4        # delete build/em and the copied web artifacts
```

`[1]`/`[2]` install the Emscripten SDK on first use (cloned into `em\emsdk`,
version 3.1.67) instead of asking you to install it yourself. The configure step
needs Ninja; it ships with emsdk, so nothing extra is required.

The CMake invocation is plain `cmake` with the Emscripten toolchain file:

```
cmake -S <repo> -B build/em -G Ninja -DCMAKE_BUILD_TYPE=Release -DEMSCRIPTEN=1 ^
  -DCMAKE_TOOLCHAIN_FILE=<emsdk>/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
```

`emcmake` is deliberately not used:

- its Python wrapper calls `exit`, which terminates the calling batch file the
  moment configure finishes, so the build step silently never runs;
- on Windows it appends `-G "MinGW Makefiles"`, which conflicts with the Ninja
  generator the Emscripten setup expects.

The target emits a complete `index.html` / `index.js` / `index.wasm` /
`index.worker.js` bundle into `build\em` (`MREmu/CMakeLists.txt` sets the output
name and shell page; the root `CMakeLists.txt` skips the desktop `bin\` output
directory for Emscripten). A post-build step also copies
[`web/mremu-web.js`](../web/mremu-web.js) and
[`web/coi-serviceworker.js`](../web/coi-serviceworker.js) next to the page and
creates `.nojekyll`. `launch.bat 8 1` then copies all of it to `bin\Release`
(or `bin\Debug`).

There is deliberately **no `.data` preload**. `bin\Release\fs` is the desktop
app's local filesystem (including whatever `.vxp` files were dropped into
`fs/e/mre`), so embedding it would publish that snapshot and add tens of
megabytes to the bundle. The web page is bring-your-own-apps: uploads are
mounted into `/fs/e/mre` at runtime instead. If a build ever adds a preload,
Emscripten will emit `index.data` again; the launcher copies it when present.

The page itself is [`web/shell.html`](../web/shell.html), passed to emcc with
`--shell-file`. It hosts the canvas, the apps panel, and the load overlay. The
shell must not declare a global `Module`: the runtime is built with
`-sMODULARIZE -sEXPORT_NAME=MREmuModule`, and `mremu-web.js` instantiates it
with its own module argument (see "How the page boots").

## Running

Browsers only expose `SharedArrayBuffer` to cross-origin isolated documents, and
the build uses `-pthread`, so **opening `index.html` from disk cannot work** -
`file://` sends no headers and the runtime dies with
`SharedArrayBuffer is not defined`.

### Local server

Serve it over loopback HTTP with COOP/COEP:

```
launch.bat 8 3
```

which starts [`tools/web_serve.py`](../tools/web_serve.py) on port 8123 and opens
the tab. To do it by hand:

```
python tools/web_serve.py --dir bin/Release --port 8123
```

The served page needs `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp`; any static server that adds that
pair works. No VXP catalog is bundled, so a fresh origin starts with an empty
emulator menu; add apps from the uploads panel.

### Static host (GitHub Pages, plain `http.server`)

A static host cannot send COOP/COEP headers, so the bundle ships
`coi-serviceworker.js`. The page registers it, the worker re-serves every
same-origin `GET` with the two headers added (plus `Cross-Origin-Resource-Policy:
same-origin`), and the page reloads itself **once** so the next navigation is
controlled by the worker. The reload is guarded by a `sessionStorage` flag, so a
worker that cannot activate produces an error message instead of a reload loop.

Consequences:

- The host only needs to serve the files. `python -m http.server` with **no
  custom headers** and GitHub Pages both work; the service worker must be
  served from the same scope (`/`), which the bundle layout guarantees.
- The first visit reloads once while the worker takes control. After that,
  `crossOriginIsolated` is true and the emulator boots normally.
- Service workers require a secure context: `https://` or `localhost` /
  `127.0.0.1`. A LAN-IP `http://` origin will not work.
- Nothing is cached. The worker only decorates response headers, so a rebuilt
  bundle is served fresh; if the browser itself cached `index.html`, do a hard
  reload.
- `.nojekyll` is included so GitHub Pages serves every bundle file verbatim.

The UI uses Inter, loaded from Google Fonts with a `system-ui` fallback. It is
the only third-party request the page makes and is purely cosmetic: if it is
blocked or the page is offline, the fallback is used and the emulator still
boots. Icons are Hugeicons Free (MIT), inlined as an SVG sprite, so they add no
network request.

Interaction feedback is deliberately flat: hovering a button changes only its
fill, never its border colour, and the primary button's border is transparent
so its outline never shifts. Pressing any button or the mute toggle scales it
to 0.95 for 160ms and back, which is the only motion in the panel.

Section headers in the panel ("DISPLAY", "AUDIO") are uppercase, brighter than
the rest of the text, and sit a uniform 8px above their control.

## Apps panel and uploads

The apps button (top-right of the page, once the runtime has started) opens the
options panel. The web build ships no apps, so the panel and the emulator menu
both start empty; this panel is how apps get into the web build. The panel
remembers whether it was open (`mremu.panel` in `localStorage`) and comes back
open after a reload. It offers:

- **Import VXP** - a multi-file picker; only `.vxp` files are accepted.
  Dropping `.vxp` files anywhere on the page opens the panel and imports them
  too.
- A conjoined list of uploaded apps (file icon, name, size) with an icon-only
  **Remove** button per row, plus an icon-only **Remove all** button. Removing
  everything asks first: a centred dialog with two equal, conjoined buttons
  (Cancel, Remove all) and the focus kept inside it.
- An icon-only **Statistics** button opens a second panel view with the uploaded
  app count, storage used, and notes about local persistence and drop importing.
  A back button returns to the apps view.
- Display scale (Fit, 1x-4x) with an icon-only fullscreen button, host mute,
  and a page reload button.

Checkbox options (currently just **Mute**) are list-like rows: the whole row is
the hit target and a circle on its left carries the state. Switching one on
pops the circle and draws the tick along its path.

Uploads are stored as `ArrayBuffer`s in IndexedDB (`mremu-web` database, `vxps`
object store, keyed by file name) and `navigator.storage.persist()` is requested
on the first upload. They survive page reloads, tab closes, and browser
restarts; they are removed only by the panel buttons, by clearing site data, or
by the browser evicting the origin.

They also appear in the emulator's app list without extra steps:

- On every load, `mremu-web.js` reads IndexedDB and writes the uploads into
  `/fs/e/mre` inside `onRuntimeInitialized`, i.e. after the runtime is up but
  before `main()` performs its first directory scan. The uploaded apps are
  therefore tiles on the very first frame, with no rescan flash.
- While the emulator is already running, an upload is written to the same
  directory immediately and `mremu_web_apps_changed()` tells the native menu to
  rescan. New tiles appear in the grid (sorted with the rest of the apps)
  and launch exactly like any other app.
- Removing an upload deletes it from IndexedDB and from the emulator's
  filesystem in the same step.

## Input

Click or tap the screen once before typing: browsers only start audio and route
keyboard input to the canvas after a user gesture, and the emulator ignores keys
while the document is unfocused.

Display scale changes the canvas's on-screen size without changing the emulated
framebuffer. Pointer coordinates are translated back into canvas pixels before
SFML sees them, so clicks and taps hit the same guest pixel at Fit and at every
fixed scale. A fixed scale taller than the viewport makes the stage scroll
instead of cropping the menu bar and the top of the screen.

- Arrow keys move the selection (they auto-repeat when held).
- **Space** activates the highlighted item ("Choose") and is the most reliable
  key to use; the browser/page can consume `Enter` before it reaches the canvas.
- The on-screen softkeys and the rest of the menu respond to mouse clicks.
- **Mute** in the panel silences host audio only. It does not change the guest's
  own volume, so unmuting restores whatever level the app had set.

## Differences from the desktop build

- **No MCP server / `--inspect`.** There is no stdio transport in a browser, so
  `McpServer.cpp`, `McpHost.cpp` and the MCP-only code paths are excluded under
  `__EMSCRIPTEN__`.
- **No ImGui debug window.** Built with `DEBUG_WINDOW=FALSE`.
- **No vibration.** `vm_vibrator_*` no-ops; the host motor hooks are Android-only.
- **No networking.** Asio sockets resolve but cannot reach the network from the
  wasm sandbox.
- **Slower.** `-pthread` plus `ASYNCIFY` keeps the guest's blocking calls working,
  at a cost; expect fewer steps per second than the native build.
- **Saves are per-tab.** The emulator runs on an in-memory Emscripten MEMFS
  tree; writes made by guest apps (save games and similar) last only for the
  lifetime of the tab. Uploaded `.vxp` files are the exception - they persist in
  IndexedDB as described above.

## How the page boots

Useful when changing the shell or the loader:

1. `shell.html` loads `coi-serviceworker.js` (page role), then `mremu-web.js`,
   then emcc's injected `index.js`.
2. `mremu-web.js` waits for `crossOriginIsolated` (up to 6 s), loads IndexedDB,
   and waits for `window.MREmuModule` to appear (up to 30 s).
3. It calls `window.MREmuModule(moduleArg)`; Emscripten uses that object as the
   module instance, so the loader keeps `Module.FS` handy for later uploads.
4. `onRuntimeInitialized` mounts the uploads, hides the overlay, focuses the
   canvas, applies the stored scale/mute preferences, and reveals the panel
   button - all before the first menu scan draws.

## Troubleshooting

| Symptom | Cause |
| :--- | :--- |
| `SharedArrayBuffer is not defined` | Page served without COOP/COEP, or opened via `file://`. Use `launch.bat 8 3`, or a secure (HTTPS/localhost) static host with `coi-serviceworker.js` beside `index.html`. |
| Page reloads once on first visit | Expected: the service worker takes control, then the page reloads to receive the COOP/COEP headers. |
| MRE menu says "No files in MRE folder" | Expected on a new web origin: the bundle contains no VXP catalog. Add `.vxp` files from the apps panel; they persist for that origin. |
| Page reports it is not cross-origin isolated | Service worker not registered: not a secure context (`http://` LAN address), `coi-serviceworker.js` missing, or storage blocked in a private window. |
| Uploaded app is gone after a reload | Site data was cleared, storage is blocked, or the origin evicted it. The panel summary left the app count at zero after reload. |
| A new upload does not appear in the menu | The menu rescan flag is consumed by the menu's own timer; make sure the emulator is running (not paused on a fatal overlay) and reopen the panel to confirm the app is listed. |
| `emcmake`/`emcc` not found | `emsdk_env.bat` only edits `PATH` when it thinks the parent shell is `cmd.exe`; started from Git Bash it writes `emsdk_set_env.sh` instead. `:web_env` in `launch.bat` sets the toolchain paths explicitly. |
| Configure fails downloading `CPM.cmake` | The CMake install has no CA bundle (`SSL certificate verification failed: certificate signer not trusted`). `launch.bat` passes `-DCMAKE_TLS_VERIFY=OFF` so the bootstrap can complete; the file is still checked against its pinned SHA256. Do not redirect `CPM_SOURCE_CACHE`: CPM's include guard then silently skips defining `CPMAddPackage`. |
| Build step prints nothing and stops | Something in the batch called `exit` (see `emcmake` above); use plain `cmake --build .`. |
| Blank page, no console errors | The canvas needs a click for audio/focus; check the network tab for a failed `index.wasm` / `index.worker.js`, and the console for a wasm compile error. |
