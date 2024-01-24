#include "SIM.h"
#include "../Memory.h"
#include <vmsim.h>

static void* imei_ptr = 0;

void MREngine::SIM::init()
{
	imei_ptr = Memory::shared_malloc(20);
	memcpy(imei_ptr, "1234567890123456", 17);
}

VMSTR vm_get_imei(void) {
	return (VMSTR)imei_ptr;
}