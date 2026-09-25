#include <iostream>
#include <fstream>
#include <chrono>
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
static auto g_last_frame_time = std::chrono::high_resolution_clock::now();

void web_main_loop() {
    auto current_time = std::chrono::high_resolution_clock::now();
    uint32_t delta_ms = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(current_time - g_last_frame_time).count();
    g_last_frame_time = current_time;

    // Clamp delta to avoid huge time skips when tab is backgrounded
    if (delta_ms > 100) delta_ms = 100;
    if (delta_ms == 0) delta_ms = 16;

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
    int mremu_wasm_load_file(const char* filename, const uint8_t* data, size_t length) {
        spdlog::info("WASM receiving client file: {} ({} bytes)", filename, length);

        if (!filename || !data || length == 0) {
            spdlog::error("Invalid file payload passed to WASM");
            return 0;
        }

        std::string dest_path = std::string("e:/") + filename;
        fs::path target_path = path_from_emu(dest_path);

        fs::create_directories(target_path.parent_path());

        std::ofstream out(target_path, std::ios::binary);
        if (!out.is_open()) {
            spdlog::error("Failed to open WASM MEMFS path for writing: {}", target_path.string());
            return 0;
        }

        out.write(reinterpret_cast<const char*>(data), length);
        out.close();

        spdlog::info("WASM MEMFS file written to: {}", target_path.string());

        if (g_webAppManager) {
            g_webAppManager->add_app_for_launch(target_path.string(), false);
            return 1;
        }

        return 0;
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
    g_last_frame_time = std::chrono::high_resolution_clock::now();
    // emscripten_set_main_loop(func, fps, simulate_infinite_loop)
    // fps = 0 uses requestAnimationFrame for browser timing synchronization
    emscripten_set_main_loop(web_main_loop, 0, 1);
#else
    spdlog::info("Web main loop skipped outside Emscripten environment.");
#endif

    return 0;
}
