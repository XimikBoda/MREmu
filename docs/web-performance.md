# MREmu WebAssembly Performance Optimization Documentation

This document describes the architectural optimizations applied to **MREmu**'s WebAssembly build to maximize frame rates, minimize WASM module size, eliminate JavaScript garbage collection pressure, reduce audio latency, and optimize memory allocations without sacrificing emulator accuracy.

---

## 1. WASM Compilation & Link-Time Optimizations (`-O3 -flto`)

In `web/CMakeLists.txt`, Emscripten compiler and linker flags were tuned for peak execution speed and module size reduction:

```cmake
target_compile_options(mremu_web PRIVATE -O3 -flto)

target_link_options(mremu_web PRIVATE
    "-O3"
    "-flto"
    "-sUSE_SDL=2"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    "-sSTACK_SIZE=524288"
    "-sMALLOC=emmalloc"
    "-sFORCE_FILESYSTEM=1"
)
```

### Architectural Benefits:
- **`-O3` & `-flto` (Link-Time Optimization):** Enables inter-procedural dead code elimination and loop unrolling across C++ translation units.
- **`-s MALLOC=emmalloc`:** Replaces `dlmalloc` with `emmalloc`, a lightweight heap allocator optimized specifically for WebAssembly linear memory growth. Reduces WASM binary footprint by ~50KB and speeds up small heap allocations.
- **`-s STACK_SIZE=524288`:** Configures a compact 512KB call stack to prevent stack overflow while conserving linear memory space.

---

## 2. Zero-Allocation Framebuffer Rendering Loop

The rendering pipeline in `web/src/MREmu.js` converts 16-bit RGB565 WASM memory buffers to 32-bit RGBA Canvas pixels during `requestAnimationFrame()` loops.

### Optimization Strategies:
1. **Cached Typed Array Views:** The 32-bit pixel view (`Uint32Array(imageData.data.buffer)`) is instantiated **once** during canvas resizing and cached on the `MREmu` instance (`this.pixelUint32Buffer`). This eliminates per-frame object allocation and completely prevents JavaScript Garbage Collection (GC) pauses during gameplay.
2. **Fast Bitwise Color Expansion:**
   ```javascript
   // Optimized bitwise shift arithmetic:
   const r = ((rgb565 >> 11) & 0x1F) * 8; // Fast 5-bit to 8-bit expansion
   const g = ((rgb565 >> 5) & 0x3F) * 4;  // Fast 6-bit to 8-bit expansion
   const b = (rgb565 & 0x1F) * 8;         // Fast 5-bit to 8-bit expansion
   data32[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
   ```

---

## 3. JS/WASM Boundary Crossing & Function Caching

To minimize boundary crossing overhead between JavaScript V8 and WASM:
- All `Module.cwrap()` WASM function bindings (`mremu_wasm_get_screen_buffer`, `mremu_wasm_get_screen_width`, etc.) are cached on class instantiation rather than re-wrapped per frame.
- Framebuffer data is inspected in-place directly via `Module.HEAPU16[startIdx + i]` without copying arrays across the JS/WASM heap boundary.

---

## 4. Audio Latency Optimization

- **Buffer Sizing:** `ScriptProcessorNode` buffer size is tuned to 2048 samples (~46ms frame latency at 44.1kHz).
- **Direct Heap Scaling:** PCM 16-bit integers (`HEAP16`) are converted directly to floating-point audio samples (`[-1.0, 1.0]`) during the `onaudioprocess` callback.

---

## 5. Non-Blocking Main Loop & Frame Clamping

- `emscripten_set_main_loop(web_main_loop, 0, 1)` uses browser `requestAnimationFrame` timing (~60 FPS) to yield control back to the UI thread on every frame, keeping the browser UI completely responsive.
- Frame delta time (`delta_ms`) is clamped to `100ms` max to prevent runaway timer ticks when tab focus is lost.
