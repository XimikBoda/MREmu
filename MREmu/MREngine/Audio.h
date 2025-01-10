#pragma once
#include "ItemsMng.h"
#include <memory>
#include "../mutex_wrapper.h"
#include <SFML/Audio/SoundStream.hpp>
#include <SFML/Audio.hpp>
#include <adlmidi.h>
#include <vmbitstream.h>

const int bitstream_buf_size = 10 * 1024;
const int play_buf = 4410;

class Midi : public sf::SoundStream
{
public:
	struct ADL_MIDIPlayer* midi_player = NULL;
	short buffer[play_buf];
	bool error = false;
	mutex_wrapper access_mutex;
	int repeat = 0;
	void* source = 0;
	bool done = true;

	Midi(const char* file);
	Midi(void* buf, size_t len);

	bool onGetData(Chunk& data);
	void onSeek(sf::Time timeOffset);

	~Midi();
};

class Bitstream : public sf::SoundStream
{
public:
	short buffer[bitstream_buf_size];
	bool error = false;
	mutex_wrapper access_mutex;
	bool done = false;

	uint32_t play_pos = 0; // in samples (16 bits)
	uint32_t data_size = 0; // in samples (16 bits)

	bool stereo = false;
	int sample_rate = 44100;
	bool data_finished = false;

	vm_bitstream_audio_result_callback callback = 0;

	Bitstream(bool stereo, int sample_rate, vm_bitstream_audio_result_callback callback);

	bool onGetData(Chunk& data);
	void onSeek(sf::Time timeOffset);

	void putData(void* buf, uint32_t size, uint32_t &writen);
};

namespace MREngine {
	class AppAudio {
	public:
		ItemsMng<std::shared_ptr<Midi>> midis;

		ItemsMng<std::shared_ptr<Bitstream>> bitstreams;

		
		~AppAudio();
	};
}

MREngine::AppAudio& get_current_app_audio();