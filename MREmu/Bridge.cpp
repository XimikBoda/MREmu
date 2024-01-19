#include "Bridge.h"
#include "Memory.h"
#include "ARModule.h"
#include <string>
#include <iostream>
#include <unicorn/unicorn.h>

#include <vmgraph.h>
#include <vmres.h>

const unsigned char bxlr[2] = { 0x70, 0x47 };
const unsigned char idle_bin[2] = { 0xfe, 0xe7 };

extern uc_engine* uc;
uc_hook trace;

namespace Bridge {
	struct br_func {
		std::string name;
		void (*f)(uc_engine* uc);
	};

	unsigned char* func_ptr;
	uint32_t idle_p = 0;

	ARModule armodule;

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

	void br_vm_malloc(uc_engine* uc) {
		write_ret(uc, ADDRESS_TO_EMU(vm_malloc(read_arg(uc, 0))));
	}
	void br_vm_realloc(uc_engine* uc) {
		write_ret(uc, ADDRESS_TO_EMU(
			vm_realloc((void*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1))));
	}

	void br_vm_free(uc_engine* uc) {
		vm_free((void*)ADDRESS_FROM_EMU(read_arg(uc, 0)));
	}

	void br_vm_reg_sysevt_callback(uc_engine* uc) {
		vm_reg_sysevt_callback((void (*)(VMINT message, VMINT param))read_arg(uc, 0));
	}

	// Graphic

	void br_vm_graphic_get_screen_width(uc_engine* uc) {
		write_ret(uc, vm_graphic_get_screen_width());
	}

	void br_vm_graphic_get_screen_height(uc_engine* uc) {
		write_ret(uc, vm_graphic_get_screen_width());
	}

	void br_vm_graphic_create_layer(uc_engine* uc) {
		write_ret(uc, 
			vm_graphic_create_layer(
				read_arg(uc, 0),
				read_arg(uc, 1),
				read_arg(uc, 2),
				read_arg(uc, 3),
				read_arg(uc, 4)));
	}

	void br_vm_graphic_get_layer_buffer(uc_engine* uc) {
		write_ret(uc, ADDRESS_TO_EMU(vm_graphic_get_layer_buffer(read_arg(uc, 0))));
	}

	// Resources

	void br_vm_load_resource(uc_engine* uc) {
		write_ret(uc, 
			ADDRESS_TO_EMU(vm_load_resource(
				(char*)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				(VMINT*)ADDRESS_FROM_EMU(read_arg(uc, 1))
			)));
	}

	//ARModule

	void br_armodule_malloc(uc_engine* uc) {
		write_ret(uc, ADDRESS_TO_EMU(armodule.app_memory.malloc(read_arg(uc, 0))));
	}

	void br_armodule_realloc(uc_engine* uc) {
		write_ret(uc, ADDRESS_TO_EMU(
			armodule.app_memory.realloc(
				(size_t)ADDRESS_FROM_EMU(read_arg(uc, 0)),
				read_arg(uc, 1))));
	}

	void br_armodule_free(uc_engine* uc) {
		armodule.app_memory.free((size_t)ADDRESS_FROM_EMU(read_arg(uc, 0)));
	}

	std::vector<br_func> func_map =
	{
		{"vm_get_sym_entry", br_vm_get_sym_entry},
		{"vm_malloc", br_vm_malloc},
		{"vm_realloc", br_vm_realloc},
		{"vm_free", br_vm_free},
		{"vm_reg_sysevt_callback", br_vm_reg_sysevt_callback},
		{"vm_graphic_get_screen_width", br_vm_graphic_get_screen_width},
		{"vm_graphic_get_screen_height", br_vm_graphic_get_screen_height},
		{"vm_graphic_create_layer", br_vm_graphic_create_layer},
		{"vm_graphic_get_layer_buffer", br_vm_graphic_get_layer_buffer},
		{"vm_load_resource", br_vm_load_resource},
		{"armodule_malloc", br_armodule_malloc},
		{"armodule_realloc", br_armodule_realloc},
		{"armodule_free", br_armodule_free},
	};

	int vm_get_sym_entry(const char* symbol) {
		std::string str = symbol;

		int ret = 0;

		for (int i = 0; i < func_map.size(); ++i)
			if (func_map[i].name == str) {
				ret = (ADDRESS_TO_EMU(func_ptr) + i * 2) | 1;
				break;
			}

		if (ret == 0)
			ret = armodule.vm_get_sym_entry(symbol);

		printf("vm_get_sym_entry(%s) -> %08x\n", symbol, ret);

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

		func_ptr = (unsigned char*)Memory::shared_malloc(func_count * 2 + 2);

		for (int i = 0; i < func_count; ++i)
			func_ptr[i * 2] = bxlr[0], func_ptr[i * 2 + 1] = bxlr[1];

		func_ptr[func_count * 2] = idle_bin[0], func_ptr[func_count * 2 + 1] = idle_bin[1];

		idle_p = ADDRESS_TO_EMU(func_ptr + func_count * 2) | 1;

		uc_hook_add(uc, &trace, UC_HOOK_CODE, bridge_hoock, 0,
			ADDRESS_TO_EMU(func_ptr), ADDRESS_TO_EMU(func_ptr + func_count * 2 - 1));

		armodule.init(vm_get_sym_entry("armodule_malloc"), 
			vm_get_sym_entry("armodule_realloc"),
			vm_get_sym_entry("armodule_free"));
	}

	int run_cpu(unsigned int adr, int n, ...) {
		va_list factor;

		va_start(factor, n);
		if (n > 4)
			throw 1;
		for (int i = 0; i < n; i++) {
			unsigned int t = va_arg(factor, unsigned int);
			if (i >= 0)
				uc_reg_write(uc, UC_ARM_REG_R0 + i, &t);
		}
		va_end(factor);
		//write_reg(uc, UC_ARM_REG_LR, (uint64_t)stack_p);
		write_reg(uc, UC_ARM_REG_LR, (uint64_t)idle_p);
		uc_err err = uc_emu_start(uc, (adr), (uint64_t)idle_p & ~1, 0, 0);
		if (err) {
			printf("uc_emu_start returned %d (%s)\n", err, uc_strerror(err));
		}
		unsigned int r0 = 0;
		uc_reg_read(uc, UC_ARM_REG_R0, &r0);
		return r0;
	}
}