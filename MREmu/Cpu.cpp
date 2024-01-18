#include "Cpu.h"
#include "Memory.h"
#include "Disassembler.h"
#include <unicorn/unicorn.h>
#include <capstone/capstone.h>

uc_engine* uc = 0;

Disassembler dism; //TODO

namespace Cpu {
	void* stack_p = 0;
	size_t stack_size = 100 * 1024;

	uc_hook uc_hu;

	static void write_reg(uc_engine* uc, int reg, unsigned int val) {
		uc_reg_write(uc, reg, &val);
	}

	void printREG(uc_engine* uc) {
		uint32_t v, cpsr;

		uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
		printf("==========================REG=================================\n");
		uc_reg_read(uc, UC_ARM_REG_R0, &v); printf(" R0=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R1, &v); printf("R1=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R2, &v); printf(" R2=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R3, &v); printf(" R3=0x%08X\tN:%d\n", v, (cpsr & (1 << 31)) >> 31);

		uc_reg_read(uc, UC_ARM_REG_R4, &v); printf(" R4=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R5, &v); printf("R5=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R6, &v); printf(" R6=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R7, &v); printf(" R7=0x%08X\tZ:%d\n", v, (cpsr & (1 << 30)) >> 30);

		uc_reg_read(uc, UC_ARM_REG_R8, &v); printf(" R8=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R9, &v); printf("R9=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R10, &v); printf("R10=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_R11, &v); printf("R11=0x%08X\tC:%d\n", v, (cpsr & (1 << 29)) >> 29);

		uc_reg_read(uc, UC_ARM_REG_R12, &v); printf("R12=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_SP, &v); printf("SP=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_LR, &v); printf(" LR=0x%08X\t", v);
		uc_reg_read(uc, UC_ARM_REG_PC, &v); printf(" PC=0x%08X\tV:%d\tThumb:%d\n", v, (cpsr & (1 << 28)) >> 28, (cpsr & (1 << 5)) >> 5);
		printf("==============================================================\n");
	}

	static void print_code(unsigned char* data, int size) {
		printf("Code:");
		for (int i = 0; i < size; ++i)
			printf(" %#04X,", data[i]);
		printf("\n");
	}

	static void hook_code(uc_engine* uc, uint64_t address, uint32_t size, void* user_data)
	{
		printREG(uc);

		uint32_t cpsr;
		uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
		bool is_thumb = cpsr & (1 << 5);

		unsigned char code[8];
		uc_mem_read(uc, address, code, size);

		cs_insn insn;
		if (dism.disasm_one(&insn, code, size, address, is_thumb))
			printf("0x%" PRIx64 ":\t%s\t%s\n", insn.address, insn.mnemonic, insn.op_str);
		else
			printf("Failed for disasm\n");

		print_code(code, size);
	}

	static void hook_read(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data)
	{
		unsigned char code[8];
		uc_mem_read(uc, address, code, size);
		printf(">>> Read block at 0x%08X, block size = 0x%08X\n", (int)address, size);
		print_code(code, size);
	}

	static void hook_write(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value, void* user_data)
	{
		unsigned char code[8];
		uc_mem_read(uc, address, code, size);
		printf(">>> Write block at 0x%08X, block size = 0x%08X, val = 0x%08llX\n", (int)address, size, value);
		print_code(code, size);
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

		uc_hook_add(uc, &uc_hu, UC_HOOK_MEM_WRITE, hook_write, 0, 1, 0);
		uc_hook_add(uc, &uc_hu, UC_HOOK_MEM_READ, hook_read, 0, 1, 0);
		uc_hook_add(uc, &uc_hu, UC_HOOK_CODE, hook_code, 0, 0, 0x100000000);
	}

};