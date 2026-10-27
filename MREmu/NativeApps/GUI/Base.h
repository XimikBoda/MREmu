#pragma once
#include "Layer.h"
#include "Touch.h"
#include <cinttypes>
#include <memory>

namespace GUI {
	class Base : public std::enable_shared_from_this<Base> {
	public:
		int w = 0, h = 0;
		uint16_t focused_color = VM_COLOR_888_TO_565(50, 50, 50);
		
		RenderBox rb;

		virtual void update_size() { w = 0, h = 0; }
		virtual void update_rb(RenderBox rb) {
			this->rb = rb;
		};
		virtual void draw(const Layer& layer, bool focused) {};

		virtual void key_event(int event, int keycode) {
			if (event == VM_KEY_EVENT_UP)
				if (keycode == VM_KEY_OK)
					ok();
		}
		virtual void pen_event(const Touch& touch) {
			if (touch.event == VM_PEN_EVENT_RELEASE)
				if (!touch.moved)
					ok();
		}

		virtual void ok() {}

		virtual std::shared_ptr<Base> get_focused() {
			return shared_from_this();
		};

		virtual void draw_focused(const Layer& layer) {
			vm_graphic_fill_rect(layer.buf, rb.x, rb.y, rb.w, rb.h, focused_color, focused_color);
		};

	};
}