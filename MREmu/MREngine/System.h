#pragma once
#include <cstdint>
#include <vmpromng.h>

namespace MREngine {
	namespace System {
		void init();
	}

	class SystemCallbacks {
	public:
		void (*sysevt)(VMINT message, VMINT param) = 0;

		int ph_app_id;
		VM_MESSAGE_PROC msg_proc = 0;
	};
}

MREngine::SystemCallbacks& get_current_app_system_callbacks();

VMINT vm_global_get_max_alloc_size(void);
void* vm_global_malloc(unsigned int size);
void vm_global_free(void* ptr);