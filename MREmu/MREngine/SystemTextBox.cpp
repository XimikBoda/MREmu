#include "SystemTextBox.h"
#include "Graphic.h"
#include "../Memory.h"
#include "../AppManager.h"
#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>

void raw_textout_to_buf(uint16_t* buf, int screen_w, int screen_h, int x, int y,
	const std::u16string& str, uint16_t color, int clip_y1, int clip_y2);

namespace MREngine {

bool SystemTextBox::m_is_open = false;
std::u16string SystemTextBox::m_title = u"Search";
std::u16string SystemTextBox::m_text = u"";
std::u16string SystemTextBox::m_original_text = u"";
size_t SystemTextBox::m_cursor_pos = 0;
size_t SystemTextBox::m_selection_anchor = 0;
bool SystemTextBox::m_is_selecting_with_mouse = false;
size_t SystemTextBox::m_max_len = 250;
int SystemTextBox::m_input_method = 1;
int SystemTextBox::m_scroll_line = 0;

uint32_t SystemTextBox::m_callback = 0;
std::function<void(bool, const std::u16string&)> SystemTextBox::m_native_callback = nullptr;

std::u16string SystemTextBox::m_left_action = u"Done";
std::u16string SystemTextBox::m_right_action = u"Cancel";
bool SystemTextBox::m_left_pressed = false;
bool SystemTextBox::m_right_pressed = false;
bool SystemTextBox::m_ok_pressed = false;
bool SystemTextBox::m_left_softkey_pressed = false;

sf::Clock SystemTextBox::m_blink_clock;
sf::Clock SystemTextBox::m_open_clock;

bool SystemTextBox::m_bg_captured = false;
std::vector<uint16_t> SystemTextBox::m_bg_buffer;

static void* g_input_text_buf = nullptr;
static uint32_t g_input_text_emu_addr = 0;

static void raw_fill_rect(uint16_t* buf, int screen_w, int screen_h, int x, int y, int rw, int rh, uint16_t col) {
	if (!buf) return;
	int x1 = std::max(0, x);
	int y1 = std::max(0, y);
	int x2 = std::min(screen_w, x + rw);
	int y2 = std::min(screen_h, y + rh);
	for (int sy = y1; sy < y2; ++sy) {
		for (int sx = x1; sx < x2; ++sx) {
			buf[sy * screen_w + sx] = col;
		}
	}
}

static void raw_line(uint16_t* buf, int screen_w, int screen_h, int x1, int y1, int x2, int y2, uint16_t col) {
	if (!buf) return;
	if (y1 == y2) {
		int start_x = std::max(0, std::min(x1, x2));
		int end_x = std::min(screen_w - 1, std::max(x1, x2));
		if (y1 >= 0 && y1 < screen_h) {
			for (int sx = start_x; sx <= end_x; ++sx)
				buf[y1 * screen_w + sx] = col;
		}
		return;
	}
	if (x1 == x2) {
		int start_y = std::max(0, std::min(y1, y2));
		int end_y = std::min(screen_h - 1, std::max(y1, y2));
		if (x1 >= 0 && x1 < screen_w) {
			for (int sy = start_y; sy <= end_y; ++sy)
				buf[sy * screen_w + x1] = col;
		}
		return;
	}
	int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
	int err = dx + dy, e2;
	while (true) {
		if (x1 >= 0 && x1 < screen_w && y1 >= 0 && y1 < screen_h)
			buf[y1 * screen_w + x1] = col;
		if (x1 == x2 && y1 == y2) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x1 += sx; }
		if (e2 <= dx) { err += dx; y1 += sy; }
	}
}

void SystemTextBox::init() {
	m_is_open = false;
	m_text.clear();
	m_original_text.clear();
	m_cursor_pos = 0;
	m_selection_anchor = 0;
	m_is_selecting_with_mouse = false;
	m_scroll_line = 0;
	m_left_pressed = false;
	m_right_pressed = false;
	m_ok_pressed = false;
	m_left_softkey_pressed = false;
	m_bg_captured = false;

	if (!g_input_text_buf) {
		g_input_text_buf = Memory::shared_malloc(8192, false);
		g_input_text_emu_addr = (uint32_t)ADDRESS_TO_EMU(g_input_text_buf);
	}
}

void SystemTextBox::deinit() {
	m_is_open = false;
	m_bg_captured = false;
	m_bg_buffer.clear();
	m_is_selecting_with_mouse = false;
	g_input_text_buf = nullptr;
	g_input_text_emu_addr = 0;
}

bool SystemTextBox::is_open() {
	return m_is_open;
}

bool SystemTextBox::delete_selection() {
	if (!has_selection())
		return false;
	size_t sel_start, sel_end;
	get_selection_range(sel_start, sel_end);
	m_text.erase(sel_start, sel_end - sel_start);
	m_cursor_pos = sel_start;
	m_selection_anchor = m_cursor_pos;
	m_blink_clock.restart();
	return true;
}

