#include "Audio.h"
#include "Resources.h"
#include "../Memory.h"
#include "../Log.h"
#include <SFML/Audio.hpp>
#include <vmmm.h>
#include <vm4res.h>

Midi::Midi(const char* file) {
	std::lock_guard lock(access_mutex);
	midi_player = adl_init(44100);
	if (!midi_player) {
		spdlog::error("Couldn't initialize ADLMIDI: {}", adl_errorString());
		error = true;
	}
	if (!error && adl_openFile(midi_player, file) < 0) {
		spdlog::error("Couldn't open music file: {}", adl_errorInfo(midi_player));
		error = true;
	}
	initialize(2, 44100);
}

Midi::Midi(void* buf, size_t len) {
	std::lock_guard lock(access_mutex);
	midi_player = adl_init(44100);
	if (!midi_player) {
		spdlog::error("Couldn't initialize ADLMIDI: {}", adl_errorString());
		error = true;
	}
	if (!error && adl_openData(midi_player, buf, len)) {
		spdlog::error("Couldn't open buffer: {}", adl_errorInfo(midi_player));
		error = true;
	}
	initialize(2, 44100);
}

bool Midi::onGetData(Chunk& data) {
	std::lock_guard lock(access_mutex);
	int samples_count = adl_play(midi_player, play_buf, buffer);
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

int* MREngine::AppAudio::tmp_int_p = 0;

void MREngine::AppAudio::init() {
	tmp_int_p = (int*)Memory::shared_malloc(sizeof(int));
}

MREngine::AppAudio::~AppAudio() {
	for (int i = 0; i < midis.size(); ++i)
		if (midis.is_active(i)) {
			midis[i]->stop();
			midis[i].reset();
			midis.remove(i);
		}

	for (int i = 0; i < bitstreams.size(); ++i)
		if (bitstreams.is_active(i)) {
			bitstreams[i]->stop();
			bitstreams[i].reset();
			bitstreams.remove(i);
		}
}

VMINT vm_audio_play_bytes(void* audio_data, VMUINT len, VMUINT8 format, VMUINT start_time, VMUINT path, void (*f)(VMINT result)) {
	auto& audio = get_current_app_audio();

	auto& music = audio.music;

	if (!music.openFromMemory(audio_data, len))
		return VM_AUDIO_FAILED;

	music.setPlayingOffset(sf::milliseconds(start_time));
	
	music.play();

	return VM_AUDIO_SUCCEED;
}

VMINT vm_audio_pause(void (*f)(VMINT result)) {
	auto& audio = get_current_app_audio();

	auto& music = audio.music;

	music.pause();

	return VM_AUDIO_SUCCEED;
}

VMINT vm_audio_resume(void (*f)(VMINT result)) {
	auto& audio = get_current_app_audio();

	auto& music = audio.music;

	music.play();

	return VM_AUDIO_SUCCEED;
}

VMINT vm_audio_stop(void (*f)(VMINT result)) {
	auto& audio = get_current_app_audio();

	auto& music = audio.music;

	music.stop();

	return VM_AUDIO_SUCCEED;
}

void vm_set_volume(VMINT volume) {
	if (volume < 0)
		volume = 0;
	if (volume > 6)
		volume = 6;
		
	// Force SFML AudioDevice initialization by creating a dummy object.
	// This prevents AL_INVALID_OPERATION if this is the first audio call on this thread.
	static sf::SoundBuffer dummy_buffer; 
	
	sf::Listener::setGlobalVolume(volume * 100 / 6);
}

VMINT vm_get_volume(void) {
	static sf::SoundBuffer dummy_buffer;

	return sf::Listener::getGlobalVolume() * 6 / 100;
}

VMINT vm_midi_play(VMINT resid, VMINT repeat, void (*f)(VMINT handle, VMINT event)) {
	return vm_midi_play_ex(resid, 0, repeat, 0, f);
}

VMINT vm_midi_play_ex(VMINT resid, VMUINT start_time, VMINT repeat, VMUINT path, void (*f)(VMINT handle, VMINT event)) {
	MREngine::Resources& resources = get_current_app_resources();
	auto& audio = get_current_app_audio();

	VMUINT8* buf = resources.call_res_provider(resid, audio.tmp_int_p);
	int size = *audio.tmp_int_p;

	if (!buf && !size)
		return VM_MIDI_FAILED;

	return vm_midi_play_by_bytes_ex(buf, size, start_time, repeat, path, f);
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


void vm_audio_resume_bg_play(void) {} //TODO

void vm_audio_suspend_bg_play(void) {} //TODO