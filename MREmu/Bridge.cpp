#include "Bridge.h"
#include "Memory.h"
#include <string>
#include <iostream>
#include <unicorn/unicorn.h>

const unsigned char bxlr[2] = { 0x70, 0x47 };

extern uc_engine * uc;
uc_hook trace;

namespace Bridge {
	struct br_func {
		std::string name;
		void (*f)(uc_engine* uc);
	};

	unsigned char* func_ptr;

	static unsigned int read_reg(uc_engine* uc, int reg) {
		unsigned int r = 0;
		uc_reg_read(uc, reg, &r);
		return r;
	}

	static void write_reg(uc_engine* uc, int reg, unsigned int val) {
		uc_reg_write(uc, reg, &val);
	}

	unsigned int top_sp(uc_engine* uc, int l) {
		unsigned int el, sp = read_reg(uc, UC_ARM_REG_SP);
		uc_mem_read(uc, sp + 4 * l, &el, 4);
		return el;
	}

	unsigned int top_from_r(uc_engine* uc, unsigned int r, int l) {
		unsigned int el;
		uc_mem_read(uc, r + 4 * l, &el, 4);
		return el;
	}

	static void write_ret(uc_engine* uc, unsigned int val) {
		write_reg(uc, UC_ARM_REG_R0, val);
	}

	unsigned int read_arg(uc_engine* uc, int ind) {
		if (ind < 4)
			return read_reg(uc, UC_ARM_REG_R0 + ind);
		else
			return top_sp(uc, ind - 4);
	}


	void br_vm_get_sym_entry(uc_engine* uc) {
		write_ret(uc, vm_get_sym_entry((char*)ADDRESS_FROM_EMU(read_arg(uc, 0))));
	}

	std::vector<br_func> func_map =
	{
		{"vm_get_sym_entry", br_vm_get_sym_entry},
	};

	int vm_get_sym_entry(char* symbol) {
		std::string str = symbol;

		int ret = 0;

		for (int i = 0; i < func_map.size(); ++i)
			if (func_map[i].name == str) {
				ret = (ADDRESS_TO_EMU(func_ptr) + i * 2) | 1;
				break;
			}

		std::cout << "vm_get_sym_entry(\"" << str << "\") -> " << ret << " \n";

		return ret;
	}

	void bridge_hoock(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
		int ind = (address - ADDRESS_TO_EMU(func_ptr)) / 2;

		if (ind > func_map.size())
			abort();

		func_map[ind].f(uc);
	}

	void init() {
		size_t func_count = func_map.size();

		func_ptr = (unsigned char*)Memory::shared_malloc(func_count * 2);

		for (int i = 0; i < func_count; ++i)
			func_ptr[i * 2] = bxlr[0], func_ptr[i * 2 + 1] = bxlr[1];

		uc_hook_add(uc, &trace, UC_HOOK_CODE, bridge_hoock, 0,
			ADDRESS_TO_EMU(func_ptr), ADDRESS_TO_EMU(func_ptr + func_count * 2));
	}
}