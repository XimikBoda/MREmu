#pragma once
#include <vector>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace MREngine {
	namespace IO {
		void init();
		void imgui_keyboard();
	};

	class AppIO{
	public:
		std::vector<std::fstream*> files;

		uint32_t key_handler = 0;
	};
};


MREngine::AppIO& get_current_app_io();
fs::path get_current_app_path();

void add_keyboard_event(int event, int keycode);
fs::path convert_path(fs::path path);