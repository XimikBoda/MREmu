#include "ProgramManager.h"
#include "System.h"
#include "CharSet.h"
#include "../Memory.h"
#include "../AppManager.h"
#include "../AppContext.h"
#include <vmpromng.h>

extern AppManager* g_appManager;

VM_P_HANDLE vm_pmng_get_current_handle(void) {
	return AppContext::get_executing_app_phandle();;
}

void vm_reg_msg_proc(VM_MESSAGE_PROC proc) {
	MREngine::SystemCallbacks& sc = AppContext::get_executing_app_system_callbacks();
	sc.msg_proc = proc;
	//TODO
}

void add_message_event(int phandle, unsigned int msg_id,
	int wparam, int lparam, int phandle_sender);

VMINT vm_post_msg(VM_P_HANDLE phandle, VMUINT msg_id, VMINT wparam, VMINT lparam) {
	add_message_event(phandle, msg_id, wparam, lparam, vm_pmng_get_current_handle());
	return 1; //TODO
}

void vm_start_app(const VMWCHAR* filepath, VMINT parent_app_handle, VMINT is_parent_exit) {
	auto u8str = ucs2_to_utf8((VMWSTR)filepath);

	g_appManager->add_app_for_launch(u8str, true);
}

void vm_start_app_with_para(const VMWCHAR* filepath, VMINT parent_app_handle, VMINT is_parent_exit, void* parameter, VMINT parameter_size) {
	
}

void vm_exit_app(void) {
	g_appManager->close_app(vm_pmng_get_current_handle(), false);
}