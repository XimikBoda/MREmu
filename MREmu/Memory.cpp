#include "Memory.h"
#include <malloc.h>
#include <stdlib.h>

#undef shared_memory_offset;

void* shared_memory_prt = NULL;
uint64_t shared_memory_offset = NULL;
size_t shared_memory_size = 0;

namespace Memory {
	void init(size_t shared_memory_size_l) {
		shared_memory_size = shared_memory_size_l;

		shared_memory_prt = _aligned_malloc(shared_memory_size, 0x1000000);

		if (shared_memory_prt == NULL)
			exit(1);

#ifdef X64MODE
		shared_memory_offset = (uint64_t)shared_memory_prt - 0x1000000;
#endif // X64MODE
	}
}