#pragma once

namespace Bridge {
	void init();
	int vm_get_sym_entry(const char* symbol);
	int run_cpu(unsigned int adr, int n, ...);
}