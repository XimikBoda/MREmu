# MREmu WebAssembly Rendering Pipeline Architecture

This document details the browser graphics architecture for **MREmu**, explaining how the 16-bit RGB565 emulator framebuffer is rendered onto an HTML Canvas 2D display.

---

## 1. Framebuffer Memory Architecture

The native MREmu graphics engine maintains the emulated screen buffer in RGB565 pixel format (`uint16_t` array) inside `MREngine::Graphic`:

```cpp
// Resolution: 240 x 320 pixels (configurable)
std::vector<uint16_t> screen;
```

For the WebAssembly build, C-linkage export functions in `web/src/main_wasm.cpp` expose the framebuffer pointer directly to JavaScript without modifying native source code:

- `mremu_wasm_get_screen_buffer()`: Returns raw WASM memory address (`uint16_t*`) pointing to `graphic->screen.data()`.
- `mremu_wasm_get_screen_width()`: Returns active screen width (e.g. 240).
- `mremu_wasm_get_screen_height()`: Returns active screen height (e.g. 320).

---

## 2. Zero-Copy WASM Heap Access & Color Format Conversion

JavaScript accesses the WASM heap directly through Emscripten's linear memory buffer (`Module.HEAPU16`).

### Pixel Conversion (RGB565 to RGBA32)
Each 16-bit pixel stored in MREmu's framebuffer consists of:
- **Red:** 5 bits (`(rgb565 >> 11) & 0x1F`)
- **Green:** 6 bits (`(rgb565 >> 5) & 0x3F`)
- **Blue:** 5 bits (`rgb565 & 0x1F`)

In `web/shell_minimal.html`, JavaScript converts RGB565 pixels into 32-bit RGBA values and populates an HTML Canvas `ImageData` object:

```javascript
var heap16 = Module.HEAPU16;
var startIdx = bufPtr >> 1; // Convert byte pointer to 16-bit array index
var data32 = new Uint32Array(imageData.data.buffer);

for (var i = 0; i < numPixels; i++) {
  var rgb565 = heap16[startIdx + i];

  var r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
  var g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
  var b = (rgb565 & 0x1F) * 255 / 31;

  // Uint32Array Little-Endian ABGR format
  data32[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
}
```

---

## 3. Render Loop & Canvas Blitting

1. **Synchronization:** The render loop is driven asynchronously by browser `requestAnimationFrame()`.
2. **Blitting:** Transformed pixel data is painted to the canvas using `ctx.putImageData(imageData, 0, 0)`.
3. **Image Sharpness:** Crisp pixel-art scaling is maintained on high-DPI displays using CSS pixelated rules (`image-rendering: pixelated`).

---

## 4. Preservation of Native Desktop Architecture

The native desktop and mobile rendering pipelines remain untouched:
- **Desktop Build:** Uses SFML `sf::Texture` and `sf::RenderWindow` inside `MREmu.cpp`.
- **Android Build:** Uses SFML Android OpenGLES texture surface.
- **Web Build:** Uses WASM linear memory inspection and HTML Canvas 2D blitting via `web/shell_minimal.html`.
