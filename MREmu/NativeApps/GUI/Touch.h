#pragma once
#include <cmath>
#include <vmio.h>

namespace GUI {
	class Touch {
	public:
		int x = 0, y = 0;
		int event;
		int start_x = 0, start_y = 0;
		int last_x = 0, last_y = 0;
		int d_x = 0, d_y = 0;
		bool moved = false;
		const int moved_triger = 10;

		Touch& pen_event(int event, int x, int y) {
			last_x = this->x, last_y = this->y;
			this->x = x, this->y = y;
			this->event = event;
			switch (event) {
			case VM_PEN_EVENT_TAP:
				start_x = x, start_y = y;
				last_x = x, last_y = y;
				d_x = d_y = 0;
				moved = false;
				break;
			case VM_PEN_EVENT_MOVE:
				d_x = x - last_x, d_y = y - last_y;
				if (std::abs(start_x - x) > moved_triger
					|| std::abs(start_y - y) > moved_triger)
					moved = true;
				break;
			case VM_PEN_EVENT_RELEASE:
			case VM_PEN_EVENT_ABORT:
				moved = false;
				break;
			}

			return *this;
		}
	};
}