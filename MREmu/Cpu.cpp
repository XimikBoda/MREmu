#include "Cpu.h"

namespace Cpu {
	uc_engine* uc;
	
	void init() {
		uc_err uc_er = uc_open(UC_ARCH_ARM, UC_MODE_THUMB, &uc);
		if (uc_er) {
			printf("Failed on uc_open() with error returned: %u (%s)\n",
				uc_er, uc_strerror(uc_er));
			abort();
		}
	}

};