#pragma once
#include "ItemsMng.h"
#include <vector>
#include <fstream>
#include <regex>
#include <filesystem>

#include <vmio.h>

namespace fs = std::filesystem;

namespace MREngine {
	namespace IO {
		void init();
	};

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

		AppIO() {
			files.push(NULL); // Reserve index 0 so that file handles start at 1
			find.push(find_el()); // Reserve index 0 so that find handles start at 1
			find_ext.push(find_el()); // Reserve index 0 so that find_ext handles start at 1
		}
	};
};


MREngine::AppIO& get_current_app_io();
fs::path get_current_app_path();
fs::path get_current_app_real_path();
std::string get_current_app_name();

void add_keyboard_event(int event, int keycode);
fs::path path_from_emu(fs::path path);