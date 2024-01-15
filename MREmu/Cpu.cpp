#include "Cpu.h"
#include "Memory.h"
#include <unicorn/unicorn.h>

uc_engine* uc = 0;

namespace Cpu {
	void* stack_p = 0;
	size_t stack_size = 100 * 1024;

	static void write_reg(uc_engine* uc, int reg, unsigned int val) {
		uc_reg_write(uc, reg, &val);
	}

	void init() {
		uc_err uc_er = uc_open(UC_ARCH_ARM, UC_MODE_THUMB, &uc);
		if (uc_er) {
			printf("Failed on uc_open() with error returned: %u (%s)\n",
				uc_er, uc_strerror(uc_er));
			abort();
		}

		uc_er = uc_mem_map_ptr(uc, shared_memory_in_emu_start, shared_memory_size, UC_PROT_ALL, shared_memory_prt);
		if (uc_er) {
			printf("Failed on uc_mem_map_ptr() with error returned: %u (%s)\n",
				uc_er, uc_strerror(uc_er));
			abort();
		}

		stack_p = Memory::shared_malloc(stack_size);
		if (stack_p == 0)
			abort;

		write_reg(uc, UC_ARM_REG_SP, ADDRESS_TO_EMU(stack_p) + stack_size);
	}

};