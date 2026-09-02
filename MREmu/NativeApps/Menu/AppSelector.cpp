#include "AppSelector.h"
#include "../../MREngine/Image.h"
#include "../../AppManager.h"
#include <vector>
#include <string>
#include <vmgraph.h>
#include <vmio.h>
#include <vmpromng.h>
#include <vmstdlib.h>
#include <vmtimer.h>

VMWSTR vm_ucs2_string(VMSTR s);
extern AppManager* g_appManager;

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
	
	int marquee_offset = 0;
	int marquee_timer_id = 0;
	int marquee_state = 0;
	int marquee_pause_ticks = 28;
	int last_selected = 0;

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
	void timer_cb(VMINT tid);

	void reset_marquee() {
		marquee_offset = 0;
		marquee_state = 0;
		marquee_pause_ticks = 28;
		last_selected = m_i;
	}

	void entry() {
		m_i = 0;
		scroll_pos = 0;
		touched = false;
		reset_marquee();

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

		if (marquee_timer_id)
			vm_delete_timer(marquee_timer_id);
		marquee_timer_id = vm_create_timer(35, timer_cb);
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
			reset_marquee();
			draw();
		}
	}

	void draw() {
		vm_graphic_fill_rect(layer_buf, 0, 0, w, h, 0x0000, 0x0000);

		for (int i = 0; i < vxps.size(); ++i) {
			int y = b_h * i - scroll_pos;
			if (y + b_h < 0 || y >= h)
				continue;

			if (i == m_i)
				vm_graphic_fill_rect(layer_buf, 0, y, w, b_h, gray, gray);

			if (vxps[i].img)
				vm_graphic_blt(layer_buf, 1, y, (VMBYTE*)vxps[i].img, 0, 0, img_wh, img_wh, 1);

			int text_x = 2 + b_h;
			int text_y = y + (b_h - c_h) / 2;
			int max_text_w = w - text_x - 4;

			if (i == m_i) {
				int text_w = vm_graphic_get_string_width((VMWSTR)vxps[i].name.c_str());
				if (text_w > max_text_w) {
					int clip_top = std::max(0, y);
					int clip_bottom = std::min(h - 1, y + b_h - 1);
					if (clip_top <= clip_bottom) {
						vm_graphic_set_clip(text_x, clip_top, w - 2, clip_bottom);
						vm_graphic_textout(layer_buf, text_x - marquee_offset, text_y, (VMWSTR)vxps[i].name.c_str(), 100, 0xFFFF);
						vm_graphic_reset_clip();
					}
				}
				else {
					vm_graphic_textout(layer_buf, text_x, text_y, (VMWSTR)vxps[i].name.c_str(), 100, 0xFFFF);
				}
			}
			else {
				vm_graphic_textout(layer_buf, text_x, text_y, (VMWSTR)vxps[i].name.c_str(), 100, 0xFFFF);
			}

			vm_graphic_line(layer_buf, 0, y + b_h - 1, w, y + b_h - 1, 0xFFFF);
		}

		if (!vxps.size()) {
			vm_graphic_textout(layer_buf, 0, 0, vm_ucs2_string((VMSTR)"No files in MRE folder."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, c_h + 2, vm_ucs2_string((VMSTR)"Put them in fs/e/mre."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, c_h*2 + 2, vm_ucs2_string((VMSTR)"Or drag them here to import..."), 100, 0xFFFF);
		}

		vm_graphic_flush_layer(&layer_h, 1);
	}

	void timer_cb(VMINT tid) {
		if (!layer_buf || vxps.empty() || touched)
			return;

		if (g_appManager) {
			App* act = g_appManager->get_active_app();
			if (act && act->system_callbacks.ph_app_id != vm_pmng_get_current_handle())
				return;
		}

		if (m_i < 0 || m_i >= (int)vxps.size())
			return;

		int text_x = 2 + b_h;
		int max_text_w = w - text_x - 4;
		int text_w = vm_graphic_get_string_width((VMWSTR)vxps[m_i].name.c_str());

		if (text_w <= max_text_w) {
			if (marquee_offset != 0) {
				marquee_offset = 0;
				draw();
			}
			return;
		}

		int max_scroll = text_w - max_text_w + 10;

		if (marquee_state == 0) {
			if (--marquee_pause_ticks <= 0)
				marquee_state = 1;
		}
		else if (marquee_state == 1) {
			marquee_offset++;
			if (marquee_offset >= max_scroll) {
				marquee_offset = max_scroll;
				marquee_state = 2;
				marquee_pause_ticks = 34;
			}
			draw();
		}
		else if (marquee_state == 2) {
			if (--marquee_pause_ticks <= 0) {
				marquee_offset = 0;
				marquee_state = 0;
				marquee_pause_ticks = 28;
				draw();
			}
		}
	}

	void key_handler(VMINT event, VMINT keycode) {
		int old_m_i = m_i;
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

		if (m_i != old_m_i)
			reset_marquee();

		if (vxps.size() * b_h > h) {
			if (b_h * m_i - scroll_pos + b_h > h)
				scroll_pos = b_h * m_i + b_h - h;

			if (b_h * m_i - scroll_pos < 0)
				scroll_pos = b_h * m_i;
		}

		draw();
	}

	void pen_handler(VMINT event, VMINT x, VMINT y) {
		int old_m_i = m_i;
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

		if (m_i != old_m_i)
			reset_marquee();

		draw();
	}
}