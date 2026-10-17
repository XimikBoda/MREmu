# MREmu WebAssembly / Browser Port Technical Analysis & Migration Plan

## Executive Summary

This document provides a comprehensive repository audit and technical migration plan for porting **MREmu** (a MediaTek MRE/VXP and VRE platform emulator) to a browser-based WebAssembly (WASM) application.

The analysis evaluates the project structure, dependencies, OS-specific implementations, execution paradigms, rendering/audio pipelines, and hardware emulation layers. It identifies components that can compile directly to WebAssembly using Emscripten, details required browser-specific replacements, and highlights major blockers and technical risks.

---

## 1. Complete Repository Audit

### 1.1 Project Structure
The repository is structured into the following primary directories:
- `MREmu/`: Core emulator logic, CPU execution wrappers, memory management, bridge layers, input handling, and platform main entry points (`MREmu.cpp`).
- `MREmu/MREngine/`: Implementation of MediaTek MRE SDK API functions (graphics, audio, file IO, timers, system, sockets, sensors, character sets).
- `MREmu/NativeApps/`: Built-in native applications compiled into the binary (e.g., `AppSelector` launcher).
- `MREmu/jni/`: Android JNI binding files (`Vibration.cpp`, `Vibration.h`).
- `deps/`: Git submodules and bundled third-party libraries (`SFML`, `imgui`, `unicorn`, `capstone`, `libADLMIDI`, `libiconv-cmake`, `spdlog`).
- `include/`: Header-only third-party libraries (`elfio`, `cmdparser.hpp`, `gif.h`).
- `sub/`: Auxiliary build tools and test executables (`FontHexToBin`, `ARModule`).
- `Android/`: Android Studio project files, Gradle build scripts, and Java wrappers for Android deployments.
- `docs/`: Build instructions and documentation.

### 1.2 C and C++ Source Files
- **Language Standards:** C++20 standard (enforced in `CMakeLists.txt` via `set_property(TARGET ... PROPERTY CXX_STANDARD 20)`) and standard C99/C11 for third-party libraries like `miniz` and `ARModule`.
- **C++ Usage:** Heavy rely on standard C++ features (`std::vector`, `std::string`, `std::mutex`, `std::thread`, `std::filesystem`, `std::atomic`).
- **Main Files:**
  - `MREmu/MREmu.cpp`: Entry point (`main()`), window creation, main loop, event loop, and thread dispatch.
  - `MREmu/Cpu.cpp` & `MREmu/Memory.cpp`: ARM CPU execution abstraction via Unicorn and linear emulated memory allocation.
  - `MREmu/Bridge.cpp`: Translates ARM system calls / SDK API calls into C++ engine host functions.
  - `MREmu/ArmApp.cpp`, `MREmu/DLLApp.cpp`, `MREmu/NativeApp.cpp`: Loaders for ARM ELF / compressed VXP apps, Windows x86 DLL apps, and C++ native host apps.

### 1.3 Build System & CMake Configuration
- **Root CMake File (`CMakeLists.txt`):** Targets CMake 3.8+, configures global build options (`CAPSTONE`, `BUILD_SHARED_LIBS=FALSE`), includes standard `deps`, and adds the `MREmu` target.
- **Subdirectory CMake Files:**
  - `deps/CMakeLists.txt`: Configures submodules (`SFML`, `imgui`, `libiconv`, `capstone`, `libADLMIDI`, `spdlog`, `unicorn`).
  - `MREmu/CMakeLists.txt`: Builds the main executable or shared library (`add_executable` / `add_library` for Android), links against dependencies, and sets C++20 flags.
  - `MREmu/MREngine/CMakeLists.txt`: Builds `MREngine` static library and includes MRE SDK headers (`$ENV{MRE_SDK}/include`).
- **Target Outputs:** Executable `bin/MREmu` on desktop platforms or shared object `libMREmu.so` on Android.

