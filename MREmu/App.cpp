#include "App.h"
#include "Memory.h"
#include "Bridge.h"
#include "Path.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

bool App::load(std::ifstream &in) {
	if (!in.is_open())
		return false;
	size_t file_size = (size_t)in.tellg();
	in.seekg(0, std::ios::beg);
	file_context.resize(file_size);
	in.read((char*)file_context.data(), file_size);
	in.close();
	return true;
}

bool App::load(fs::path path, bool local) {
	path_is_local = local;

	if (path_is_local) {
		real_path = Path::path_from_emu(path);
		this->path = path;
	}
	else
		real_path = path;

	std::ifstream in(real_path, std::ios::in | std::ios::binary | std::ios::ate);

	return load(in);
}

bool App::load(int fd, fs::path path) {
#ifdef  WIN32
	std::ifstream in(::_fdopen(fd, "r"));
#elif
	__gnu_cxx::stdio_filebuf<char> filebuf(fd, std::ios::in);
    istream in(&filebuf);
#endif

	real_path = path;

	return load(in);
}

#define APP_TO_THIS(x) this->x = app->x;

void App::init_from(App* app) {
	APP_TO_THIS(file_context);

	APP_TO_THIS(path);
	APP_TO_THIS(real_path);
	APP_TO_THIS(path_is_local);
}
