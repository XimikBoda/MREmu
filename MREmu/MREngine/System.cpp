#include "System.h"
#include "../Memory.h"
#include <vmsys.h>
#include <vmgettag.h>
#include <string>

//MRE API

malloc_stat_t* vm_get_malloc_stat(void) {
	return 0; //TODO: make this correct
}

void* vm_malloc(int size) {
	return Memory::app_malloc(size);
}

void* vm_calloc(int size) {
	void* adr = vm_malloc(size);
	if (adr)
		memset(adr, 0, size);
	return adr;
}

void* vm_realloc(void* p, int size) {
	return Memory::app_realloc(p, size);
}

void vm_free(void* ptr) {
	Memory::app_free(ptr);
}

void vm_reg_sysevt_callback(void (*f)(VMINT message, VMINT param)) {
	MREngine::SystemCallbacks& sc = get_current_app_system_callbacks();
	sc.sysevt = (uint32_t)f;
}


VMUINT vm_get_mre_total_mem_size(void) {
	return shared_memory_size;
}

VMINT vm_get_exec_filename(VMWSTR filename) {//TODO
	swprintf_s((wchar_t*)filename, 260, L"e:\\TEMP");
	return 0;
}

VMUINT vm_get_sys_property(const VMINT key, VMCHAR* value, VMUINT len) { // TODO
	const char* str = "";
	switch (key) {
	case MRE_SYS_EQUIPMENT_ID:
		str = "MREmu";
		break;
	case MRE_SYS_HOST_MAX_MEM:
	{
		static std::string mem = std::to_string(vm_get_mre_total_mem_size() / 1024);
		str = mem.c_str();
		break;
	}
	}
	if (value) {
		size_t len_to_cpy = std::min<size_t>(strlen(str) + 1, len);
		memcpy(value, str, len_to_cpy);
		return len_to_cpy;
	}
	return 0;
}

VMINT vm_get_vm_tag(short* filename, int tag_num, void* buf, int* buf_size) { // TODO
	//if (tag_num == 0xF) {
	//	*(uint32_t*)buf = 1500; // TEMP
	//	return 1;
	//}
	return 0;
}