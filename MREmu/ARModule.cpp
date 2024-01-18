#include "ARModule.h"
#include "ARModuleBin.h"
#include "Memory.h"
#include "Bridge.h"
#include <elfio/elfio.hpp>
#include <elfio/elf_types.hpp>
#include <sstream>

void ARModule::init(uint32_t m, uint32_t r, uint32_t f)
{
	mem_size = 512 * 1024;

	mem_location = Memory::shared_malloc(mem_size, 0x100000);
	memset(mem_location, 0, mem_size);

	offset_mem = ADDRESS_TO_EMU(mem_location);

	ELFIO::elfio elf;

	{
		std::stringstream ss;
		ss.write((char*)stdio_module_axf, stdio_module_axf_len);
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
			stdio_module_axf + pseg->get_offset(), pseg->get_file_size());

		segments_size = std::max<size_t>(segments_size,
			pseg->get_virtual_address() + pseg->get_memory_size());
	}

	for (int i = 0; i < elf.sections.size(); ++i) {
		ELFIO::section* psec = elf.sections[i];

		if (psec->get_name() == std::string(".rel.dyn") || psec->get_name() == std::string(".rel.plt")) {
			ELFIO::Elf32_Rel* sym = (ELFIO::Elf32_Rel*)&stdio_module_axf[psec->get_address()]; //TODO
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
	}

	uint32_t ar_adr = Bridge::run_cpu(entry_point, 3, m, r, f);

	uint32_t *array = (uint32_t*)ADDRESS_FROM_EMU(ar_adr);
	vm_vsprintf = array[0];
	vm_sprintf = array[1];
	vm_sscanf = array[2];

	fn_m["vm_vsprintf"] = vm_vsprintf;
	fn_m["vm_sprintf"] = vm_sprintf;
	fn_m["vm_sscanf"] = vm_sscanf;
}

int ARModule::vm_get_sym_entry(const char* symbol) {
	auto it = fn_m.find(std::string(symbol));
	if (it != fn_m.end())
		return it->second;
	else
		return 0;
}
