#pragma once
#include "ItemsMng.h"
#include <vector>
#include <fstream>
#include <regex>
#include <filesystem>
#include <variant>
#include <functional>

#include <vmio.h>

namespace fs = std::filesystem;

namespace MREngine {
	struct find_el{
		fs::directory_iterator di;
		std::regex find_reg;
		fs::path lfolder;
		fs::path path;
		bool find_recv;
		bool first = true;

		find_el() = default;
		find_el(fs::path path_f);
		find_el(const find_el&) = default;

		fs::path next();
	};

	class AppIO{
	public:
		ItemsMng<FILE*> files;

		ItemsMng<find_el> find;
		ItemsMng<find_el> find_ext;

		vm_key_handler_t key_handler = 0;
		vm_pen_handler_t pen_handler = 0;

		std::function<void(char32_t)> unicode_event;
		std::function<void(std::u16string, int, int, int, int)> ime_event;

		~AppIO();
	};
};

namespace Event {
	struct keyboard {
		int event;
		int keycode;
	};

	struct pen {
		int event;
		int x, y;
	};

	struct unicode {
		char32_t code;
	};

	struct andoid_ime {
		std::u16string text;
		int selStart, selEnd;
		int compStart, compEnd;
	};

	struct input {
		std::variant<keyboard, pen, unicode, andoid_ime> data;
	};
}