### 1.4 Git Submodules and Dependencies
- **`deps/SFML` (v2.6.2):** Used for windowing, event management, OpenGL rendering wrappers, texturing, clock/timing, audio streaming (`sf::SoundStream`), and TCP networking (`sf::TcpSocket`).
- **`deps/unicorn` (master):** CPU emulator framework based on QEMU. Simulates ARM core (ARMv5TE instruction set execution).
- **`deps/capstone` (v5.0.1):** Optional disassembly framework used in debug mode to disassemble ARM instructions.
- **`deps/libADLMIDI` (v1.5.1):** Software OPL3 synthesizer used for MIDI audio playback.
- **`deps/libiconv-cmake` (1.17):** Character encoding conversion library (`iconv`) used in `MREngine/CharSet.cpp` to convert UCS-2 / UTF-16 to UTF-8 and back.
- **`deps/spdlog` (v1.17.0):** Logging library supporting stdout/file sinks and Android logcat.
- **`deps/imgui`:** Dear ImGui library for debug windows and UI widgets.

### 1.5 MRE SDK Dependencies and `vm*.h` Headers
- MRE applications rely on MediaTek MRE C SDK headers (`vmgraph.h`, `vmmm.h`, `vmio.h`, `vmsys.h`, `vmtimer.h`, `vmpromng.h`, `vmsim.h`, `vmlog.h`, `vmchset.h`, `vmbitstream.h`, `vmsensor.h`, `vmres.h`, `vm4res.h`, `vmgfxold.h`).
- In `MREngine/`, C++ implementations export standard MRE C APIs and hook them into host logic. `Bridge.cpp` dynamically registers these function addresses into the emulated CPU address space.

### 1.6 Unicorn Usage
- Unicorn (`uc_engine*`) is the core engine for executing ARM binary code in MREmu (`Cpu.cpp`).
- **Key Unicorn APIs Used:**
  - `uc_open(UC_ARCH_ARM, UC_MODE_THUMB, &uc)`
  - `uc_mem_map_ptr(...)`: Maps shared host memory into ARM emulated space.
  - `uc_reg_read()` / `uc_reg_write()`: Accesses ARM registers (R0-R12, SP, LR, PC, CPSR).
  - `uc_hook_add()`: Hooks code execution, unmapped memory reads/writes, and unaligned accesses.
  - `uc_emu_start()` / `uc_emu_stop()`: Starts and stops ARM instruction execution.
- **Multi-Context Architecture:** `Cpu::push_cpu()` and `Cpu::pop_cpu()` save and restore CPU contexts using `uc_context_alloc`, `uc_context_save`, and `uc_context_restore` to support nested callbacks and API re-entrancy.

### 1.7 Capstone Usage
- Guarded by preprocessor macro `#ifdef CAPSTONE` (enabled via CMake option `CAPSTONE=ON`).
- Disassembles ARM and Thumb instructions into human-readable assembly during CPU execution hooks in debug builds.
- Non-essential for app execution; strictly a developer diagnostic feature.

### 1.8 SFML Usage
- **Graphics & Windowing (`SFML/Graphics.hpp`, `SFML/Window.hpp`):** Creates desktop display windows (`sf::RenderWindow`), renders hardware sprites (`sf::Sprite`), manages screen textures (`sf::Texture`), and polls GUI/input events (`sf::Event`).
- **Audio Subsystem (`SFML/Audio.hpp`):** Custom audio streaming class `Midi` inherits from `sf::SoundStream` to push PCM buffers generated by `libADLMIDI` to host audio devices.
- **System Utilities (`SFML/System.hpp`):** High-resolution time measurement (`sf::Clock`), sleep utility (`sf::sleep`), and time representations (`sf::Time`).
- **Networking (`SFML/Network.hpp`):** `sf::TcpSocket` used in `MREngine/Sock.cpp` and `GDB.cpp`.

### 1.9 libADLMIDI and Audio Dependencies
- Used in `MREngine/Audio.cpp` to decode and synthesize MIDI/OPL audio buffers.
- Synthesizes 16-bit PCM audio chunks (44.1 kHz, stereo) via `adl_play()` and feeds them into SFML's `sf::SoundStream` buffer callback `onGetData()`.
- Thread-safe wrapper via `std::lock_guard<std::mutex> access_mutex`.

