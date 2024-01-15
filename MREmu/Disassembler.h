#pragma once
#include <capstone/capstone.h>

class Disassembler {
	csh cs_arm, cs_thumb;
public:
	Disassembler();
	bool disasm_one(cs_insn* ret_insn, unsigned char* code, size_t size, uint32_t address, bool is_thumb);
};