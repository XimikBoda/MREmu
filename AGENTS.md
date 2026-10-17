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
Covered by `assembleDebug`; CMake builds `sfml-main` and `MREmu` for `arm64-v8a` only.

### Offline mode (when deps are already cached)
Append `--offline` to any Gradle command:
```
./gradlew :app:compileDebugKotlin --offline -q
```

### Trim noisy output (keep context small)
```
./gradlew ... -q 2>&1 | tail -40
```

## Rules the agent must follow

1. **NEVER run `clean` or `./gradlew --stop`.** Both throw away the warm Gradle daemon and caches, forcing a full rebuild.
2. **Batch all edits, then build once.** Do not build after every small change.
3. **Use narrow tasks.** Prefer `:app:compileDebugKotlin` for checking edits. Use `assembleDebug` only when an APK is needed. Skip lint and full test suites unless explicitly asked.
4. **Use `--offline`** when dependencies are already cached.
5. **ABIs are restricted to `arm64-v8a`** in `Android/app/build.gradle`. Do not add more ABIs.
6. **Configuration cache is enabled** (`org.gradle.configuration-cache=true` in `Android/gradle.properties`). Avoid editing build scripts in ways that invalidate the configuration cache unnecessarily.
7. **Generated directories are gitignored** and must not be committed: `build/`, `.gradle/`, `.cxx/`, `Android/app/build`, `Android/app/.cxx`, `Android/.gradle`.

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
- `Android/app/build.gradle` — `externalNativeBuild.cmake.path = ../../CMakeLists.txt`, targets `sfml-main`, `MREmu`, `abiFilters arm64-v8a`