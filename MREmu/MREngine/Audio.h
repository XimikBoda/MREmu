#pragma once
#include "ItemsMng.h"
#include <memory>
#include "../mutex_wrapper.h"
#include <SFML/Audio/SoundStream.hpp>
#include <SFML/Audio.hpp>
#include <adlmidi.h>


class Midi : public sf::SoundStream
{
public:
	struct ADL_MIDIPlayer* midi_player = NULL;
	short buffer[4410];
	bool error = false;
	mutex_wrapper access_mutex;
	int repeat = 0;
	void* source = 0;
	bool done = false;

	Midi(const char* file);
	Midi(void* buf, size_t len);

	bool onGetData(Chunk& data);

	void onSeek(sf::Time timeOffset);

	~Midi();
};

namespace MREngine {
	class AppAudio {
	public:
		ItemsMng<std::shared_ptr<Midi>> midis;

		~AppAudio();
	};
}

MREngine::AppAudio& get_current_app_audio();