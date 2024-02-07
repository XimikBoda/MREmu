#pragma once
#include "ItemsMng.h"
#include <vector>
#include <fstream>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

namespace MREngine {
	namespace IO {
		void init();
		void imgui_keyboard();
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
		ItemsMng<std::fstream*> files;

		ItemsMng<find_el> find;
		ItemsMng<find_el> find_ext;

		uint32_t key_handler = 0;
	};
};


MREngine::AppIO& get_current_app_io();
fs::path get_current_app_path();

void add_keyboard_event(int event, int keycode);
fs::path convert_path(fs::path path);