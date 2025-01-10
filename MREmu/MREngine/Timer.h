#pragma once
#include "ItemsMng.h"
#include <vector>
#include <vmtimer.h>

namespace MREngine {
	struct timer_el
	{
		size_t time = 0;
		size_t cur_val = 0;
		VM_TIMERPROC_T adr = 0;
		bool is_arm = false;

		void run(int tid);
	};
	class Timer {
	public:
		ItemsMng<timer_el> gui_timers;
		ItemsMng<timer_el> timers;

		void update(size_t delta_ms);

		int create(size_t time, VM_TIMERPROC_T adr, bool is_arm, bool gui);
		int destroy(int id, bool gui);
	};
};


MREngine::Timer& get_current_app_timer();