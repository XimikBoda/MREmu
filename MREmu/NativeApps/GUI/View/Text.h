#pragma once
#include "View.h"
#include <string>
#include <vmgraph.h>

namespace GUI::View {
	class Text : public View {
	public:
		std::u16string str;
		uint16_t color;

		Text(std::u16string str, uint16_t color = 0xFFFF) {
			this->str = str;
			this->color = color;
			update_size();
		}

		void update_size() override {
			w = vm_graphic_get_string_width((VMWSTR)str.c_str());
			h = vm_graphic_get_string_height((VMWSTR)str.c_str());
		}

		void draw(const Layer& layer, bool focused) override {
			if (focused)
				draw_focused(layer);
			vm_graphic_textout(layer.buf, rb.x, rb.y, (VMWSTR)str.c_str(), str.length(), color);
		};
	};
}