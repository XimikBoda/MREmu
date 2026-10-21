#include "McpServer.h"

#include "App.h"
#include "AppManager.h"
#include "ArmApp.h"
#include "Bridge.h"
#include "Cpu.h"
#include "Device/KeyboardControl.h"
#include "Log.h"
#include "MREmu.h"
#include "MREngine/CharSet.h"
#include "MREngine/Graphic.h"
#include "MreTags.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <sstream>
#include <thread>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <unicorn/unicorn.h>

// Host side of the MCP server. This is the only file that knows how to reach
// emulator internals; McpServer.cpp stays transport-only.
//
// Threading: the emulator thread calls McpHost::tick() once per frame to publish
// a snapshot, drain queued commands and pump the emulation budget. MCP tool calls
// run on the stdio thread and only enqueue commands or read mutex-guarded state,
// so nothing blocks emulation.

extern uc_engine* uc;

namespace McpHost {

using Mcp::Command;

AppManager* g_app_manager = nullptr;
KeyboardControl* g_keyboard = nullptr;
Mcp::Control* g_ctl = nullptr;
MREngine::Graphic* g_graphic = nullptr;

static std::atomic<bool> g_quit{false};
static std::atomic<bool> g_trace_on{false};
// Timers the most recent step made due. Published in the snapshot so an agent can
// tell "stepped and the app had nothing scheduled" from "step did not take effect".
static std::atomic<int> g_timers_made_due{0};
// Set when the emulator raises a hard-reset fault (typically uc_emu_start failing
// mid-slice). The faulted CPU is left as-is rather than auto-reset: a reset would
// tear down every app and discard the agent's context, so the agent is told and
// decides whether to reload.
static std::atomic<bool> g_faulted{false};
static std::mutex g_last_error_mtx;
static std::string g_last_error;

// The host table type lives in McpServer.h so the transport can call it without
// emulator headers; declared here as a pointer to avoid duplicating the layout.
static const Mcp::HostApi*& api_ref();
static const Mcp::HostApi& api();

// SFML 2's Image has no resize(); this SFML2-compatible scaler samples the source
// with nearest-neighbour into a fresh image.
static sf::Image scale_image(const sf::Image& src, unsigned nw, unsigned nh) {
	sf::Image out;
	out.create(nw, nh);
	unsigned sw = src.getSize().x, sh = src.getSize().y;
	if (!sw || !sh) return out;
	for (unsigned y = 0; y < nh; y++) {
		unsigned sy = (unsigned)((uint64_t)y * sh / nh);
		if (sy >= sh) sy = sh - 1;
		for (unsigned x = 0; x < nw; x++) {
			unsigned sx = (unsigned)((uint64_t)x * sw / nw);
			if (sx >= sw) sx = sw - 1;
			out.setPixel(x, y, src.getPixel(sx, sy));
		}
	}
	return out;
}

static std::string b64_encode(const std::vector<unsigned char>& in) {
	static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve(((in.size() + 2) / 3) * 4);
	size_t i = 0;
	for (; i + 2 < in.size(); i += 3) {
		unsigned v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8) | in[i + 2];
		out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63];
		out += T[(v >> 6) & 63];  out += T[v & 63];
	}
	if (i < in.size()) {
		unsigned v = (unsigned)in[i] << 16;
		bool two = (i + 1 < in.size());
		if (two) v |= (unsigned)in[i + 1] << 8;
		out += T[(v >> 18) & 63];
		out += T[(v >> 12) & 63];
		out += two ? T[(v >> 6) & 63] : '=';
		out += '=';
	}
	return out;
}

// ---------------------------------------------------------------- register I/O

static bool reg_id(const std::string& n, int& out) {
	static const char* names[] = { "r0","r1","r2","r3","r4","r5","r6","r7",
		"r8","r9","r10","r11","r12","sp","lr","pc" };
	for (int i = 0; i < 16; i++) {
		if (n == names[i]) { out = (int)UC_ARM_REG_R0 + i; return true; }
	}
	if (n == "cpsr") { out = UC_ARM_REG_CPSR; return true; }
	return false;
}

