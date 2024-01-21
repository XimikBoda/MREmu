#include "ProgramManager.h"
#include <vmpromng.h>

VM_P_HANDLE vm_pmng_get_current_handle(void) {
	return 1; //TODO
}

void vm_reg_msg_proc(VM_MESSAGE_PROC proc) {
	//TODO
}

VMINT vm_post_msg(VM_P_HANDLE phandle, VMUINT msg_id, VMINT wparam, VMINT lparam) {
	return 1; //TODO
}