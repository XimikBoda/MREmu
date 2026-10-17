# MREmu WebAssembly Input Subsystem & Controls Documentation

This document describes the browser input architecture for **MREmu**, detailing how keyboard events, virtual keypad controls, touch/pen interactions, and the browser Gamepad API are mapped into the emulator's input subsystem.

---

## 1. Input Architecture Overview

Browser input events are captured in JavaScript and dispatched to C-linkage export functions defined in `web/src/input/input_wasm.cpp`:

- `mremu_wasm_send_key_event(int keycode, int event_type)`: Dispatches keypress messages (`VM_MSG_KEY_DOWN` / `VM_MSG_KEY_UP`) to the active MRE app.
- `mremu_wasm_send_pen_event(int x, int y, int event_type)`: Dispatches touch screen pen events (`VM_MSG_PEN_DOWN`, `VM_MSG_PEN_MOVE`, `VM_MSG_PEN_UP`) with packed `(y << 16) | x` coordinates.

---

## 2. Default Keyboard Mappings

| PC Keyboard Key | MRE Control Code | MRE Action |
| :--- | :--- | :--- |
| **Arrow Up** | `1` | D-Pad Up (`VM_KEY_UP`) |
| **Arrow Down** | `2` | D-Pad Down (`VM_KEY_DOWN`) |
| **Arrow Left** | `3` | D-Pad Left (`VM_KEY_LEFT`) |
| **Arrow Right** | `4` | D-Pad Right (`VM_KEY_RIGHT`) |
| **Enter / Space** | `5` | Select / OK (`VM_KEY_OK`) |
| **Q Key** | `1001` | Left Softkey |
| **E Key** | `1002` | Right Softkey |
| **0 – 9 Keys** | `48 – 57` | Phone Numpad `0` – `9` |

---

## 3. Gamepad API Support

The browser `Gamepad API` (`navigator.getGamepads()`) maps connected controller buttons directly to emulator controls:
- **D-Pad / Left Stick:** Mapped to Arrow key directions.
- **A / Cross Button:** Mapped to `OK` (`VM_KEY_OK`).
- **L1 / R1 Shoulder Buttons:** Mapped to Left and Right Softkeys.
- **Connection Events:** Listened via `gamepadconnected` and `gamepaddisconnected` events.

---

## 4. Preventing Shortcut Interference

To prevent default browser navigation and scrolling behaviors (such as Space or Arrow keys scrolling the web page), event handlers call `e.preventDefault()` on mapped input events:

```javascript
window.addEventListener('keydown', function(e) {
  if (keyMap[e.code] !== undefined) {
    e.preventDefault(); // Suppress default browser scroll / shortcut
    sendKey(keyMap[e.code], 0); // 0 = Key Down
  }
});
```

---

## 5. Native Code Preservation

Native input implementations (`Keyboard.cpp`, `Touch.cpp`) and desktop keymaps remain completely untouched.
