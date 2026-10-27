#pragma once
#include "Base.h"
#include "Layer.h"
#include "Touch.h"
#include <memory>
#include <vmio.h>

namespace GUI {
	class Engine{
	public:
		std::shared_ptr<Base> ui;
		Layer layer;
		Touch touch;
		int key_press_count = 0;
		bool pen_pressed = false;

		void update() {
			if (ui) {
				ui->update_size();
				ui->update_rb(layer.get_full_box());
			}
		}

		void draw() {
			layer.clear();
			if (ui)
				ui->draw(layer, true);
			layer.flash();
		}

		void sys_event(int message, int param, std::shared_ptr<Engine>& self) {
			switch (message) {
			case VM_MSG_CREATE:
			case VM_MSG_ACTIVE:
				layer.create();
				update();
				draw();
				break;
			case VM_MSG_INACTIVE:
				layer.dectroy();
				break;
			case VM_MSG_PAINT:
				draw();
				break;
			case VM_MSG_QUIT:
				self.reset();
				return;
			}
		}

		void key_event(int event, int keycode) {
			if (event == VM_KEY_EVENT_DOWN)
				key_press_count++;

			if (ui && key_press_count) {
				ui->key_event(event, keycode);
				draw();
			}

			if (event == VM_KEY_EVENT_UP)
				if (key_press_count > 0)
					key_press_count--;
		}

		void pen_event(int event, int x, int y) {
			if (event == VM_PEN_EVENT_TAP)
				pen_pressed = true;

			if (ui && pen_pressed) {
				ui->pen_event(touch.pen_event(event, x, y));
				draw();
			}

			if (event == VM_PEN_EVENT_RELEASE || event == VM_PEN_EVENT_ABORT)
				pen_pressed = false;
		}
	};
}