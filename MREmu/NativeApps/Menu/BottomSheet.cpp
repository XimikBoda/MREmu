#include "BottomSheet.h"
#include <algorithm>

namespace NativeApps {

int BottomSheet::calculate_height(int char_h, int screen_h) const {
	int action_h = (!m_left_action.empty() || !m_right_action.empty()) ? (char_h + 16) : (char_h + 8);
	int h = (char_h + 6) + (int)m_lines.size() * (char_h + 3) + action_h;
	if (h > screen_h - 10)
		h = screen_h - 10;
	return h;
}

void BottomSheet::draw(VMUINT8* layer_buf, int screen_w, int screen_h, int char_h,
	int marquee_offset, int& out_max_scroll) {
	if (!m_open || !layer_buf)
		return;

	int sheet_h = calculate_height(char_h, screen_h);
	int sheet_y = screen_h - sheet_h;

	// Dim background above sheet
	for (int sy = 0; sy < sheet_y; ++sy) {
		for (int sx = 0; sx < screen_w; ++sx) {
			VMUINT16* ptr = (VMUINT16*)layer_buf + sy * screen_w + sx;
			*ptr = (*ptr & 0xF7DE) >> 1;
		}
	}

	VMUINT16 bg_col = VM_COLOR_888_TO_565(20, 20, 20);
	VMUINT16 border_col = VM_COLOR_888_TO_565(90, 90, 90);
	VMUINT16 header_bg = VM_COLOR_888_TO_565(36, 36, 36);
	VMUINT16 div_col = VM_COLOR_888_TO_565(55, 55, 55);

	// Sheet background and top border line
	vm_graphic_fill_rect(layer_buf, 0, sheet_y, screen_w, sheet_h, bg_col, bg_col);
	vm_graphic_line(layer_buf, 0, sheet_y, screen_w, sheet_y, border_col);

	// Title bar
	int title_h = char_h + 6;
	vm_graphic_fill_rect(layer_buf, 0, sheet_y + 1, screen_w, title_h, header_bg, header_bg);
	vm_graphic_line(layer_buf, 0, sheet_y + title_h + 1, screen_w, sheet_y + title_h + 1, div_col);
	if (!m_title.empty())
		vm_graphic_textout(layer_buf, 8, sheet_y + 3, (VMWSTR)m_title.c_str(), 100, 0xFFFF);

	// Content lines
	int ty = sheet_y + title_h + 6;
	int max_text_w = screen_w - 16;
	for (const auto& line : m_lines) {
		int text_w = vm_graphic_get_string_width((VMWSTR)line.text.c_str());
		if (line.marquee && text_w > max_text_w) {
			int overflow = text_w - max_text_w + 10;
			if (overflow > out_max_scroll)
				out_max_scroll = overflow;
			int off = std::min(marquee_offset, overflow);
			int clip_top = std::max(0, ty);
			int clip_bottom = std::min(screen_h - 1, ty + char_h);
			if (clip_top <= clip_bottom) {
				vm_graphic_set_clip(8, clip_top, 8 + max_text_w, clip_bottom);
				vm_graphic_textout(layer_buf, 8 - off, ty, (VMWSTR)line.text.c_str(), 100, line.color);
				vm_graphic_reset_clip();
			}
		}
		else {
			vm_graphic_textout(layer_buf, 8, ty, (VMWSTR)line.text.c_str(), 100, line.color);
		}
		ty += char_h + 3;
	}

	// Bottom action bar (if actions are defined)
	if (!m_left_action.empty() || !m_right_action.empty()) {
		int act_y = screen_h - char_h - 7;
		vm_graphic_line(layer_buf, 0, act_y - 3, screen_w, act_y - 3, div_col);

		VMUINT16 highlight_col = VM_COLOR_888_TO_565(150, 150, 150);

		if (m_left_pressed && !m_left_action.empty()) {
			int bx1 = 0;
			int bx2 = screen_w / 2 - 1;
			int by1 = act_y - 2;
			int by2 = screen_h - 1;
			for (int sy = by1; sy <= by2; ++sy) {
				for (int sx = bx1; sx <= bx2; ++sx) {
					VMUINT16* ptr = (VMUINT16*)layer_buf + sy * screen_w + sx;
					*ptr = ((*ptr & 0xF7DE) >> 1) + ((highlight_col & 0xF7DE) >> 1);
				}
			}
		}

		if (m_right_pressed && !m_right_action.empty()) {
			int bx1 = screen_w / 2;
			int bx2 = screen_w - 1;
			int by1 = act_y - 2;
			int by2 = screen_h - 1;
			for (int sy = by1; sy <= by2; ++sy) {
				for (int sx = bx1; sx <= bx2; ++sx) {
					VMUINT16* ptr = (VMUINT16*)layer_buf + sy * screen_w + sx;
					*ptr = ((*ptr & 0xF7DE) >> 1) + ((highlight_col & 0xF7DE) >> 1);
				}
			}
		}

		if (!m_left_action.empty()) {
			VMUINT16 col = m_left_pressed ? 0xFFFF : m_left_action_color;
			vm_graphic_textout(layer_buf, 8, act_y, (VMWSTR)m_left_action.c_str(), 100, col);
		}

		if (!m_right_action.empty()) {
			VMUINT16 col = m_right_pressed ? 0xFFFF : m_right_action_color;
			int rw = vm_graphic_get_string_width((VMWSTR)m_right_action.c_str());
			vm_graphic_textout(layer_buf, screen_w - rw - 8, act_y, (VMWSTR)m_right_action.c_str(), 100, col);
		}
	}
}

bool BottomSheet::handle_key(VMINT event, VMINT keycode) {
	if (!m_open)
		return false;

	if (event == VM_KEY_EVENT_DOWN) {
		if (keycode == VM_KEY_LEFT_SOFTKEY || (m_left_ok_triggers && keycode == VM_KEY_OK) ||
			(keycode == VM_KEY_NUM1 && !m_left_action.empty())) {
			m_left_pressed = true;
			return true;
		}
		else if (keycode == VM_KEY_RIGHT_SOFTKEY || keycode == VM_KEY_BACK || keycode == VM_KEY_CLEAR ||
			(m_right_ok_triggers && keycode == VM_KEY_OK)) {
			m_right_pressed = true;
			return true;
		}
	}
	else if (event == VM_KEY_EVENT_UP) {
		if (keycode == VM_KEY_LEFT_SOFTKEY || (m_left_ok_triggers && keycode == VM_KEY_OK) ||
			(keycode == VM_KEY_NUM1 && !m_left_action.empty())) {
			m_left_pressed = false;
			if (m_on_left)
				m_on_left();
			return true;
		}
		else if (keycode == VM_KEY_RIGHT_SOFTKEY || keycode == VM_KEY_BACK || keycode == VM_KEY_CLEAR ||
			(m_right_ok_triggers && keycode == VM_KEY_OK)) {
			m_right_pressed = false;
			if (m_on_right)
				m_on_right();
			else if (m_on_dismiss)
				m_on_dismiss();
			else
				hide();
			return true;
		}
		m_left_pressed = false;
		m_right_pressed = false;
	}
	return true;
}

bool BottomSheet::handle_pen(VMINT event, VMINT x, VMINT y, int screen_w, int screen_h, int char_h) {
	if (!m_open)
		return false;

	int sheet_h = calculate_height(char_h, screen_h);
	int sheet_y = screen_h - sheet_h;
	int act_y = screen_h - char_h - 12;

	if (event == VM_PEN_EVENT_TAP) {
		if (y < sheet_y) {
			m_left_pressed = false;
			m_right_pressed = false;
			if (m_on_dismiss)
				m_on_dismiss();
			else if (m_on_right)
				m_on_right();
			else
				hide();
			return true;
		}

		if (y >= act_y) {
			if (x < screen_w / 2) {
				m_left_pressed = true;
				m_right_pressed = false;
			}
			else {
				m_left_pressed = false;
				m_right_pressed = true;
			}
			return true;
		}
		else {
			m_left_pressed = false;
			m_right_pressed = false;
			return true;
		}
	}
	else if (event == VM_PEN_EVENT_MOVE) {
		if (m_left_pressed || m_right_pressed) {
			if (y >= act_y) {
				if (x < screen_w / 2) {
					m_left_pressed = true;
					m_right_pressed = false;
				}
				else {
					m_left_pressed = false;
					m_right_pressed = true;
				}
			}
			else {
				m_left_pressed = false;
				m_right_pressed = false;
			}
			return true;
		}
	}
	else if (event == VM_PEN_EVENT_RELEASE) {
		if (m_left_pressed) {
			m_left_pressed = false;
			if (m_on_left)
				m_on_left();
			return true;
		}
		else if (m_right_pressed) {
			m_right_pressed = false;
			if (m_on_right)
				m_on_right();
			else if (m_on_dismiss)
				m_on_dismiss();
			else
				hide();
			return true;
		}
	}
	else if (event == VM_PEN_EVENT_ABORT) {
		m_left_pressed = false;
		m_right_pressed = false;
		return true;
	}

	return true;
}

} // namespace NativeApps