### 1.10 Android-Specific Code
- Conditional blocks (`#ifdef ANDROID`) in `MREmu.cpp`, `MREngine/IO.cpp`, and CMake files.
- `MREmu/jni/Vibration.cpp`: Calls Android NDK / JNI APIs (`ANativeActivity`, `JNIEnv`) for device vibration.
- Uses Android-specific path (`/sdcard/MREmu/`) for root filesystem access.
- Custom JNI entry points for permission handling (`Java_com_ximikboda_mremu_MainActivity_notifyPermissionState`) and VXP file loading (`Java_com_ximikboda_mremu_MainActivity_nativeLoadVxpFile`).
- Links against OpenAL (`libopenal.so`) bundled in SFML android external libraries.

### 1.11 Linux / Windows Specific Code
- **Windows Specific (`#ifdef _WIN32`):**
  - `MREmu/DLLApp.cpp`: Windows-only loader for MRE applications compiled as x86 Windows DLL files (uses Windows API `LoadLibrary`, `GetProcAddress`).
  - Filename handling: Wide string paths (`_wfopen`, `_wfopen_s` in `miniz.c`, `IO.cpp`).
  - System commands: `system("cls")` in `Cpu.cpp`.
- **Linux Specific:** Standard POSIX file paths, `/dev/null` access in `AppManager.cpp`, slash normalization in path handling.

