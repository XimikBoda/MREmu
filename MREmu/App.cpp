#include "App.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

bool App::load_from_file(fs::path path, bool local)
{
	path_is_local = local;

	if (path_is_local) {
		real_path = path_from_emu(path);
		this->path = path;
	}
	else
		real_path = path;

	std::ifstream in(real_path, std::ios::in | std::ios::binary | std::ios::ate);
	if (!in.is_open())
		return false;
	size_t file_size = (size_t)in.tellg();
	in.seekg(0, std::ios::beg);
	file_context.resize(file_size);
	in.read((char*)file_context.data(), file_size);
	in.close();
	return true;
}