static unsigned reg_read(int id) {
	unsigned v = 0;
	if (uc) uc_reg_read(uc, id, &v);
	return v;
}

// Write a register and reflect it in the published snapshot.
static bool reg_write(const std::string& n, unsigned val) {
	if (!uc) return false;
	int id;
	if (!reg_id(n, id)) return false;
	if (uc_reg_write(uc, id, &val) != UC_ERR_OK) return false;
	// Apply to the live snapshot too. Without this a write is invisible to the
	// next emu_regs call, because the emulator thread republishes from the CPU
	// only on its next slice and a paused CPU never advances.
	Mcp::Snapshot s = g_ctl ? g_ctl->snapshot() : Mcp::Snapshot();
	static const char* names[] = { "r0","r1","r2","r3","r4","r5","r6","r7",
		"r8","r9","r10","r11","r12","sp","lr","pc" };
	for (int i = 0; i < 16; i++) {
		if (n == names[i]) {
			s.regs[i] = val;
			if (i == 15) s.pc = val;
			break;
		}
	}
	if (n == "cpsr") s.cpsr = val;
	if (g_ctl) g_ctl->publish(s);
	return true;
}

// ---------------------------------------------------------------- host tools

static std::string load_vxp(const std::string& path) {
	if (!g_app_manager) return "error: app manager not ready";
	Command c;
	c.kind = "load";
	c.a = path;
	g_ctl->push(c);
	return "queued load of " + path;
}

static bool close_app(int index) {
	if (!g_app_manager) return false;
	if (index < 0 || (std::size_t)index >= g_app_manager->app_count()) return false;
	g_app_manager->close_app(index, false);
	return true;
}

static std::vector<std::string> list_apps() {
	std::vector<std::string> v;
	if (!g_app_manager) return v;
	for (std::size_t i = 0; i < g_app_manager->app_count(); i++) {
		App* a = g_app_manager->app_at(i);
		if (!a) continue;
		std::ostringstream o;
		o << "index=" << i << " name=" << u8_to_utf8(a->app_name)
		  << " running=" << ((int)a->state == (int)App::Runned ? "true" : "false")
		  << " segments_size=" << a->segments_size
		  << " mem_size=" << a->mem_size;
		v.push_back(o.str());
	}
	return v;
}

static std::string screen_png_base64(int max_side) {
	if (!g_graphic) return "";
	try {
		g_graphic->update_screen();
		sf::Image img = g_graphic->screen_tex.copyToImage();

		if (max_side > 0 && (img.getSize().x > max_side || img.getSize().y > max_side)) {
			unsigned nw = img.getSize().x, nh = img.getSize().y;
			if (nw >= nh) nw = (unsigned)max_side;
			else nh = (unsigned)max_side;
			img = scale_image(img, nw, nh);
		}

		// SFML 2 takes the format as a string rather than an enum.
		std::vector<unsigned char> buf;
		if (!img.saveToMemory(buf, "png")) return "";
		return b64_encode(buf);
	}
	catch (...) {
		return "";
	}
}

static bool inject_key(int key, bool down) {
	if (!g_keyboard) return false;
	if (down) g_keyboard->press_key(key, KeyboardControl::ImGui);
	else g_keyboard->unpress_key(key);
	return true;
}

static bool inject_touch(int x, int y, int event) {
	// Touch injection is handled on the emulator thread; the command is queued.
	Mcp::Command c;
	c.kind = "touch";
	c.n = event;
	char buf[32];
	snprintf(buf, sizeof(buf), "%d,%d", x, y);
	c.a = buf;
	g_ctl->push(c);
	return true;
}

