#pragma once
#include <filesystem>
#include <vmsys.h>

namespace fs = std::filesystem;

namespace Path {
	void init();

	fs::path get_screenshots_path();

	bool validate_path(fs::path path);

	fs::path UCS2_to_path(const VMWSTR wstr);
	fs::path path_from_emu(fs::path path);
	fs::path path_from_emu(const VMWSTR str);
}