static void copy_to_emu_buffer(const std::u16string& text) {
	if (!g_input_text_buf) {
		g_input_text_buf = Memory::shared_malloc(8192, false);
		g_input_text_emu_addr = (uint32_t)ADDRESS_TO_EMU(g_input_text_buf);
	}
	size_t copy_len = std::min(text.length(), (size_t)2046);
	char16_t* dst = (char16_t*)g_input_text_buf;
	if (copy_len > 0) {
		memcpy(dst, text.c_str(), copy_len * sizeof(char16_t));
	}
	dst[copy_len] = 0;
}

int SystemTextBox::open(const VMWSTR def_str, int max_size, int input_method, uint32_t callback, const std::u16string& title) {
	init();
	m_is_open = true;
	if (!title.empty())
		m_title = title;
	else if (m_title.empty())
		m_title = u"Search";

	if (def_str)
		m_text = (char16_t*)def_str;
	else
		m_text.clear();

	m_original_text = m_text;
	m_cursor_pos = m_text.length();
	m_selection_anchor = m_cursor_pos;
	m_is_selecting_with_mouse = false;
	m_max_len = max_size > 0 ? (size_t)max_size : 250;
	m_input_method = input_method;
	m_callback = callback;
	m_native_callback = nullptr;
	m_scroll_line = 0;
	m_left_pressed = false;
	m_right_pressed = false;
	m_blink_clock.restart();
	m_open_clock.restart();

	spdlog::info("[SystemTextBox] Opened for input, max_len={}, callback=0x{:x}, buf_addr=0x{:x}",
		m_max_len, callback, g_input_text_emu_addr);
	return 0;
}

void SystemTextBox::open_native(const std::u16string& def_str, int max_size,
	std::function<void(bool ok, const std::u16string& text)> cb, const std::u16string& title) {
	init();
	m_is_open = true;
	m_title = !title.empty() ? title : u"Search";
	m_text = def_str;
	m_original_text = def_str;
	m_cursor_pos = m_text.length();
	m_selection_anchor = m_cursor_pos;
	m_is_selecting_with_mouse = false;
	m_max_len = max_size > 0 ? (size_t)max_size : 250;
	m_input_method = 1;
	m_callback = 0;
	m_native_callback = std::move(cb);
	m_scroll_line = 0;
	m_left_pressed = false;
	m_right_pressed = false;
	m_blink_clock.restart();
	m_open_clock.restart();
}

void SystemTextBox::set_title(const std::u16string& title) {
	m_title = title;
}

void SystemTextBox::move_cursor_to_start() {
	m_cursor_pos = 0;
	m_blink_clock.restart();
}

void SystemTextBox::close_by_app() {
	if (!m_is_open) return;
	m_is_open = false;
	copy_to_emu_buffer(m_text);
	if (m_bg_captured && graphic) {
		memcpy(graphic->screen.data(), m_bg_buffer.data(), m_bg_buffer.size() * sizeof(uint16_t));
		graphic->screen_changed = true;
	}
	m_bg_captured = false;
	if (m_callback) {
		add_input_event(m_callback, VM_INPUT_CLOSE, g_input_text_emu_addr);
	}
	if (m_native_callback) {
		m_native_callback(false, m_text);
	}
}

void SystemTextBox::submit() {
	if (!m_is_open) return;
	m_is_open = false;
	copy_to_emu_buffer(m_text);
	if (m_bg_captured && graphic) {
		memcpy(graphic->screen.data(), m_bg_buffer.data(), m_bg_buffer.size() * sizeof(uint16_t));
		graphic->screen_changed = true;
	}
	m_bg_captured = false;
	spdlog::info("[SystemTextBox] Submitted input, text length: {}, buf_addr=0x{:x}",
		m_text.length(), g_input_text_emu_addr);
	if (m_callback) {
		add_input_event(m_callback, VM_INPUT_OK, g_input_text_emu_addr);
	}
	if (m_native_callback) {
		m_native_callback(true, m_text);
	}
}

void SystemTextBox::cancel() {
	if (!m_is_open) return;
	m_is_open = false;
	copy_to_emu_buffer(m_original_text);
	if (m_bg_captured && graphic) {
		memcpy(graphic->screen.data(), m_bg_buffer.data(), m_bg_buffer.size() * sizeof(uint16_t));
		graphic->screen_changed = true;
	}
	m_bg_captured = false;
	spdlog::info("[SystemTextBox] Cancelled input");
	if (m_callback) {
		add_input_event(m_callback, VM_INPUT_CANCEL, g_input_text_emu_addr);
	}
	if (m_native_callback) {
		m_native_callback(false, m_original_text);
	}
}

