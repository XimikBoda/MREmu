#pragma once

// Minimal MCP (Model Context Protocol) server for MREmu, spoken over stdio.
//
// The emulator runs on its own thread while this serves JSON-RPC on stdin/stdout,
// so tool calls never block emulation. Shared mutable emulator state is reached
// through the control block below rather than touched directly, and every access
// is serialised by its mutex.

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace Mcp {

// Commands are queued from the server thread and drained by the emulator thread,
// so the emulator never calls back into the transport.
struct Command {
	std::string kind; // "load" | "close" | "step" | "pause" | "resume" | "key" | "touch" | "reset"
	std::string a, b;
	int n = 0;
};

// Everything the server needs to see, refreshed by the emulator thread.
struct Snapshot {
	bool app_running = false;
	std::string app_name;
	uint32_t pc = 0;
	uint32_t cpsr = 0;
	uint32_t regs[16] = {}; // r0-r12, sp, lr
	bool running = true;
	// "running" | "paused". The app is slice-driven, so "paused" means it yields at
	// each slice boundary; its timers still fire and it resumes on emu_pause(false).
	std::string run_state = "running";
	// Instructions executed by bounded (stepped) slices, and VM API calls seen
	// since start. Unbounded slices run to idle and their length is not knowable,
	// so no total-instruction figure is reported.
	uint64_t stepped_instructions = 0;
	uint64_t vm_calls = 0;
	uint64_t step_budget = 0;
	// Timers the most recent emu_step made due. 0 means the app had nothing
	// scheduled, so a step legitimately has nothing to execute.
	int timers_made_due = 0;
	// True once the emulator has raised a hard-reset fault. The CPU is left as-is;
	// reload a VXP to recover. Cleared by a successful emu_load_vxp.
	bool faulted = false;
	std::string last_error;
	bool trace_ready = false;
	int screen_w = 0, screen_h = 0;
	uint64_t screen_hash = 0;
	size_t apps_loaded = 0;
};

class Control {
public:
	// --- written by the emulator thread ---
	void publish(const Snapshot& s);
	// Drain-and-execute, called only by the emulator thread.
	bool take(Command& out);

	// --- written by the server thread, read by the emulator thread ---
	void push(Command c);
	void request_pause(bool on) { pause_requested_.store(on); }
	bool pause_requested() const { return pause_requested_.load(); }

	// --- thread-safe readers ---
	Snapshot snapshot() const;
	std::vector<Command> pending();

	void set_trace_request(bool on) { trace_request_.store(on); }
	void clear_trace_request() { trace_request_.store(false); }
	bool take_trace_request(bool& out);

	// Errors surfaced by the emulator thread for the agent to read.
	void note_error(const std::string& e);
	std::vector<std::string> errors() const;
	void clear_errors();

private:
	mutable std::mutex snap_mtx_;
	Snapshot snap_;

	mutable std::mutex cmd_mtx_;
	std::deque<Command> cmds_;

	mutable std::mutex err_mtx_;
	std::deque<std::string> errs_;

	std::atomic<bool> pause_requested_{false};
	std::atomic<bool> trace_request_{false};
};

// Host-side entry points, implemented by McpHost.cpp. Keeping this table in the
// header lets the transport stay free of emulator headers while still being
// driven by them.
struct HostApi {
	std::string (*load_vxp)(const std::string& path);
	bool (*close_app)(int index);
	bool (*write_reg)(const std::string& name, unsigned value);
	bool (*inject_key)(int key, bool down);
	bool (*inject_touch)(int x, int y, bool down);
	std::string (*screen_png_base64)(int max_side);
	std::vector<std::string> (*list_apps)(void);
	bool (*set_trace)(bool on);
	std::string (*trace_dump)(size_t offset, size_t limit);
	std::size_t (*trace_count)(void);
	void (*trace_clear)(void);
	std::vector<std::string> (*trace_api_names)(void);
	bool (*step)(uint64_t instructions);
	void (*set_pause)(bool on);
};

// Reserve the real stdout for the JSON-RPC stream and point fd 1 at stderr, so
// the spdlog sink, printf and SFML cannot corrupt a reply. Returns a stream
// bound to the ORIGINAL stdout, or nullptr on failure. MUST be called before any
// emulator initialisation or logging, not merely before serve().
std::ostream* claim_stdout_for_protocol();

// Install the host implementation. Must be called before serve().
void set_host(const HostApi* api);

// Serve MCP on the given streams until EOF. Returns process exit code.
int serve(std::istream& in, std::ostream& out, Control& ctl);

} // namespace Mcp