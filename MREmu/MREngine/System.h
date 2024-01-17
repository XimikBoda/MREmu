#pragma once
#include <cstdint>


namespace MREngine {
	class SystemCallbacks {
	public:
		uint32_t sysevt = 0;
	};
}

MREngine::SystemCallbacks& get_current_app_system_callbacks();