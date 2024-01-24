#include "IO.h"
#include <vmio.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void MREngine::IO::init()
{
	fs::create_directory(".\\fs");
	fs::create_directory(".\\fs\\e");
	fs::create_directory(".\\fs\\c");
	fs::create_directory(".\\fs\\d");
}

fs::path convert_path(const VMWSTR str) { // TODO rewrite this
	fs::path path = (wchar_t*)str, res = ".\\fs\\";
	//if(string(res.string())== string(path.string(),res.string().length()))
	if (path.begin() != path.end() && (++path.begin()) != path.end() && path.begin()->string() == "." && (++path.begin())->string() == "fs")
		return path;
	auto litter = path.root_name().string();
	if (litter.size()) {
		litter.resize(2);
		litter[1] = '\\';
		res += litter;
	}
	else {
		res += "e\\";
	}
	res += path.relative_path();
	std::cout << "convert_path: " << path << " to " << res << '\n';
	return res;
}

VMINT vm_file_mkdir(const VMWSTR dirname) {
	fs::path path = convert_path(dirname);
	bool res = fs::create_directory(path);
	if (res)
		return 0;
	else
		return -1;
}

VMINT vm_file_set_attributes(const VMWSTR filename, VMBYTE attributes) {
	fs::path path = convert_path(filename);
	return 0; //todo
}

VMINT vm_file_get_attributes(const VMWSTR filename) {
	fs::path path = convert_path(filename);
	int res = 0;

	if (std::filesystem::exists(path)) {
		if (std::filesystem::is_directory(path))
			res |= VM_FS_ATTR_DIR;
		if (!(int)(std::filesystem::status(path).permissions() & fs::perms::owner_write))
			res |= VM_FS_ATTR_READ_ONLY;
	}
	return res;
}

VMINT vm_get_removable_driver(void) {
	return 'e';
}

VMINT vm_get_system_driver(void) {
	return 'c';
}

VMUINT vm_get_disk_free_space(VMWSTR drv_name) {
	return 256 * 1024 * 1024;
}


