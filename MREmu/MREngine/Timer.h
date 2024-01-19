#pragma once
#include <cstdint>
#include <vector>

namespace MREngine {
	struct timer_el
	{
		size_t time = 0;
		size_t cur_val = 0;
		uint32_t adr = 0;
		bool active = false;
	};
	class Timer {
	public:
		std::vector<timer_el> gui_timers;
		std::vector<timer_el> timers;

		void update(size_t delta_ms);

		int create(size_t time, uint32_t adr, bool gui);
		int destroy(int id, bool gui);
	};
};


MREngine::Timer& get_current_app_timer();