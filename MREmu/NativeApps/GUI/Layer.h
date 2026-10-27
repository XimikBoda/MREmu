#pragma once
#include <vmgraph.h>
#include "RenderBox.h"

namespace GUI {
	class Layer {
	public:
		int handle = 0;
		VMUINT8* buf = 0;
		int w = 0, h = 0;

		void create() {
			w = vm_graphic_get_screen_width();
			h = vm_graphic_get_screen_height();

			handle = vm_graphic_create_layer(0, 0, w, h, -1);
			buf = vm_graphic_get_layer_buffer(handle);
		}

		void dectroy() {
			if (handle != -1)
				vm_graphic_delete_layer(handle);
			
			handle = -1;
			buf = 0;
		}

		void clear() {
			vm_graphic_fill_rect(buf, 0, 0, w, h, 0, 0);
		}

		void flash() {
			vm_graphic_flush_layer(&handle, 1);
		}

		RenderBox get_full_box() {
			return {0, 0, w, h};
		}

		~Layer() {
			dectroy();
		}
	};
}