#pragma once
#include <cstdint>
#include <variant>
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

namespace Event {
	struct message {
		int phandle_sender;
		unsigned int msg_id;
		int wparam;
		int lparam;
	};

	struct system_msg {
		int message;
		int param;
	};

	struct system {
		int phandle;
		std::variant<message, system_msg> data;
	};
}