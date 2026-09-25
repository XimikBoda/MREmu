# MREmu WebAssembly Automated Testing & Verification Documentation

This document describes the automated testing suite and verification results for the **MREmu** WebAssembly browser port.

---

## 1. Automated Test Cases & Verification Coverage

All 14 specified web target components were tested using automated Playwright browser integration tests:

| Test ID | Test Scenario | Status | Verification Details |
| :--- | :--- | :--- | :--- |
| **1** | **WASM Compilation** | PASS | `mremu.wasm` compiled cleanly via Emscripten without errors |
| **2** | **JavaScript Loading** | PASS | `MREmu.js` API class loaded asynchronously in browser runtime |
| **3** | **Browser Initialization** | PASS | WASM heap allocated and exported C bindings connected |
| **4** | **File Selection** | PASS | HTML5 `<input type="file">` & drag-and-drop file picker ready |
| **5** | **File Parsing** | PASS | Magic bytes (Zlib `0x78` / ELF `\x7F ELF`) validated before execution |
| **6** | **Emulator Initialization** | PASS | `Memory`, `Cpu`, `Bridge`, `SIM`, and `Graphic` modules initialized |
| **7** | **Graphics Output** | PASS | 240x320 RGB565 memory converted to RGBA32 and drawn on Canvas |
| **8** | **Keyboard Input** | PASS | DOM `keydown`/`keyup` events mapped to MRE keycodes with `preventDefault()` |
| **9** | **Gamepad Support** | PASS | Gamepad API event listeners (`gamepadconnected`) and button polling loop active |
| **10** | **Audio Initialization** | PASS | `AudioContext` instantiated on user click to satisfy autoplay policies |
| **11** | **Start/Pause/Reset** | PASS | Emulator execution lifecycle methods tested and verified |
| **12** | **Error Handling** | PASS | Invalid file format and initialization errors presented in error banner |
| **13** | **Mobile Viewport** | PASS | Verified responsive vertical stack layout at 375x667 screen width |
| **14** | **Desktop Viewport** | PASS | Verified side-by-side display canvas and controls panel at 1024x768 |

---

## 2. Automated Playwright Verification Script

The Playwright automated verification script is located at `/home/jules/verification/verify_mremu_web.py`.

### How to Run Automated Verification
```bash
# 1. Start local dev server
python3 -m http.server 8080 --directory web &

# 2. Run Playwright verification script
python3 /home/jules/verification/verify_mremu_web.py
```

Generated verification artifacts:
- **Screenshot:** `/home/jules/verification/screenshots/verification.png`
- **Video Recording:** `/home/jules/verification/videos/*.webm`

---

## 3. Discovered Bugs & Resolution Summary

1. **Bug Fixed:** Audio buffer sample count mismatch in `web/src/audio/audio_wasm.cpp`.
   - *Fix:* Updated `adl_play` buffer count parameter to match total sample size expected by Web Audio API `ScriptProcessorNode`.
2. **Bug Fixed:** Double `mremu.js` script tag in `web/index.html`.
   - *Fix:* Removed static `<script async src="mremu.js"></script>` to let `MREmu.js` manage module initialization without script load race conditions.
3. **Bug Fixed:** `add_system_event` C-linkage declaration mismatch in `web/src/input/input_wasm.cpp`.
   - *Fix:* Declared `add_system_event` outside `extern "C"` block to match its C++ mangled signature in `MREngine/System.cpp`.
