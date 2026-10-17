#pragma once
#include "Cpu.h"
#include "Memory.h"
#include <cstdint>

namespace Bridge {
	void init();
	int vm_get_sym_entry(const char* symbol);
	void* vm_get_sym_entry_native(const char* symbol);
	int run_cpu(uint32_t adr, int n, ...);
	int ads_start(uint32_t entry, uint32_t vm_get_sym_entry_p, uint32_t data_base, uint32_t heap_base, uint32_t heap_limit, uint32_t stack_size);

	// Headless inspection support. set_step_limit bounds uc_emu_start to a fixed
	// instruction count instead of running until an error or the idle handler.
	// 0 means unlimited, which is the default and the only behaviour the GUI uses.
	void set_step_limit(uint64_t instructions);
	uint32_t get_pc();

	template <typename T>
	uint32_t prepare_emu_arg(T arg) {
		if constexpr (std::is_pointer_v<T>) {
			return (uint32_t)ADDRESS_TO_EMU((void*)arg);
		}
		else {
			return (uint32_t)arg;
		}
	}

	template <bool from_hook, typename Func, typename... Args>
	auto run(bool is_native, Func func, Args... args) {
		if (is_native)
			return func(std::forward<Args>(args)...);
		else {
			using RetType = decltype(func(std::forward<Args>(args)...));
			int n = sizeof...(args);

			if constexpr (from_hook) Cpu::push_cpu();
			auto raw_ret = Bridge::run_cpu(FUNC_TO_UINT32(func), n, prepare_emu_arg(args)...);
			if constexpr (from_hook) Cpu::pop_cpu();

			if constexpr (std::is_void_v<RetType>)
				return;
			else if constexpr (std::is_pointer_v<RetType>)
				return (RetType)ADDRESS_FROM_EMU((uint32_t)raw_ret);
			else
				return (RetType)raw_ret;
		}
	}
}