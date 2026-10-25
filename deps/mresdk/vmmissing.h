// There api that missing in MRE SDK
#pragma once 
#include "vmsys.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log
void vm_app_log(char* str); 

// Res
VMINT vm_get_res_header();

// CharSet
VMWSTR vm_ucs2_string(VMSTR s);

#ifdef __cplusplus
}
#endif