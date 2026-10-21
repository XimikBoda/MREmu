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

		VM_MESSAGE_PROC msg_proc = 0;
	};
}

MREngine::SystemCallbacks& get_current_app_system_callbacks();