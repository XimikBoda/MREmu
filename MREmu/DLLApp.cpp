#include "DLLApp.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

using namespace std::string_literals;


bool DLLApp::preparation()
{
	if (!tags.load(file_context))
		return false;

	resources.file_context = &file_context;

	{
		if (tags.tags_offset < 4)
			return 0;

		uint32_t resources_end = tags.tags_offset - 4;
		uint32_t resources_start = *(uint32_t*)&file_context[resources_end] + 8;

		if (resources_end < resources_start)
			return 0;

		
		resources.offset = resources_start;
		resources.size = resources_end - resources_start;
	}

	dll = LoadLibraryW(real_path.wstring().c_str());

	if(!dll)
		return false;

	entry_point = (dll_vm_entry)GetProcAddress(dll, "vm_entry");

	if (!entry_point) {
		FreeLibrary(dll);
		return false;
	}

	mem_size = tags.get_ram() * 1024;
	mem_size = std::max<size_t>(512 * 1024, mem_size);

	mem_location = Memory::shared_malloc(mem_size, true, 0x100000);
	memset(mem_location, 0, mem_size);

	offset_mem = ADDRESS_TO_EMU(mem_location);

	if (!path_is_local)
		path = "@:\\"s + std::to_string(offset_mem) + ".rom"s;

	app_memory.setup((size_t)mem_location, mem_size);

	if(resources.size)
		resources.scan();
	return true;
}

void DLLApp::start()
{
	run(entry_point, (void*)Bridge::vm_get_sym_entry_native);
}

bool DLLApp::load_from_file(fs::path path, bool local)
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