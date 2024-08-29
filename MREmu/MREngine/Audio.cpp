#include "Audio.h"
#include <SFML/Audio.hpp>
#include <vmmm.h>

Midi::Midi(const char* file) {
	std::lock_guard lock(access_mutex);
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

Midi::Midi(void* buf, size_t len) {
	std::lock_guard lock(access_mutex);
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

bool Midi::onGetData(Chunk& data) {
	std::lock_guard lock(access_mutex);
	int samples_count = adl_play(midi_player, 4410, buffer);
	data.samples = buffer;
	data.sampleCount = samples_count;
	if (samples_count == 0) {
		if (repeat) {
			repeat--;
			setLoop(true);
		}
		else {
			setLoop(false);
			done = true;
		}
		return false;
	}
	else
		return true;
}

void Midi::onSeek(sf::Time timeOffset) {
	std::lock_guard lock(access_mutex);
	adl_positionSeek(midi_player, timeOffset.asSeconds());
}

Midi::~Midi() {
	std::lock_guard lock(access_mutex);
	adl_close(midi_player);
}

MREngine::AppAudio::~AppAudio() {
	for (int i = 0; i < midis.size(); ++i)
		if (midis.is_active(i)) {
			midis[i]->stop();
			midis[i].reset();
			midis.remove(i);
		}
}

void vm_set_volume(VMINT volume) {
	if (volume < 0)
		volume = 0;
	if (volume > 6)
		volume = 6;
	sf::Listener::setGlobalVolume(volume * 100 / 6);
}

VMINT vm_midi_play_by_bytes(VMUINT8* midibuf, VMINT len, VMINT repeat, void (*f)(VMINT handle, VMINT event)) {
	return vm_midi_play_by_bytes_ex(midibuf, len, 0, repeat, 0, f);
}

VMINT vm_midi_play_by_bytes_ex(VMUINT8* midibuf, VMINT len, VMUINT start_time,
	VMINT repeat, VMUINT path, void (*f)(VMINT handle, VMINT event))
{
	auto& audio = get_current_app_audio();

	std::shared_ptr<Midi> midi = std::make_shared<Midi>(midibuf, len);
	if (midi->error)
		return -1;

	for (int i = 0; i < audio.midis.size(); ++i)
		if (audio.midis.is_active(i))
			audio.midis[i]->pause();

	if (repeat == 0)
		repeat = 999; //TODO

	midi->source = midibuf;
	midi->repeat = repeat - 1;
	midi->setPlayingOffset(sf::milliseconds(start_time));
	midi->play();

	for (int i = 0; i < audio.midis.size(); ++i)
		if (audio.midis.is_active(i)
			&& audio.midis[i]->source == midibuf)
		{
			audio.midis[i]->stop();
			audio.midis[i].swap(midi);
			return i;
		}


	return audio.midis.push(midi);
}

VMINT vm_midi_pause(VMINT handle) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.midis.is_active(handle)) {
		audio.midis[handle]->pause();
		return 0;
	}
	return -1;
}

VMINT vm_midi_get_time(VMINT handle, VMUINT* current_time) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.midis.is_active(handle) && !audio.midis[handle]->done) {
		*current_time = audio.midis[handle]->getPlayingOffset().
			asMilliseconds();

		if (!*current_time) // some games don`t like when it is rezo
			*current_time += 1;

		return 0;
	}
	return -1;
}

void vm_midi_stop(VMINT handle) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.midis.is_active(handle)) {
		audio.midis[handle]->stop();
		audio.midis[handle].reset();
		audio.midis.remove(handle);
	}
}
void vm_midi_stop_all(void) {
	MREngine::AppAudio& audio = get_current_app_audio();

	for (int i = 0; i < audio.midis.size(); ++i)
		if (audio.midis.is_active(i)) {
			audio.midis[i]->stop();
			audio.midis[i].reset();
			audio.midis.remove(i);
		}
}
