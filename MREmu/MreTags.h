#pragma once
#include <vector>
#include <string>
#include <map>
#include <cstdint>

class MreTags
{
public:
	std::map<uint32_t, std::vector<uint8_t>> raw_tags;

	uint32_t tags_offset;

	bool load(std::vector<uint8_t>&file);

	bool is_ads();
	bool is_simple_ads();
	bool is_zipped();
	bool is_tags_ucs2();

	uint32_t get_ram();

	std::u8string get_dev_name();

	bool is_tag_exist(int id);
	bool read_bool(int id);
	uint32_t read_uint32(int id);
	std::u8string read_string(int id);

	std::vector<uint8_t> get_raw_tag(int id);
};

MreTags* get_tags_by_mem_adr(size_t offset_mem);