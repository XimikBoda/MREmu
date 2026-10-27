#include "AppSelector.h"
#include "../../AppContext.h"
#include "../../AppManagerEvents.h"
#include "../GUI/Engine.h"
#include "../GUI/View/Text.h"
#include "../GUI/View/Image.h"
#include "../GUI/Container/VerticalList.h"
#include "../GUI/Container/VerticalScroll.h"
#include "../GUI/Base.h"
#include <vmgraph_fix.h>
#include <vmmissing.h>
#include <vmio.h>
#include <vmpromng.h>
#include <memory>

namespace NativeApps::Menu::AppSelector {
	std::shared_ptr<GUI::Engine> engine;
	const std::u16string mre_folder_path = u"e:\\mre\\";

	const int image_size = 30;

	class AppItem : public GUI::Base {
	public:
		std::u16string path;

		GUI::View::Text text;
		GUI::View::Image image;

		const int text_x = 34, text_y = 8;
		const int image_x = 1, image_y = 1;

		AppItem(std::u16string name, std::u16string path, VMINT_CANVAS img) : text(name), image(img, image_size, image_size) {
			this->path = path;
			h = 32;
		}

		void update_size() override {
			text.update_size();
			w = text_x + text.w;
		}

		void update_rb(GUI::RenderBox rb) override {
			this->rb = rb;
			image.update_rb({ rb.x + image_x, rb.y + image_y, rb.w - image_x, rb.h - image_y });
			text.update_rb({ rb.x + text_x, rb.y + text_y, rb.w - text_x, rb.h - text_y });
		}

		void draw(const GUI::Layer& layer, bool focused) override {
			if (focused)
				draw_focused(layer);

			image.draw(layer, focused);
			text.draw(layer, focused);
		};

		void ok() override {
			vm_start_app((VMWSTR)path.c_str(), vm_pmng_get_current_handle(), 0);
		}
	};

	VMINT_CANVAS load_vxp_logo(std::u16string path, int w, int h) {
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

				img = vm_graphic_load_image_resized_FIX(img_data, img_size, w, h);
			}
		}

		return img;
	}

	void entry() {
		engine = std::make_shared<GUI::Engine>();
		auto list = std::make_shared<GUI::Container::VertocalList>();
		auto scroll = std::make_shared<GUI::Container::VerticalScroll>();
		scroll->ui = list;
		engine->ui = scroll;

		vm_fileinfo_ext direntry;

		for (int ret = 0, find_h = vm_find_first_ext((VMWSTR)(mre_folder_path + u"*.vxp").c_str(), &direntry);
			find_h >= 0 && !ret;
			ret = vm_find_next_ext(find_h, &direntry))
		{
			std::u16string path = mre_folder_path;
			std::u16string name = (char16_t*)direntry.filefullname;
			path += name;

			list->items.push_back({ std::make_shared<AppItem>(name, path, load_vxp_logo(path, image_size, image_size)) });
		}

		vm_reg_sysevt_callback([](int m, int p) { engine->sys_event(m, p, engine); });
		vm_reg_keyboard_callback([](int e, int k) {engine->key_event(e, k); });
		vm_reg_pen_callback([](int e, int x, int y) {engine->pen_event(e, x, y); });
	}

	void start() {
		AppContext::push_event(Event::launch{ Event::native_app{entry, u8"AppSelector"} });
	}
}