### 1.12 Filesystem Operations
- **Library:** Standard C++ header `<filesystem>` (`std::filesystem` aliased as `fs`).
- **Implementation (`MREngine/IO.cpp`):**
  - Translates MRE virtual drive paths (e.g., `C:\`, `E:\`, `D:\`) to local system paths (`./fs/c/`, `./fs/e/`, `./fs/d/`).
  - Uses `fs::directory_iterator` for MRE `vm_find_first` / `vm_find_next` implementations.
  - File I/O uses standard C stdio calls (`fopen`, `fread`, `fwrite`, `fseek`, `ftell`, `fclose`) and `<fstream>`.
  - UTF-16 / UCS-2 string conversions to UTF-8 for filesystem paths via `MREngine/CharSet.cpp`.

### 1.13 Threading and Synchronization
- **Threading Model:** Multi-threaded architecture.
  - **Thread 1 (Main UI Thread):** Host window rendering (SFML/ImGui), frame updates, window event processing, touch/keyboard input polling.
  - **Thread 2 (`mre_main` Thread):** Created via `std::thread second_thread(mre_main, &appManager)`. Handles GDB updates and core MRE application execution (`appManager.update(delta_ms)`).
  - **Thread 3 (Audio Stream Thread):** Managed internally by SFML `sf::SoundStream` background worker thread to pull audio samples via `onGetData()`.
- **Synchronization:** `std::mutex`, `std::lock_guard`, `std::unique_lock`, and `mutex_wrapper.h`.

### 1.14 Graphics & Rendering Architecture
- **Buffer Architecture:**
  - Standard MRE screen buffer (typically 240x320 or custom resolutions) maintained in RGB565 / ARGB8888 pixel formats in `MREngine/Graphic.cpp` and `MREngine/Canvas.cpp`.
  - Framebuffer converted/uploaded to SFML texture (`sf::Texture screen_tex`).
  - SFML renders `screen_tex` via `sf::Sprite` onto host `sf::RenderWindow`.
- **ImGui Debug Overlay:** Integrates `imgui-SFML` to draw CPU register states, memory utilization modal dialogues, layer controls, and FPS counter.

### 1.15 Audio Architecture
- **MRE Audio APIs (`MREngine/Audio.cpp`, `MREngine/AudioBitstream.cpp`):** Supports MIDI playback (`vm_midi_*`), audio resource playback (`vm_audio_*`), and bitstream recording/playback.
- **Synthesis & Output Pipeline:** `libADLMIDI` synthesizes raw PCM -> custom `sf::SoundStream` subclass pushes PCM -> SFML OpenAL backend outputs to system speakers.

### 1.16 Input Handling
- **Hardware Keyboard Emulation (`Keyboard.cpp`):** Maps host PC keyboard keys (or on-screen touch buttons) to MRE keycodes (`VM_KEY_NUM0`–`VM_KEY_NUM9`, `VM_KEY_LEFT`, `VM_KEY_RIGHT`, `VM_KEY_UP`, `VM_KEY_DOWN`, `VM_KEY_OK`, etc.).
- **Touch Screen Emulation (`Touch.cpp`):** Converts SFML mouse/touch coordinates relative to the screen sprite into MRE touch events (`VM_PEN_EVENT_TAP`, `VM_PEN_EVENT_MOVE`, `VM_PEN_EVENT_RELEASE`).

### 1.17 VXP / VRE / MRE File Loading and Execution
- **ARM Executable Loading (`ArmApp.cpp`):**
  - Parses VXP files containing compressed ADS ELF binaries (Zlib compressed via `miniz`).
  - Reads segment section offsets (`RO`, `RW`, `ZI`), allocates linear emulated RAM (`Memory::shared_malloc`), decompresses sections into memory.
  - Configures ADS ARM stack/heap pointers and sets entry point PC in Unicorn CPU engine.
- **Dynamic Bridge Call Binding (`Bridge.cpp`):**
  - Allocates trampoline ARM code (`bxlr` instructions) in dedicated emulated bridge memory.
  - When ARM app executes an API call, it jumps to the trampoline address, triggering a Unicorn hook or custom CPU exit, which executes the corresponding C++ engine function.

### 1.18 Dynamic Libraries and Native APIs
- **Windows DLL Applications (`DLLApp.cpp`):** Executed natively on Windows host by loading dynamic x86 DLL binaries. *Incompatible with non-Windows x86 platforms and WebAssembly.*
- **Native Host Applications (`NativeApps/`):** C++ applications (like `AppSelector`) compiled directly into the emulator executable using host C++ code.

---

## 2. WebAssembly Compatibility Analysis

### 2.1 Portable C/C++ Core Components (Direct WASM Compilation)
The following core components are pure C/C++ logic with minimal OS dependencies and can be compiled directly to WebAssembly using Emscripten (`emcc`/`em++`):

1. **Unicorn Engine (`deps/unicorn`):** Pure C core based on QEMU target translation. Fully compilable to WASM.
2. **`MREngine` Business Logic:** `Graphic.cpp`, `Canvas.cpp`, `Blt.cpp`, `Image.cpp`, `Resources.cpp`, `CharSet.cpp`, `STDLib.cpp`, `MreTags.cpp`, `miniz.c`, `ARModule.cpp`.
3. **Memory Management (`Memory.cpp`):** Allocates large contiguous std::vector or `malloc` buffers for emulated 32-bit ARM memory space.
4. **ARM App Loader (`ArmApp.cpp`):** Zlib decompressor and ADS ELF memory layout parser.
5. **Bridge Invocation Dispatcher (`Bridge.cpp`):** Function lookup tables and trampoline address mappers.
6. **`libADLMIDI` (`deps/libADLMIDI`):** C++ OPL3 synthesis engine.
7. **`libiconv` (`deps/libiconv-cmake`):** Encoding conversion.
8. **`spdlog`:** Can output directly to browser `console.log` / `console.error`.

### 2.2 Platform-Bound Components (Requiring Browser Replacements)

| Component | Current Desktop / Mobile Implementation | Web / Browser Target Replacement |
| :--- | :--- | :--- |
| **Window & Rendering** | SFML `sf::RenderWindow`, `sf::Texture` | WebGL 1.0/2.0 via SDL2 Wasm / Canvas2D API |
| **Audio Output** | SFML `sf::SoundStream` / OpenAL | Web Audio API / AudioWorklet / SDL2 Audio |
| **File I/O & Persistence** | `std::filesystem`, stdio (`fopen`) | Emscripten IDBFS / MEMFS / HTML5 Drag-and-Drop |
| **Threading Model** | `std::thread` (`mre_main` worker thread) | Single-threaded Emscripten Main Loop (`emscripten_set_main_loop`) or Web Workers with `SharedArrayBuffer` |
| **Input Events** | SFML `sf::Event` polling | HTML5 Canvas Mouse/Touch/Keyboard Event Listeners |
| **Networking** | SFML `sf::TcpSocket` (BSD Sockets) | Emscripten WebSocket Proxy or WebSockets JS Wrapper |
| **Windows DLL Apps** | `DLLApp.cpp` (`LoadLibrary`) | *Disabled/Excluded* (WASM only supports ARM VXP & Native C++ apps) |

---

## 3. Web & Browser-Specific Replacement Strategy

### 3.1 Rendering Engine
- **Replace SFML Graphics with SDL2 / WebGL:**
  - Configure Emscripten CMake toolchain with `-s USE_SDL=2`.
  - Create SDL2 window and renderer pointing to an HTML `<canvas id="canvas">`.
  - Render screen framebuffers (RGB565 / ARGB8888) directly into an `SDL_Texture` or HTML5 `ImageData` buffer.
  - *Optionally* replace `imgui-SFML` with `imgui-SDL2` + `imgui-OpenGL3` if debug overlay UI is desired in web builds.

### 3.2 Audio Subsystem
- **Replace SFML Audio with Web Audio API:**
  - Route PCM output generated by `libADLMIDI` directly into Web Audio API via Emscripten SDL2 Audio callbacks (`SDL_QueueAudio` / `SDL_OpenAudioDevice`).
  - Implement Web Audio context auto-resume on first canvas touch/click event to comply with browser autoplay policies.

### 3.3 Input & UI System
- **Browser Event Listeners:**
  - Bind HTML5 DOM input events (`keydown`, `keyup`, `mousedown`, `mousemove`, `mouseup`, `touchstart`, `touchmove`, `touchend`) directly to `Keyboard.cpp` and `Touch.cpp`.
  - Render virtual phone keypad overlay directly on the HTML page using CSS/JS or draw it onto the Canvas.

### 3.4 Filesystem Subsystem
- **Emscripten Virtual Filesystem:**
  - Use **MEMFS** for fast in-memory execution of VXP files.
  - Mount **IDBFS** (IndexedDB Filesystem) under `/fs/` (`/fs/c`, `/fs/e`) to persist save data, configuration files, and game states across browser reloads.
  - Provide JavaScript file drop handlers (`<input type="file">` / drag-and-drop) to upload VXP files into MEMFS dynamically.

### 3.5 Threading Model & Async Execution
- **Refactoring Dual Threads into Single-Threaded Emscripten Main Loop:**
  - Standard browsers block synchronous `while(true)` loops and synchronous sleep functions (`sf::sleep`).
  - Convert `MREmu.cpp` main execution loop into an asynchronous main loop using `emscripten_set_main_loop_arg()` running at 60 FPS.
  - Split `mre_main` CPU execution update steps into slice-based frame updates executed inside the main loop iteration.
  - Alternatively, compile with Emscripten Pthreads (`-s USE_PTHREADS=1 -s SHARED_MEMORY=1`) using Web Workers and `SharedArrayBuffer` (requires COOP/COEP HTTP headers on web server).

### 3.6 Network / Sockets
- **WebSocket Replacement:**
  - Map `MREngine/Sock.cpp` calls to Emscripten Sockets library (`-s POSIX_SOCKETS=1 -s WEBSOCKETS=1`).
  - Relay TCP traffic through a WebSocket-to-TCP proxy server.

---

## 4. Technical Blockers & Key Risks

### 4.1 Unicorn Engine WASM Compilation & Performance
- **Blocker / Risk:** Unicorn relies on QEMU Just-In-Time (JIT) code generation and dynamic binary translation (`tcg`). Standard JIT memory allocation (`mprotect` with `PROT_EXEC`) is unsupported in standard WebAssembly sandbox environments.
- **Mitigation / Solution:** Unicorn MUST be compiled with QEMU's TCG interpreter mode (**TCI** - Tiny Code Generator Interpreter) or compiled using Unicorn v2.x with JIT disabled for WebAssembly target. Performance will be slower than native execution, but sufficient for 200MHz ARMv5TE MRE feature phone software.

### 4.2 JIT / Memory Mapping Constraints (`uc_mem_map_ptr`)
- **Blocker / Risk:** `Cpu.cpp` uses `uc_mem_map_ptr()` to map host pointers directly into Unicorn ARM emulated space. In WebAssembly, pointers are 32-bit offsets within WASM linear memory (`WebAssembly.Memory`).
- **Mitigation / Solution:** Ensure Unicorn WASM build allocates memory inside the WASM linear heap or utilizes `uc_mem_map()` with memory copying if raw pointer mapping fails across WASM module boundaries.

### 4.3 Multithreading & Synchronous Blocking
- **Blocker / Risk:** `MREmu.cpp` spawns a background thread `second_thread(mre_main)` and relies on synchronous `sf::sleep()`. Standard single-threaded WASM builds will hang indefinitely if synchronous sleep is called on the main thread.
- **Mitigation / Solution:** Refactor execution engine into frame-driven tick updates (`appManager.update(delta_ms)` called per requestAnimationFrame/emscripten frame loop) or enable Emscripten Asyncify (`-s ASYNCIFY=1`), though Asyncify introduces performance overhead.

### 4.4 DLLApp Dynamic Loading
- **Blocker / Risk:** `DLLApp.cpp` uses Windows x86 dynamic libraries (`LoadLibrary`, `GetProcAddress`). Windows DLL binaries CANNOT run in WebAssembly or non-x86 browser environments.
- **Mitigation / Solution:** Disable `DLLApp` entirely in web builds (`#ifndef __EMSCRIPTEN__`). Only support standard ARM ELF VXP files and C++ native applications.

### 4.5 Audio Streaming Latency & Web Audio Policy
- **Blocker / Risk:** Web browsers block Web Audio AudioContext initialization until user interaction (click/tap).
- **Mitigation / Solution:** Delay `MREngine::AppAudio::init()` audio hardware initialization until the user interacts with the canvas element on the webpage.

### 4.6 Filesystem Persistence & Cross-Origin Constraints
- **Blocker / Risk:** Standard `std::filesystem` directory creation (`/sdcard/MREmu/`) fails on browser sandboxes without virtual filesystem mounting.
- **Mitigation / Solution:** Set `base_path` to `/offline` or `/fs` under Emscripten IDBFS and invoke `FS.syncfs()` after write operations.

---

## 5. Phased Web Port Migration Roadmap

1. **Phase 1: Build System & Dependency Abstraction**
   - Create Emscripten CMake toolchain configuration (`CMakePresets.json` or cross-compilation toolchain file).
   - Add conditional macros (`#ifndef __EMSCRIPTEN__`) to isolate SFML, Windows DLL loader, and native desktop window creation.

2. **Phase 2: Execution Engine Adaptation (Unicorn WASM)**
   - Port Unicorn engine to compile under Emscripten with TCI interpreter mode.
   - Refactor `MREmu.cpp` dual-threaded loop into a frame-based asynchronous callback architecture using `emscripten_set_main_loop`.

3. **Phase 3: WebGL & Input Abstraction (SDL2 Integration)**
   - Replace SFML rendering and windowing calls with SDL2 WebGL canvas backend.
   - Map browser touch and keyboard events to `Touch.cpp` and `Keyboard.cpp`.

4. **Phase 4: Web Audio & Virtual Filesystem**
   - Connect `libADLMIDI` PCM buffer output to SDL2 Audio / Web Audio API.
   - Setup Emscripten MEMFS / IDBFS persistent filesystem for MRE application virtual drives (`C:`, `E:`).

5. **Phase 5: Web Frontend Shell & CI/CD Packaging**
   - Build responsive HTML5 wrapper with mobile phone canvas display and on-screen keypad.
   - Implement JavaScript file input loader to drop `.vxp` files directly into the emulator.
