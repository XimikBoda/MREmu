#pragma once
#include "ItemsMng.h"
#include <vector>
#include <vmtimer.h>
#include <SFML/System/Time.hpp>

class App;

namespace MREngine {
	struct timer_el
	{
		VM_TIMERPROC_T adr = 0;
		sf::Time interval;
		sf::Time last_point;
		sf::Time pause_point;
	};
	class Timer {
		ItemsMng<timer_el> gui_timers;
		ItemsMng<timer_el> timers;

		bool active = true;
	public:

		void set_active(bool active);

		sf::Time get_time_for_nearest();

		void update(App* cur_app);

		int create(uint32_t time, VM_TIMERPROC_T adr, bool gui);
		int destroy(int id, bool gui);
	};
};