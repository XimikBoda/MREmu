#pragma once
#include <string_view>

namespace GDB {
	enum CpuState
	{
		Stop,
		Step,
		Work
	};

	extern CpuState cpu_state;
	extern bool gdb_mode;
	extern int gdb_port;

	void wait();
	void update();
	void make_answer(std::string_view str);
}
