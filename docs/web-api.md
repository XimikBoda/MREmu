# MREmu JavaScript API Reference Documentation

This document describes the high-level object-oriented JavaScript API for **MREmu** provided by `web/src/MREmu.js`.

The `MREmu` class encapsulates low-level WebAssembly memory operations, C-function wrappers, canvas rendering, Web Audio Context management, and client-side file loading into an elegant, promise-based API.

---

## 1. Quick Start Example

```javascript
import MREmu from './src/MREmu.js';

// Instantiate emulator with target canvas element
const emulator = new MREmu({
  canvas: document.getElementById('canvas'),
  wasmScriptUrl: 'mremu.js'
});

// Initialize WASM runtime
await emulator.init();

// Load application binary (.vxp / .vre)
const fileInput = document.getElementById('fileInput');
await emulator.load(fileInput.files[0]);

// Start execution and rendering
emulator.start();

// Enable audio on user gesture
document.getElementById('audioBtn').onclick = () => {
  emulator.enableAudio();
};
```

---

## 2. API Method Reference

### Constructor
```javascript
const emulator = new MREmu(options);
```
- **`options.canvas`** (`HTMLCanvasElement`, optional): Target canvas element for rendering.
- **`options.wasmScriptUrl`** (`string`, optional, default: `'mremu.js'`): Path to Emscripten JS loader script.

---

### `await emulator.init()`
Asynchronously initializes the WebAssembly module, attaches exported C functions, and prepares the execution state.
- **Returns:** `Promise<void>`

---

### `await emulator.load(fileOrBuffer, [filename])`
Asynchronously loads a `.vxp`, `.vre`, or `.mre` file buffer into the emulator's virtual filesystem (`e:/`).
- **`fileOrBuffer`** (`File | ArrayBuffer | Uint8Array`): Binary payload or HTML5 File object.
- **`filename`** (`string`, optional): Destination filename.
- **Returns:** `Promise<boolean>` - `true` on successful loading.
- **Throws:** `Error` if file format is invalid or file buffer is corrupt.

---

### `emulator.start()`
Starts or resumes application execution and canvas rendering loop. Sets status to `'running'`.

---

### `emulator.pause()`
Pauses application execution and cancels animation frame rendering. Sets status to `'paused'`.

---

### `emulator.reset()`
Resets the emulator state, clears canvas buffers, and resets status to `'ready'`.

---

### `emulator.setKey(keycode, eventType)`
Dispatches keypress events to the emulated application.
- **`keycode`** (`number`): MRE key code (e.g. `1` = Up, `2` = Down, `5` = OK, `1001` = Left Softkey).
- **`eventType`** (`number`, optional, default: `0`): `0` for Key Down, `1` for Key Up.

---

### `emulator.setPen(x, y, eventType)`
Dispatches touch screen events to the emulated application.
- **`x`**, **`y`** (`number`): Display coordinates.
- **`eventType`** (`number`): `1` = Touch Tap/Down, `2` = Touch Move, `3` = Touch Release/Up.

---

### `emulator.setVolume(volume)`
Adjusts output audio volume.
- **`volume`** (`number`): Volume scale factor between `0.0` (mute) and `1.0` (full volume).

---

### `emulator.enableAudio()`
Instantiates the Web Audio Context (`AudioContext`) and connects the OPL3 PCM streaming node. Should be called within a user click or keypress event handler to comply with browser autoplay policies.

---

### `emulator.getStatus()`
Returns the current execution state of the emulator instance.
- **Returns:** `string` - One of `'uninitialized'`, `'loading'`, `'ready'`, `'running'`, `'paused'`, or `'error'`.
