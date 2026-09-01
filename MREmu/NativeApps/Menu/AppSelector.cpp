#include "AppSelector.h"
#include "../../MREngine/Image.h"
#include <vector>
#include <string>
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
	int img_wh = 0;

	int scroll_pos = 0;
	int touch_start_y = 0;
	int touch_last_y = 0;
	bool touched = false;
	int touch_time = 0;
	

	VMUINT16 gray = VM_COLOR_888_TO_565(50, 50, 50);

	int m_i = 0;

	struct vxp {
		std::u16string name;
		std::u16string path;
		VMINT_CANVAS img;
	};

	std::vector<vxp> vxps;

	void scan();
	void draw();
	void key_handler(VMINT event, VMINT keycode);
	void pen_handler(VMINT event, VMINT x, VMINT y);

	void entry() {
		w = vm_graphic_get_screen_width();
		h = vm_graphic_get_screen_height();

		c_h = vm_graphic_get_character_height();
		b_h = c_h * 2;
		img_wh = b_h - 2;

		layer_h = vm_graphic_create_layer(0, 0, w, h, -1);
		layer_buf = vm_graphic_get_layer_buffer(layer_h);

		scan();
		draw();

		vm_reg_keyboard_callback(key_handler);
		vm_reg_pen_callback(pen_handler);
	}

	void scan() {
		for (auto& v : vxps)
			if (v.img)
				vm_graphic_release_canvas_FIX(v.img);
		vxps.clear();

		vm_fileinfo_ext direntry;

		for (int ret = 0, find_h = vm_find_first_ext((VMWSTR)u"e:\\mre\\*.vxp", &direntry);
			find_h >= 0 && !ret;
			ret = vm_find_next_ext(find_h, &direntry))
		{
			std::u16string path = u"e:\\mre\\";
			std::u16string name = (char16_t*)direntry.filefullname;
			path += name;

			VMINT_CANVAS img = 0;

			int f = vm_file_open((VMWSTR)path.c_str(), MODE_READ, 1);
			if (f >= 0) {
				VMUINT size = 0, rsize = 0;
				vm_file_getfilesize(f, &size);

				std::vector<uint8_t> data(size);
				vm_file_read(f, data.data(), size, &rsize);

				vm_file_close(f);

				std::string_view data_view(reinterpret_cast<const char*>(data.data()), data.size());
				size_t pos = data_view.find("VREAPPLOGO09BVRE");

				if (pos != std::string_view::npos) {
					int img_size = *(int*)(data.data() + pos + 16 + 3);
					VMUINT8* img_data = (data.data() + pos + 16 + 3 + 4);
					if (data_view.substr(pos + 16, 3) == "PNG")
						img_data += 8;

					img = vm_graphic_load_image_resized_FIX(img_data, img_size, img_wh, img_wh);
				}
			}

			vxps.push_back({ name, path, img });
		}
	}

	void rescan() {
		if (layer_buf) {
			scan();
			if (m_i >= (int)vxps.size())
				m_i = vxps.empty() ? 0 : vxps.size() - 1;
			draw();
		}
	}

	void draw() {
		vm_graphic_fill_rect(layer_buf, 0, 0, w, h, 0x0000, 0x0000);

		for (int i = 0; i < vxps.size(); ++i) {
			int y = b_h * i - scroll_pos;

			if (i == m_i)
				vm_graphic_fill_rect(layer_buf, 0, y, w, b_h, gray, gray);

			if (vxps[i].img)
				vm_graphic_blt(layer_buf, 1, y, (VMBYTE*)vxps[i].img, 0, 0, img_wh, img_wh, 1);

			vm_graphic_textout(layer_buf, 2 + b_h, y + (b_h - c_h) / 2, (VMWSTR)vxps[i].name.c_str(), 100, 0xFFFF);

			vm_graphic_line(layer_buf, 0, y + b_h - 1, w, y + b_h - 1, 0xFFFF);
		}

		if (!vxps.size()) {
			vm_graphic_textout(layer_buf, 0, 0, vm_ucs2_string((VMSTR)"No files in MRE folder."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, c_h + 2, vm_ucs2_string((VMSTR)"Put them in fs/e/mre."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, c_h*2 + 2, vm_ucs2_string((VMSTR)"Or drag them here to import..."), 100, 0xFFFF);
		}

		vm_graphic_flush_layer(&layer_h, 1);
	}

	void key_handler(VMINT event, VMINT keycode) {
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
				vm_start_app((VMWSTR)vxps[m_i].path.c_str(), 0, 0);
				break;
			}
		}

		if (vxps.size() * b_h > h) {
			if (b_h * m_i - scroll_pos + b_h > h)
				scroll_pos = b_h * m_i + b_h - h;

			if (b_h * m_i - scroll_pos < 0)
				scroll_pos = b_h * m_i;
		}

		draw();
	}

	void pen_handler(VMINT event, VMINT x, VMINT y) {
		switch (event) {
			case VM_PEN_EVENT_TAP:
				m_i = (y + scroll_pos) / b_h;
				touch_start_y = y;
				touch_last_y = y;
				touched = true;
				touch_time = vm_get_tick_count();
				break;
			case VM_PEN_EVENT_MOVE:
			case VM_PEN_EVENT_REPEAT:
			case VM_PEN_EVENT_LONG_TAP:
			case VM_PEN_EVENT_DOUBLE_CLICK:
				if (touched) {
					scroll_pos += touch_last_y - y;
					touch_last_y = y;

					if (scroll_pos < 0)
						scroll_pos = 0;

					if (scroll_pos > vxps.size() * b_h - h)
						scroll_pos = vxps.size() * b_h - h;
				}
				break;
			case VM_PEN_EVENT_RELEASE:
			case VM_PEN_EVENT_ABORT:
				touched = false;
				if (std::abs(touch_start_y - y) < b_h / 2 && vm_get_tick_count() - touch_time < 150)
					vm_start_app((VMWSTR)vxps[m_i].path.c_str(), 0, 0);
				break;
		}

		if (m_i < 0)
			m_i = vxps.size() - 1;

		if (m_i >= vxps.size())
			m_i = 0;

		draw();
	}
}