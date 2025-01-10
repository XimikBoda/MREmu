#include "ArmApp.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

using namespace std::string_literals;

typedef struct
{
	uint32_t ro_offset;
	uint32_t ro_size;
	uint32_t org_ro_size;
	uint32_t rw_offset;
	uint32_t rw_size;
	uint32_t org_rw_size;
	uint32_t zi_size;
	uint32_t res_offset;
	uint32_t res_size;
} compress_ads_elf_info;

bool ArmApp::preparation()
{
	if (!tags.load(file_context))
		return false;

	is_arm = true;

	resources.file_context = &file_context;

	is_ads = tags.is_ads();
	is_zipped = tags.is_zipped();

	mem_size = tags.get_ram() * 1024;
	mem_size = std::max<size_t>(512 * 1024 * 4, mem_size);

	mem_location = Memory::shared_malloc(mem_size, true, 0x100000);
	memset(mem_location, 0, mem_size);

	offset_mem = ADDRESS_TO_EMU(mem_location);

	if (!path_is_local)
		path = "@:\\"s + std::to_string(offset_mem) + ".rom"s;

	if (!is_zipped)
	{
		ELFIO::elfio elf;

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

			if (pseg->get_virtual_address() + pseg->get_memory_size() > mem_size) {
				printf("Segment loading error, %d bytes required, but %d allocated\n",
					pseg->get_virtual_address() + pseg->get_memory_size(), mem_size);
				return false;
			}

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
				resources.offset = psec->get_offset();
				resources.size = psec->get_size();
			}
		}
	}
	else
	{
		if (is_ads) {
			uint32_t elf_info_size = *(uint32_t*)(file_context.data() + tags.tags_offset - 4);

			if (elf_info_size != sizeof(compress_ads_elf_info)) {
				printf("compress_ads_elf_info size error\n");
				return false;
			}

			compress_ads_elf_info* info = (compress_ads_elf_info*)(file_context.data() + tags.tags_offset - 4 - elf_info_size);

			uLongf dL = info->org_ro_size;
			if(uncompress((unsigned char*)mem_location, 
				&dL, file_context.data() + info->ro_offset, info->ro_size)) {
				printf("uncompress error\n");
				return false;
			}

			dL = info->org_rw_size;
			if (uncompress((unsigned char*)mem_location + info->org_ro_size, 
				&dL, file_context.data() + info->rw_offset, info->rw_size)) {
				printf("uncompress error\n");
				return false;
			}

			resources.offset = info->res_offset;
			resources.size = info->res_size;

			segments_size = info->org_ro_size + info->org_rw_size + info->zi_size;

			entry_point = offset_mem;

		}
		else {
			printf("zipped no ads is not realized\n");
			return false;
		}
	}

	{//temp
		std::ofstream out("unpack.bin", std::ios::binary);
		if (out.is_open()) {
			out.write((char*)mem_location, segments_size);
			out.close();
		}
	}

	app_memory.setup((size_t)mem_location, mem_size);
	app_memory.malloc(segments_size); // for "protect" code

	if(resources.size)
		resources.scan();
	return true;
}

void ArmApp::start()
{
	uint32_t vm_get_sym_entry_p = Bridge::vm_get_sym_entry("vm_get_sym_entry");
	if (is_ads) {
		Bridge::ads_start(entry_point, vm_get_sym_entry_p, offset_mem + mem_size+ 0x100);
	}
	else {
		Bridge::run_cpu(entry_point, 3, vm_get_sym_entry_p, 0, 0);
	}
}

bool ArmApp::load_from_file(fs::path path, bool local)
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