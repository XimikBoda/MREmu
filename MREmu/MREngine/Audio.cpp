#include "Audio.h"
#include <vmmm.h>


VMINT vm_midi_play_by_bytes(VMUINT8* midibuf, VMINT len, VMINT repeat, void (*f)(VMINT handle, VMINT event)) {
	return vm_midi_play_by_bytes_ex(midibuf, len, 0, repeat, 0, f);
}

VMINT vm_midi_play_by_bytes_ex(VMUINT8* midibuf, VMINT len, VMUINT start_time, 
	VMINT repeat, VMUINT path, void (*f)(VMINT handle, VMINT event)) 
{
	if (repeat > 1)
		printf("Need repeat in vm_midi_play_by_bytes_ex\n");

	std::shared_ptr<Midi> midi = std::make_shared<Midi>(midibuf, len);
	if (midi->error)
		return -1;

	midi->play();

	auto& audio = get_current_app_audio();
	return audio.midis.push(midi);
}

void vm_midi_stop(VMINT handle) {
	MREngine::AppAudio& audio = get_current_app_audio();

	if (audio.midis.is_active(handle)) {
		audio.midis[handle]->stop();
		audio.midis[handle].reset();
		audio.midis.remove(handle);
	}
}