#include "Timer.h"
#include "../Bridge.h"
#include "../Memory.h"
#include "../AppContext.h"
#include <vmtimer.h>
#include <SFML/System/Clock.hpp>

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
	return AppContext::get_executing_app_timer().create(millisec, timerproc, true);
}


VMINT vm_delete_timer(VMINT timerid) {
	return AppContext::get_executing_app_timer().destroy(timerid, true);
}


VMINT vm_create_timer_ex(VMUINT32 millisec, VM_TIMERPROC_T timerproc) {
	return AppContext::get_executing_app_timer().create(millisec, timerproc, false);
}


VMINT vm_delete_timer_ex(VMINT timerid) {
	return AppContext::get_executing_app_timer().destroy(timerid, false);
}

