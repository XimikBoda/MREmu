#pragma once
#include "Memory.h"
#include <map>
#include <string>

class ARModule
{
	void* mem_location = 0;
	size_t offset_mem;
	size_t mem_size;
	size_t segments_size;

	uint32_t entry_point;

	uint32_t vm_vsprintf;
	uint32_t vm_sprintf;
	uint32_t vm_sscanf;

	std::map<std::string, uint32_t> fn_m;

public:
	void init(uint32_t m, uint32_t r, uint32_t f);

	int vm_get_sym_entry(const char* symbol);

	Memory::MemoryManager app_memory;

};