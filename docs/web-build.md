# MREmu WebAssembly Build Instructions

This document explains how to build the WebAssembly (WASM) browser target for **MREmu** using Emscripten.

The WebAssembly build configuration is fully isolated in the `web/` directory and coexists with the existing native desktop and Android build configurations without modifying native C/C++ source files or CMake scripts.

---

## Prerequisites

1. **Emscripten SDK (emsdk)**
   Install and activate the latest Emscripten SDK:
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **CMake (3.13+) & Make/Ninja**
   Ensure modern CMake and build tools are installed:
   ```bash
   cmake --version
   ```

---

## Directory Structure (`web/`)

```
web/
├── CMakeLists.txt        # Isolated CMake configuration for Emscripten WASM build
├── main_web.cpp          # Web entry point using emscripten_set_main_loop
├── shell_minimal.html    # Emscripten HTML template with canvas viewport
└── build_web.sh          # Helper shell script for building via emcmake
```

---

## How to Build

### Using the Helper Script

Run the build script from the repository root:
```bash
./web/build_web.sh
```

### Manual Build Steps

1. Navigate to the `web/` folder and create a `build` directory:
   ```bash
   cd web
   mkdir -p build
   cd build
   ```

2. Run CMake with the Emscripten toolchain wrapper:
   ```bash
   emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

3. Compile the WASM target:
   ```bash
   emmake make -j$(nproc)
   ```

Upon completion, the output artifacts will be generated in `web/build/`:
- `mremu.html`: HTML shell page to launch the WebAssembly emulator.
- `mremu.js`: Emscripten JavaScript glue code.
- `mremu.wasm`: WebAssembly binary.

---

## Running the Web Build Locally

Browser security restrictions prevent loading `.wasm` files directly from `file://` URLs. Serve the generated build directory using a local HTTP server:

```bash
cd web/build
python3 -m http.server 8080
```

Open `http://localhost:8080/mremu.html` in your web browser.

---

## Coexistence with Native Desktop / Android Builds

- **Native Desktop Build:** Handled by `/CMakeLists.txt` and `/MREmu/CMakeLists.txt` (builds `bin/MREmu` with SFML and ImGui).
- **Android Build:** Handled by `/Android/` Gradle project and NDK CMake.
- **WebAssembly Build:** Handled independently by `/web/CMakeLists.txt`.
