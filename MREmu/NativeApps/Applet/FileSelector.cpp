#include "FileSelector.h"
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

namespace NativeApps::Applet::FileSelector {
	std::shared_ptr<GUI::Engine> engine;
	GUI::Container::VerticalList* g_list;
	GUI::Container::VerticalScroll* g_scroll;
	std::u16string cur_path;
	int parent_phandle = -1;
	VMINT(*ret_callback)(VMWCHAR*, VMINT);

	void enter_directory(std::u16string path);
	std::u16string prev_dir(std::u16string path);

	class MenuItem : public GUI::Base {
	public:
		const int text_x = 2, text_y = 2;
		GUI::View::Text text;

		GUI::SoftKeySet soft_key_set;

		MenuItem(std::u16string name, GUI::SoftKeySet soft_key_set) : text(name)
		{
			this->soft_key_set = soft_key_set;
			h = 20;
		}

		void update_size() override {
			text.update_size();
			w = text_x + text.w;
		}

		void update_rb(GUI::RenderBox rb) override {
			this->rb = rb;
			text.update_rb({ rb.x + text_x, rb.y + text_y, rb.w - text_x, rb.h - text_y });
		}

		void draw(const GUI::Layer& layer, bool focused) override {
			draw_back(layer);

			if (focused)
				draw_focused(layer);

			text.draw(layer, focused);
		}

		void ok() override {
			if (soft_key_set.keys[GUI::SoftKeySet::Ok].has_value())
				if (auto func = soft_key_set.keys[GUI::SoftKeySet::Ok]->func) {
					engine->key_event(VM_KEY_EVENT_DOWN, VM_KEY_OK); // TODO
					engine->key_event(VM_KEY_EVENT_UP, VM_KEY_OK);
				}
		}

		GUI::SoftKeySet get_soft_keys() override {
			return soft_key_set;
		}
	};

	class FileItem : public GUI::Base {
	public:
		const int text_x = 2, text_y = 2;
		const uint16_t folder_text_color = VM_COLOR_888_TO_565(255, 165, 0);

		std::u16string name;
		std::u16string path;

		GUI::View::Text text;

		bool is_folder = false;
		bool is_root = false;

		FileItem(std::u16string name, std::u16string path, bool is_folder, bool is_root = false)
			: text(name, is_folder ? folder_text_color : 0xFFFF) 
		{
			this->name = name;
			this->path = path;
			this->is_folder = is_folder;
			this->is_root = is_root;
			h = 20;
		}

		void update_size() override {
			text.update_size();
			w = text_x * 2 + text.w;
		}

		void update_rb(GUI::RenderBox rb) override {
			this->rb = rb;
			text.update_rb({ rb.x + text_x, rb.y + text_y, rb.w - text_x, rb.h - text_y });
		}

		void draw(const GUI::Layer& layer, bool focused) override {
			if (focused)
				draw_focused(layer);

			text.draw(layer, focused);
		};

		void left(GUI::BaseSh& ui_menu) {
			auto list = std::make_shared<GUI::Container::VerticalList>();

			GUI::SoftKeySet sks = {
				std::nullopt,
				GUI::SoftKey{u"Ok", [this](GUI::BaseSh& ui_menu) { ok(); ui_menu.reset();; }},
				GUI::SoftKey{u"Back", [this](GUI::BaseSh& ui_menu) { ui_menu.reset(); }},
			};

			if (is_folder)
				list->items.push_back({ std::make_shared<MenuItem>(u"Open", sks) });

			sks.keys[GUI::SoftKeySet::Ok]->func = [this](GUI::BaseSh& ui_menu) { select(); ui_menu.reset(); };
			list->items.push_back({ std::make_shared<MenuItem>(u"Select", sks) });

			sks.keys[GUI::SoftKeySet::Ok]->func = [this](GUI::BaseSh& ui_menu) { cancel(); };
			list->items.push_back({ std::make_shared<MenuItem>(u"Cancel", sks) });

			auto aling = std::make_shared<GUI::Container::AlignContainer>(list, 
				GUI::Container::AlignContainer::Horisontal::Left, GUI::Container::AlignContainer::Vertical::Bottom);

			ui_menu = aling;
			engine->update();
		}

		void ok() override {
			if (is_folder)
				enter_directory(path);
			else
				select();
		}

