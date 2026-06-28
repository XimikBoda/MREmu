#include "Audio.h"
#include <cstring>

bool BitstreamRecord::onStart() {
	this->setChannelCount(1);
	return true;
}

bool BitstreamRecord::onProcessSamples(const std::int16_t* samples, std::size_t sampleCount) {
	std::lock_guard lock(data_mutex);
	if (!buf)
		return false;

	int len = sampleCount;
	if (len > buf_len - size)
		len = buf_len - size;

	memcpy(buf + size, samples, len * 2);

	size += len;

	return true;
}

void BitstreamRecord::onStop() {}

void BitstreamRecord::setBuffer(int16_t* buf, uint32_t buf_len) {
	std::lock_guard lock(data_mutex);
	this->buf = buf;
	this->buf_len = buf_len;
}

void BitstreamRecord::getReadBuffer(int16_t** buf, uint32_t* buf_len) {
	std::lock_guard lock(data_mutex);
	*buf = this->buf;
	*buf_len = size;
}

void BitstreamRecord::readDataDone(uint32_t len) {
	std::lock_guard lock(data_mutex);
	memmove(buf, buf + len, (size - len) * 2); // TODO
	size -= len;
}

BitstreamRecord::~BitstreamRecord() {
	stop();
}

std::shared_ptr<BitstreamRecord> bitsream_record;

Media_Status mremu_media_record(Media_Format format, mremu_media_handler handler, void* param) {
	uint32_t sampleRate = 0;
	switch (format) {
	case MEDIA_FORMAT_PCM_8K:
		sampleRate = 8000;
		break;
	case MEDIA_FORMAT_PCM_16K:
		sampleRate = 16000;
		break;
	default:
		return MEDIA_FAIL;
	}

	if (!BitstreamRecord::isAvailable())
		return MEDIA_FAIL;

	if(!bitsream_record)
		return MEDIA_FAIL;

	bitsream_record->start(sampleRate);

	return MEDIA_SUCCESS;
}

void mremu_media_setbufer(VMUINT16* buffer, VMUINT32 buf_len) {
	if (!bitsream_record)
		bitsream_record = std::make_shared<BitstreamRecord>();

	bitsream_record->setBuffer((int16_t*)buffer, buf_len);
}

void mremu_media_getreadbuffer(VMUINT16** buffer, VMUINT32* buf_len) {
	if (bitsream_record)
		bitsream_record->getReadBuffer((int16_t**)buffer, (uint32_t*)buf_len);
}

void mremu_media_readdatadone(VMUINT32 len) {
	if (bitsream_record)
		bitsream_record->readDataDone(len);
}

void mremu_media_stop() {
	if (bitsream_record)
		bitsream_record->stop();
}