std::vector<WrappedLine> SystemTextBox::compute_wrapped_lines(const std::u16string& text, int max_w) {
	std::vector<WrappedLine> lines;
	if (text.empty()) {
		lines.push_back({ 0, 0 });
		return lines;
	}

	size_t start = 0;
	while (start < text.length()) {
		size_t cur = start;
		size_t last_space = std::u16string::npos;
		int cur_w = 0;

		while (cur < text.length()) {
			char16_t ch = text[cur];
			if (ch == u'\n') {
				lines.push_back({ start, cur - start });
				start = cur + 1;
				break;
			}

			int ch_w = vm_graphic_get_character_width(ch);
			if (cur_w + ch_w > max_w && cur > start) {
				if (last_space != std::u16string::npos && last_space >= start) {
					lines.push_back({ start, last_space - start });
					start = last_space + 1;
				}
				else {
					lines.push_back({ start, cur - start });
					start = cur;
				}
				break;
			}

			if (ch == u' ') {
				last_space = cur;
			}

			cur_w += ch_w;
			cur++;
		}

		if (cur >= text.length()) {
			lines.push_back({ start, cur - start });
			break;
		}
	}

	if (!text.empty() && text.back() == u'\n') {
		lines.push_back({ text.length(), 0 });
	}

	return lines;
}

void SystemTextBox::get_cursor_coord(const std::u16string& text, const std::vector<WrappedLine>& lines, size_t cursor_pos, int& out_line, int& out_x) {
	out_line = 0;
	out_x = 0;
	if (lines.empty()) return;

	for (size_t i = 0; i < lines.size(); ++i) {
		size_t l_start = lines[i].start_idx;
		size_t l_len = lines[i].length;
		if (cursor_pos >= l_start && (cursor_pos <= l_start + l_len || i == lines.size() - 1)) {
			out_line = (int)i;
			size_t sub_len = (cursor_pos > l_start) ? (cursor_pos - l_start) : 0;
			if (sub_len > l_len) sub_len = l_len;
			std::u16string pref = text.substr(l_start, sub_len);
			out_x = vm_graphic_get_string_width((VMWSTR)pref.c_str());
			break;
		}
	}
}

size_t SystemTextBox::get_pos_at_line_x(const std::u16string& text, const std::vector<WrappedLine>& lines, int line_idx, int target_x) {
	if (lines.empty()) return 0;
	if (line_idx < 0) line_idx = 0;
	if (line_idx >= (int)lines.size()) line_idx = (int)lines.size() - 1;

	const auto& line = lines[line_idx];
	if (line.length == 0 || target_x <= 0)
		return line.start_idx;

	int accum_x = 0;
	for (size_t i = 0; i < line.length; ++i) {
		char16_t ch = text[line.start_idx + i];
		int ch_w = vm_graphic_get_character_width(ch);
		if (target_x < accum_x + ch_w / 2) {
			return line.start_idx + i;
		}
		accum_x += ch_w;
	}
	return line.start_idx + line.length;
}

