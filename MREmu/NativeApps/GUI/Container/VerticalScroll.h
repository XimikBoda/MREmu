#pragma once
#include "Container.h"
#include <memory>
#include <vector>

namespace GUI::Container {
	class VerticalScroll : public Container {
	public:
		std::shared_ptr<Base> ui;
		int scroll_pos = 0;

		void update_size() override {
			ui->update_size();
			w = ui->w, h = ui->h;
		}

		void update_rb(RenderBox rb) override {
			this->rb = rb;
			rb.y -= scroll_pos;
			ui->update_rb(rb);
		};

		void draw(const Layer& layer, bool focused) override {
			ui->draw(layer, focused);
		};

		void check_focused_pos() {
			auto f_ui = ui->get_focused();
			if (!f_ui)
				return;

			int f_top = f_ui->rb.y, top = rb.y;
			int f_bottom = f_ui->rb.y + f_ui->rb.h, bottom = rb.y + rb.h;

			bool changed = false;

			if (f_bottom > bottom)
				scroll_pos += f_bottom - bottom, changed = true;

			if (f_top < top)
				scroll_pos += f_top - top, changed = true;

			if(changed)
				update_rb(rb);
		}

		void key_event(int event, int keycode) override {
			ui->key_event(event, keycode);

			check_focused_pos();
		}

		void pen_event(const Touch& touch) override {
			if (touch.event == VM_PEN_EVENT_MOVE && touch.moved) {
				scroll_pos -= touch.d_y;

				if (scroll_pos > ui->h - rb.h)
					scroll_pos = ui->h - rb.h;


				if (scroll_pos < 0)
					scroll_pos = 0;

				update_rb(rb);
			}
			else
				ui->pen_event(touch);

			check_focused_pos();
		}

		std::shared_ptr<Base> get_focused() {
			return ui->get_focused();
		}
	};
}