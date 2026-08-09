#include "AppSelector.h"
#include <vector>
#include <vmgraph.h>
#include <vmio.h>
#include <vmpromng.h>
#include <vmstdlib.h>

VMWSTR vm_ucs2_string(VMSTR s);

namespace NativeApps::Menu::AppSelector {
	int layer_h = 0;
	VMUINT8* layer_buf = 0;
	int w = 0, h = 0;
	int c_h = 0, b_h = 0;

	VMUINT16 gray = VM_COLOR_888_TO_565(50, 50, 50);

	int m_i = 0;
	
	std::vector<vm_fileinfo_ext> vxps;

	void draw();
	void key_handler(VMINT event, VMINT keycode);

	void entry() {
		w = vm_graphic_get_screen_width();
		h = vm_graphic_get_screen_height();

		layer_h = vm_graphic_create_layer(0, 0, w, h, -1);
		layer_buf = vm_graphic_get_layer_buffer(layer_h);
		
		vm_fileinfo_ext direntry;

		for (int ret = 0, find_h = vm_find_first_ext(vm_ucs2_string((VMSTR)"e:\\mre\\*.vxp"), &direntry);
			find_h >= 0 && !ret; 
			ret = vm_find_next_ext(find_h, &direntry)) 
		{
			vxps.push_back(direntry);
		}

		c_h = vm_graphic_get_character_height();
		b_h = c_h * 2;

		draw();

		vm_reg_keyboard_callback(key_handler);
	}

	void draw() {
		vm_graphic_fill_rect(layer_buf, 0, 0, w, h, 0x0000, 0x0000);

		for (int i = 0; i < vxps.size(); ++i) {
			int y = b_h * i;

			if (i == m_i)
				vm_graphic_fill_rect(layer_buf, 0, y, w, b_h, gray, gray);

			vm_graphic_textout(layer_buf, 2, y + (b_h - c_h) / 2, vxps[i].filefullname, 100, 0xFFFF);

			vm_graphic_line(layer_buf, 0, y + b_h - 1, w, y + b_h - 1, 0xFFFF);
		}

		if(!vxps.size())
			vm_graphic_textout(layer_buf, 0, 0, vm_ucs2_string((VMSTR)"No files in mre folder"), 100, 0xFFFF);

		vm_graphic_flush_layer(&layer_h, 1);
	}

	void key_handler(VMINT event, VMINT keycode) {
		VMWCHAR str[260] = { 0 };

		if (vxps.size() && event == VM_KEY_EVENT_UP) {
			switch (keycode) {
			case VM_KEY_UP:
				if (--m_i < 0)
					m_i = vxps.size() - 1;
				break;
			case VM_KEY_DOWN:
				if (++m_i >= vxps.size())
					m_i = 0;
				break;
			case VM_KEY_OK:
				vm_wstrcat(str, vm_ucs2_string((VMSTR)"e:\\mre\\"));
				vm_wstrcat(str, vxps[m_i].filefullname);
				vm_start_app(str, 0, 0);
				break;
			}
		}

		draw();
	}
}