void SystemTextBox::draw(VMUINT8* screen_buf, int screen_w, int screen_h) {
	if (!m_is_open || !screen_buf)
		return;

	uint16_t* sbuf = (uint16_t*)screen_buf;

	int char_h = vm_graphic_get_character_height();
	if (char_h <= 0) char_h = 16;

	// Bottom sheet dimensions: covers bottom ~60% of the screen
	int sheet_h = std::min(screen_h - 20, std::max(160, screen_h * 3 / 5));
	int sheet_y = screen_h - sheet_h;

	// Capture clean background snapshot on the first draw
	int total_pixels = screen_w * screen_h;
	if (!m_bg_captured || (int)m_bg_buffer.size() != total_pixels) {
		m_bg_buffer.assign(sbuf, sbuf + total_pixels);
		m_bg_captured = true;
	}

	// Restore background from snapshot above sheet and dim it (50% brightness)
	for (int sy = 0; sy < sheet_y; ++sy) {
		for (int sx = 0; sx < screen_w; ++sx) {
			VMUINT16 src_px = m_bg_buffer[sy * screen_w + sx];
			sbuf[sy * screen_w + sx] = (src_px & 0xF7DE) >> 1;
		}
	}

	VMUINT16 bg_col = VM_COLOR_888_TO_565(20, 20, 20);
	VMUINT16 header_bg = VM_COLOR_888_TO_565(36, 36, 36);
	VMUINT16 div_col = VM_COLOR_888_TO_565(55, 55, 55);
	VMUINT16 border_col = VM_COLOR_888_TO_565(80, 80, 80);
	VMUINT16 text_box_bg = VM_COLOR_888_TO_565(28, 28, 28);
	VMUINT16 text_col = 0xFFFF;
	VMUINT16 cursor_col = 0xFFFF;
	VMUINT16 counter_col = VM_COLOR_888_TO_565(140, 140, 140);
	VMUINT16 highlight_col = VM_COLOR_888_TO_565(150, 150, 150);

	// 1. Sheet background & top border line
	raw_fill_rect(sbuf, screen_w, screen_h, 0, sheet_y, screen_w, sheet_h, bg_col);
	raw_line(sbuf, screen_w, screen_h, 0, sheet_y, screen_w - 1, sheet_y, border_col);

	// 2. Title bar
	int title_h = char_h + 6;
	raw_fill_rect(sbuf, screen_w, screen_h, 0, sheet_y + 1, screen_w, title_h, header_bg);
	raw_line(sbuf, screen_w, screen_h, 0, sheet_y + title_h + 1, screen_w - 1, sheet_y + title_h + 1, div_col);
	if (!m_title.empty()) {
		raw_textout_to_buf(sbuf, screen_w, screen_h, 8, sheet_y + 3, m_title, 0xFFFF, sheet_y, sheet_y + title_h + 1);
	}

	// 3. Character counter
	if (m_max_len > 0) {
		std::string cnt_str = std::to_string(m_text.length()) + "/" + std::to_string(m_max_len);
		std::u16string u16_cnt(cnt_str.begin(), cnt_str.end());
		int cw = vm_graphic_get_string_width((VMWSTR)u16_cnt.c_str());
		raw_textout_to_buf(sbuf, screen_w, screen_h, screen_w - cw - 8, sheet_y + 3, u16_cnt, counter_col, sheet_y, sheet_y + title_h + 1);
	}

	// 4. Bottom Action Bar
	int act_h = char_h + 12;
	int act_y = screen_h - act_h;
	raw_fill_rect(sbuf, screen_w, screen_h, 0, act_y, screen_w, act_h, header_bg);
	raw_line(sbuf, screen_w, screen_h, 0, act_y - 1, screen_w - 1, act_y - 1, div_col);
	raw_line(sbuf, screen_w, screen_h, screen_w / 2, act_y, screen_w / 2, screen_h - 1, div_col);

	// Highlight on press down - exactly matches BottomSheet
	if (m_left_pressed) {
		int bx1 = 0;
		int bx2 = screen_w / 2 - 1;
		int by1 = act_y;
		int by2 = screen_h - 1;
		for (int sy = by1; sy <= by2; ++sy) {
			for (int sx = bx1; sx <= bx2; ++sx) {
				VMUINT16* ptr = sbuf + sy * screen_w + sx;
				*ptr = ((*ptr & 0xF7DE) >> 1) + ((highlight_col & 0xF7DE) >> 1);
			}
		}
	}
	if (m_right_pressed) {
		int bx1 = screen_w / 2;
		int bx2 = screen_w - 1;
		int by1 = act_y;
		int by2 = screen_h - 1;
		for (int sy = by1; sy <= by2; ++sy) {
			for (int sx = bx1; sx <= bx2; ++sx) {
				VMUINT16* ptr = sbuf + sy * screen_w + sx;
				*ptr = ((*ptr & 0xF7DE) >> 1) + ((highlight_col & 0xF7DE) >> 1);
			}
		}
	}

	int text_y = act_y + (act_h - char_h) / 2;
	int lw = vm_graphic_get_string_width((VMWSTR)m_left_action.c_str());
	int left_x = (screen_w / 2 - lw) / 2;
	raw_textout_to_buf(sbuf, screen_w, screen_h, left_x, text_y, m_left_action, 0xFFFF, act_y, screen_h);

	int rw = vm_graphic_get_string_width((VMWSTR)m_right_action.c_str());
	int right_x = screen_w / 2 + (screen_w / 2 - rw) / 2;
	raw_textout_to_buf(sbuf, screen_w, screen_h, right_x, text_y, m_right_action, 0xFFFF, act_y, screen_h);

	// 5. Text Box Area inside the bottom sheet
	int box_x = 6;
	int box_y = sheet_y + title_h + 7;
	int box_w = screen_w - 12;
	int box_h = act_y - box_y - 6;

	raw_fill_rect(sbuf, screen_w, screen_h, box_x, box_y, box_w, box_h, text_box_bg);
	raw_line(sbuf, screen_w, screen_h, box_x, box_y, box_x + box_w - 1, box_y, border_col);
	raw_line(sbuf, screen_w, screen_h, box_x, box_y + box_h - 1, box_x + box_w - 1, box_y + box_h - 1, border_col);
	raw_line(sbuf, screen_w, screen_h, box_x, box_y, box_x, box_y + box_h - 1, border_col);
	raw_line(sbuf, screen_w, screen_h, box_x + box_w - 1, box_y, box_x + box_w - 1, box_y + box_h - 1, border_col);

	// 6. Word Wrapping & Text Lines
	int pad_x = 5;
	int pad_y = 5;
	int text_area_w = box_w - pad_x * 2 - 4;
	int text_area_h = box_h - pad_y * 2;
	int line_h = char_h + 3;

	auto lines = compute_wrapped_lines(m_text, text_area_w);

	int cur_line = 0;
	int cur_col_x = 0;
	get_cursor_coord(m_text, lines, m_cursor_pos, cur_line, cur_col_x);

	int max_vis_lines = std::max(1, text_area_h / line_h);
	if (cur_line < m_scroll_line)
		m_scroll_line = cur_line;
	if (cur_line >= m_scroll_line + max_vis_lines)
		m_scroll_line = cur_line - max_vis_lines + 1;
	if (m_scroll_line < 0)
		m_scroll_line = 0;

	int clip_top = box_y + 1;
	int clip_bottom = box_y + box_h - 2;

	// Draw selection highlight if active
	if (has_selection()) {
		size_t sel_start, sel_end;
		get_selection_range(sel_start, sel_end);
		VMUINT16 sel_bg_col = VM_COLOR_888_TO_565(30, 80, 160); // Blue selection highlight

		for (size_t i = m_scroll_line; i < lines.size(); ++i) {
			int ly = box_y + pad_y + (int)(i - m_scroll_line) * line_h;
			if (ly > clip_bottom) break;

			size_t l_st = lines[i].start_idx;
			size_t l_en = lines[i].start_idx + lines[i].length;

			if (l_en > sel_start && l_st < sel_end) {
				size_t sub_st = std::max(l_st, sel_start);
				size_t sub_en = std::min(l_en, sel_end);

				std::u16string pref = m_text.substr(l_st, sub_st - l_st);
				std::u16string sel_sub = m_text.substr(sub_st, sub_en - sub_st);

				int sx1 = box_x + pad_x + vm_graphic_get_string_width((VMWSTR)pref.c_str());
				int sw = vm_graphic_get_string_width((VMWSTR)sel_sub.c_str());
				if (sw <= 0) sw = 4;

				raw_fill_rect(sbuf, screen_w, screen_h, sx1, ly, sw, char_h, sel_bg_col);
			}
		}
	}

	for (size_t i = m_scroll_line; i < lines.size(); ++i) {
		int ly = box_y + pad_y + (int)(i - m_scroll_line) * line_h;
		if (ly > clip_bottom) break;
		if (lines[i].length > 0) {
			std::u16string lstr = m_text.substr(lines[i].start_idx, lines[i].length);
			raw_textout_to_buf(sbuf, screen_w, screen_h, box_x + pad_x, ly, lstr, text_col, clip_top, clip_bottom);
		}
	}

	// 7. Blinking Cursor (500 ms interval)
	bool cursor_visible = ((m_blink_clock.getElapsedTime().asMilliseconds() / 500) % 2) == 0;
	if (cursor_visible && cur_line >= m_scroll_line && cur_line < m_scroll_line + max_vis_lines) {
		int cx = box_x + pad_x + cur_col_x;
		int cy = box_y + pad_y + (cur_line - m_scroll_line) * line_h;
		if (cx <= box_x + box_w - 4 && cy + char_h <= clip_bottom + 1) {
			raw_line(sbuf, screen_w, screen_h, cx, cy, cx, cy + char_h - 1, cursor_col);
			raw_line(sbuf, screen_w, screen_h, cx + 1, cy, cx + 1, cy + char_h - 1, cursor_col);
		}
	}

	// 8. Scrollbar
	if ((int)lines.size() > max_vis_lines) {
		int sb_x = box_x + box_w - 4;
		int sb_y = box_y + 2;
		int sb_h = box_h - 4;
		int thumb_h = std::max(8, sb_h * max_vis_lines / (int)lines.size());
		int thumb_y = sb_y + (sb_h - thumb_h) * m_scroll_line / std::max(1, (int)lines.size() - max_vis_lines);
		raw_fill_rect(sbuf, screen_w, screen_h, sb_x, thumb_y, 2, thumb_h, border_col);
	}
}

