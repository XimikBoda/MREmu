#include "AppSelector.h"
#include "BottomSheet.h"
#include "../../MREngine/Image.h"
#include "../../AppManager.h"
#include "../../DragAndDrop.h"
#include <vector>
#include <string>
#include <cctype>
#include <cstring>
#include <vmcert.h>
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

	VMUINT last_scroll_time = 0;
	bool scrollbar_visible = false;

	BottomSheet details_sheet;
	BottomSheet confirm_sheet;

	VMUINT16 gray = VM_COLOR_888_TO_565(50, 50, 50);

	int m_i = 0;

	struct vxp {
		std::u16string name;
		std::u16string path;
		VMINT_CANVAS img = 0;
		size_t file_size = 0;
		std::u16string app_name;
		std::u16string dev_name;
	};

	std::vector<vxp> vxps;

	void scan();
	void draw();
	void key_handler(VMINT event, VMINT keycode);
	void pen_handler(VMINT event, VMINT x, VMINT y);
	void timer_cb(VMINT tid);

	static const char* get_t9_chars(int keycode) {
		switch (keycode) {
		case VM_KEY_NUM1: return "1";
		case VM_KEY_NUM2: return "abc2";
		case VM_KEY_NUM3: return "def3";
		case VM_KEY_NUM4: return "ghi4";
		case VM_KEY_NUM5: return "jkl5";
		case VM_KEY_NUM6: return "mno6";
		case VM_KEY_NUM7: return "pqrs7";
		case VM_KEY_NUM8: return "tuv8";
		case VM_KEY_NUM9: return "wxyz9";
		case VM_KEY_NUM0: return "0 ";
		default: return "";
		}
	}

	static std::string u16_to_u8(const std::u16string& u16) {
		std::string s;
		s.reserve(u16.size());
		for (char16_t c : u16) {
			if (c < 128)
				s.push_back((char)c);
			else
				s.push_back('?');
		}
		return s;
	}

	void trigger_scrollbar() {
		last_scroll_time = vm_get_tick_count();
	}

	void reset_marquee() {
		marquee_offset = 0;
		marquee_state = 0;
		marquee_pause_ticks = 28;
		last_selected = m_i;
	}

	void entry() {
		details_sheet.hide();
		confirm_sheet.hide();
		m_i = 0;
		scroll_pos = 0;
		touched = false;
		reset_marquee();
		trigger_scrollbar();

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
			size_t file_size = 0;
			std::u16string app_name;
			std::u16string dev_name;

			int f = vm_file_open((VMWSTR)path.c_str(), MODE_READ, 1);
			if (f >= 0) {
				VMUINT size = 0, rsize = 0;
				vm_file_getfilesize(f, &size);
				file_size = size;

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

				if (data.size() >= 12) {
					uint32_t tags_offset = *(uint32_t*)&data[data.size() - 12];
					if (tags_offset < data.size() - 8) {
						uint32_t cur = tags_offset;
						bool is_ucs2 = false;

						uint32_t scan_cur = cur;
						while (scan_cur + 8 < data.size()) {
							uint32_t tag_id = *(uint32_t*)&data[scan_cur];
							uint32_t tag_len = *(uint32_t*)&data[scan_cur + 4];
							scan_cur += 8;
							if (tag_id == 0 || scan_cur + tag_len > data.size())
								break;
							if (tag_id == VM_CE_INFO_CHARSET && tag_len >= 4) {
								is_ucs2 = (*(uint32_t*)&data[scan_cur] != 0);
							}
							scan_cur += tag_len;
						}

						while (cur + 8 < data.size()) {
							uint32_t tag_id = *(uint32_t*)&data[cur];
							uint32_t tag_len = *(uint32_t*)&data[cur + 4];
							cur += 8;
							if (tag_id == 0 || cur + tag_len > data.size())
								break;
							if (tag_id == VM_CE_INFO_NAME) {
								if (is_ucs2)
									app_name = std::u16string((char16_t*)&data[cur], tag_len / 2);
								else {
									std::string s((char*)&data[cur], tag_len);
									app_name = std::u16string(s.begin(), s.end());
								}
							}
							else if (tag_id == VM_CE_INFO_DEV) {
								if (is_ucs2)
									dev_name = std::u16string((char16_t*)&data[cur], tag_len / 2);
								else {
									std::string s((char*)&data[cur], tag_len);
									dev_name = std::u16string(s.begin(), s.end());
								}
							}
							else if (tag_id == VM_CE_INFO_NAME_LIST && app_name.empty()) {
								if (tag_len > 4) {
									uint32_t str_len = *(uint32_t*)&data[cur + 4];
									if (cur + 8 + str_len <= data.size()) {
										if (is_ucs2)
											app_name = std::u16string((char16_t*)&data[cur + 8], str_len / 2);
										else {
											std::string s((char*)&data[cur + 8], str_len);
											app_name = std::u16string(s.begin(), s.end());
										}
									}
								}
							}
							cur += tag_len;
						}

						auto trim_u16 = [](std::u16string& s) {
							while (!s.empty() && (s.back() == 0 || s.back() == ' ' || s.back() == '\r' || s.back() == '\n'))
								s.pop_back();
						};
						trim_u16(app_name);
						trim_u16(dev_name);
					}
				}
			}

			vxps.push_back({ name, path, img, file_size, app_name, dev_name });
		}

		std::string last_saved = DragAndDrop::get_last_selected_app();
		if (!last_saved.empty()) {
			for (int i = 0; i < (int)vxps.size(); ++i) {
				if (u16_to_u8(vxps[i].name) == last_saved) {
					m_i = i;
					if ((int)vxps.size() * b_h > h) {
						if (b_h * m_i - scroll_pos + b_h > h)
							scroll_pos = b_h * m_i + b_h - h;
						if (b_h * m_i - scroll_pos < 0)
							scroll_pos = b_h * m_i;
					}
					break;
				}
			}
		}
	}

	void update_confirm_sheet(const vxp& app);

	void update_details_sheet(const vxp& app) {
		details_sheet.set_title(u"App Details");
		details_sheet.clear_lines();
		details_sheet.add_line(u"File: " + app.name);
		if (!app.app_name.empty())
			details_sheet.add_line(u"Name: " + app.app_name);
		if (!app.dev_name.empty())
			details_sheet.add_line(u"Dev: " + app.dev_name);
		std::string sz_str = "Size: " + std::to_string((app.file_size + 1023) / 1024) + " KB";
		details_sheet.add_line(std::u16string(sz_str.begin(), sz_str.end()), 0xFFFF, false);
		std::string res_str = "Screen: " + std::to_string(w) + "x" + std::to_string(h);
		details_sheet.add_line(std::u16string(res_str.begin(), res_str.end()), 0xFFFF, false);

		details_sheet.set_left_action(u"Delete", []() {
			update_confirm_sheet(vxps[m_i]);
			confirm_sheet.show();
			reset_marquee();
			draw();
		}, VM_COLOR_888_TO_565(180, 180, 180), false);

		details_sheet.set_right_action(u"Close", []() {
			details_sheet.hide();
			reset_marquee();
			draw();
		}, 0xFFFF, true);

		details_sheet.set_on_dismiss([]() {
			details_sheet.hide();
			reset_marquee();
			draw();
		});
	}

	void update_confirm_sheet(const vxp& app) {
		confirm_sheet.set_title(u"Warning!");
		confirm_sheet.clear_lines();
		confirm_sheet.add_line(u"Delete this application?", 0xFFFF, false);
		confirm_sheet.add_line(app.name, VM_COLOR_888_TO_565(210, 210, 210), true);
		confirm_sheet.add_line(u"Removal is permanent.", VM_COLOR_888_TO_565(160, 160, 160), false);

		confirm_sheet.set_left_action(u"Delete", []() {
			vm_file_delete((VMWSTR)vxps[m_i].path.c_str());
			confirm_sheet.hide();
			details_sheet.hide();
			rescan();
		}, VM_COLOR_888_TO_565(180, 180, 180), true);

		confirm_sheet.set_right_action(u"Cancel", []() {
			confirm_sheet.hide();
			reset_marquee();
			draw();
		}, 0xFFFF, false);

		confirm_sheet.set_on_dismiss([]() {
			confirm_sheet.hide();
			reset_marquee();
			draw();
		});
	}

	void rescan() {
		if (layer_buf) {
			details_sheet.hide();
			confirm_sheet.hide();
			scan();
			if (m_i >= (int)vxps.size())
				m_i = vxps.empty() ? 0 : vxps.size() - 1;
			reset_marquee();
			trigger_scrollbar();
			draw();
		}
	}

	static int g_max_marquee_scroll = 0;

	void draw_marquee_text(int x, int y, int max_w, int line_h, const std::u16string& text, VMUINT16 color) {
		int text_w = vm_graphic_get_string_width((VMWSTR)text.c_str());
		if (text_w > max_w) {
			int overflow = text_w - max_w + 10;
			if (overflow > g_max_marquee_scroll)
				g_max_marquee_scroll = overflow;
			int off = std::min(marquee_offset, overflow);
			int clip_top = std::max(0, y);
			int clip_bottom = std::min(h - 1, y + line_h - 1);
			if (clip_top <= clip_bottom) {
				vm_graphic_set_clip(x, clip_top, x + max_w, clip_bottom);
				vm_graphic_textout(layer_buf, x - off, y, (VMWSTR)text.c_str(), 100, color);
				vm_graphic_reset_clip();
			}
		}
		else {
			vm_graphic_textout(layer_buf, x, y, (VMWSTR)text.c_str(), 100, color);
		}
	}

	void draw() {
		g_max_marquee_scroll = 0;
		vm_graphic_fill_rect(layer_buf, 0, 0, w, h, 0x0000, 0x0000);

		for (int i = 0; i < (int)vxps.size(); ++i) {
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

			if (i == m_i && !details_sheet.is_open() && !confirm_sheet.is_open())
				draw_marquee_text(text_x, text_y, max_text_w, b_h, vxps[i].name, 0xFFFF);
			else
				vm_graphic_textout(layer_buf, text_x, text_y, (VMWSTR)vxps[i].name.c_str(), 100, 0xFFFF);

			vm_graphic_line(layer_buf, 0, y + b_h - 1, w, y + b_h - 1, 0xFFFF);
		}

		int total_h = (int)vxps.size() * b_h;
		if (total_h > h && (vm_get_tick_count() - last_scroll_time < 2000)) {
			scrollbar_visible = true;
			int max_scroll = total_h - h;
			int thumb_h = (h * h) / total_h;
			if (thumb_h < 16) thumb_h = 16;
			if (thumb_h > h) thumb_h = h;
			int thumb_y = (max_scroll > 0) ? (scroll_pos * (h - thumb_h)) / max_scroll : 0;
			if (thumb_y < 0) thumb_y = 0;
			if (thumb_y + thumb_h > h) thumb_y = h - thumb_h;

			VMUINT16 sb_col = VM_COLOR_888_TO_565(190, 190, 190);
			int sb_w = 3;
			for (int sy = thumb_y; sy < thumb_y + thumb_h; ++sy) {
				for (int sx = w - sb_w; sx < w; ++sx) {
					VMUINT16* ptr = (VMUINT16*)layer_buf + sy * w + sx;
					*ptr = ((*ptr & 0xF7DE) >> 1) + ((sb_col & 0xF7DE) >> 1);
				}
			}
		}
		else {
			scrollbar_visible = false;
		}

		if (!vxps.size()) {
			vm_graphic_textout(layer_buf, 0, 0, vm_ucs2_string((VMSTR)"No files in MRE folder."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, c_h + 2, vm_ucs2_string((VMSTR)"Put them in fs/e/mre."), 100, 0xFFFF);
			vm_graphic_textout(layer_buf, 0, c_h*2 + 2, vm_ucs2_string((VMSTR)"Or drag them here to import..."), 100, 0xFFFF);
		}

		// Render modal bottom sheets
		if (details_sheet.is_open())
			details_sheet.draw(layer_buf, w, h, c_h, marquee_offset, g_max_marquee_scroll);

		if (confirm_sheet.is_open()) {
			g_max_marquee_scroll = 0;
			confirm_sheet.draw(layer_buf, w, h, c_h, marquee_offset, g_max_marquee_scroll);
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

		if (scrollbar_visible && vm_get_tick_count() - last_scroll_time >= 2000) {
			scrollbar_visible = false;
			draw();
			return;
		}

		if (g_max_marquee_scroll <= 0) {
			if (marquee_offset != 0) {
				marquee_offset = 0;
				draw();
			}
			return;
		}

		if (marquee_state == 0) {
			if (--marquee_pause_ticks <= 0)
				marquee_state = 1;
		}
		else if (marquee_state == 1) {
			marquee_offset++;
			if (marquee_offset >= g_max_marquee_scroll) {
				marquee_offset = g_max_marquee_scroll;
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
		if (confirm_sheet.is_open()) {
			confirm_sheet.handle_key(event, keycode);
			return;
		}

		if (details_sheet.is_open()) {
			details_sheet.handle_key(event, keycode);
			return;
		}

		int old_m_i = m_i;
		if (vxps.size() && event == VM_KEY_EVENT_UP) {
			trigger_scrollbar();
			switch (keycode) {
			case VM_KEY_UP:
				if (--m_i < 0)
					m_i = (int)vxps.size() - 1;
				break;
			case VM_KEY_DOWN:
				if (++m_i >= (int)vxps.size())
					m_i = 0;
				break;
			case VM_KEY_LEFT_SOFTKEY:
				if (!vxps.empty()) {
					update_details_sheet(vxps[m_i]);
					details_sheet.show();
					reset_marquee();
					draw();
				}
				return;
			case VM_KEY_OK:
				DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
				vm_start_app((VMWSTR)vxps[m_i].path.c_str(), 0, 0);
				break;
			case VM_KEY_NUM1:
			case VM_KEY_NUM2:
			case VM_KEY_NUM3:
			case VM_KEY_NUM4:
			case VM_KEY_NUM5:
			case VM_KEY_NUM6:
			case VM_KEY_NUM7:
			case VM_KEY_NUM8:
			case VM_KEY_NUM9:
			case VM_KEY_NUM0: {
				const char* chars = get_t9_chars(keycode);
				if (*chars && !vxps.empty()) {
					for (int step = 1; step <= (int)vxps.size(); ++step) {
						int idx = (m_i + step) % (int)vxps.size();
						if (vxps[idx].name.empty())
							continue;
						char first = (char)std::tolower((char)vxps[idx].name[0]);
						if (std::strchr(chars, first)) {
							m_i = idx;
							break;
						}
					}
				}
				break;
			}
			}
		}

		if (m_i != old_m_i) {
			reset_marquee();
			if (m_i >= 0 && m_i < (int)vxps.size())
				DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
		}

		if ((int)vxps.size() * b_h > h) {
			if (b_h * m_i - scroll_pos + b_h > h)
				scroll_pos = b_h * m_i + b_h - h;

			if (b_h * m_i - scroll_pos < 0)
				scroll_pos = b_h * m_i;
		}

		draw();
	}

	void pen_handler(VMINT event, VMINT x, VMINT y) {
		if (confirm_sheet.is_open()) {
			confirm_sheet.handle_pen(event, x, y, w, h, c_h);
			return;
		}

		if (details_sheet.is_open()) {
			details_sheet.handle_pen(event, x, y, w, h, c_h);
			return;
		}

		int old_m_i = m_i;
		trigger_scrollbar();
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
			case VM_PEN_EVENT_DOUBLE_CLICK:
				if (touched) {
					scroll_pos += touch_last_y - y;
					touch_last_y = y;

					if (scroll_pos < 0)
						scroll_pos = 0;

					if (scroll_pos > (int)vxps.size() * b_h - h)
						scroll_pos = (int)vxps.size() * b_h - h;
				}
				break;
			case VM_PEN_EVENT_LONG_TAP: {
				int item = (y + scroll_pos) / b_h;
				if (item >= 0 && item < (int)vxps.size()) {
					m_i = item;
					update_details_sheet(vxps[m_i]);
					details_sheet.show();
					reset_marquee();
					DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
					draw();
				}
				break;
			}
			case VM_PEN_EVENT_RELEASE:
			case VM_PEN_EVENT_ABORT:
				touched = false;
				if (std::abs(touch_start_y - y) < b_h / 2 && vm_get_tick_count() - touch_time < 150) {
					DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
					vm_start_app((VMWSTR)vxps[m_i].path.c_str(), 0, 0);
				}
				break;
		}

		if (m_i < 0)
			m_i = (int)vxps.size() - 1;

		if (m_i >= (int)vxps.size())
			m_i = 0;

		if (m_i != old_m_i) {
			reset_marquee();
			if (m_i >= 0 && m_i < (int)vxps.size())
				DragAndDrop::set_last_selected_app(u16_to_u8(vxps[m_i].name));
		}

		draw();
	}
}