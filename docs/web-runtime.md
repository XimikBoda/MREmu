# MREmu Browser Runtime Execution, Threading & Timing Architecture

This document describes the browser runtime execution model for **MREmu**, analyzing native multi-threading mechanisms, single-threaded browser main loop execution, `requestAnimationFrame` synchronization, timer scheduling, and WASM pthread configurations.

---

## 1. Native vs. Browser Threading & Execution Analysis

In the native desktop build (`MREmu.cpp`), execution is split across two primary OS threads:
1. **Main UI Thread:** Polls window events (`sf::Event`), renders SFML graphics, and updates ImGui windows in a synchronous `while(win_device.isOpen())` loop.
2. **`mre_main` Execution Thread:** Spawned via `std::thread second_thread(mre_main, &appManager)` running a synchronous `while(work)` loop with `sf::sleep(sf::milliseconds(1000 / 120))`.

### Browser Environment Constraints
Browsers execute JavaScript and WebAssembly within a single-threaded event loop. Blocking synchronous loops (`while(true)` or `sf::sleep()`) on the main thread will immediately freeze the browser tab, trigger "Page Unresponsive" browser warnings, and halt UI rendering and user event dispatch.

---

## 2. Non-Blocking Browser Main Loop Integration

In `web/src/main_wasm.cpp`, the dual-threaded desktop execution model is refactored into an asynchronous callback loop registered via Emscripten's `emscripten_set_main_loop()` API:

```cpp
void web_main_loop() {
    auto current_time = std::chrono::high_resolution_clock::now();
    uint32_t delta_ms = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(current_time - g_last_frame_time).count();
    g_last_frame_time = current_time;

    // Clamp delta to avoid large jump when browser tab is hidden/backgrounded
    if (delta_ms > 100) delta_ms = 100;
    if (delta_ms == 0) delta_ms = 16;

    if (g_webAppManager) {
        g_webAppManager->update(delta_ms);
    }
}

int main(int argc, char** argv) {
    // ... Subsystem Initialization ...
    emscripten_set_main_loop(web_main_loop, 0, 1);
    return 0;
}
```

### Key Execution Properties:
- **`fps = 0`:** Passing `0` to `emscripten_set_main_loop()` instructs Emscripten to synchronize ticks directly with the browser's native `requestAnimationFrame()` display refresh cycle (~60 Hz / 120 Hz).
- **Non-Blocking Time-Slicing:** Each iteration executes one update slice (`g_webAppManager->update(delta_ms)`) and yields control back to the browser event loop immediately.
- **Background Tab Handling:** `delta_ms` is clamped to `100ms` max to prevent runaway timer ticks when the user switches browser tabs.

---

## 3. Emulator Timer & Synchronization Accuracy

MRE SDK timers (`vmtimer.h`) are driven by `AppManager::update(delta_ms)` inside `MREngine/Timer.cpp`:
- Active timers evaluate elapsed millisecond intervals and fire corresponding MRE callback functions (`vm_create_timer`).
- Time-slice updates match native clock accuracy while avoiding busy loops or thread blocking.

---

## 4. Web Worker & Pthread Support Requirements (`SharedArrayBuffer`)

For advanced multi-threaded WASM builds utilizing Emscripten Pthreads (`-s USE_PTHREADS=1`):
1. **`SharedArrayBuffer` Requirement:** Web Workers share memory with the main WASM instance using `SharedArrayBuffer`.
2. **Cross-Origin Isolation HTTP Headers:** Web browsers require cross-origin isolation headers on the web server to enable `SharedArrayBuffer`:
   ```http
   Cross-Origin-Opener-Policy: same-origin
   Cross-Origin-Embedder-Policy: require-corp
   ```

---

## 5. Preservation of Native Code

The native desktop multi-threading model (`std::thread second_thread` in `MREmu.cpp`) remains 100% untouched.
