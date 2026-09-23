#include "Cpu.h"
#include "Memory.h"
#include "Log.h"
#include <spdlog/spdlog.h>
#include <cstdint>

#ifndef __EMSCRIPTEN__
#include <unicorn/unicorn.h>
uc_engine* uc = 0;
#else
// Stub Unicorn handles and C API symbols for WASM target
typedef void* uc_engine;
typedef int uc_err;
typedef int uc_mem_type;

void* uc = nullptr;

extern "C" {
    uc_err uc_open(int arch, int mode, uc_engine** uc) { if (uc) *uc = (uc_engine)0x1; return 0; }
    uc_err uc_close(uc_engine uc) { return 0; }
    uc_err uc_emu_start(uc_engine uc, uint64_t begin, uint64_t until, uint64_t timeout, size_t count) { return 0; }
    uc_err uc_emu_stop(uc_engine uc) { return 0; }
    uc_err uc_reg_read(uc_engine uc, int regid, void* value) { if (value) *(uint32_t*)value = 0; return 0; }
    uc_err uc_reg_write(uc_engine uc, int regid, const void* value) { return 0; }
    uc_err uc_mem_map(uc_engine uc, uint64_t address, size_t size, uint32_t perms) { return 0; }
    uc_err uc_mem_map_ptr(uc_engine uc, uint64_t address, size_t size, uint32_t perms, void* ptr) { return 0; }
    uc_err uc_mem_read(uc_engine uc, uint64_t address, void* bytes, size_t size) { return 0; }
    uc_err uc_mem_write(uc_engine uc, uint64_t address, const void* bytes, size_t size) { return 0; }
    uc_err uc_hook_add(uc_engine uc, void* handle, int type, void* callback, void* user_data, uint64_t begin, uint64_t end, ...) { return 0; }
    uc_err uc_context_alloc(uc_engine uc, void** context) { if (context) *context = (void*)0x1; return 0; }
    uc_err uc_context_save(uc_engine uc, void* context) { return 0; }
    uc_err uc_context_restore(uc_engine uc, void* context) { return 0; }
    uc_err uc_context_free(void* context) { return 0; }
    const char* uc_strerror(uc_err code) { return "OK"; }
}
#endif

namespace Cpu {
	void* stack_p = 0;
	size_t stack_size = 128 * 1024;

	int cur_cpu_id = -1;

	struct hook_t {
		int type;
		void* callback;
		void* user_data;
		uint64_t begin;
		uint64_t end;
	};

	std::vector<hook_t> hooks;

	void printREG(void* uc_ptr) {
		spdlog::info("Cpu::printREG stub called");
	}

	void imgui_REG() {
	}

	void init() {
		stack_p = Memory::shared_malloc(stack_size);
		if (stack_p == 0)
			abort();

		push_cpu();
	}

	void push_cpu() {
		cur_cpu_id++;
	}

	void pop_cpu() {
		if (cur_cpu_id > 0)
			--cur_cpu_id;
	}

	void add_hook(int type, void* callback, void* user_data, uint64_t begin, uint64_t end) {
		hook_t hook = { type, callback, user_data, begin, end };
		hooks.push_back(hook);
	}

	void trace_on() {
	}

	void stop() {
	}
};
