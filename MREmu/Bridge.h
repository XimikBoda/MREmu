#pragma once
#include <cstdint>

namespace Bridge {
	void init();
	int vm_get_sym_entry(const char* symbol);
	int run_cpu(uint32_t adr, int n, ...);
	int ads_start(uint32_t entry, uint32_t vm_get_sym_entry_p, uint32_t data_base);
}