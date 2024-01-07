#pragma once
#include <cstdint>
#include <stdint.h>

#if INTPTR_MAX == INT64_MAX
#define X64MODE
#elif INTPTR_MAX == INT32_MAX
//nothing
#else
#error Unknown pointer size or missing size macros!
#endif

#ifdef X64MODE
#define ADRESS_TO_EMU(x) ((uint32_t)(uint64_t(x)+shared_memory_offset))
#define ADRESS_FROM_EMU(x) ((void*)((x)+shared_memory_offset))
#else
#define ADRESS_TO_EMU(x) (x)
#define ADRESS_FROM_EMU(x) (x)
#endif // X64MODE

extern uint64_t shared_memory_offset;

namespace Memory {
	void init(size_t shared_memory_size);
}