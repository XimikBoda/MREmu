#pragma once
#include "View.h"
#include <string>
#include <vmgraph.h>
#include <vmgraph_fix.h>

namespace GUI::View {
	class Image : public View {
	public:
		VMINT_CANVAS img;

		Image(VMINT_CANVAS img, int w, int h) {
			this->img = img;
			this->w = w, this->h = h;
		}

		void update_size() override {}

		void draw(const Layer& layer, bool focused) override {
			if (focused)
				draw_focused(layer);
			if (img)
				vm_graphic_blt(layer.buf, rb.x, rb.y, (VMBYTE*)img, 0, 0, w, h, 1);
		};
	};
}