		void right() {
			if (is_root)
				cancel();
			else
				enter_directory(prev_dir(cur_path));
		}

		void select() {
			AppContext::push_event(Event::file_selector{ path, ret_callback, parent_phandle });
			AppContext::close_executing_app(true);
		}

		void cancel() {
			AppContext::push_event(Event::file_selector{ u"", ret_callback, parent_phandle });
			AppContext::close_executing_app(true);
		}

		GUI::SoftKeySet get_soft_keys() override {
			return {
				GUI::SoftKey{u"Options", [this](GUI::BaseSh& ui_menu) { left(ui_menu); }},
				GUI::SoftKey{is_folder ? u"Open" : u"Select", [this](GUI::BaseSh& ui_menu) { ok(); }},
				GUI::SoftKey{is_root ? u"Cancel" : u"Back", [this](GUI::BaseSh& ui_menu) { right(); }},
			};
		}
	};

	static bool file_sort(GUI::Container::VerticalList::list_item& ai, GUI::Container::VerticalList::list_item& bi) {
		FileItem* a = (FileItem*)ai.ui.get(), * b = (FileItem*)bi.ui.get();
		if (a->is_folder == b->is_folder)
			return a->name < b->name;
		else if (a->is_folder)
			return true;
		else
			return false;
	}

	std::u16string prev_dir(std::u16string path) {
		if (path.length() <= 3)
			return u"";
		else {
			auto pos = path.find_last_of(u'\\');
			if(pos == path.npos)
				return u"";
			if (pos == path.length() - 1)
				pos = path.find_last_of(u'\\', pos - 1);
			if (pos == path.npos)
				return u"";

			return path.substr(0, pos + 1);
		}
	}

	void enter_directory(std::u16string path) {
		cur_path = path;

		g_list->items.clear();

		if (cur_path.size()) 
		{
			vm_fileinfo_ext direntry;
			for (int ret = 0, find_h = vm_find_first_ext((VMWSTR)cur_path.c_str(), &direntry);
				find_h >= 0 && !ret;
				ret = vm_find_next_ext(find_h, &direntry))
			{
				std::u16string path = cur_path;
				std::u16string name = (char16_t*)direntry.filefullname;
				path += name;

				bool is_folder = direntry.attributes & VM_FS_ATTR_DIR;

				if (is_folder)
					path += u'\\';

				g_list->items.push_back({ std::make_shared<FileItem>(name, path, is_folder) });
			}

			g_list->items.push_back({ std::make_shared<FileItem>(u"..\\ (previus dir)", prev_dir(cur_path), true)});
			std::sort(g_list->items.begin(), g_list->items.end(), file_sort);
		}
		else {
			g_list->items.push_back({ std::make_shared<FileItem>(u"E:\\ (SD Card)", u"e:\\", true, true) });
			g_list->items.push_back({ std::make_shared<FileItem>(u"C:\\ (Internal memory)", u"c:\\", true, true) });
			g_list->items.push_back({ std::make_shared<FileItem>(u"D:\\ (Hiden internal memory)", u"d:\\", true, true) });
		}

		g_scroll->scroll_pos = 0;
		engine->update();
	}

	void entry() {
		engine = std::make_shared<GUI::Engine>();
		auto local = std::make_shared<GUI::Container::VerticalList>();
		auto scroll = std::make_shared<GUI::Container::VerticalScroll>();
		auto main = std::make_shared<GUI::Container::MainContainer>();
		scroll->ui = local;
		main->ui = scroll;
		engine->ui = main;

		g_list = local.get();
		g_scroll = scroll.get();

		enter_directory(cur_path);

		vm_reg_sysevt_callback([](int m, int p) { engine->sys_event(m, p, engine); });
		vm_reg_keyboard_callback([](int e, int k) {engine->key_event(e, k); });
		vm_reg_pen_callback([](int e, int x, int y) {engine->pen_event(e, x, y); });
	}

	vm_selector_error_t start(vm_selector_type_t type, std::u16string path, VMINT(*callback)(VMWCHAR*, VMINT)) {
		cur_path = path;
		ret_callback = callback;
		parent_phandle = vm_pmng_get_current_handle();
		AppContext::push_event(Event::launch{ Event::native_app{entry, u8"File Selector"} });
		return VM_SELECTOR_ERR_NO_RES;
	}
}