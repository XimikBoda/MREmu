#include "TextInput.h"
#include "../../AppContext.h"
#include "../../AppManagerEvents.h"
#include "../../MREngine/IO.h"
#include "../../MREngine/CharSet.h"
#include "../../jni/AndroidIme.h"
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
	class TextInput* input_p = 0;

	std::u8string title = u8"Text input";

	std::u16string def_text;
	int max_size = 0;
	int input_method = 0;
	int parent_phandle = -1;
	void(*ret_callback)(VMINT, VMWSTR);

	int phandle = -1;

	static int mod(int val, int lim) {
		int r = val % lim;
		return r < 0 ? r + lim : r;
	}

	static void run_callback(int status, const std::u16string &text) {
		AppContext::push_event(Event::text_input{ status, text, ret_callback, parent_phandle });
		AppContext::close_app_by_phandle(phandle, false, true); // todo
		engine->sys_event(VM_MSG_QUIT, 0, engine);
	}

	class TextInput : public GUI::Base {
	public:
		const int text_x = 2, text_y = 2;
		const uint16_t folder_text_color = VM_COLOR_888_TO_565(255, 165, 0);
		const int space_width = vm_graphic_get_character_width(u' ');
		const int star_width = vm_graphic_get_character_width(u'*');
		const int char_heigth = vm_graphic_get_character_height();
		std::u16string password_text;

		AndroidIme ime;

		std::u16string text;
		int max_size = 0;
		int cursor_pos = 0;
		int input_method = 0;

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

		TextInput(std::u16string text, int max_size, int input_method) {
			this->text = text;
			this->max_size = max_size;
			this->input_method = input_method;

			cursor_pos = text.size();

			ime.open(text, max_size, input_method);
		}

		void update_text() {
			lines.clear();
			text_info.resize(text.size());

			text_line line;
			text_line world;

			if (input_method & VM_INPUT_METHOD_PASSWORD) {
				for (int i = 0; i < text.size(); ++i) {
					text_info[i] = { (int)lines.size(),  star_width, line.width };

					if (line.width + text_info[i].width < rb.w) {
						line.size += 1;
						line.width += text_info[i].width;
					}
					else {
						lines.push_back(line);
						line = { i };
						world = { i };
						--i;
					}
				}

				if (line.size)
					lines.push_back(line);

				return;
			}


			for (int i = 0; i < text.size(); ++i) {
				text_info[i] = { (int)lines.size(),  vm_graphic_get_character_width(text[i]), line.width };

                bool new_line = false;

				if (text[i] == u'\n') {
					new_line = true;
					text_info[i] = { (int)lines.size() + 1 };
					++i;
				}
				else if (text[i] == u' ') {
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
				else if (line.pos != world.pos) {
					line.size -= world.size;
					line.width -= world.width;
					lines.push_back(line);
					line = { world.pos };
					world = { world.pos };
					i = world.pos - 1;
				}
				else
                    new_line = true;

                if (new_line){
                    lines.push_back(line);
                    line = { i };
                    world = { i };
					--i;
                }
			}

			if (line.size)
				lines.push_back(line);

		}

		void update_size() override {
			h = char_heigth * lines.size();
		}

		void update_rb(GUI::RenderBox rb) override {
			this->rb = rb;

			if (input_method & VM_INPUT_METHOD_PASSWORD)
				password_text.resize(rb.w / star_width + 1, u'*');

			update_text();
		}

		int get_chapter_x(int pos) {
			if (pos > 0 && pos <= text.size() && text.size())
				return text_info[pos - 1].x + text_info[pos - 1].width;
			return 0;
		}

		int get_chapter_line(int pos) {
			if (pos > 0 && pos <= text.size() && text.size())
				return text_info[pos - 1].line;
			return 0;
		}

		void set_cursor_pos(int pos) {
			cursor_pos = pos;

			if (cursor_pos < 0)
				cursor_pos = text.size();
			else if (cursor_pos > text.size())
				cursor_pos = 0;

			ime.setSelection(cursor_pos);
		}

		void draw(const GUI::Layer& layer, bool focused) override {
			vm_graphic_fill_rect(layer.buf, rb.x, rb.y, rb.w, rb.h, 0xFFFF, 0xFFFF);

			for (int i = 0; i < lines.size(); ++i)
				if (input_method & VM_INPUT_METHOD_PASSWORD)
					vm_graphic_textout(layer.buf, rb.x, rb.y + i * char_heigth, 
						(VMWSTR)password_text.c_str(), lines[i].size, 0);
				else
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
				else if (keycode == VM_KEY_CLEAR)
					clear();

				set_cursor_pos(cursor_pos);
			}
		}

		void pen_event(const GUI::Touch& touch) override {
			if (!lines.size()) return;

			int line = (touch.y - rb.y) / char_heigth;

			if (line < 0) line = 0;
			if (line >= lines.size()) line = lines.size() - 1;

			cursor_pos = find_pos_by_x(mod(line, lines.size()), touch.x - rb.x);

			set_cursor_pos(cursor_pos);

			ime.setVisible(true);
		}

		void ime_event(std::u16string text, int sBeg, int sEnd, int cBeg, int cEnd) {
			this->text = text;
			this->cursor_pos = sEnd;
			this->update_text();
		}

		void unicode_event(char16_t code) {
			if (code == u'\b' || text.size() + 1 >= max_size)
				return;

			if (input_method & VM_INPUT_METHOD_NUMERIC)
				if ((code < u'0' || code > u'9')
					&& code != u'*' && code != '#')
					return;

			text.insert(text.begin() + cursor_pos, code);
			cursor_pos++;

			update_text();

			ime.setText(text, cursor_pos);
		}

		void left(GUI::BaseSh& ui_menu) {
		}

		void ok() override {
			run_callback(VM_INPUT_OK, text);
		}

		void right() {
			if (text.size())
				clear();
			else 
				cancel();
		}

		void cancel() {
			run_callback(VM_INPUT_CANCEL, u"");
		}

		void close(bool by_parent) {
			run_callback(by_parent ? VM_INPUT_CLOSE : VM_INPUT_NOTIFY, u"");
		}

		void clear() {
			if (text.size() && cursor_pos) {
				text.erase(text.begin() + --cursor_pos);

				update_text();

				ime.setText(text, cursor_pos);
			}
		}

		GUI::SoftKeySet get_soft_keys() override {
			return {
				GUI::SoftKey{u"", [this](GUI::BaseSh& ui_menu) { left(ui_menu); }},
				GUI::SoftKey{u"Ok", [this](GUI::BaseSh& ui_menu) { ok(); }},
				GUI::SoftKey{text.size() ? u"Clear" : u"Close", [this](GUI::BaseSh& ui_menu) { right(); }},
			};
		}
	};
	
	void entry() {
		engine = std::make_shared<GUI::Engine>();
		auto main = std::make_shared<GUI::Container::MainContainer>();
		auto input = std::make_shared<TextInput>(def_text, max_size, input_method);
		main->ui = input;
		engine->ui = main;

		input_p = input.get();

		phandle = vm_pmng_get_current_handle();

		vm_reg_sysevt_callback([](int m, int p) { engine->sys_event(m, p, engine); });
		vm_reg_keyboard_callback([](int e, int k) {engine->key_event(e, k); });
		vm_reg_pen_callback([](int e, int x, int y) {engine->pen_event(e, x, y); });

		AppContext::get_executing_app_io().ime_event = 
			[](std::u16string text, int sBeg, int sEnd, int cBeg, int cEnd) {
				if (engine) {
					input_p->ime_event(text, sBeg, sEnd, cBeg, cEnd);
					engine->draw();
				}
			};

		AppContext::get_executing_app_io().unicode_event =
			[](char32_t code) {
				if (engine)
					if (code <= 0xFFFF) {
						if (code >= 0xD800 && code <= 0xDFFF)
							return; 
					
						input_p->unicode_event(code);
						engine->draw();
					}
			};
	}

	int start(std::u16string def_str, int max_size_, int input_method_, void (*callback)(VMINT state, VMWSTR text)) {
		if (engine)
			return -1;
		
		def_text = def_str;
		max_size = max_size_;
		input_method = input_method_;
		ret_callback = callback;
		parent_phandle = vm_pmng_get_current_handle();
		AppContext::push_event(Event::launch{ Event::native_app{entry, title} }, true);
		return 0;
	}

	void close() {
		if (engine)
			input_p->close(vm_pmng_get_current_handle() == parent_phandle);
	}

	void move_cursor_to_start() {
		if (engine)
			input_p->set_cursor_pos(0);
	}

	int set_title(std::u16string title_) {
		title = ucs2_to_utf8(title_);
		return 1;
	}
}