#include <iostream>
#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "Keyboard.h"
#include "Touch.h"
#include "vmio.h"

extern void add_system_event(int phandle, int message, int param);

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void mremu_wasm_send_key_event(int keycode, int event_type) {
        // event_type: 0 = KEY_DOWN, 1 = KEY_UP
        int vm_msg = (event_type == 0) ? VM_MSG_KEY_DOWN : VM_MSG_KEY_UP;

        spdlog::debug("WASM Key Event: Keycode={}, Event={}", keycode, event_type);
        add_system_event(0, vm_msg, keycode);
    }

    EMSCRIPTEN_KEEPALIVE
    void mremu_wasm_send_pen_event(int x, int y, int event_type) {
        // event_type: 1 = TAP/DOWN, 2 = MOVE, 3 = RELEASE/UP
        int vm_msg = 0;
        if (event_type == 1) vm_msg = VM_MSG_PEN_DOWN;
        else if (event_type == 2) vm_msg = VM_MSG_PEN_MOVE;
        else if (event_type == 3) vm_msg = VM_MSG_PEN_UP;

        int param = (y << 16) | (x & 0xFFFF);
        spdlog::debug("WASM Pen Event: X={}, Y={}, Event={}", x, y, event_type);
        if (vm_msg != 0) {
            add_system_event(0, vm_msg, param);
        }
    }
}
