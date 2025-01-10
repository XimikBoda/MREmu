#include "ProgramManager.h"
#include "System.h"
#include "../Memory.h"
#include <vmpromng.h>

VM_P_HANDLE vm_pmng_get_current_handle(void) {
	MREngine::SystemCallbacks& sc = get_current_app_system_callbacks();
	return sc.ph_app_id; //TODO
}

void vm_reg_msg_proc(VM_MESSAGE_PROC proc) {
	MREngine::SystemCallbacks& sc = get_current_app_system_callbacks();
	sc.msg_proc = proc;
	//TODO
}

void add_message_event(int phandle, unsigned int msg_id,
	int wparam, int lparam, int phandle_sender);

VMINT vm_post_msg(VM_P_HANDLE phandle, VMUINT msg_id, VMINT wparam, VMINT lparam) {
	add_message_event(phandle, msg_id, wparam, lparam, vm_pmng_get_current_handle());
	return 1; //TODO
}