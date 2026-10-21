#pragma once
#include <string>
#include <vector>
#include <functional>
#include <SFML/Window/Event.hpp>
#include <SFML/System/Clock.hpp>
#include <vmgraph.h>
#include <vmio.h>

namespace MREngine {

struct WrappedLine {
	size_t start_idx;
	size_t length;
};

class SystemTextBox {
private:
	static bool m_is_open;
	static std::u16string m_title;
	static std::u16string m_text;
	static std::u16string m_original_text;
	static size_t m_cursor_pos;
	static size_t m_selection_anchor; // anchor position for selection; == m_cursor_pos if no selection
	static bool m_is_selecting_with_mouse;
	static size_t m_max_len;
	static int m_input_method;
	static int m_scroll_line;

	static uint32_t m_callback;
	static std::function<void(bool, const std::u16string&)> m_native_callback;

	static std::u16string m_left_action;
	static std::u16string m_right_action;
	static bool m_left_pressed;
	static bool m_right_pressed;
	static bool m_ok_pressed;
	static bool m_left_softkey_pressed;

	static sf::Clock m_blink_clock;
	static sf::Clock m_open_clock;

	static bool m_bg_captured;
	static std::vector<uint16_t> m_bg_buffer;

	static std::vector<WrappedLine> compute_wrapped_lines(const std::u16string& text, int max_w);
	static void get_cursor_coord(const std::u16string& text, const std::vector<WrappedLine>& lines, size_t cursor_pos, int& out_line, int& out_x);
	static size_t get_pos_at_line_x(const std::u16string& text, const std::vector<WrappedLine>& lines, int line_idx, int target_x);

	static bool has_selection() { return m_cursor_pos != m_selection_anchor; }
	static void get_selection_range(size_t& start, size_t& end) {
		start = std::min(m_cursor_pos, m_selection_anchor);
		end = std::max(m_cursor_pos, m_selection_anchor);
	}
	static bool delete_selection();

public:
	static void init();
	static void deinit();
	static bool is_open();

	static int open(const VMWSTR def_str, int max_size, int input_method, uint32_t callback, const std::u16string& title = u"Search");
	static void open_native(const std::u16string& def_str, int max_size,
		std::function<void(bool ok, const std::u16string& text)> cb, const std::u16string& title = u"Search");

	static void set_title(const std::u16string& title);
	static void move_cursor_to_start();
	static void close_by_app();

	static void submit();
	static void cancel();

	static void draw(VMUINT8* screen_buf, int screen_w, int screen_h);
	static bool handle_sfml_event(const sf::Event& event, int screen_w, int screen_h, float scale);
	static bool handle_key(int event, int keycode);
	static bool handle_pen(int event, int x, int y, int screen_w, int screen_h);
};

}
