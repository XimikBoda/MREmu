#include "App.h"
#include "Memory.h"
#include "Bridge.h"
#include <fstream>
#include <sstream>

static App* app_tmp = 0; //delete this

void App::preparation()
{
	app_tmp = this;

	is_ads = tags.is_ads();
	is_zipped = tags.is_zipped();

	mem_size = tags.get_ram() * 1024;
	mem_size = std::max<size_t>(512 * 1024, mem_size);

	mem_location = Memory::shared_malloc(mem_size, 0x100000);
	memset(mem_location, 0, mem_size);

	offset_mem = ADDRESS_TO_EMU(mem_location);

	if (!is_zipped)
	{
		{
			std::stringstream ss;
			ss.write((char*)file_context.data(), file_context.size());
			if (!elf.load(ss))
				abort();
		}

		entry_point = (elf.get_entry() + offset_mem);

		segments_size = 0;
		for (int i = 0; i < elf.segments.size(); ++i) {
			const ELFIO::segment* pseg = elf.segments[i];

			if (pseg->get_virtual_address() + pseg->get_memory_size() > mem_size)
				abort();

			memcpy((unsigned char*)mem_location + pseg->get_virtual_address(),
				file_context.data() + pseg->get_offset(), pseg->get_file_size());

			segments_size = std::max<size_t>(segments_size,
				pseg->get_virtual_address() + pseg->get_memory_size());
		}

		for (int i = 0; i < elf.sections.size(); ++i) {
			ELFIO::section* psec = elf.sections[i];

			if (psec->get_name() == std::string(".rel.dyn") || psec->get_name() == std::string(".rel.plt")) {
				ELFIO::Elf32_Rel* sym = (ELFIO::Elf32_Rel*)&file_context[psec->get_address()]; //TODO
				for (int i = 0; i < psec->get_size() / sizeof(ELFIO::Elf32_Rel); ++i) {
					switch (sym[i].r_info & 0xFF) {
					case 0x17:
						*(uint32_t*)((unsigned char*)mem_location + sym[i].r_offset) += offset_mem;
						break;
					case 0x02:
					case 0x16:
						*(uint32_t*)((unsigned char*)mem_location + sym[i].r_offset) = 0;
						break;
					}
				}
			}
			if (psec->get_name() == std::string(".vm_res")) {
				res_offset = psec->get_offset();
				res_size = psec->get_size();
			}
		}
	}
	else
	{
		printf("zipped files is not realized\n");
		abort();
	}

	app_memory = Memory::MemoryManager((size_t)mem_location, mem_size);
	app_memory.malloc(segments_size); // for "protect" code
}

void App::start()
{
	if (is_ads) {
		uint32_t *init_params = (uint32_t*)((unsigned char*)mem_location + mem_size - 6*4); //TODO move to another place

		init_params[0] = offset_mem + segments_size; //I'll have to figure out exactly how it works
		init_params[1] = 0; //vm_get_sym_entry from bridge
		init_params[2] = init_params[0] + 1024;
		init_params[3] = init_params[2] + 2 * 1024;
		init_params[4] = 3 * 1024;
		init_params[5] = entry_point;
	}
	else {
		uint32_t vm_get_sym_entry_p = Bridge::vm_get_sym_entry("vm_get_sym_entry");
		Bridge::run_cpu(entry_point, 3, vm_get_sym_entry_p, 0, 0);
	}
}


bool App::load_from_file(fs::path path)
{
	std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
	if (!in.is_open())
		return 0;
	size_t file_size = (size_t)in.tellg();
	in.seekg(0, std::ios::beg);
	file_context.resize(file_size);
	in.read((char*)file_context.data(), file_size);
	in.close();
	tags.load(file_context);
}

Memory::MemoryManager& get_current_app_memory() { //TODO: move to app manager
	return app_tmp->app_memory;
}