bool SystemTextBox::handle_sfml_event(const sf::Event& event, int screen_w, int screen_h, float scale) {
	if (!m_is_open) return false;

	int char_h = vm_graphic_get_character_height();
	if (char_h <= 0) char_h = 16;
	int sheet_h = std::min(screen_h - 20, std::max(160, screen_h * 3 / 5));
	int sheet_y = screen_h - sheet_h;
	int title_h = char_h + 6;
	int act_h = char_h + 12;
	int act_y = screen_h - act_h;
	int box_x = 6;
	int box_y = sheet_y + title_h + 7;
	int box_w = screen_w - 12;
	int box_h = act_y - box_y - 6;

	if (event.type == sf::Event::TextEntered) {
		char32_t unicode = event.text.unicode;
		if (unicode >= 32 && unicode != 127) {
			if (has_selection()) {
				delete_selection();
			}
			if (m_max_len == 0 || m_text.length() < m_max_len) {
				m_text.insert(m_cursor_pos, 1, (char16_t)unicode);
				m_cursor_pos++;
				m_selection_anchor = m_cursor_pos;
				m_blink_clock.restart();
			}
			return true;
		}
		return false;
	}

	if (event.type == sf::Event::KeyPressed) {
		m_blink_clock.restart();

		// Keep F1 and F2 shortcuts for phone softkeys
		if (event.key.code == sf::Keyboard::F1 || event.key.code == sf::Keyboard::F2) {
			return false;
		}

		if (event.key.control && event.key.code == sf::Keyboard::A) {
			m_selection_anchor = 0;
			m_cursor_pos = m_text.length();
			return true;
		}

		switch (event.key.code) {
		case sf::Keyboard::Left: {
			size_t new_pos = (m_cursor_pos > 0) ? (m_cursor_pos - 1) : 0;
			m_cursor_pos = new_pos;
			if (!event.key.shift) {
				m_selection_anchor = m_cursor_pos;
			}
			return true;
		}

		case sf::Keyboard::Right: {
			size_t new_pos = (m_cursor_pos < m_text.length()) ? (m_cursor_pos + 1) : m_text.length();
			m_cursor_pos = new_pos;
			if (!event.key.shift) {
				m_selection_anchor = m_cursor_pos;
			}
			return true;
		}

		case sf::Keyboard::Up: {
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int cur_l = 0, cur_x = 0;
			get_cursor_coord(m_text, lines, m_cursor_pos, cur_l, cur_x);
			size_t new_pos = 0;
			if (cur_l > 0) {
				new_pos = get_pos_at_line_x(m_text, lines, cur_l - 1, cur_x);
			}
			m_cursor_pos = new_pos;
			if (!event.key.shift) {
				m_selection_anchor = m_cursor_pos;
			}
			return true;
		}

		case sf::Keyboard::Down: {
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int cur_l = 0, cur_x = 0;
			get_cursor_coord(m_text, lines, m_cursor_pos, cur_l, cur_x);
			size_t new_pos = m_text.length();
			if (cur_l + 1 < (int)lines.size()) {
				new_pos = get_pos_at_line_x(m_text, lines, cur_l + 1, cur_x);
			}
			m_cursor_pos = new_pos;
			if (!event.key.shift) {
				m_selection_anchor = m_cursor_pos;
			}
			return true;
		}

		case sf::Keyboard::Home: {
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int cur_l = 0, cur_x = 0;
			get_cursor_coord(m_text, lines, m_cursor_pos, cur_l, cur_x);
			m_cursor_pos = lines[cur_l].start_idx;
			if (!event.key.shift) {
				m_selection_anchor = m_cursor_pos;
			}
			return true;
		}

		case sf::Keyboard::End: {
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int cur_l = 0, cur_x = 0;
			get_cursor_coord(m_text, lines, m_cursor_pos, cur_l, cur_x);
			m_cursor_pos = lines[cur_l].start_idx + lines[cur_l].length;
			if (!event.key.shift) {
				m_selection_anchor = m_cursor_pos;
			}
			return true;
		}

		case sf::Keyboard::BackSpace:
			if (has_selection()) {
				delete_selection();
			}
			else if (m_cursor_pos > 0) {
				m_text.erase(m_cursor_pos - 1, 1);
				m_cursor_pos--;
				m_selection_anchor = m_cursor_pos;
			}
			return true;

		case sf::Keyboard::Delete:
			if (has_selection()) {
				delete_selection();
			}
			else if (m_cursor_pos < m_text.length()) {
				m_text.erase(m_cursor_pos, 1);
				m_selection_anchor = m_cursor_pos;
			}
			return true;

		case sf::Keyboard::Enter:
			submit();
			return true;

		case sf::Keyboard::Escape:
			cancel();
			return true;

		default:
			break;
		}
		// Consume all other keys to prevent them from triggering phone keypad shortcuts (e.g. 1-9, W/A/S/D, Q/E, etc.)
		return true;
	}

	if (event.type == sf::Event::KeyReleased) {
		// Keep F1 and F2 shortcuts for phone softkeys
		if (event.key.code == sf::Keyboard::F1 || event.key.code == sf::Keyboard::F2) {
			return false;
		}
		// Consume all other key releases
		return true;
	}

	if (event.type == sf::Event::MouseButtonPressed) {
		if (event.mouseButton.button == sf::Mouse::Left) {
			float mx = event.mouseButton.x / scale;
			float my = event.mouseButton.y / scale;

			// If click is outside the screen area (e.g. on phone keypad below), don't consume!
			if (mx < 0 || mx >= screen_w || my < 0 || my >= screen_h) {
				return false;
			}

			// Tapping outside the bottom sheet (dimmed background area): cancel / close
			if (my < sheet_y) {
				cancel();
				return true;
			}

			if (my >= act_y && my < screen_h && mx >= 0 && mx < screen_w / 2) {
				m_left_pressed = true;
				return true;
			}
			if (my >= act_y && my < screen_h && mx >= screen_w / 2 && mx < screen_w) {
				m_right_pressed = true;
				return true;
			}
			if (mx >= box_x && mx < box_x + box_w && my >= box_y && my < box_y + box_h) {
				auto lines = compute_wrapped_lines(m_text, box_w - 14);
				int line_h = char_h + 3;
				int clicked_line = m_scroll_line + (int)(my - (box_y + 5)) / line_h;
				if (clicked_line < 0) clicked_line = 0;
				if (clicked_line >= (int)lines.size()) clicked_line = (int)lines.size() - 1;
				m_cursor_pos = get_pos_at_line_x(m_text, lines, clicked_line, (int)(mx - (box_x + 5)));
				m_selection_anchor = m_cursor_pos;
				m_is_selecting_with_mouse = true;
				m_blink_clock.restart();
				return true;
			}
			return true;
		}
	}

	if (event.type == sf::Event::MouseMoved) {
		if (m_is_selecting_with_mouse) {
			float mx = event.mouseMove.x / scale;
			float my = event.mouseMove.y / scale;
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int line_h = char_h + 3;
			int clicked_line = m_scroll_line + (int)(my - (box_y + 5)) / line_h;
			if (clicked_line < 0) clicked_line = 0;
			if (clicked_line >= (int)lines.size()) clicked_line = (int)lines.size() - 1;
			m_cursor_pos = get_pos_at_line_x(m_text, lines, clicked_line, (int)(mx - (box_x + 5)));
			m_blink_clock.restart();
			return true;
		}
	}

	if (event.type == sf::Event::MouseButtonReleased) {
		if (event.mouseButton.button == sf::Mouse::Left) {
			m_is_selecting_with_mouse = false;
			float mx = event.mouseButton.x / scale;
			float my = event.mouseButton.y / scale;

			bool was_left = m_left_pressed;
			bool was_right = m_right_pressed;
			m_left_pressed = false;
			m_right_pressed = false;

			if (was_left) {
				if (my >= act_y && my < screen_h && mx >= 0 && mx < screen_w / 2) {
					submit();
				}
				return true;
			}
			if (was_right) {
				if (my >= act_y && my < screen_h && mx >= screen_w / 2 && mx < screen_w) {
					cancel();
				}
				return true;
			}

			// If click was outside screen (on keypad below), don't consume!
			if (mx < 0 || mx >= screen_w || my < 0 || my >= screen_h) {
				return false;
			}
			return true;
		}
	}

	return false;
}

