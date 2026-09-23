#include <iostream>
#include <vector>
#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "MREngine/Audio.h"

static std::vector<int16_t> g_web_audio_buffer(2048 * 2, 0);

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int mremu_wasm_get_audio_sample_rate() {
        return 44100;
    }

    EMSCRIPTEN_KEEPALIVE
    int mremu_wasm_get_audio_channels() {
        return 2; // Stereo PCM
    }

    EMSCRIPTEN_KEEPALIVE
    int16_t* mremu_wasm_get_audio_buffer() {
        return g_web_audio_buffer.data();
    }

    EMSCRIPTEN_KEEPALIVE
    int mremu_wasm_get_audio_buffer_size() {
        return (int)g_web_audio_buffer.size();
    }
}
