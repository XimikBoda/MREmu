#include "Audio.h"
#include "Audio.h"
#include <SFML/Audio.hpp>
#include <vmmm.h>
#include <vmbitstream.h>

static const int sample_rate_enum_to_int[VM_BITSTREAM_SAMPLE_FREQ_TOTAL] =
{
	8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000
};

class vm_bitstream_pcm_audio_cfg_struct_Remaper { // Must be beter solution
	vm_bitstream_pcm_audio_cfg_struct* p = 0;
	bool is_native = true;
public:
	vm_bitstream_pcm_audio_cfg_struct_Remaper(vm_bitstream_pcm_audio_cfg_struct* p) {
		this->p = p;
		is_native = cur_app_is_native();
	}

	vm_bitstream_codec_type_enum get_vm_codec_type() {
		if (is_native) return p->vm_codec_type;
		else return (vm_bitstream_codec_type_enum)(uint8_t)(*((uint8_t*)p + 0));
	}
	VMBOOL get_isStereo() {
		if (is_native) return p->isStereo;
		else return (VMBOOL)(*((uint8_t*)p + 4));
	}
	VMUINT8 get_bitPerSample() {
		if (is_native) return p->bitPerSample;
		else return (VMUINT8)(*((uint8_t*)p + 8));
	}
	vm_bitstream_sample_freq_enum get_sampleFreq() {
		if (is_native) return p->sampleFreq;
		else return (vm_bitstream_sample_freq_enum)(uint8_t)(*((uint8_t*)p + 9));
	}
};

Bitstream::Bitstream(bool stereo, int sample_rate, vm_bitstream_audio_result_callback callback) {
	std::lock_guard lock(access_mutex);
	this->stereo = stereo;
	this->sample_rate = sample_rate;
	this->callback = callback;

	initialize(stereo ? 2 : 1, sample_rate);
}

bool Bitstream::onGetData(Chunk& data) {
	std::lock_guard lock(access_mutex);
	data.samples = buffer + play_pos;

	uint32_t available_size_16 = std::min(data_size, bitstream_buf_size - play_pos);
	data.sampleCount = std::min<uint32_t>(available_size_16, play_buf);

	play_pos += data.sampleCount;
	if (play_pos == bitstream_buf_size)
		play_pos = 0;

	data_size -= data.sampleCount;

	return data.sampleCount != 0;
}

void Bitstream::onSeek(sf::Time timeOffset) {
	std::lock_guard lock(access_mutex);
	//TODO
}

void Bitstream::putData(void* buf, uint32_t size, uint32_t& writen)
{
	std::lock_guard lock(access_mutex);
	uint32_t size16 = size / 2;

	writen = 0;

	if (size16 && play_pos + data_size < bitstream_buf_size) {
		uint32_t can_write_16 = bitstream_buf_size - data_size - play_pos;
		uint32_t size_to_write = std::min(size16, can_write_16);

		memcpy(buffer + play_pos + data_size, buf, size_to_write * 2);
		writen += size_to_write * 2;
		size16 -= size_to_write;
		data_size += size_to_write;
	}

	if (size16 && play_pos + data_size >= bitstream_buf_size && data_size < bitstream_buf_size)
	{
		uint32_t can_write_16 = bitstream_buf_size - data_size;
		uint32_t size_to_write = std::min(size16, can_write_16);

		memcpy(buffer + play_pos + data_size - bitstream_buf_size, buf, size_to_write * 2);
		writen += size_to_write * 2;
		size16 -= size_to_write;
		data_size += size_to_write;
	}
}

VMINT vm_bitstream_audio_open(
	VMINT* handle,
	vm_bitstream_audio_cfg_struct* audio_type,
	vm_bitstream_audio_result_callback callback)
{
	vm_bitstream_pcm_audio_cfg_struct audio_type_pcm = {};
	audio_type_pcm.vm_codec_type = audio_type->vm_codec_type;
	return vm_bitstream_audio_open_pcm(handle, &audio_type_pcm, callback);
}


VMINT vm_bitstream_audio_open_pcm(
	VMINT* handle,
	vm_bitstream_pcm_audio_cfg_struct* audio_type,
	vm_bitstream_audio_result_callback callback)
{
	auto& audio = get_current_app_audio();

	vm_bitstream_pcm_audio_cfg_struct_Remaper audio_type_Remap(audio_type);

	if (audio_type_Remap.get_vm_codec_type() != VM_BITSTREAM_CODEC_TYPE_PCM ||
		audio_type_Remap.get_bitPerSample() != 16)
		return VM_BITSTREAM_ERR_UNSUPPORTED_FORMAT;

	if (((uint32_t)audio_type_Remap.get_sampleFreq()) >= VM_BITSTREAM_SAMPLE_FREQ_TOTAL)
		return VM_BITSTREAM_ERR_FAILED;

	std::shared_ptr<Bitstream> bitstream = std::make_shared<Bitstream>(
		audio_type_Remap.get_isStereo(),
		sample_rate_enum_to_int[(uint32_t)audio_type_Remap.get_sampleFreq()],
		callback
	);

	*handle = audio.bitstreams.push(bitstream);

	return VM_BITSTREAM_SUCCEED;
}


VMINT vm_bitstream_audio_finished(VMINT handle) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		audio.bitstreams[handle]->data_finished = true;
		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}


VMINT vm_bitstream_audio_close(VMINT handle) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		audio.bitstreams[handle]->stop();
		audio.bitstreams[handle].reset();
		audio.bitstreams.remove(handle);
		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}


VMINT vm_bitstream_audio_get_buffer_status(
	VMINT handle,
	vm_bitstream_audio_buffer_status* status)
{
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		status->total_buf_size = bitstream_buf_size;
		status->free_buf_size = status->total_buf_size - audio.bitstreams[handle]->data_size;
		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}


VMINT vm_bitstream_audio_put_data(
	VMINT 	handle,
	VMUINT8* buffer,
	VMUINT 	buffer_size,
	VMUINT* written)
{
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		auto& bs = *audio.bitstreams[handle];
		bs.putData(buffer, buffer_size, *written);
		if (bs.getStatus() != sf::SoundSource::Status::Playing && !bs.data_finished)
			bs.play();

		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}


VMINT vm_bitstream_audio_start(
	VMINT handle,
	vm_bitstream_audio_start_param* para)
{
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		audio.bitstreams[handle]->setVolume(para->volume * 100 / 6);
		audio.bitstreams[handle]->setPlayingOffset(sf::milliseconds(para->start_time));
		audio.bitstreams[handle]->data_finished = false;
		audio.bitstreams[handle]->play();
		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}


VMINT vm_bitstream_audio_stop(VMINT handle) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		audio.bitstreams[handle]->data_finished = true;
		audio.bitstreams[handle]->play_pos = 0;
		audio.bitstreams[handle]->data_size = 0;
		audio.bitstreams[handle]->stop();
		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}


VMINT vm_bitstream_audio_get_play_time(
	VMINT handle,
	VMUINT* current_time)
{
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.bitstreams.is_active(handle)) {
		*current_time = audio.bitstreams[handle]->getPlayingOffset().asMilliseconds();
		return VM_BITSTREAM_SUCCEED;
	}
	return VM_BITSTREAM_FAILED;
}
