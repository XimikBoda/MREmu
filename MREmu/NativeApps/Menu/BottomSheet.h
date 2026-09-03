#pragma once
#include <string>
#include <vector>
#include <functional>
#include <vmgraph.h>
#include <vmio.h>

namespace NativeApps {

class BottomSheet {
public:
	struct Line {
		std::u16string text;
		VMUINT16 color = 0xFFFF;
		bool marquee = true;
	};

private:
	bool m_open = false;
	std::u16string m_title;
	std::vector<Line> m_lines;

	std::u16string m_left_action;
	VMUINT16 m_left_action_color = VM_COLOR_888_TO_565(180, 180, 180);
	bool m_left_ok_triggers = false;
	std::function<void()> m_on_left;

	std::u16string m_right_action;
	VMUINT16 m_right_action_color = 0xFFFF;
	bool m_right_ok_triggers = false;
	std::function<void()> m_on_right;

	std::function<void()> m_on_dismiss;

public:
	BottomSheet() = default;

	void set_title(const std::u16string& title) { m_title = title; }
	void clear_lines() { m_lines.clear(); }
	void add_line(const std::u16string& text, VMUINT16 color = 0xFFFF, bool marquee = true) {
		m_lines.push_back({ text, color, marquee });
	}

	void set_left_action(const std::u16string& label, std::function<void()> cb,
		VMUINT16 color = VM_COLOR_888_TO_565(180, 180, 180), bool ok_triggers = false) {
		m_left_action = label;
		m_on_left = std::move(cb);
		m_left_action_color = color;
		m_left_ok_triggers = ok_triggers;
	}

	void set_right_action(const std::u16string& label, std::function<void()> cb,
		VMUINT16 color = 0xFFFF, bool ok_triggers = false) {
		m_right_action = label;
		m_on_right = std::move(cb);
		m_right_action_color = color;
		m_right_ok_triggers = ok_triggers;
	}

	void set_on_dismiss(std::function<void()> cb) {
		m_on_dismiss = std::move(cb);
	}

	bool is_open() const { return m_open; }
	void show() { m_open = true; }
	void hide() { m_open = false; }

	int calculate_height(int char_h, int screen_h) const;

	void draw(VMUINT8* layer_buf, int screen_w, int screen_h, int char_h, int marquee_offset, int& out_max_scroll);

	bool handle_key(VMINT event, VMINT keycode);
	bool handle_pen(VMINT event, VMINT x, VMINT y, int screen_w, int screen_h, int char_h);
};

} // namespace NativeApps
