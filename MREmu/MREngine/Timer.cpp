#include "Timer.h"
#include "../Bridge.h"
#include "../Memory.h"
#include <vmtimer.h>

bool get_current_app_is_arm();

void MREngine::timer_el::run(int tid) {
	if (is_arm)
		Bridge::run_cpu(FUNC_TO_UINT32(adr), 1, tid);
	else
		adr(tid);
}

void MREngine::Timer::update(size_t delta_ms)
{
	for (int i = 0; i < gui_timers.size(); ++i)
		if (gui_timers.is_active(i)) {
			auto& el = gui_timers[i];
			el.cur_val += delta_ms;
			if (el.cur_val >= el.time) {
				el.cur_val = 0;
				el.run(i);
			}
		}
	for (int i = 0; i < timers.size(); ++i)
		if (timers.is_active(i)) {
			auto& el = timers[i];
			el.cur_val += delta_ms;
			if (el.cur_val >= el.time) {
				el.cur_val = 0;
				el.run(i);
			}
		}
}

int MREngine::Timer::create(size_t time, VM_TIMERPROC_T adr, bool is_arm, bool gui)
{
	auto& tim = gui ? gui_timers : timers;
	int i = tim.push({ time, 0, adr, is_arm });
	return i;
}

int MREngine::Timer::destroy(int id, bool gui)
{
	auto& tim = gui ? gui_timers : timers;

	if (!tim.is_active(id))
		return -1;

	tim.remove(id);
	return 0;
}


VMINT vm_create_timer(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	bool is_arm = get_current_app_is_arm();
	return get_current_app_timer().create(millisec, timerproc, is_arm, true);
}


VMINT vm_delete_timer(VMINT timerid) {
	return get_current_app_timer().destroy(timerid, true);
}


VMINT vm_create_timer_ex(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	bool is_arm = get_current_app_is_arm();
	return get_current_app_timer().create(millisec, timerproc, is_arm, false);
}


VMINT vm_delete_timer_ex(VMINT timerid) {
	return get_current_app_timer().destroy(timerid, false);
}

