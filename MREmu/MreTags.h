#pragma once
#include<vector>

class MreTags
{
public:
	std::vector<std::vector<unsigned char>> raw_tags;

	void load(std::vector<unsigned char>&file);

	bool is_ads();
	bool is_simple_ads();
	bool is_zipped();

	unsigned int get_ram();
};