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

#ifdef X64MODE
#define ADDRESS_TO_EMU(x) ((uint32_t)(uint64_t(x)-shared_memory_offset))
#define ADDRESS_FROM_EMU(x) ((void*)((x)+shared_memory_offset))
#else
#define ADDRESS_TO_EMU(x) (x)
#define ADDRESS_FROM_EMU(x) (x)
#endif // X64MODE

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
		std::vector<memory_region_t> regions;
	public:
		MemoryManager() = default;
		MemoryManager(size_t start_adr, size_t size);

		size_t malloc(size_t size, size_t align = 8); // align is 8 righd? //todo
		size_t malloc_topmost(size_t size);

		void free(size_t addr);
	};

	void init(size_t shared_memory_size);
	void* shared_malloc(size_t size, size_t align = 8);
	void shared_free(void* addr);
}