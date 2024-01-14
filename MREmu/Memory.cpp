#include "Memory.h"
#include <malloc.h>
#include <stdlib.h>

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

namespace Memory {
	MemoryManager shared_memory;

	void init(size_t shared_memory_size_l)
	{
		shared_memory_size = shared_memory_size_l;

		shared_memory_prt = _aligned_malloc(shared_memory_size, 0x1000000);

		if (shared_memory_prt == NULL)
			exit(1);

#ifdef X64MODE
		shared_memory_in_emu_start = 0x1000000;
#else
		shared_memory_in_emu_start = shared_memory_prt;
#endif

		shared_memory_offset = (uint64_t)shared_memory_prt - shared_memory_in_emu_start;

		shared_memory = MemoryManager((uint64_t)shared_memory_prt, shared_memory_size);
	}


	void* shared_malloc(size_t size, size_t align)
	{
		return (void*)shared_memory.malloc(size, align);
	}

	void shared_free(void*addr)
	{
		shared_memory.free((size_t)addr);
	}

	void deinit()
	{
		_aligned_free(shared_memory_prt);
	}

	MemoryManager::MemoryManager(size_t start_adr, size_t size)
	{
		this->start_adr = start_adr;
		this->mem_size = size;
		free_memory_size = this->mem_size;
	}

	size_t MemoryManager::malloc(size_t size, size_t align)
	{
		if (size > free_memory_size)
			return 0;

		size_t new_adr = start_adr;
		if (regions.size()) {
			auto& last_region = regions[regions.size() - 1];
			new_adr = last_region.adr + last_region.size;
		}
		if (new_adr % align != 0)
			new_adr = ((new_adr / align) + 1) * align;

		if (new_adr + size > start_adr + mem_size)
			return malloc_topmost(size);
		else {
			regions.push_back({ new_adr, size });
			free_memory_size -= size;
			return new_adr;
		}
	}

	size_t MemoryManager::malloc_topmost(size_t size)
	{
		printf("%s:%d, malloc_topmost() not completed\n", __FILE__, __LINE__);
		return 0; //TODO
	}

	void MemoryManager::free(size_t addr)
	{
		for (int i = 0; i < regions.size(); ++i) {
			if (regions[i].adr == addr) {
				regions.erase(regions.begin() + i);
				return;
			}
		}
	}
}