#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Event {
	struct path_app {
		fs::path path;
		bool local;
	};

	struct fd_app {
		int fd;
        fs::path path;
	};

	struct native_app {
		void (*entry)() = 0;
		std::u8string name = u8"Native";
	};

	struct launch {
		std::variant<path_app, native_app, fd_app> data;
		int parent_phandle = -1;
		bool close_parrent = false;
	};
}