static std::string read_mem(uint32_t addr, size_t size) {
	if (!uc) return "error: cpu not initialized";
	if (size == 0) return "error: size must be greater than 0";
	if (size > 4096) size = 4096;

	std::vector<uint8_t> buf(size);
	uc_err err = uc_mem_read(uc, addr, buf.data(), size);
	if (err != UC_ERR_OK) {
		char err_msg[96];
		snprintf(err_msg, sizeof(err_msg), "error: uc_mem_read failed (code %d: %s) at 0x%08X",
			(int)err, uc_strerror(err), addr);
		return err_msg;
	}

	std::ostringstream o;
	char header[64];
	snprintf(header, sizeof(header), "0x%08X (%zu bytes):\n", addr, size);
	o << header;

	for (size_t i = 0; i < size; i += 16) {
		char hex_part[64] = "";
		char ascii_part[17] = "";
		size_t chunk = std::min<size_t>(16, size - i);
		for (size_t j = 0; j < 16; j++) {
			if (j < chunk) {
				char b[4];
				snprintf(b, sizeof(b), "%02X ", buf[i + j]);
				strcat(hex_part, b);
				uint8_t c = buf[i + j];
				ascii_part[j] = (c >= 32 && c <= 126) ? (char)c : '.';
			} else {
				strcat(hex_part, "   ");
			}
		}
		ascii_part[chunk] = '\0';
		char line[128];
		snprintf(line, sizeof(line), "  %08X:  %-48s |%s|\n",
			addr + (uint32_t)i, hex_part, ascii_part);
		o << line;
	}
	return o.str();
}

static bool set_trace(bool on) {
	g_trace_on = on;
	Bridge::set_trace_enabled(on);
	return true;
}

static size_t trace_count() { return Bridge::trace_count(); }
static void trace_clear() { Bridge::trace_clear(); }
static std::vector<std::string> trace_api_names() { return Bridge::trace_api_names(); }

// The dispatch table is built by init(), and trace_api_names() reads func_map. The
// table only becomes useful once an app is actually loaded, because that is when
// ArmApp::start() resolves symbols and the app begins jumping through it.
static std::string trace_dump(size_t offset, size_t limit, const std::string& filter, bool tail) {
	auto calls = Bridge::trace_snapshot();
	auto names = Bridge::trace_api_names();

	std::vector<size_t> matched;
	matched.reserve(calls.size());
	for (size_t i = 0; i < calls.size(); i++) {
		const auto& c = calls[i];
		std::string nm = (c.index < names.size()) ? names[c.index] : ("idx" + std::to_string(c.index));
		if (!filter.empty() && nm.find(filter) == std::string::npos)
			continue;
		matched.push_back(i);
	}

	std::ostringstream o;
	o << "total=" << calls.size()
	  << " matched=" << matched.size()
	  << " dropped=" << Bridge::trace_dropped()
	  << " offset=" << offset << " limit=" << limit
	  << " dispatch_ready=" << (Bridge::dispatch_ready() ? "true" : "false")
	  << " trace_enabled=" << (Bridge::trace_enabled() ? "true" : "false")
	  << " apps=" << (g_app_manager ? g_app_manager->app_count() : 0);
	if (!filter.empty()) o << " filter=" << filter;
	if (tail) o << " tail=true";
	o << "\n";

	uint64_t dropped = Bridge::trace_dropped();
	if (dropped)
		o << "WARNING: " << dropped << " later calls were NOT recorded: the trace "
		  << "buffer is full. This trace is incomplete; narrow the window or read "
		  << "it in segments.\n";

	if (matched.empty()) {
		o << "(no recorded calls match query)\n";
		return o.str();
	}

	size_t start = offset;
	size_t count = limit;
	if (tail) {
		if (count > matched.size()) count = matched.size();
		start = matched.size() - count;
	}

	if (start >= matched.size()) {
		o << "(no further recorded calls; offset is past the end)\n";
		return o.str();
	}

	size_t end = std::min(matched.size(), start + count);
	for (size_t k = start; k < end; k++) {
		size_t i = matched[k];
		const auto& c = calls[i];
		std::string nm = (c.index < names.size()) ? names[c.index] : ("idx" + std::to_string(c.index));
		char line[320];
		snprintf(line, sizeof(line),
			"[%zu] %-24s pc=0x%08X a=(0x%08X,0x%08X,0x%08X,0x%08X) -> 0x%08X\n",
			i, nm.c_str(), c.pc, c.args_in[0], c.args_in[1], c.args_in[2], c.args_in[3], c.ret);
		o << line;
	}
	if (end < matched.size()) o << "... " << (matched.size() - end) << " more\n";
	return o.str();
}

