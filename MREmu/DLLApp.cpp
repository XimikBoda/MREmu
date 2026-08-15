#include "DLLApp.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

using namespace std::string_literals;

#ifdef  WIN32

bool DLLApp::check_format(fs::path path, bool local) {
	if (local) {
		path = path_from_emu(path);
	}

	unsigned char buf[4];
	std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
	if (!in.is_open())
		return false;
	size_t file_size = (size_t)in.tellg();
	if (file_size < 4)
		return false;
	in.seekg(0, std::ios::beg);
	in.read((char*)buf, 4);
	in.close();

	if (!memcmp(buf, "MZ", 2))
		return true;

	return false;
}

bool DLLApp::preparation()
{
	if (!tags.load(file_context))
		return false;

	app_name = (const char*)tags.get_app_name().c_str();
	if (!app_name.size())
		app_name = real_path.stem().string();

	resources.file_context = &file_context;

	{
		if (tags.tags_offset < 4)
			return 0;

		uint32_t resources_end = tags.tags_offset - 4;
		uint32_t resources_start = *(uint32_t*)&file_context[resources_end] + 8;

		if (resources_end < resources_start)
			return 0;


		resources.vm_res_offset = resources_start;
		resources.vm_res_size = resources_end - resources_start;
	}

	if (file_context.size() > 512 * 1024 * 1024) {
		memmove(file_context.data(), file_context.data() + resources.vm_res_offset, resources.vm_res_size);
		file_context.resize(resources.vm_res_size);
		file_context = std::vector<unsigned char>(file_context);
		resources.vm_res_offset = 0;
	}

	dll = LoadLibraryW(real_path.wstring().c_str());

	if (!dll) {
		DWORD errorCode = GetLastError();
		spdlog::error("Can`t load DLL, error code: {}", errorCode);
		return false;
	}

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

	if (resources.vm_res_offset && resources.vm_res_size)
		resources.scan();
	return true;
}

void DLLApp::start()
{
	run(entry_point, (void*)Bridge::vm_get_sym_entry_native);
}

#endif //  WIN32