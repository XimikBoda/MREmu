#include "Memory.h"
#include <malloc.h>
#include <stdlib.h>
#include <cstring>

#ifndef WIN32
#define _aligned_malloc(size, alignment) aligned_alloc((alignment), (size))
#define _aligned_free(ptr) free((ptr))
#endif // !WIN32

#undef shared_memory_prt;
#undef shared_memory_offset;
#undef shared_memory_size;
#undef shared_memory_in_emu_start;

void* shared_memory_prt = NULL;
uint64_t shared_memory_offset = NULL;
size_t shared_memory_size = 0;
size_t shared_memory_in_emu_start = 0;

uint32_t ADDRESS_TO_EMU(size_t x) {
	if (x == 0)
		return 0;
	return ((uint32_t)(uint64_t(x) - shared_memory_offset));
}

uint32_t ADDRESS_TO_EMU(void* x) {
	return ADDRESS_TO_EMU((size_t)x);
}

void* ADDRESS_FROM_EMU(uint32_t x) {
	if (x == 0)
		return 0;
	return ((void*)((x)+shared_memory_offset));
}


namespace Memory {
	MemoryManager shared_memory;

	void init(size_t shared_memory_size_l)
	{
		shared_memory_size = shared_memory_size_l;

		shared_memory_prt = _aligned_malloc(shared_memory_size, 0x1000000);

		if (shared_memory_prt == NULL)
			exit(1);

		shared_memory_in_emu_start = 0x00000000; // 0 for gdb

		shared_memory_offset = (uint64_t)shared_memory_prt - shared_memory_in_emu_start;

		shared_memory.setup((uint64_t)shared_memory_prt, shared_memory_size, 10 * 1024 * 1024);
	}


	void* shared_malloc(size_t size, bool allow_protected, size_t align)
	{
		return (void*)shared_memory.malloc(size, allow_protected, align);
	}

	void shared_free(void* addr)
	{
		shared_memory.free((size_t)addr);
	}

	void* app_malloc(int size) {
		MemoryManager& mm = get_current_app_memory();
		return (void*)mm.malloc(size, 4);
	}

	void* app_realloc(void* p, int size) {
		MemoryManager& mm = get_current_app_memory();
		return (void*)mm.realloc((size_t)p, size);
	}

	void app_free(void* addr)
	{
		MemoryManager& mm = get_current_app_memory();
		mm.free((size_t)addr);
	}

	void deinit()
	{
		_aligned_free(shared_memory_prt);
	}

	void MemoryManager::setup(size_t start_adr, size_t size, size_t protected_size)
	{
		this->start_adr = start_adr;
		this->mem_size = size;
		this->protected_size = protected_size;
		free_memory_size = this->mem_size;
		regions.clear();
	}

	size_t MemoryManager::malloc(size_t size, bool allow_protected, size_t align)
	{
		if (size > free_memory_size - (allow_protected ? 0 : protected_size))
			return 0;

		size_t new_adr = start_adr + (allow_protected ? 0 : protected_size);

		for (int i = 0; i < regions.size(); ++i) {
			if (new_adr % align != 0)
				new_adr = ((new_adr / align) + 1) * align;

			if (new_adr + size < regions[i].adr) {
				regions.insert(regions.begin() + i, { new_adr, size });
				free_memory_size -= size;
				return new_adr;
			}

			new_adr = regions[i].adr + regions[i].size;
		}

		if (new_adr % align != 0)
			new_adr = ((new_adr / align) + 1) * align;

		if (new_adr + size < start_adr + mem_size) {
			regions.push_back({ new_adr, size });
			free_memory_size -= size;
			return new_adr;
		}
	}

	size_t MemoryManager::realloc(size_t addr, size_t size)
	{
		if (addr == 0)
			return malloc(size);

		if (size == 0) {
			free(addr);
			return addr;
		}

		int mem_ind = -1;
		for (int i = 0; i < regions.size(); ++i) {
			if (regions[i].adr == addr) {
				mem_ind = i;
				break;
			}
		}

		if(mem_ind == -1)
			return malloc(size);

		if (size <= regions[mem_ind].size) {
			free_memory_size += regions[mem_ind].size - size;
			regions[mem_ind].size = size;
			return regions[mem_ind].adr;
		}

		size_t allow_max_size = mem_size - (regions[mem_ind].adr - start_adr);
		if (mem_ind + 1 < regions.size())
			allow_max_size = regions[mem_ind + 1].adr - regions[mem_ind].adr;

		if (allow_max_size >= size) {
			free_memory_size -= size - regions[mem_ind].size;
			regions[mem_ind].size = size;
			return regions[mem_ind].adr;
		}

		size_t new_adr = malloc(size);

		if (new_adr == 0)
			return 0;

		memcpy((void*)new_adr, (void*)regions[mem_ind].adr, regions[mem_ind].size); //need to be careful
		free(regions[mem_ind].adr);

		return new_adr;
	}

	void MemoryManager::free(size_t addr)
	{
		for (int i = 0; i < regions.size(); ++i) {
			if (regions[i].adr == addr) {
				free_memory_size += regions[i].size;
				regions.erase(regions.begin() + i);
				return;
			}
		}
	}
}