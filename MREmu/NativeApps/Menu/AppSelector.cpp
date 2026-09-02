#include "AppSelector.h"
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

	bool show_details = false;
	bool show_delete_confirm = false;

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

	void rescan() {
		if (layer_buf) {
			scan();
			if (m_i >= (int)vxps.size())
				m_i = vxps.empty() ? 0 : vxps.size() - 1;
			reset_marquee();
			trigger_scrollbar();
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

		if (show_details && m_i >= 0 && m_i < (int)vxps.size()) {
			const auto& app = vxps[m_i];
			int lines_count = 3 + (!app.app_name.empty() ? 1 : 0) + (!app.dev_name.empty() ? 1 : 0);
			int sheet_h = (c_h + 8) + lines_count * (c_h + 3) + (c_h + 14);
			if (sheet_h > h - 10)
				sheet_h = h - 10;
			int sheet_y = h - sheet_h;

			for (int sy = 0; sy < sheet_y; ++sy) {
				for (int sx = 0; sx < w; ++sx) {
					VMUINT16* ptr = (VMUINT16*)layer_buf + sy * w + sx;
					*ptr = (*ptr & 0xF7DE) >> 1;
				}
			}

			VMUINT16 bg_col = VM_COLOR_888_TO_565(20, 20, 20);
			VMUINT16 border_col = VM_COLOR_888_TO_565(80, 80, 80);
			VMUINT16 header_bg = VM_COLOR_888_TO_565(36, 36, 36);
			VMUINT16 div_col = VM_COLOR_888_TO_565(55, 55, 55);
			VMUINT16 txt_dim = VM_COLOR_888_TO_565(180, 180, 180);

			vm_graphic_fill_rect(layer_buf, 0, sheet_y, w, sheet_h, bg_col, bg_col);
			vm_graphic_line(layer_buf, 0, sheet_y, w, sheet_y, border_col);

			int title_h = c_h + 6;
			vm_graphic_fill_rect(layer_buf, 0, sheet_y + 1, w, title_h, header_bg, header_bg);
			vm_graphic_line(layer_buf, 0, sheet_y + title_h + 1, w, sheet_y + title_h + 1, div_col);
			vm_graphic_textout(layer_buf, 8, sheet_y + 3, (VMWSTR)u"App Details", 100, 0xFFFF);

			int ty = sheet_y + title_h + 6;
			int pad_x = 8;

			std::u16string f_line = u"File: " + app.name;
			vm_graphic_textout(layer_buf, pad_x, ty, (VMWSTR)f_line.c_str(), 100, 0xFFFF);
			ty += c_h + 3;

			if (!app.app_name.empty()) {
				std::u16string n_line = u"Name: " + app.app_name;
				vm_graphic_textout(layer_buf, pad_x, ty, (VMWSTR)n_line.c_str(), 100, 0xFFFF);
				ty += c_h + 3;
			}

			if (!app.dev_name.empty()) {
				std::u16string d_line = u"Dev: " + app.dev_name;
				vm_graphic_textout(layer_buf, pad_x, ty, (VMWSTR)d_line.c_str(), 100, 0xFFFF);
				ty += c_h + 3;
			}

			std::string sz_str = "Size: " + std::to_string((app.file_size + 1023) / 1024) + " KB";
			std::u16string sz_line(sz_str.begin(), sz_str.end());
			vm_graphic_textout(layer_buf, pad_x, ty, (VMWSTR)sz_line.c_str(), 100, 0xFFFF);
			ty += c_h + 3;

			std::string res_str = "Screen: " + std::to_string(w) + "x" + std::to_string(h);
			std::u16string res_line(res_str.begin(), res_str.end());
			vm_graphic_textout(layer_buf, pad_x, ty, (VMWSTR)res_line.c_str(), 100, 0xFFFF);

			int act_y = h - c_h - 7;
			vm_graphic_line(layer_buf, 0, act_y - 3, w, act_y - 3, div_col);
			vm_graphic_textout(layer_buf, 8, act_y, (VMWSTR)u"Delete", 100, txt_dim);
			int close_w = vm_graphic_get_string_width((VMWSTR)u"Close");
			vm_graphic_textout(layer_buf, w - close_w - 8, act_y, (VMWSTR)u"Close", 100, 0xFFFF);
		}
		else if (show_delete_confirm && m_i >= 0 && m_i < (int)vxps.size()) {
			const auto& app = vxps[m_i];
			int sheet_h = (c_h + 6) + 3 * (c_h + 3) + (c_h + 16);
			if (sheet_h > h - 10)
				sheet_h = h - 10;
			int sheet_y = h - sheet_h;

			for (int sy = 0; sy < sheet_y; ++sy) {
				for (int sx = 0; sx < w; ++sx) {
					VMUINT16* ptr = (VMUINT16*)layer_buf + sy * w + sx;
					*ptr = (*ptr & 0xF7DE) >> 1;
				}
			}

			VMUINT16 bg_col = VM_COLOR_888_TO_565(20, 20, 20);
			VMUINT16 border_col = VM_COLOR_888_TO_565(90, 90, 90);
			VMUINT16 header_bg = VM_COLOR_888_TO_565(36, 36, 36);
			VMUINT16 div_col = VM_COLOR_888_TO_565(55, 55, 55);

			vm_graphic_fill_rect(layer_buf, 0, sheet_y, w, sheet_h, bg_col, bg_col);
			vm_graphic_line(layer_buf, 0, sheet_y, w, sheet_y, border_col);

			int title_h = c_h + 6;
			vm_graphic_fill_rect(layer_buf, 0, sheet_y + 1, w, title_h, header_bg, header_bg);
			vm_graphic_line(layer_buf, 0, sheet_y + title_h + 1, w, sheet_y + title_h + 1, div_col);
			vm_graphic_textout(layer_buf, 8, sheet_y + 3, (VMWSTR)u"Warning", 100, 0xFFFF);

			int ty = sheet_y + title_h + 6;
			vm_graphic_textout(layer_buf, 8, ty, (VMWSTR)u"Delete this application?", 100, 0xFFFF);
			ty += c_h + 3;
			vm_graphic_textout(layer_buf, 8, ty, (VMWSTR)app.name.c_str(), 100, VM_COLOR_888_TO_565(210, 210, 210));
			ty += c_h + 3;
			vm_graphic_textout(layer_buf, 8, ty, (VMWSTR)u"File will be removed permanently.", 100, VM_COLOR_888_TO_565(160, 160, 160));

			int act_y = h - c_h - 7;
			vm_graphic_line(layer_buf, 0, act_y - 3, w, act_y - 3, div_col);
			vm_graphic_textout(layer_buf, 8, act_y, (VMWSTR)u"Delete", 100, VM_COLOR_888_TO_565(220, 220, 220));
			int cancel_w = vm_graphic_get_string_width((VMWSTR)u"Cancel");
			vm_graphic_textout(layer_buf, w - cancel_w - 8, act_y, (VMWSTR)u"Cancel", 100, 0xFFFF);
		}

		vm_graphic_flush_layer(&layer_h, 1);
	}

	void timer_cb(VMINT tid) {
		if (!layer_buf || vxps.empty() || touched || show_details || show_delete_confirm)
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
		if (show_delete_confirm) {
			if (event == VM_KEY_EVENT_UP) {
				if (keycode == VM_KEY_OK || keycode == VM_KEY_LEFT_SOFTKEY) {
					vm_file_delete((VMWSTR)vxps[m_i].path.c_str());
					show_delete_confirm = false;
					show_details = false;
					rescan();
					return;
				}
				else if (keycode == VM_KEY_BACK || keycode == VM_KEY_RIGHT_SOFTKEY || keycode == VM_KEY_CLEAR) {
					show_delete_confirm = false;
					draw();
					return;
				}
			}
			return;
		}

		if (show_details) {
			if (event == VM_KEY_EVENT_UP) {
				if (keycode == VM_KEY_LEFT_SOFTKEY || keycode == VM_KEY_NUM1) {
					show_delete_confirm = true;
					draw();
					return;
				}
				else if (keycode == VM_KEY_OK || keycode == VM_KEY_RIGHT_SOFTKEY || keycode == VM_KEY_BACK || keycode == VM_KEY_CLEAR) {
					show_details = false;
					draw();
					return;
				}
			}
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
				show_details = true;
				draw();
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
		if (show_delete_confirm) {
			if (event == VM_PEN_EVENT_TAP) {
				int sheet_h = (c_h + 6) + 3 * (c_h + 3) + (c_h + 16);
				if (sheet_h > h - 10)
					sheet_h = h - 10;
				int sheet_y = h - sheet_h;

				if (y < sheet_y) {
					show_delete_confirm = false;
					draw();
					return;
				}

				int act_y = h - c_h - 12;
				if (y >= act_y) {
					if (x < w / 2) {
						vm_file_delete((VMWSTR)vxps[m_i].path.c_str());
						show_delete_confirm = false;
						show_details = false;
						rescan();
						return;
					}
					else {
						show_delete_confirm = false;
						draw();
						return;
					}
				}
			}
			return;
		}

		if (show_details) {
			if (event == VM_PEN_EVENT_TAP) {
				const auto& app = vxps[m_i];
				int lines_count = 3 + (!app.app_name.empty() ? 1 : 0) + (!app.dev_name.empty() ? 1 : 0);
				int sheet_h = (c_h + 8) + lines_count * (c_h + 3) + (c_h + 14);
				if (sheet_h > h - 10)
					sheet_h = h - 10;
				int sheet_y = h - sheet_h;

				if (y < sheet_y) {
					show_details = false;
					draw();
					return;
				}

				int act_y = h - c_h - 12;
				if (y >= act_y) {
					if (x < w / 2) {
						show_delete_confirm = true;
						draw();
						return;
					}
					else {
						show_details = false;
						draw();
						return;
					}
				}
			}
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
					show_details = true;
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