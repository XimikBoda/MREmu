#include "App.h"
#include "Memory.h"
#include <fstream>
#include <sstream>

void App::preparation_and_start()
{
	bool is_ads = tags.is_ads();
	bool is_zipped = tags.is_zipped();

	mem_size = tags.get_ram() * 1024;
	mem_size = std::max<size_t>(512 * 1024, mem_size);

	mem_location = Memory::shared_malloc(mem_size, 0x100000);
	memset(mem_location, 0, mem_size);

	if (!is_zipped)
	{
		{
			std::stringstream ss;
			ss.write((char*)file_context.data(), file_context.size());
			if (!elf.load(ss))
				abort();
		}

		segments_size = 0;
		for (int i = 0; i < elf.segments.size(); ++i) {
			const ELFIO::segment* pseg = elf.segments[i];

			if (pseg->get_virtual_address() + pseg->get_memory_size() > mem_size)
				abort();

			memcpy((unsigned char*)mem_location + pseg->get_virtual_address(),
				file_context.data() + pseg->get_offset(), pseg->get_memory_size());

			segments_size = std::max<size_t>(segments_size,
				pseg->get_virtual_address() + pseg->get_memory_size());
		}

	}
	else
	{
		printf("zipped files is not realized\n");
		abort();
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
