#include "ProgramManager.h"
#include "System.h"
#include "CharSet.h"
#include "../Memory.h"
#include "../AppManagerEvents.h"
#include "../AppContext.h"
#include <vmpromng.h>

VM_P_HANDLE vm_pmng_get_current_handle(void) {
	return AppContext::get_executing_app_phandle();;
}

void vm_reg_msg_proc(VM_MESSAGE_PROC proc) {
	MREngine::SystemCallbacks& sc = AppContext::get_executing_app_system_callbacks();
	sc.msg_proc = proc;
	//TODO
}

VMINT vm_post_msg(VM_P_HANDLE phandle, VMUINT msg_id, VMINT wparam, VMINT lparam) {
	AppContext::push_event(Event::system{ phandle, Event::message{ 
		vm_pmng_get_current_handle(), msg_id, wparam, lparam }});
	return 1; //TODO
}

void vm_start_app(const VMWCHAR* filepath, VMINT parent_app_handle, VMINT is_parent_exit) {
	auto u8str = ucs2_to_utf8((VMWSTR)filepath);

	AppContext::push_event(Event::launch{ Event::path_app{u8str, true}, parent_app_handle, (bool)is_parent_exit });
}

void vm_start_app_with_para(const VMWCHAR* filepath, VMINT parent_app_handle, VMINT is_parent_exit, void* parameter, VMINT parameter_size) {
	//TODO
}

void vm_exit_app(void) {
	AppContext::close_executing_app();
}