#pragma once
#include <cstdint>
#include <vmpromng.h>

namespace MREngine {
	class SystemCallbacks {
	public:
		void (*sysevt)(VMINT message, VMINT param) = 0;

		int ph_app_id;
		VM_MESSAGE_PROC msg_proc = 0;
	};
}

MREngine::SystemCallbacks& get_current_app_system_callbacks();