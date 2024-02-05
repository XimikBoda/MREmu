#pragma once
#include <vector>
#include <cstdint>

class MreTags
{
public:
	std::vector<std::vector<unsigned char>> raw_tags;

	uint32_t tags_offset;

	bool load(std::vector<unsigned char>&file);

	bool is_ads();
	bool is_simple_ads();
	bool is_zipped();

	unsigned int get_ram();
};

MreTags* get_tags_by_mem_adr(size_t offset_mem);