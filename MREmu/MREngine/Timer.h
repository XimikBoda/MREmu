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

		// Mark every active timer as overdue so the next AppManager tick fires it.
		//
		// The app is purely event-driven - it only executes inside a slice entered
		// by a timer callback or an incoming event - so an app with a long timer
		// looks frozen to external tooling. Making timers due lets a debugger drive
		// it. Returns how many timers were made due.
		//
		// Note the app still reads the real clock via vm_get_tick_count, so it can
		// observe that its timers fired early. That is intended for a debug facility.
		int make_all_due();

		int create(uint32_t time, VM_TIMERPROC_T adr, bool gui);
		int destroy(int id, bool gui);
	};
};


MREngine::Timer& get_current_app_timer();