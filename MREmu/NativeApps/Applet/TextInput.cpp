#include "TextInput.h"
#include "../../AppContext.h"
#include "../../AppManagerEvents.h"
#include "../GUI/Engine.h"
#include "../GUI/View/Text.h"
#include "../GUI/View/Image.h"
#include "../GUI/Container/AlignContainer.h"
#include "../GUI/Container/MainContainer.h"
#include "../GUI/Container/VerticalList.h"
#include "../GUI/Container/VerticalScroll.h"
#include "../GUI/Base.h"
#include <vmgraph_fix.h>
#include <vmmissing.h>
#include <vmio.h>
#include <vmpromng.h>
#include <functional>
#include <memory>

namespace NativeApps::Applet::TextInput {
	std::shared_ptr<GUI::Engine> engine;

	std::u16string def_text;
	int max_size = 0;
	int input_method = 0;
	int parent_phandle = -1;
	void(*ret_callback)(VMINT, VMWSTR);

	int mod(int val, int lim) {
		int r = val % lim;
		return r < 0 ? r + lim : r;
	}

	class TextInput : public GUI::Base {
	public:
		const int text_x = 2, text_y = 2;
		const uint16_t folder_text_color = VM_COLOR_888_TO_565(255, 165, 0);
		const int space_width = vm_graphic_get_character_width(u' ');
		const int char_heigth = vm_graphic_get_character_height();

		std::u16string text;
		int max_size = 0;
		int cursor_pos = 0;

		struct char_info {
			int line = 0;
			int width = 0;
			int x = 0;
		};

		std::vector<char_info> text_info;

		struct text_line {
			int pos = 0, size = 0, width = 0;
		};

		std::vector<text_line> lines;

		TextInput(std::u16string text, int max_size) {
			this->text = text;
			this->max_size = max_size;
		}

		void update_text() {
			text_info.resize(text.size());

			text_line line;
			text_line world;

			for (int i = 0; i < text.size(); ++i) {
				text_info[i] = { (int)lines.size(),  vm_graphic_get_character_width(text[i]), line.width };

				if (text[i] == u' ') {
					world = { i + 1 };
					line.size += 1;
					line.width += text_info[i].width;
				}
				else if (line.width + text_info[i].width <= rb.w) {
					line.size += 1;
					line.width += text_info[i].width;
					world.size += 1;
					world.width += text_info[i].width;
				}
				else {
					if (line.pos != world.pos) {
						line.size -= world.size;
						line.width -= world.width;
						lines.push_back(line);
						line = { world.pos };
						world = { world.pos };
						i = world.pos - 1;
					}
					else {
						lines.push_back(line);
						line = { i };
						world = { i };
						--i;
					}
				}
			}

			if (line.size)
				lines.push_back(line);

		}

		//void update_size() override {
		//}

		void update_rb(GUI::RenderBox rb) override {
			this->rb = rb;

			update_text();
		}

		int get_chapter_x(int pos) {
			if (cursor_pos > 0 && cursor_pos <= text.size() && text.size())
				return text_info[pos - 1].x + text_info[pos - 1].width;
			return 0;
		}

		int get_chapter_line(int pos) {
			if (cursor_pos > 0 && cursor_pos <= text.size() && text.size())
				return text_info[cursor_pos - 1].line;
			return 0;
		}

		void draw(const GUI::Layer& layer, bool focused) override {
			vm_graphic_fill_rect(layer.buf, rb.x, rb.y, rb.w, rb.h, 0xFFFF, 0xFFFF);

			for (int i = 0; i < lines.size(); ++i)
				vm_graphic_textout(layer.buf, rb.x, rb.y + i * char_heigth, 
					(VMWSTR)text.c_str() + lines[i].pos, lines[i].size, 0);

			vm_graphic_fill_rect(layer.buf, rb.x + get_chapter_x(cursor_pos), rb.y + char_heigth * get_chapter_line(cursor_pos),
				2, char_heigth, VM_COLOR_RED, VM_COLOR_RED);
		};

		int find_pos_by_x(int line_n, int x) {
			auto line = lines[line_n];

			int pos = line.pos;
			int len = rb.w;

			for (int i = line.pos; i < line.pos + line.size; ++i) {
				int t_len = abs(text_info[i].x + text_info[i].width - x);

				if (len > t_len)
					len = t_len,
					pos = i;
			}

			return pos + 1;
		}

		void key_event(int event, int keycode) override {
			if (event == VM_KEY_EVENT_UP) {
				int line = get_chapter_line(cursor_pos);
				int x = get_chapter_x(cursor_pos);

				if (keycode == VM_KEY_LEFT)
					cursor_pos--;
				else if (keycode == VM_KEY_RIGHT)
					cursor_pos++;
				else if (keycode == VM_KEY_UP)
					cursor_pos = find_pos_by_x(mod(line - 1, lines.size()), x);
				else if (keycode == VM_KEY_DOWN)
					cursor_pos = find_pos_by_x(mod(line + 1, lines.size()), x);

				if (cursor_pos < 0)
					cursor_pos = text.size();
				else if (cursor_pos > text.size())
					cursor_pos = 0;

			}
		}

		void left(GUI::BaseSh& ui_menu) {
		}

		void ok() override {
		}

		void right() {
		}

		GUI::SoftKeySet get_soft_keys() override {
			return {
				GUI::SoftKey{u"", [this](GUI::BaseSh& ui_menu) { left(ui_menu); }},
				GUI::SoftKey{u"Ok", [this](GUI::BaseSh& ui_menu) { ok(); }},
				GUI::SoftKey{u"Clean", [this](GUI::BaseSh& ui_menu) { right(); }},
			};
		}
	};
	
	void entry() {
		engine = std::make_shared<GUI::Engine>();
		auto main = std::make_shared<GUI::Container::MainContainer>();
		auto input = std::make_shared<TextInput>(def_text, max_size);
		main->ui = input;
		engine->ui = main;

		vm_reg_sysevt_callback([](int m, int p) { engine->sys_event(m, p, engine); });
		vm_reg_keyboard_callback([](int e, int k) {engine->key_event(e, k); });
		vm_reg_pen_callback([](int e, int x, int y) {engine->pen_event(e, x, y); });
	}

	int start(std::u16string def_str, int max_size_, int input_method_, void (*callback)(VMINT state, VMWSTR text)) {
		def_text = def_str;
		max_size = max_size_;
		input_method = input_method_;
		ret_callback = callback;
		parent_phandle = vm_pmng_get_current_handle();
		AppContext::push_event(Event::launch{ Event::native_app{entry, u8"Text input"} });
		return VM_SELECTOR_ERR_NO_RES;
	}
}