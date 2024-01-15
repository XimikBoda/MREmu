#include "Disassembler.h"

Disassembler::Disassembler()
{
	cs_err cs_error;

	cs_error = cs_open(CS_ARCH_ARM, CS_MODE_ARM, &cs_arm);
	if (cs_error) {
		printf("Failed on cs_open() with error returned: %u\n", cs_error);
		abort();
	}
	cs_error = cs_option(cs_arm, CS_OPT_DETAIL, CS_OPT_ON);
	if (cs_error) {
		printf("Failed on cs_option() with error returned: %u\n", cs_error);
		abort();
	}
	cs_error = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &cs_thumb);
	if (cs_error) {
		printf("Failed on cs_open() with error returned: %u\n", cs_error);
		abort();
	}
	cs_error = cs_option(cs_thumb, CS_OPT_DETAIL, CS_OPT_ON);
	if (cs_error) {
		printf("Failed on cs_option() with error returned: %u\n", cs_error);
		abort();
	}
}

bool Disassembler::disasm_one(cs_insn* ret_insn, unsigned char* code, size_t size, uint32_t address, bool is_thumb)
{
	cs_insn* insn;
	bool ret = false;

	size_t count = cs_disasm((is_thumb ? cs_thumb : cs_arm), code, size, address, 1, &insn);
	if (count)
		*ret_insn = insn[0], ret = true;

	cs_free(insn, count);
	return ret;
}
