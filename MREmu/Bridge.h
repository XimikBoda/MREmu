#pragma once
#include "Cpu.h"
#include "Memory.h"
#include <cstdint>

namespace Bridge {
	void init();
	int vm_get_sym_entry(const char* symbol);

	// True once init() has built the VM dispatch table and installed the hook, so
	// tooling can tell "no calls traced yet" apart from "not able to trace".
	bool dispatch_ready();
	void* vm_get_sym_entry_native(const char* symbol);
	int run_cpu(uint32_t adr, int n, ...);
	int ads_start(uint32_t entry, uint32_t vm_get_sym_entry_p, uint32_t data_base, uint32_t heap_base, uint32_t heap_limit, uint32_t stack_size);

	// Headless inspection support. set_step_limit bounds uc_emu_start to a fixed
	// instruction count instead of running until an error or the idle handler.
	// 0 means unlimited, which is the default and the only behaviour the GUI uses.
	void set_step_limit(uint64_t instructions);
	uint32_t get_pc();

	// Run control for external tooling (the MCP host).
	//
	// The app is slice-driven: every entry re-enters run_cpu, which runs until the
	// app returns to its idle handler or the slice budget runs out. Pausing makes
	// run_cpu yield at the next slice boundary, so the app stops consuming CPU but
	// its timers still fire - resuming clears the flag and the app picks up again on
	// its next timer. These are plain atomics, safe to set from another thread; all
	// Unicorn access stays on the AppManager thread.
	void set_paused(bool on);
	bool paused();

	// Instructions the next run_cpu slices are allowed to execute before yielding.
	// 0 means "run to idle", the default.
	void set_step_budget(uint64_t instructions);
	uint64_t step_budget();

	// Instructions executed by *bounded* slices, and the number of VM API calls
	// seen since start. An unbounded slice runs to idle and its exact length is not
	// knowable here, so total instructions are deliberately not reported - use
	// vm_call_count() as the always-on activity metric instead.
	uint64_t bounded_instructions();
	uint64_t vm_call_count();

	// VM API call tracer. When enabled, bridge_hoock records every VM entry point
	// the app jumps through, with PC, index and the first four argument registers
	// read before and after the call. Off by default: this is a hot path.
	struct TraceCall {
		uint32_t pc;
		uint32_t index;
		uint32_t argc;
		uint32_t args_in[4];
		uint32_t ret;
	};
	void set_trace_enabled(bool on);
	bool trace_enabled();
	// Snapshot of the recorded calls. Caller must copy under the trace lock.
	std::vector<TraceCall> trace_snapshot();
	std::vector<std::string> trace_api_names();
	void trace_clear();
	size_t trace_count();
	// Calls observed but dropped because the trace buffer hit its cap.
	uint64_t trace_dropped();

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
			auto raw_ret = Bridge::run_cpu(ADDRESS_TO_EMU((void*)func), n, prepare_emu_arg(args)...);
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