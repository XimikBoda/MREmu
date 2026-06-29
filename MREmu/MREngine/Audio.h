#pragma once
#include "ItemsMng.h"
#include <memory>
#include "../mutex_wrapper.h"
#include <SFML/Audio/SoundStream.hpp>
#include <SFML/Audio.hpp>
#include <adlmidi.h>
#include <vmbitstream.h>
#include <condition_variable>

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
	std::condition_variable cv;
	mutex_wrapper data_mutex;

	bool stereo = false;
	int sample_rate = 44100;
	bool data_finished = false;

	vm_bitstream_audio_result_callback callback = 0;

	Bitstream(bool stereo, int sample_rate, vm_bitstream_audio_result_callback callback);

	bool onGetData(Chunk& data);
	void onSeek(sf::Time timeOffset);
	
	void putData(void* buf, uint32_t size, uint32_t &writen);
	void dataFinished();

	~Bitstream();
};

namespace MREngine {
	class AppAudio {
	public:
		ItemsMng<std::shared_ptr<Midi>> midis;

		ItemsMng<std::shared_ptr<Bitstream>> bitstreams;

		
		~AppAudio();
	};
}

typedef enum {
	MEDIA_FORMAT_PCM_8K = 7,
	MEDIA_FORMAT_PCM_16K = 8,
} Media_Format;

typedef enum {
	MEDIA_NONE,
	MEDIA_DATA_REQUEST,
	MEDIA_DATA_NOTIFICATION,
	MEDIA_END,
	MEDIA_ERROR,
} Media_Event;

typedef enum {
	MEDIA_SUCCESS = 200,
	MEDIA_FAIL
} Media_Status;

typedef void (*mremu_media_handler)(Media_Event event);

class BitstreamRecord : public sf::SoundRecorder {
	int16_t* buf = 0;
	size_t buf_len = 0;
	size_t size = 0;
	mutex_wrapper data_mutex;

	bool onStart() override;
	bool onProcessSamples(const std::int16_t* samples, std::size_t sampleCount) override;
	void onStop() override;
public:
	void setBuffer(int16_t* buf, uint32_t buf_len);
	void getReadBuffer(int16_t** buf, uint32_t* buf_len);
	void readDataDone(uint32_t len);

	~BitstreamRecord();
};

void mremu_media_setbufer(VMUINT16* buffer, VMUINT32 buf_len);
void mremu_media_getreadbuffer(VMUINT16** buffer, VMUINT32* buf_len);
Media_Status mremu_media_record(Media_Format format, mremu_media_handler handler, void* param);
void mremu_media_readdatadone(VMUINT32 len);
void mremu_media_stop();

MREngine::AppAudio& get_current_app_audio();
bool cur_app_is_native();