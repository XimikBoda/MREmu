#pragma once
#include "MreTags.h"
#include <filesystem>
#include <vector>
#include <elfio/elfio.hpp>
#include <elfio/elf_types.hpp>

namespace fs = std::filesystem;

class App 
{
public:
	std::vector<unsigned char> file_context;

	MreTags tags;

	void* mem_location = 0;
	size_t mem_size;
	size_t segments_size;

	ELFIO::elfio elf;

	void preparation_and_start();
	bool load_from_file(fs::path path);//tmp
};