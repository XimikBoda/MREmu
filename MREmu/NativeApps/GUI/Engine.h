#pragma once
#include "Base.h"
#include "Layer.h"
#include "Touch.h"
#include <memory>
#include <vmio.h>

namespace GUI {
	class Engine {
	public:
		std::shared_ptr<Base> ui;
		Layer layer;
		Touch touch;

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

		void sys_event(int message, int param) {
			switch (message) {
			case VM_MSG_CREATE:
			case VM_MSG_ACTIVE:
				layer.create();
				update();
				break;
			case VM_MSG_QUIT:
			case VM_MSG_INACTIVE:
				layer.dectroy();
				break;
			case VM_MSG_PAINT:
				draw();
				break;
			}
		}

		void key_event(int event, int keycode) {
			if (ui)
				ui->key_event(event, keycode);
			draw();
		}

		void pen_event(int event, int x, int y) {
			if (ui)
				ui->pen_event(touch.pen_event(event, x, y));
			draw();
		}
	};
}