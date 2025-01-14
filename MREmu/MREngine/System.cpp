#include "System.h"
#include "IO.h"
#include "CharSet.h"
#include "../Memory.h"
#include <vmsys.h>
#include <string>
#include <cstring>
#include <filesystem>
#include <SFML/System/Clock.hpp>

#include <cstring>

namespace fs = std::filesystem;

//MRE API

VMINT vm_get_time(vm_time_t* time) {
	std::time_t t = std::time(nullptr);
	std::tm* const pTInfo = std::localtime(&t);
	time->year = 1900 + pTInfo->tm_year;
	time->mon = pTInfo->tm_mon;
	time->day = pTInfo->tm_mday;
	time->hour = pTInfo->tm_hour;
	time->min = pTInfo->tm_min;
	time->sec = pTInfo->tm_sec;
	return 0;
}

VMINT vm_get_curr_utc(VMUINT* utc) {
	if (utc == 0)
		return -1;
	*utc = std::time(nullptr);
	return 0;
}

float vm_get_sys_time_zone(void) {
	return 0;
}

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

	//printf("vm_calloc(%d) -> %#016x\n", size, adr);
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
	sc.sysevt = f;
}


VMUINT vm_get_mre_total_mem_size(void) {
	return shared_memory_size;
}

VMINT vm_get_tick_count(void) {
	extern sf::Clock global_clock;
	return global_clock.getElapsedTime().asMilliseconds();
}

VMINT vm_get_exec_filename(VMWSTR filename) {//TODO
	fs::path path = get_current_app_path();
	utf8_to_ucs2(path.u8string(), filename, 260);
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

VMINT vm_switch_power_saving_mode(power_saving_mode_enum mode){
	return 0;
}

VMBOOL vm_appmgr_is_installed(VMWSTR path, vm_install_id install_id) {
	return 0;
}

VMINT vm_appmgr_get_installed_list(VMINT flag, vm_install_id* install_id_list, VMUINT* num) {
	*num = 0;
	return 0; // tmp
}