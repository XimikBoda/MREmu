#include "Audio.h"
#include <vector>
#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static std::vector<int16_t> g_web_audio_buffer(2048 * 2, 0);

namespace MREngine {
    void update_web_audio_samples() {
        auto& audio = get_current_app_audio();
        for (auto& midi_pair : audio.midis.items) {
            auto& midi = midi_pair.second;
            if (midi && midi->midi_player && !midi->error && !midi->done) {
                std::lock_guard lock(midi->access_mutex);
                int samples = adl_play(midi->midi_player, (int)g_web_audio_buffer.size() / 2, g_web_audio_buffer.data());
                if (samples > 0) return;
            }
        }
    }
}

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
        MREngine::update_web_audio_samples();
        return g_web_audio_buffer.data();
    }

    EMSCRIPTEN_KEEPALIVE
    int mremu_wasm_get_audio_buffer_size() {
        return (int)g_web_audio_buffer.size();
    }
}
