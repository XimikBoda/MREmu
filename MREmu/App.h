#pragma once
#include "MreTags.h"
#include "Memory.h"
#include "MREngine/System.h"
#include "MREngine/Resources.h"
#include "MREngine/Graphic.h"
#include "MREngine/Timer.h"
#include "MREngine/IO.h"
#include <filesystem>
#include <vector>
#include <elfio/elfio.hpp>
#include <elfio/elf_types.hpp>

namespace fs = std::filesystem;

class App 
{
public:
	std::vector<unsigned char> file_context;


	void* mem_location = 0;
	size_t offset_mem;
	size_t mem_size;
	size_t segments_size;

	uint32_t entry_point;

	bool is_ads;
	bool is_zipped;

	MreTags tags;

	Memory::MemoryManager app_memory;
	MREngine::SystemCallbacks system_callbacks;
	MREngine::Resources resources;
	MREngine::AppGraphic graphic;
	MREngine::Timer timer;
	MREngine::AppIO io;

	bool preparation();
	void start();
	bool load_from_file(fs::path path);//tmp
};