static bool step(uint64_t instructions) {
	if (!g_ctl) return false;
	Mcp::Command c;
	c.kind = "step";
	// Wide value through the string fields to avoid widening Command.
	char buf[32];
	snprintf(buf, sizeof(buf), "%llu", (unsigned long long)instructions);
	c.a = buf;
	g_ctl->push(c);
	return true;
}

// ---------------------------------------------------------------- snapshot

static void publish_snapshot(bool running) {
	Mcp::Snapshot s;
	s.running = running;
	s.run_state = Bridge::paused() ? "paused" : "running";
	s.stepped_instructions = Bridge::bounded_instructions();
	s.vm_calls = Bridge::vm_call_count();
	s.step_budget = Bridge::step_budget();
	s.timers_made_due = g_timers_made_due.load(std::memory_order_relaxed);
	s.faulted = g_faulted.load(std::memory_order_relaxed);
	if (g_app_manager) s.apps_loaded = g_app_manager->app_count();

	if (uc) {
		static const int ids[] = {
			UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
			UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
			UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
			UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC };
		for (int i = 0; i < 16; i++) s.regs[i] = reg_read(ids[i]);
		s.cpsr = reg_read(UC_ARM_REG_CPSR);
		s.pc = s.regs[15];
	}

	App* cur = g_app_manager ? g_app_manager->current_app() : nullptr;
	s.trace_ready = Bridge::dispatch_ready();
	if (cur) {
		s.app_running = true;
		s.app_name = u8_to_utf8(cur->app_name);
	}

	if (g_graphic) {
		s.screen_w = g_graphic->width;
		s.screen_h = g_graphic->height;
	}

	{
		std::lock_guard<std::mutex> lk(g_last_error_mtx);
		s.last_error = g_last_error;
	}

	if (g_ctl) g_ctl->publish(s);
}

void note_error(const std::string& e) {
	std::lock_guard<std::mutex> lk(g_last_error_mtx);
	g_last_error = e;
	if (g_ctl) g_ctl->note_error(e);
}

// ---------------------------------------------------------------- command pump

// Bounded stepping and pause/resume.
//
// run_cpu is the only place the app executes, and it runs on the AppManager thread,
// so these must not call uc_emu_start from the MCP thread. Instead they set the
// flags that run_cpu reads, and the app's own timer ticks drive the slices.
static bool do_step(uint64_t instructions) {
	if (!uc || !instructions) return false;
	Bridge::set_step_budget(instructions);
	Bridge::set_paused(false);
	// The app only executes inside a slice entered by a timer callback or an
	// incoming event, and its next timer can be seconds away. Without this the
	// budget would sit unused and the step would appear to do nothing.
	App* app = g_app_manager ? g_app_manager->current_app() : nullptr;
	g_timers_made_due.store(app ? app->timer.make_all_due() : 0, std::memory_order_relaxed);
	// The AppManager loop sleeps in events_cv.wait_for until the nearest timer is
	// due, so an overdue timer is not noticed until it wakes on its own.
	if (g_app_manager) g_app_manager->wake();
	return true;
}

static void do_pause(bool on) {
	Bridge::set_paused(on);
	if (g_app_manager) g_app_manager->wake();
}

static void handle_command(const Mcp::Command& c) {
	if (c.kind == "load") {
		if (!g_app_manager) { note_error("load failed: app manager not ready"); return; }
		fs::path p(c.a);
		std::error_code ec;
		if (!fs::exists(p, ec)) { note_error("load failed: no such file " + c.a); return; }
		if (!ArmApp::check_format(p, false)) {
			note_error("load failed: not a recognised VXP (need ELF or zlib): " + c.a);
			return;
		}
		g_app_manager->add_app_for_launch(p, false);
		g_faulted.store(false);
		note_error("");
		return;
	}
	if (c.kind == "close") {
		close_app(c.n);
		return;
	}
	if (c.kind == "step") {
		do_step((uint64_t)strtoull(c.a.c_str(), nullptr, 10));
		return;
	}
	if (c.kind == "trace") {
		set_trace(c.n != 0);
		return;
	}
	if (c.kind == "key") {
		inject_key(c.n, !c.b.empty() ? c.b != "up" : true);
		return;
	}
	if (c.kind == "touch") {
		int x = 0, y = 0;
		if (sscanf(c.a.c_str(), "%d,%d", &x, &y) == 2) {
			void add_pen_event(int event, int x, int y);
			add_pen_event(c.n, x, y);
		}
		return;
	}
}

