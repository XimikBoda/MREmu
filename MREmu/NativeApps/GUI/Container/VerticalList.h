#pragma once
#include "Container.h"
#include <memory>
#include <vector>

namespace GUI::Container {
	class VertocalList : public Container {
	public:
		struct list_item{
			std::shared_ptr<Base> ui;
			int offset = 0;
		};

		std::vector<list_item> items;
		uint16_t color = 0xFFFF;

		int focused_item_id = 0;

		void update_size() override {
			int offset = 0, w = 0;

			for (int i = 0; i < items.size(); ++i) {
				items[i].offset = offset;
				items[i].ui->update_size();
				offset += items[i].ui->h + 1;
				w = std::max(w, items[i].ui->w);
			}

			this->h = offset;
			this->w = w;
		}

		void update_rb(RenderBox rb) override {
			this->rb = rb;
			for (int i = 0; i < items.size(); ++i) {
				int i_h = items[i].ui->h;
				int i_off = items[i].offset;
				RenderBox i_rb = {
					rb.x, rb.y + i_off,
					rb.w, i_h
				};
				items[i].ui->update_rb(i_rb);
			}
		};

		void draw(const Layer& layer, bool focused) override {
			for (int i = 0; i < items.size(); ++i) {
				bool i_focused = focused && (i == focused_item_id);

				items[i].ui->draw(layer, i_focused);

				auto& i_rb = items[i].ui->rb;
				int i_h = items[i].ui->h;
				vm_graphic_line(layer.buf, 
					rb.x, i_rb.y + i_h,
					rb.x + rb.w, i_rb.y + i_h, color);
			}
		}

		void key_event(int event, int keycode) override {
			if (event == VM_KEY_EVENT_UP) {
				if (keycode == VM_KEY_DOWN)
					focused_item_id++;
				else if (keycode == VM_KEY_UP)
					focused_item_id--;
				else
					if (focuse_item_valid())
						items[focused_item_id].ui->key_event(event, keycode);
				
				if (focused_item_id < 0)
					focused_item_id = items.size() - 1;
				else if (focused_item_id >= items.size())
					focused_item_id = 0;

			}
		}

		void pen_event(const Touch& touch) override {
			int t_x = touch.x - rb.x, t_y = touch.y - rb.y;
			int t_i = 0;

			for (t_i = 0; t_i < items.size(); ++t_i) {
				int i_offset = items[t_i].offset;
				int i_h = items[t_i].ui->h;

				if (t_y >= i_offset && t_y < i_offset + i_h + 1)
					break;
			}

			if (t_i == items.size())
				return;

			if (touch.event == VM_PEN_EVENT_TAP)
				focused_item_id = t_i;

			items[t_i].ui->pen_event(touch);
		}

		bool focuse_item_valid() {
			return focused_item_id >= 0 && focused_item_id < items.size();
		}

		std::shared_ptr<Base> get_focused() {
			if (focuse_item_valid())
				return items[focused_item_id].ui->get_focused();
			return nullptr;
		}
	};
}