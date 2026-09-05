#pragma once
#include <cstdint>
#include <string>

typedef struct uc_struct uc_engine;

namespace Cpu {
	void init();
	void deinit();
	void imgui_REG();
	void trace_on();
	void stop();

	void push_cpu();
	void pop_cpu();

	void add_hook(int type, void* callback, void* user_data, uint64_t begin, uint64_t end);
	void printREG(uc_engine* uc);
	std::string dumpREG(uc_engine* uc);
	std::string get_app_name();
};