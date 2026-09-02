#pragma once
#include <cstdint>

namespace Cpu {
	void init();
	void deinit();
	void imgui_REG();
	void trace_on();
	void stop();

	void push_cpu();
	void pop_cpu();

	void add_hook(int type, void* callback, void* user_data, uint64_t begin, uint64_t end);
};