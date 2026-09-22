#include <iostream>
#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "Memory.h"
#include "Cpu.h"
#include "Bridge.h"
#include "AppManager.h"
#include "MREngine/SIM.h"
#include "MREngine/System.h"
#include "MREngine/CharSet.h"
#include "MREngine/Audio.h"
#include "MREngine/Graphic.h"
#include "MREngine/IO.h"

extern MREngine::Graphic* graphic;

AppManager* g_webAppManager = nullptr;

void web_main_loop() {
    static uint32_t last_tick = 0;
    uint32_t delta_ms = 16; // ~60 FPS frame tick slice

    if (g_webAppManager) {
        g_webAppManager->update(delta_ms);
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    uint16_t* mremu_wasm_get_screen_buffer() {
        if (graphic && !graphic->screen.empty()) {
            return graphic->screen.data();
        }
        return nullptr;
    }

    EMSCRIPTEN_KEEPALIVE
    int mremu_wasm_get_screen_width() {
        if (graphic) {
            return graphic->width;
        }
        return 240;
    }

    EMSCRIPTEN_KEEPALIVE
    int mremu_wasm_get_screen_height() {
        if (graphic) {
            return graphic->height;
        }
        return 320;
    }

    EMSCRIPTEN_KEEPALIVE
    int load_vxp_from_memory(const char* file_path) {
        spdlog::info("Loading VXP application in WASM: {}", file_path);
        if (g_webAppManager) {
            g_webAppManager->add_app_for_launch(file_path, false);
            return 1;
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Initializing MREmu WebAssembly Core Subsystems...");

    static AppManager appManager;
    g_webAppManager = &appManager;

    Memory::init(32 * 1024 * 1024);
    Cpu::init();
    Bridge::init();

    MREngine::SIM::init();
    MREngine::System::init();
    MREngine::CharSet::init();
    MREngine::AppAudio::init();
    MREngine::IO::init();

    static MREngine::Graphic web_graphic;
    web_graphic.activate();

    spdlog::info("MREmu Core & Graphics initialized successfully for WebAssembly.");

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(web_main_loop, 0, 1);
#else
    spdlog::info("Web main loop skipped outside Emscripten environment.");
#endif

    return 0;
}
