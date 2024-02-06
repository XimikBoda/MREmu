#pragma once
#include <cstdint>


namespace MREngine {
	class SystemCallbacks {
	public:
		uint32_t sysevt = 0;

		int ph_app_id;
		uint32_t msg_proc = 0;
	};
}

MREngine::SystemCallbacks& get_current_app_system_callbacks();