bool SystemTextBox::handle_key(int event, int keycode) {
	if (!m_is_open) return false;

	if (event == VM_KEY_EVENT_DOWN || event == VM_KEY_EVENT_REPEAT) {
		switch (keycode) {
		case VM_KEY_LEFT_SOFTKEY:
			m_left_softkey_pressed = true;
			m_left_pressed = true;
			return true;
		case VM_KEY_RIGHT_SOFTKEY:
			m_right_pressed = true;
			return true;
		case VM_KEY_OK:
			m_ok_pressed = true;
			m_left_pressed = true;
			return true;
		case VM_KEY_BACK:
			m_right_pressed = true;
			return true;
		case VM_KEY_LEFT:
			if (m_cursor_pos > 0) m_cursor_pos--;
			m_blink_clock.restart();
			return true;
		case VM_KEY_RIGHT:
			if (m_cursor_pos < m_text.length()) m_cursor_pos++;
			m_blink_clock.restart();
			return true;
		case VM_KEY_UP: {
			int box_w = 240 - 12;
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int cur_l = 0, cur_x = 0;
			get_cursor_coord(m_text, lines, m_cursor_pos, cur_l, cur_x);
			if (cur_l > 0)
				m_cursor_pos = get_pos_at_line_x(m_text, lines, cur_l - 1, cur_x);
			else
				m_cursor_pos = 0;
			m_blink_clock.restart();
			return true;
		}
		case VM_KEY_DOWN: {
			int box_w = 240 - 12;
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int cur_l = 0, cur_x = 0;
			get_cursor_coord(m_text, lines, m_cursor_pos, cur_l, cur_x);
			if (cur_l + 1 < (int)lines.size())
				m_cursor_pos = get_pos_at_line_x(m_text, lines, cur_l + 1, cur_x);
			else
				m_cursor_pos = m_text.length();
			m_blink_clock.restart();
			return true;
		}
		case VM_KEY_CLEAR:
			if (m_cursor_pos > 0) {
				m_text.erase(m_cursor_pos - 1, 1);
				m_cursor_pos--;
				m_blink_clock.restart();
			}
			return true;

		case VM_KEY_NUM0:
		case VM_KEY_NUM1:
		case VM_KEY_NUM2:
		case VM_KEY_NUM3:
		case VM_KEY_NUM4:
		case VM_KEY_NUM5:
		case VM_KEY_NUM6:
		case VM_KEY_NUM7:
		case VM_KEY_NUM8:
		case VM_KEY_NUM9:
		case VM_KEY_STAR:
		case VM_KEY_POUND: {
			char16_t ch = u' ';
			if (keycode >= VM_KEY_NUM0 && keycode <= VM_KEY_NUM9)
				ch = u'0' + (keycode - VM_KEY_NUM0);
			else if (keycode == VM_KEY_STAR)
				ch = u'*';
			else if (keycode == VM_KEY_POUND)
				ch = u'#';

			if (m_max_len == 0 || m_text.length() < m_max_len) {
				m_text.insert(m_cursor_pos, 1, ch);
				m_cursor_pos++;
				m_blink_clock.restart();
			}
			return true;
		}
		default:
			break;
		}
	}
	else if (event == VM_KEY_EVENT_UP) {
		if (keycode == VM_KEY_OK) {
			bool was_pressed = m_ok_pressed;
			m_ok_pressed = false;
			m_left_pressed = false;
			// Ignore OK key release if it was pressed down before textbox opened
			if (was_pressed && m_open_clock.getElapsedTime().asMilliseconds() > 250) {
				submit();
			}
			return true;
		}
		if (keycode == VM_KEY_LEFT_SOFTKEY) {
			bool was_pressed = m_left_softkey_pressed;
			m_left_softkey_pressed = false;
			m_left_pressed = false;
			if (was_pressed && m_open_clock.getElapsedTime().asMilliseconds() > 150) {
				submit();
			}
			return true;
		}
		if (keycode == VM_KEY_RIGHT_SOFTKEY || keycode == VM_KEY_BACK) {
			m_right_pressed = false;
			if (m_open_clock.getElapsedTime().asMilliseconds() > 150) {
				cancel();
			}
			return true;
		}
		return true;
	}

	return true;
}

