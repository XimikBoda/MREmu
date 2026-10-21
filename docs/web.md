# Web (Emscripten) Build

MREmu also compiles to WebAssembly, which runs the emulator and the whole VXP
catalog inside a browser tab. This page covers building it, running it, and the
parts that behave differently from the desktop build.

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
`index.data` bundle into `build\em` (`MREmu/CMakeLists.txt` sets the output name
and shell page; the root `CMakeLists.txt` skips the desktop `bin\` output
directory for Emscripten). `launch.bat 8 1` then copies the bundle to
`bin\Release` (or `bin\Debug`).

The page itself is [`web/shell.html`](../web/shell.html), passed to emcc with
`--shell-file`. It hosts the canvas, reports load progress, and focuses the
canvas on click so keyboard input reaches the emulated phone.

## Running

Browsers only expose `SharedArrayBuffer` to cross-origin isolated documents, and
the build uses `-pthread`, so **opening `index.html` from disk cannot work** -
`file://` sends no headers and the runtime dies with
`SharedArrayBuffer is not defined`.

Serve it over loopback HTTP with COOP/COEP instead:

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
pair works. The `.data` preload carries `bin/Release/fs` (the VXP catalog and
`mremu_view.cfg`), so whatever is in `fs/e/mre` at configure time is what the web
build can launch - about 43 MB with the full catalog.

Click the page once before typing: browsers only start audio and accept focus
after a user gesture.

## Differences from the desktop build

- **No MCP server / `--inspect`.** There is no stdio transport in a browser, so
  `McpServer.cpp`, `McpHost.cpp` and the MCP-only code paths are excluded under
  `__EMSCRIPTEN__`.
- **No vibration.** `vm_vibrator_*` no-ops; the host motor hooks are Android-only.
- **No networking.** Asio sockets resolve but cannot reach the network from the
  wasm sandbox.
- **Slower.** `-pthread` plus `ASYNCIFY` keeps the guest's blocking calls working,
  at a cost; expect fewer steps per second than the native build.
- **Filesystem is read-only.** The preloaded tree is an Emscripten MEMFS overlay;
  writes (save games) live only for the lifetime of the tab.

## Troubleshooting

| Symptom | Cause |
| :--- | :--- |
| `SharedArrayBuffer is not defined` | Page served without COOP/COEP, or opened via `file://`. Use `launch.bat 8 3`. |
| `emcmake`/`emcc` not found | `emsdk_env.bat` only edits `PATH` when it thinks the parent shell is `cmd.exe`; started from Git Bash it writes `emsdk_set_env.sh` instead. `:web_env` in `launch.bat` sets the toolchain paths explicitly. |
| Configure fails downloading `CPM.cmake` | The CMake install has no CA bundle (`SSL certificate verification failed: certificate signer not trusted`). When no cached copy exists, `launch.bat` passes `-DCMAKE_TLS_VERIFY=OFF` so the bootstrap can complete; the file is still checked against its pinned SHA256. Once downloaded the copy is reused, so this only happens on the first build. |
| Build step prints nothing and stops | Something in the batch called `exit` (see `emcmake` above); use plain `cmake --build .`. |
| Blank page, no console errors | The canvas needs a click for audio/focus; check the network tab for `index.data` (43 MB) still downloading. |
