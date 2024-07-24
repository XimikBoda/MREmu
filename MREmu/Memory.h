#pragma once
#include <cstdint>
#include <vector>
#include <stdint.h>

#if INTPTR_MAX == INT64_MAX
#define X64MODE
#elif INTPTR_MAX == INT32_MAX
//nothing
#else
#error Unknown pointer size or missing size macros!
#endif

uint32_t ADDRESS_TO_EMU(void* x);
uint32_t ADDRESS_TO_EMU(size_t x);
void* ADDRESS_FROM_EMU(uint32_t x);

#define FUNC_TO_UINT32(x) (uint32_t)(((size_t)(x))&UINT32_MAX)

extern void* shared_memory_prt;
extern uint64_t shared_memory_offset;
extern size_t shared_memory_size;
extern size_t shared_memory_in_emu_start;

namespace Memory {
	class MemoryManager {
		struct memory_region_t {
			size_t adr = 0;
			size_t size = 0;
		};
		size_t start_adr = 0;
		size_t mem_size = 0;
		size_t free_memory_size = 0;
		size_t protected_size = 0;
		std::vector<memory_region_t> regions;
	public:
		void setup(size_t start_adr, size_t size, size_t protected_size = 0);

		size_t malloc(size_t size, bool allow_protected = false, size_t align = 8); // align is 8 right? //todo

		size_t realloc(size_t addr, size_t size);

		void free(size_t addr);
	};

	void init(size_t shared_memory_size);
	void* shared_malloc(size_t size, bool allow_protected = false, size_t align = 8);
	void shared_free(void* addr);

	void* app_malloc(int size);
	void* app_realloc(void* p, int size);
	void app_free(void* addr);
}

Memory::MemoryManager& get_current_app_memory();