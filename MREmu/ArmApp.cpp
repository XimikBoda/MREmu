#include "ArmApp.h"
#include "Memory.h"
#include "Bridge.h"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <vmsys.h>

using namespace std::string_literals;

const uint32_t ADS_HEAP_SIZE = 2 * 1024;
const uint32_t ADS_STACK_SIZE = 3 * 1024;

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

bool ArmApp::check_format(fs::path path, bool local) {
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

	if (!memcmp(buf + 1, "ELF", 3))
		return true;

	if (buf[0] == 0x78) // Zlib 
		switch (buf[1]) {
		case 0x01:
		case 0x5E:
		case 0x9C:
		case 0xDA:
			return true;
		}

	return false;
}

bool ArmApp::preparation()
{
	if (!tags.load(file_context))
		return false;

	app_name = (const char*)tags.get_app_name().c_str();
	if (!app_name.size())
		app_name = real_path.stem().string();

	resources.file_context = &file_context;

	is_ads = tags.is_ads() || tags.is_vre_ads();
	is_zipped = tags.is_zipped();

	mem_size = tags.get_ram() * 1024;
	mem_size = std::max<size_t>(512 * 1024, mem_size);

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
			if (!elf.load(ss)) {
				spdlog::error("Failed to load ELF!");
				return false;
			}
		}

		segments_size = 0;

		if (is_ads) {
			uint32_t current_offset = 0x8000; // for debug
			uint32_t er_ro_vaddr = 0;

			for (int i = 0; i < elf.sections.size(); ++i) {
				ELFIO::section* psec = elf.sections[i];

				if (psec->get_name() == "ER_ZI" || psec->get_name() == ".bss") {
					zi_size = psec->get_size();
				}
				if (psec->get_name() == "ER_RO" || psec->get_name() == "ER_RW") {
					if (current_offset + psec->get_size() > mem_size) {
						spdlog::error("Segment loading error, {} bytes required, but {} allocated",
							current_offset + psec->get_size(), mem_size);
						return false;
					}

					memcpy((unsigned char*)mem_location + current_offset,
						file_context.data() + psec->get_offset(), psec->get_size());

					current_offset += psec->get_size();

					if (psec->get_name() == "ER_RO") {
						er_ro_vaddr = psec->get_address();
					}
					if (psec->get_name() == "ER_RW") {
						rw_size = psec->get_size();
					}
				}
			}

			entry_point = offset_mem + (elf.get_entry() - er_ro_vaddr);

			segments_size = current_offset;
		}
		else {
			entry_point = (elf.get_entry() + offset_mem);

			for (int i = 0; i < elf.segments.size(); ++i) {
				const ELFIO::segment* pseg = elf.segments[i];

				if (pseg->get_virtual_address() + pseg->get_memory_size() > mem_size) {
					spdlog::error("Segment loading error, {} bytes required, but {} allocated",
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
					ELFIO::Elf32_Rel* sym = (ELFIO::Elf32_Rel*)&file_context[psec->get_offset()]; //TODO
					for (int i = 0; i < psec->get_size() / sizeof(ELFIO::Elf32_Rel); ++i) {
						if (sym[i].r_offset & 3) {
							spdlog::critical("Unaligned relocation pointer detected!\n"
								"  -> Offset: {:#010X}\n"
								"  -> MRE Loader will corrupt memory at this address on ARMv5TE!",
								sym[i].r_offset
							);
						}
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
		}

		for (int i = 0; i < elf.sections.size(); ++i) {
			ELFIO::section* psec = elf.sections[i];

			if (psec->get_name() == std::string(".vm_res")) {
				resources.vm_res_offset = psec->get_offset();
				resources.vm_res_size = psec->get_size();
			}
		}
	}
	else
	{
		resources.local_offsets = true;

		if (is_ads) {
			uint32_t elf_info_size = *(uint32_t*)(file_context.data() + tags.tags_offset - 4);

			if (elf_info_size != sizeof(compress_ads_elf_info)) {
				spdlog::error("compress_ads_elf_info size error");
				return false;
			}

			compress_ads_elf_info* info = (compress_ads_elf_info*)(file_context.data() + tags.tags_offset - 4 - elf_info_size);

			uLongf dL = info->org_ro_size;
			if (uncompress((unsigned char*)mem_location,
				&dL, file_context.data() + info->ro_offset, info->ro_size)) {
				spdlog::error("uncompress error");
				return false;
			}

			dL = info->org_rw_size;
			if (uncompress((unsigned char*)mem_location + info->org_ro_size,
				&dL, file_context.data() + info->rw_offset, info->rw_size)) {
				spdlog::error("uncompress error");
				return false;
			}

			resources.vm_res_offset = info->res_offset;
			resources.vm_res_size = info->res_size;

			segments_size = info->org_ro_size + info->org_rw_size;
			rw_size = info->org_rw_size;
			zi_size = info->zi_size;

			entry_point = offset_mem;

		}
		else {
			spdlog::error("zipped no ads is not realized");
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

	uint32_t reserved_size = segments_size;
	if (is_ads) {
		reserved_size += 0x80 + rw_size + zi_size + ADS_HEAP_SIZE;
	}
	
	reserved_size = (reserved_size + 7) & ~7;

	app_memory.setup((size_t)mem_location, mem_size, reserved_size);
	app_memory.malloc(reserved_size, true); // for "protect" code

	if (resources.vm_res_size)
		resources.scan();
	return true;
}

void ArmApp::start()
{
	uint32_t vm_get_sym_entry_p = Bridge::vm_get_sym_entry("vm_get_sym_entry");

	Log::set_module(app_name);

	if (is_ads) {
		uint32_t data_base = offset_mem + segments_size + 0x80;

		uint32_t heap_base = data_base + rw_size + zi_size;
		uint32_t heap_limit = heap_base + ADS_HEAP_SIZE;


		Bridge::ads_start(entry_point, vm_get_sym_entry_p, data_base, heap_base, heap_limit, ADS_STACK_SIZE);
	}
	else {
		Bridge::run_cpu(entry_point, 3, vm_get_sym_entry_p, 0, 0);
	}
}