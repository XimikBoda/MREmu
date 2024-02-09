#pragma once
#include "ItemsMng.h"
#include <memory>
#include <SFML/Audio/SoundStream.hpp>
#include <adlmidi.h>


class Midi : public sf::SoundStream
{
public:
	struct ADL_MIDIPlayer* midi_player = NULL;
	short buffer[4096];
	bool error = false;

	Midi(const char* file) {
		midi_player = adl_init(44100);
		if (!midi_player) {
			printf("Couldn't initialize ADLMIDI: %s\n", adl_errorString());
			error = true;
		}
		if (!error && adl_openFile(midi_player, file) < 0) {
			printf("Couldn't open music file: %s\n", adl_errorInfo(midi_player));
			error = true;
		}
		initialize(2, 44100);
	}

	Midi(void *buf, size_t len) {
		midi_player = adl_init(44100);
		if (!midi_player) {
			printf("Couldn't initialize ADLMIDI: %s\n", adl_errorString());
			error = true;
		}
		if (!error && adl_openData(midi_player, buf, len)) {
			printf("Couldn't open buffer: %s\n", adl_errorInfo(midi_player));
			error = true;
		}
		initialize(2, 44100);
	}

	bool onGetData(Chunk& data) {
		int samples_count = adl_play(midi_player, 4096, buffer);
		data.samples = buffer;
		data.sampleCount = samples_count;
		return samples_count > 0;
	}

	void onSeek(sf::Time timeOffset) {}

	~Midi() {
		adl_close(midi_player);
	}
};

namespace MREngine {
	class AppAudio {
	public:
		ItemsMng<std::shared_ptr<Midi>> midis;
	};
}

MREngine::AppAudio& get_current_app_audio();