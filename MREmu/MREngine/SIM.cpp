#include "SIM.h"
#include "../Memory.h"
#include <vmsim.h>
#include <cstring>

static void* imei_ptr = 0;
static void* imsi_ptr = 0;

void MREngine::SIM::init()
{
	imei_ptr = Memory::shared_malloc(20);
	memcpy(imei_ptr, "1234567890123456", 17);
	imsi_ptr = Memory::shared_malloc(20);
	memcpy(imsi_ptr, "123456789012345", 16);
}

operator_t vm_get_operator(void) {
	return UNKOWN_OPERATOR;
}

VMINT vm_has_sim_card(void) {
	return 1;
}

VMSTR vm_get_imei(void) {
	return (VMSTR)imei_ptr;
}

VMSTR vm_get_imsi(void) {
	return (VMSTR)imsi_ptr;
}

VMINT vm_sim_card_count(void) {
	return 1;
}

VMINT vm_set_active_sim_card(VMINT card_id) {
	return card_id == 1;
}

vm_sim_state_t vm_get_sim_card_status(VMINT card_id) {
	if (card_id == 1)
		return VM_SIM_STATE_WORKING;
	else
		return VM_SIM_STATE_VACANT;
}

VMINT vm_query_operator_code(VMCHAR* buffer, VMUINT buffer_size) {
	if (buffer == 0 || buffer_size <= 3)
		return -1;
	strcpy(buffer, "+0");
	return 0;
}

VMINT vm_sim_get_active_sim_card(void) {
	return 1;
}

VMINT vm_sim_max_card_count(void) {
	return 1;
}

VMINT vm_sim_get_prefer_sim_card(void) {
	return 1;
}