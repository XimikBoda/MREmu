#include "Timer.h"
#include "../Bridge.h"
#include "../Memory.h"
#include <vmtimer.h>

void MREngine::Timer::update(size_t delta_ms)
{
	for (int i = 0; i < gui_timers.size(); ++i) {
		auto& el = gui_timers[i];
		if (el.active) {
			el.cur_val += delta_ms;
			if (el.cur_val >= el.time) {
				el.cur_val = 0;
				Bridge::run_cpu(el.adr, 1, i);
			}
		}
	}
	for (int i = 0; i < timers.size(); ++i) {
		auto& el = timers[i];
		if (el.active) {
			el.cur_val += delta_ms;
			if (el.cur_val >= el.time) {
				el.cur_val = 0;
				Bridge::run_cpu(el.adr, 1, i);
			}
		}
	}
}

int MREngine::Timer::create(size_t time, uint32_t adr, bool gui)
{
	auto& tim = gui ? gui_timers : timers;
	int i = -1;

	for (i = 0; i < tim.size(); ++i)
		if (!tim[i].active)
			break;

	if (i == tim.size())
		tim.push_back({});

	tim[i] = { time, 0, adr, true };

	return i;
}

int MREngine::Timer::destroy(int id, bool gui)
{
	auto& tim = gui ? gui_timers : timers;

	if (id < 0 || id >= tim.size())
		return -1;

	if (!tim[id].active)
		return -1;

	tim[id].active = false;

	while (tim.size() && !tim[tim.size() - 1].active)
		tim.resize(tim.size() - 1);

	return 0;
}


VMINT vm_create_timer(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	return get_current_app_timer().create(millisec, FUNC_TO_UINT32(timerproc), true);
}


VMINT vm_delete_timer(VMINT timerid) {
	return get_current_app_timer().destroy(timerid, true);
}


VMINT vm_create_timer_ex(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	return get_current_app_timer().create(millisec, FUNC_TO_UINT32(timerproc), false);
}


VMINT vm_delete_timer_ex(VMINT timerid) {
	return get_current_app_timer().destroy(timerid, false);
}

