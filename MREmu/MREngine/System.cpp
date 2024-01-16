#include "System.h"
#include "../Memory.h"
#include <vmsys.h>

//MRE API

void* vm_malloc(int size) {
	return Memory::app_malloc(size);
}

void vm_free(void* ptr) {
	Memory::app_free(ptr);
}