// run_cpu executes at most one chunk per VM callback and leaves the rest of the
// budget behind. Keep the app's timers due while budget remains so the remainder
// drains promptly instead of waiting for the app's own (possibly 10s) timer.
static void drain_step_budget() {
	if (!g_app_manager || Bridge::step_budget() == 0)
		return;
	App* app = g_app_manager->current_app();
	if (!app)
		return;
	g_timers_made_due.store(app->timer.make_all_due(), std::memory_order_relaxed);
	g_app_manager->wake();
}

void tick(bool running) {
	if (!g_ctl) return;
	if (g_keyboard) g_keyboard->update();

	// Drain any fault the emulator raised. run_cpu() reports uc_emu_start failures
	// through trigger_hard_reset_with_error, which in the GUI becomes a modal; with
	// no GUI it would otherwise vanish and the agent would see a dead CPU with no
	// explanation. Pause as well, so a faulted app does not keep faulting.
	std::string fault = take_error_message();
	if (!fault.empty()) {
		g_faulted.store(true);
		Bridge::set_paused(true);
		note_error(fault);
	}

	bool want = false;
	if (g_ctl->take_trace_request(want)) set_trace(want);

	Mcp::Command c;
	int drained = 0;
	while (drained < 64 && g_ctl->take(c)) {
		handle_command(c);
		drained++;
	}

	// run_cpu executes at most one chunk per VM callback and leaves the rest of the
// budget behind. Keep the app's timers due while budget remains so the remainder
// drains promptly instead of waiting for the app's own (possibly 10s) timer.
	drain_step_budget();

	publish_snapshot(running);
}

void setup(AppManager* am, KeyboardControl* kb, Mcp::Control* ctl, MREngine::Graphic* g) {
	g_app_manager = am;
	g_keyboard = kb;
	g_ctl = ctl;
	g_graphic = g;
	Mcp::set_host(&api());
	publish_snapshot(true);
}

// In headless mode the pump thread starts before AppManager::start(), so the
// launch request that arrived on argv is still sitting in the queue. The GUI path
// queues it before start() too, so drain it here to match, otherwise a headless
// session never loads the VXP the user passed in.
void bootstrap_launch(const std::string& path, bool local) {
	if (!g_app_manager || path.empty()) return;
	fs::path p(path);
	std::error_code ec;
	if (!fs::exists(p, ec)) {
		note_error("VXP does not exist: " + path);
		return;
	}
	if (!ArmApp::check_format(p, local)) {
		note_error("not a recognised VXP (need ELF or zlib): " + path);
		return;
	}
	g_app_manager->add_app_for_launch(p, local);
}

// Publish state and drain commands. Deliberately does NOT call uc_emu_start:
// AppManager::start() runs its own loop thread, and process_launch_app drives
// emulation through Bridge::run_cpu on that thread. Running a second uc_emu_start
// from this thread puts two threads inside one Unicorn engine and crashes with an
// access violation, so emulation must be left to AppManager entirely.
void pump(AppManager& am) {
	(void)am;
	tick(true);
}

void shutdown() {
	g_quit.store(true);
	Bridge::set_trace_enabled(false);
}

bool quitting() { return g_quit.load(); }

void request_quit() { g_quit.store(true); }

// Host function table the server calls into.
static const Mcp::HostApi& api() {
	static Mcp::HostApi a = {
		load_vxp, close_app, reg_write,
		inject_key, inject_touch, screen_png_base64, list_apps,
		set_trace, trace_dump, trace_count, trace_clear, trace_api_names,
		step, do_pause, read_mem
	};
	return a;
}

} // namespace McpHost