bool SystemTextBox::handle_pen(int event, int x, int y, int screen_w, int screen_h) {
	if (!m_is_open) return false;

	int char_h = vm_graphic_get_character_height();
	if (char_h <= 0) char_h = 16;
	int sheet_h = std::min(screen_h - 20, std::max(160, screen_h * 3 / 5));
	int sheet_y = screen_h - sheet_h;
	int title_h = char_h + 6;
	int act_h = char_h + 12;
	int act_y = screen_h - act_h;
	int box_x = 6;
	int box_y = sheet_y + title_h + 7;
	int box_w = screen_w - 12;
	int box_h = act_y - box_y - 6;

	if (event == VM_PEN_EVENT_TAP) {
		// Tapping outside sheet cancels
		if (y < sheet_y) {
			cancel();
			return true;
		}
		if (y >= act_y && y < screen_h && x >= 0 && x < screen_w / 2) {
			m_left_pressed = true;
			return true;
		}
		if (y >= act_y && y < screen_h && x >= screen_w / 2 && x < screen_w) {
			m_right_pressed = true;
			return true;
		}
		if (x >= box_x && x < box_x + box_w && y >= box_y && y < box_y + box_h) {
			auto lines = compute_wrapped_lines(m_text, box_w - 14);
			int line_h = char_h + 3;
			int clicked_line = m_scroll_line + (y - (box_y + 5)) / line_h;
			if (clicked_line < 0) clicked_line = 0;
			if (clicked_line >= (int)lines.size()) clicked_line = (int)lines.size() - 1;
			m_cursor_pos = get_pos_at_line_x(m_text, lines, clicked_line, x - (box_x + 5));
			m_blink_clock.restart();
			return true;
		}
	}
	else if (event == VM_PEN_EVENT_RELEASE) {
		if (m_left_pressed) {
			m_left_pressed = false;
			if (y >= act_y && y < screen_h && x >= 0 && x < screen_w / 2) {
				submit();
			}
			return true;
		}
		if (m_right_pressed) {
			m_right_pressed = false;
			if (y >= act_y && y < screen_h && x >= screen_w / 2 && x < screen_w) {
				cancel();
			}
			return true;
		}
	}
	return true;
}

}
