#include "System.h"
#include "../Memory.h"
#include <vmsys.h>

//MRE API

void* vm_malloc(int size) {
	return Memory::app_malloc(size);
}

void* vm_calloc(int size) {
	void* adr = vm_malloc(size);
	if (adr)
		memset(adr, 0, size);
	return adr;
}

void vm_free(void* ptr) {
	Memory::app_free(ptr);
}

void vm_reg_sysevt_callback(void (*f)(VMINT message, VMINT param)) {
	MREngine::SystemCallbacks& sc = get_current_app_system_callbacks();
	sc.sysevt = (uint32_t)f;
}