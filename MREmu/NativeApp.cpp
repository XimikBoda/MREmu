#include "NativeApp.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

using namespace std::string_literals;

bool NativeApp::preparation() {
	app_name = "Native";
	mem_size = 512 * 1024;
	mem_location = Memory::shared_malloc(mem_size, false);
	app_memory.setup((size_t)mem_location, mem_size);
	return true;
}

void NativeApp::start()
{
	run(conf.entry);
}
