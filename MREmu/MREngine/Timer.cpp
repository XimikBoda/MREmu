#include "Timer.h"
#include "../Bridge.h"
#include "../Memory.h"
#include <vmtimer.h>

#include "../App.h"

extern sf::Clock global_clock;

void MREngine::Timer::set_active(bool active)
{
	if (this->active == active)
		return;

	this->active = active;

	auto cur = global_clock.getElapsedTime();

	for (int i = 0; i < gui_timers.size(); ++i)
		if (gui_timers.is_active(i)) {
			auto& t = gui_timers[i];

			if (active) 
				t.last_point = cur - (t.pause_point - t.last_point);
			else
				t.pause_point = cur;
		}
}

sf::Time MREngine::Timer::get_time_for_nearest()
{
	auto cur = global_clock.getElapsedTime();

	sf::Time nearst = sf::milliseconds(INT32_MAX);

	if(active)
		for (int i = 0; i < gui_timers.size(); ++i)
			if (gui_timers.is_active(i)) {
				auto& t = gui_timers[i];
				
				sf::Time next_point = t.last_point + t.interval;
				sf::Time time_to_next = next_point - cur;

				if (nearst > time_to_next)
					nearst = time_to_next;
			}

	for (int i = 0; i < timers.size(); ++i)
		if (timers.is_active(i)) {
			auto& t = timers[i];

			sf::Time next_point = t.last_point + t.interval;
			sf::Time time_to_next = next_point - cur;

			if (nearst > time_to_next)
				nearst = time_to_next;
		}

	return nearst;
}

void MREngine::Timer::update(App* cur_app)
{
	auto cur = global_clock.getElapsedTime();

	if (active)
		for (int i = 0; i < gui_timers.size(); ++i)
			if (gui_timers.is_active(i)) {
				auto& t = gui_timers[i];
				if (t.last_point + t.interval <= cur) {
					t.last_point = cur;
					cur_app->run(t.adr, i + 1);
				}
			}

	for (int i = 0; i < timers.size(); ++i)
		if (timers.is_active(i)) {
			auto& t = timers[i];
			if (t.last_point + t.interval <= cur) {
				t.last_point = cur;
				cur_app->run(t.adr, i + 1);
			}
		}
}

int MREngine::Timer::make_all_due() {
	auto cur = global_clock.getElapsedTime();
	int made = 0;

	// One extra millisecond so the due test (last_point + interval <= cur) is
	// satisfied even for a zero-interval timer, whose last_point is reset to cur
	// by update() the moment it fires.
	ItemsMng<timer_el>* banks[2] = { &gui_timers, &timers };

	for (ItemsMng<timer_el>* bank : banks)
		for (size_t i = 0; i < bank->size(); ++i) {
			if (!bank->is_active(i))
				continue;
			(*bank)[i].last_point = cur - (*bank)[i].interval - sf::milliseconds(1);
			++made;
		}

	return made;
}

int MREngine::Timer::create(uint32_t time, VM_TIMERPROC_T adr, bool gui)
{
	auto& tim = gui ? gui_timers : timers;
	auto cur = global_clock.getElapsedTime();
	int i = tim.push({ adr, sf::milliseconds(time), cur, cur });
	return i + 1;
}

int MREngine::Timer::destroy(int id, bool gui)
{
	--id;

	auto& tim = gui ? gui_timers : timers;

	if (!tim.is_active(id))
		return -1;

	tim.remove(id);
	return 0;
}


VMINT vm_create_timer(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	return get_current_app_timer().create(millisec, timerproc, true);
}


VMINT vm_delete_timer(VMINT timerid) {
	return get_current_app_timer().destroy(timerid, true);
}


VMINT vm_create_timer_ex(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	return get_current_app_timer().create(millisec, timerproc, false);
}


VMINT vm_delete_timer_ex(VMINT timerid) {
	return get_current_app_timer().destroy(timerid, false);
}

