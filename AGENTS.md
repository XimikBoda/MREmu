# MREmu — Agent Build & Repo Guide

## Project overview
MREmu is a C++ emulator with an Android (Compose) frontend.
- Root C++ source: `MREmu/` (engine, JNI bridge in `MREmu/jni/`)
- Root headers: `include/`
- Dependencies (CMake): `deps/` (SFML, imgui, spdlog, capstone, unicorn, libiconv-cmake, libADLMIDI, asio.cmake)
- Root CMake: `CMakeLists.txt`, `CMakePresets.json`
- Android module: `Android/` (Gradle), single `:app` module
- Android app source: `Android/app/src/main/java/com/ximikboda/mremu/` (Kotlin)
- JNI/CMake entry for Android: `Android/app/build.gradle` points `externalNativeBuild.cmake.path` at `../../CMakeLists.txt`
- Android native targets built: `sfml-main`, `MREmu`

## Build commands (use these, nothing broader)

### Kotlin compile check (fast — use for edit verification)
```
./gradlew :app:compileDebugKotlin -q
```

### Full Android APK (only when an APK is actually needed)
```
./gradlew :app:assembleDebug
```

### Native / JNI build (via Gradle, triggers CMake)
Covered by `assembleDebug`; CMake builds `sfml-main` and `MREmu` for whichever ABIs the `mremuAbis` property selects (default `arm64-v8a`).

### Offline mode (when deps are already cached)
Append `--offline` to any Gradle command:
```
./gradlew :app:compileDebugKotlin --offline -q
```

### Trim noisy output (keep context small)
```
./gradlew ... -q 2>&1 | tail -40
```

## Desktop (non-Android) tooling

The desktop target additionally builds `McpServer.cpp` and `McpHost.cpp`, which are
excluded on Android. Desktop-only changes still need a desktop build to be verified:

```
cmake --build build --config Release      # from repo root -> bin\Release\MREmu.exe
```

Once built, the emulator can be driven without a display:

- `MREmu.exe <vxp> --json out.json --steps 2000 --screen shot.png --inspect` — one-shot
  report. Note `--steps` above ~5,000 crashes; see README known issues.
- `MREmu.exe <vxp> --mcp` — MCP server over stdio (12 tools: status, registers,
  pause/step/resume, screen, errors, VM API tracer). Registered in the user's OpenCode
  config as the `mremu` MCP server.
- `MREmu.exe <vxp> --mcp --mcp-window` — same server, plus a real window showing the
  emulated screen and a status line (run state, VM call count, trace, fault) so a human can
  watch an agent work. The protocol thread moves to a worker because Win32 delivers window
  messages to the creating thread; closing the window exits the process via `std::_Exit`
  (the protocol thread is parked reading stdin, so running static destructors would race
  it). `MREMU_MCP_WINDOW=1` in the environment does the same without a CLI flag.
- `cmdparser` gotcha: long names are matched **exactly**, and an unrecognised `-`-token is
  swallowed as an *argument to the previous flag*. So `--mcp-window` must be declared as
  `"mcp-window"` (a `mcp_window` declaration makes `--mcp` fail with
  "The parameter m has invalid arguments"), and a bool flag must be the last token.
- Canvas buffer convention: a **layer** hands out its pixel pointer (`base_buf1`), but
  `vm_graphic_get_canvas_buffer` hands out the **canvas base**. `find_canvas_signature()`
  accepts both; any draw routine that hand-rolls `buf - VM_CANVAS_DATA_OFFSET` will fail
  the magic check and silently draw nothing for app-owned canvases. Use the helper.
- Only `vm_graphic_flush_layer` writes `graphic->screen` from layers. App canvases are
  otherwise never presented, so `vm_graphic_flush_layer` composites display-sized
  (exactly 240x320) canvases underneath first. The exact size match is deliberate — apps
  also own scratch canvases (Opera Mini allocates a 320x4 rule and a 320x16 text row) that
  must not be presented.
- `python C:\Users\User\Downloads\Opera Mini 6.1-analysis\test_mcp.py` — the stdio test
  suite that exercises every tool end to end.
- `python C:\Users\User\Downloads\Opera Mini 6.1-analysis\test_mcp_window.py` — the same
  tools through `--mcp --mcp-window`, plus an `EnumWindows` check that a real visible window
  opened and that its title tracks the agent's run state. A window flashes on screen.

## Rules the agent must follow

1. **NEVER run `clean` or `./gradlew --stop`.** Both throw away the warm Gradle daemon and caches, forcing a full rebuild.
2. **Batch all edits, then build once.** Do not build after every small change.
3. **Use narrow tasks.** Prefer `:app:compileDebugKotlin` for checking edits. Use `assembleDebug` only when an APK is needed. Skip lint and full test suites unless explicitly asked.
4. **Use `--offline`** when dependencies are already cached.
5. **The default ABI list is `arm64-v8a` only**, set by the `mremuAbis` property in `Android/app/build.gradle`. Keep that default so everyday dev builds stay single-ABI and fast. Build other ABIs on demand — `-PmremuAbis=armeabi-v7a`, comma-separated for several, or `launch.bat` Android option `[6]`, which builds a Release APK for a chosen ABI and saves it to `~/Downloads` as `MREmu-<abi>-<version>.apk`. Supported: `arm64-v8a`, `armeabi-v7a`, `x86_64`.
6. **Configuration cache is enabled** (`org.gradle.configuration-cache=true` in `Android/gradle.properties`). Avoid editing build scripts in ways that invalidate the configuration cache unnecessarily.
7. **Generated directories are gitignored** and must not be committed: `build/`, `.gradle/`, `.cxx/`, `Android/app/build`, `Android/app/.cxx`, `Android/.gradle`.
8. **Ask the user with the harness `question` tool, never as plain text.** Any time a decision is needed — scope, trade-offs, which of several options, anything that would otherwise be typed as "which do you prefer?" — call the `question` tool rather than embedding a question in the response. Each question must offer a concrete set of options and mark the recommended one first with `(Recommended)`, stating the reasoning, so the user can accept it in one keystroke or override it without having to type anything. Do not also restate the question in prose alongside the tool call. Reserve prose for reporting findings, not for requesting decisions.

## Repository layout (top level)
```
Android/          Gradle Android project (settings.gradle, build.gradle, gradle.properties, app/)
MREmu/            C++ emulator core (+ jni/ for Android JNI bridge)
include/          Public headers (elfio, cmdparser, gif)
deps/             Third-party CMake dependencies
bin/              Build output root
build/            Root build artifacts (gitignored)
```

## CMake / JNI files
- `CMakeLists.txt` (root) — includes `deps/` and `MREmu/`
- `deps/CMakeLists.txt`
- `MREmu/CMakeLists.txt`
- `Android/app/build.gradle` — `externalNativeBuild.cmake.path = ../../CMakeLists.txt`, targets `sfml-main`, `MREmu`; `ndk.abiFilters` is driven by the `mremuAbis` property (default `arm64-v8a`)