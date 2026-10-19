#include <iostream>
#include <thread>
#include <fstream>
#include <cstdlib>

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Memory.h"
#include "Cpu.h"
#include "GDB.h"
#include "Bridge.h"
#include "App.h"
#include "AppManager.h"
#include "Keyboard.h"
#include "Touch.h"
#include "Log.h"

#include "MREngine/Graphic.h"
#include "MREngine/IO.h"
#include "MREngine/SIM.h"
#include "MREngine/CharSet.h"
#include "MREngine/SystemTextBox.h"
#include <cmdparser.hpp>

#include "NativeApps/Menu/AppSelector.h"
#include "NativeApp.h"
#include "ArmApp.h"
#include "DLLApp.h"

#ifndef ANDROID
#include "McpServer.h"
namespace McpHost {
	void setup(AppManager*, Keyboard*, Mcp::Control*, MREngine::Graphic*);
	void bootstrap_launch(const std::string& path, bool local);
	void pump(AppManager&);
	void tick(bool running);
	void shutdown();
	bool quitting();
	void note_error(const std::string&);
}
#endif

AppManager* g_appManager = 0;

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

#include "DragAndDrop.h"

using DragAndDrop::show_warning;
using DragAndDrop::warning_clock;

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif


static void open_folder(const fs::path& p) {
	fs::create_directories(p);
	fs::path abs_p = fs::absolute(p);
#ifdef _WIN32
	ShellExecuteW(NULL, L"open", abs_p.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
	std::string cmd = "open \"" + abs_p.string() + "\" &";
	system(cmd.c_str());
#else
	std::string cmd = "xdg-open \"" + abs_p.string() + "\" &";
	system(cmd.c_str());
#endif
}

static void clear_user_data(const fs::path& root_fs) {
	std::error_code ec;
	if (!fs::exists(root_fs, ec))
		return;

	std::vector<fs::path> files_to_delete;
	for (auto it = fs::recursive_directory_iterator(root_fs, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (ec) break;
		if (it->is_regular_file(ec)) {
			std::string ext = it->path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
			if (ext != ".vxp") {
				files_to_delete.push_back(it->path());
			}
		}
	}

	for (const auto& f : files_to_delete) {
		fs::remove(f, ec);
	}

	std::vector<fs::path> dirs_to_check;
	for (auto it = fs::recursive_directory_iterator(root_fs, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (ec) break;
		if (it->is_directory(ec)) {
			dirs_to_check.push_back(it->path());
		}
	}

	std::sort(dirs_to_check.begin(), dirs_to_check.end(), [](const fs::path& a, const fs::path& b) {
		return a.string().length() > b.string().length();
	});

	for (const auto& d : dirs_to_check) {
		std::string s = d.generic_string();
		if (s == "fs/c" || s == "fs/d" || s == "fs/e" || s == "fs/e/mre")
			continue;
		if (fs::is_empty(d, ec)) {
			fs::remove(d, ec);
		}
	}

	fs::create_directories(root_fs / "c", ec);
	fs::create_directories(root_fs / "d", ec);
	fs::create_directories(root_fs / "e" / "mre", ec);
}

// ---------------------------------------------------------------------------
// Options button
//
// The device window is taller than the emulated screen so the on-screen keyboard has
// room. Whatever vertical space is left below the keyboard is free, so we centre a
// small button there. Tapping it opens the native Android options dialog.
// ---------------------------------------------------------------------------

static sf::FloatRect options_button_rect(int win_w, int win_h, int keyboard_bottom) {
	// The device window is scaled to the real window size, so size the button relative to
	// the free band underneath the keyboard rather than in fixed pixels.
	int free_h = win_h - keyboard_bottom;
	if (free_h < 24)
		return sf::FloatRect();

	// 76 rather than 72: the taller target is easier to hit. The free-space budget is
	// deliberately NOT reduced to match, because that budget decides how much room the
	// emulated screen and on-screen keyboard get, and shrinking it costs a few pixels of
	// keyboard height to buy four pixels on a button.
	int h = std::min(76, free_h - 12);
	if (h < 16)
		h = free_h;
	int w = h * 4;
	if (w > win_w - 40)
		w = win_w - 40;
	if (w < 40)
		w = std::min(40, win_w);

	int left = (win_w - w) / 2;
	// Centre it in the band, which keeps it clear of the system navigation bar.
	int top = keyboard_bottom + (free_h - h) / 2;
	return sf::FloatRect((float)left, (float)top, (float)w, (float)h);
}

static void draw_options_button(sf::RenderTarget& rt, const sf::FloatRect& r, bool pressed) {
	if (r.width <= 0)
		return;

	sf::RectangleShape back(sf::Vector2f(r.width, r.height));
	back.setPosition(r.left, r.top);
	back.setFillColor(pressed ? sf::Color(90, 90, 96) : sf::Color(58, 58, 64));
	back.setOutlineColor(sf::Color(150, 150, 158));
	back.setOutlineThickness(1);
	rt.draw(back);

	// Three dots in a row, so the button reads as a menu without needing a font.
	const float cx = r.left + r.width / 2.f;
	const float cy = r.top + r.height / 2.f;
	const float d = r.height / 8.f;
	const float gap = r.height / 3.f;
	for (int i = -1; i <= 1; ++i) {
		sf::CircleShape dot(d);
		dot.setOrigin(d, d);
		dot.setPosition(cx + i * gap, cy);
		dot.setFillColor(sf::Color(235, 235, 240));
		rt.draw(dot);
	}
}

sf::Clock global_clock;

bool work = true;

static std::mutex g_error_mutex;
std::string error_message = "";
bool show_error = false;
std::atomic<bool> g_request_hard_reset = false;

void trigger_hard_reset_with_error(const std::string& err_msg) {
	{
		std::lock_guard<std::mutex> lock(g_error_mutex);
		error_message = err_msg;
	}
	show_error = true;
	g_request_hard_reset = true;
	Cpu::stop();
}

std::string take_error_message() {
	std::lock_guard<std::mutex> lock(g_error_mutex);
	std::string out = error_message;
	error_message.clear();
	return out;
}

#ifdef ANDROID
#include <spdlog/sinks/android_sink.h>

std::atomic<int> storage_permission_state{0};

static JavaVM* g_jvm = nullptr;
static jobject g_activity = nullptr;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	g_jvm = vm;
	return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_notifyPermissionState(JNIEnv *env, jobject thiz, jboolean granted) {
    // Keep a global ref so the render thread can call back into the activity later.
    if (!g_activity)
        g_activity = env->NewGlobalRef(thiz);

    storage_permission_state = granted ? 1 : -1;
}

// Called from the SFML render thread when the Options button is tapped. `thiz` is the
// MainActivity instance, so we simply ask it to present the dialog on the UI thread.
static void request_options_dialog() {
	if (!g_jvm || !g_activity)
		return;

	JNIEnv* env = nullptr;
	bool attached = false;
	if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK)
			return;
		attached = true;
	}

	jclass cls = env->GetObjectClass(g_activity);
	if (cls) {
		jmethodID mid = env->GetMethodID(cls, "showOptionsDialogUi", "()V");
		if (mid) {
			env->CallVoidMethod(g_activity, mid);
			// The call can itself throw (BadTokenException when the activity is
			// finishing). Leaving that pending and then detaching the thread while
			// it still owns the exception is undefined and aborts the process.
			if (env->ExceptionCheck())
				env->ExceptionClear();
		}
		else {
			env->ExceptionClear();
		}
		env->DeleteLocalRef(cls);
	}

	if (attached)
		g_jvm->DetachCurrentThread();
}

// Called from the SFML render thread when the emulator raises a fatal error
// (bad VXP, failed load, preparation fault). Hands the message to the activity,
// which presents it as a Compose dialog with a copy button. The desktop build
// shows the same text in the ImGui "VXP Error" popup instead.
static void request_error_dialog(const std::string& message) {
	if (!g_jvm || !g_activity)
		return;

	JNIEnv* env = nullptr;
	bool attached = false;
	if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK)
			return;
		attached = true;
	}

	jclass cls = env->GetObjectClass(g_activity);
	if (cls) {
		jmethodID mid = env->GetMethodID(cls, "showErrorDialogUi", "(Ljava/lang/String;)V");
		if (mid) {
			jstring j_msg = env->NewStringUTF(message.c_str());
			if (j_msg) {
				env->CallVoidMethod(g_activity, mid, j_msg);
				env->DeleteLocalRef(j_msg);
			}
			else {
				// NewStringUTF raises OutOfMemoryError; leaving it pending and then
				// detaching the thread while it still owns the exception is undefined
				// and aborts the process.
				env->ExceptionClear();
			}
			// The call can itself throw (BadTokenException when the activity is
			// finishing). Same reasoning as above.
			if (env->ExceptionCheck())
				env->ExceptionClear();
		}
		else {
			env->ExceptionClear();
		}
		env->DeleteLocalRef(cls);
	}

	if (attached)
		g_jvm->DetachCurrentThread();
}

// Called from the SFML render thread when the Options button is tapped. Uses
// performHapticFeedback() rather than the vibrator so the platform picks a
// predefined pattern for the current input device, which reads as a deliberate
// "this opened something" tick instead of the generic buzz used for key presses.
static void trigger_options_haptic() {
	if (!g_jvm || !g_activity)
		return;

	JNIEnv* env = nullptr;
	bool attached = false;
	if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK)
			return;
		attached = true;
	}

	jclass cls = env->GetObjectClass(g_activity);
	if (cls) {
		jmethodID mid = env->GetMethodID(cls, "triggerHapticFeedback", "(I)V");
		if (mid) {
			// 2 == View.HAPTIC_TICK, added in API 21, so it is valid on every
			// supported release. HAPTIC_CLOCK_TICK would read better but is API 27
			// and would be a silent no-op below that.
			env->CallVoidMethod(g_activity, mid, (jint)2);
			if (env->ExceptionCheck())
				env->ExceptionClear();
		}
		else {
			env->ExceptionClear();
		}
		env->DeleteLocalRef(cls);
	}

	if (attached)
		g_jvm->DetachCurrentThread();
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_requestHardReset(JNIEnv *env, jobject thiz) {
    // The main loop picks this up and performs the reset on the render thread.
    g_request_hard_reset = true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_nativeLoadVxpFile(JNIEnv *env, jobject thiz, jstring j_path) {
    if (!j_path)
        return;
    const char *path_cstr = env->GetStringUTFChars(j_path, nullptr);
    if (!path_cstr) {
        // Raises OutOfMemoryError; the callee takes a const char* and would
        // strlen() a null pointer.
        env->ExceptionClear();
        return;
    }

    // g_appManager is published by the render thread during start-up. Bounded so a
    // native start-up failure cannot block the calling thread forever.
    for (int i = 0; i < 300 && !g_appManager; ++i)
        sf::sleep(sf::milliseconds(100));

    if (g_appManager)
        g_appManager->add_app_for_launch(path_cstr, false);

    env->ReleaseStringUTFChars(j_path, path_cstr);
}
#else
// The options dialog is Android-only; desktop builds simply have no button.
static void request_options_dialog() {}

// Desktop raises errors through the ImGui popup in the main loop instead.
static void request_error_dialog(const std::string&) {}

// No on-screen Options button outside Android.
static void trigger_options_haptic() {}
#endif

#ifndef ANDROID
// ---------------------------------------------------------------------------
// Headless inspection
// ---------------------------------------------------------------------------
//   MREmu.exe --inspect <vxp> [--json report.json] [--screen shot.png] [--steps N]
//
// Runs the same loader the GUI runs, reports the loader's own decisions as JSON,
// and optionally executes the app for a bounded number of instructions before
// dumping the emulated screen. No window is created, so this is usable from
// scripts and CI. This exists because the GUI's failure reasons live in an ImGui
// panel and stdout is block-buffered when redirected, so neither is observable
// to an automated caller.
//
// Exit codes: 0 prepared (and executed, if --steps given), 1 preparation failed,
// 2 execution error, 3 usage or file error.
static std::string json_escape(const std::string& in)
{
	std::string out;
	for (char c : in) {
		if (c == '"' || c == '\\') out += '\\';
		if ((unsigned char)c < 0x20) { out += ' '; continue; }
		out += c;
	}
	return out;
}

static int run_inspection(MREngine::Graphic& graphic,
	const std::string& app_path_in, bool path_is_local_in,
	const std::string& json_path, const std::string& screen_path,
	int steps)
{
	std::vector<std::string> f;
	auto S = [&](const char* k, const std::string& v) {
		f.push_back("\"" + std::string(k) + "\":\"" + json_escape(v) + "\"");
	};
	auto N = [&](const char* k, unsigned long long v) {
		f.push_back("\"" + std::string(k) + "\":" + std::to_string(v));
	};
	auto B = [&](const char* k, bool v) {
		f.push_back("\"" + std::string(k) + "\":" + (v ? "true" : "false"));
	};

	int rc = 0;
	auto trace = [&](const char* m) { std::cerr << "[inspect] " << m << std::endl; };
	trace("begin");
	S("mode", "inspect");
	S("path", app_path_in);
	B("path_is_local", path_is_local_in);
	N("requested_steps", (unsigned long long)(steps > 0 ? steps : 0));

	fs::path p(app_path_in);
	bool exists = false;
	try { exists = fs::exists(p); } catch (...) {}
	B("file_exists", exists);

	bool fmt_ok = false;
	try { fmt_ok = ArmApp::check_format(p, path_is_local_in); } catch (...) {}
	trace("check_format done");
	B("check_format", fmt_ok);

	ArmApp app;
	trace("ArmApp constructed");
	bool loaded = false, prep = false, ran = false, threw = false;
	std::string exmsg;

	if (!exists) { rc = 3; }
	else if (!fmt_ok) { rc = 3; }
	else {
		try {
			loaded = app.load_from_file(p, path_is_local_in);
		} catch (const std::exception& e) { threw = true; exmsg = e.what(); }
		catch (...) { threw = true; exmsg = "unknown exception"; }
		B("load_from_file", loaded);
		trace("load_from_file done");

		if (loaded && !threw) {
			try { prep = app.preparation(); }
			catch (const std::exception& e) { threw = true; exmsg = e.what(); }
			catch (...) { threw = true; exmsg = "unknown exception"; }
			trace("preparation done");
			B("preparation", prep);
			if (!prep && !threw) rc = 1;
		}
		else if (!loaded && !threw) { rc = 3; }

		// Loader facts, reported even when preparation failed so a failure is
		// diagnosable rather than just "invalid ELF".
		S("app_name", app.app_name);
		B("is_ads", app.is_ads);
		B("is_vre_ads", app.tags.is_vre_ads());
		B("is_zipped", app.is_zipped);
		N("ram_kb_tag", (unsigned long long)app.tags.get_ram());
		N("mem_size", (unsigned long long)app.mem_size);
		N("offset_mem", (unsigned long long)app.offset_mem);
		N("segments_size", (unsigned long long)app.segments_size);
		N("rw_size", (unsigned long long)app.rw_size);
		N("zi_size", (unsigned long long)app.zi_size);
		N("code_load_offset", (unsigned long long)app.code_load_offset);
		N("entry_point", (unsigned long long)app.entry_point);

		// The whole point: does entry_point actually point at the loaded code?
		long long expected = (long long)(app.offset_mem + app.code_load_offset);
		long long delta = (long long)app.entry_point - expected;
		f.push_back("\"entry_delta_vs_code_load\":" + std::to_string(delta));
		B("entry_matches_code_load", delta == 0);

		B("vm_res_present", app.resources.vm_res_size != 0);
		N("vm_res_size", (unsigned long long)app.resources.vm_res_size);
		N("vm_res_offset", (unsigned long long)app.resources.vm_res_offset);

		// Tag table exactly as the loader parsed it.
		std::string tj = "\"tags\":{";
		bool first = true;
		for (const auto& kv : app.tags.raw_tags) {
			if (!first) tj += ",";
			first = false;
			unsigned long long v = 0;
			const std::vector<uint8_t>& pl = kv.second;
			if (pl.size() == 4)
				v = (unsigned long long)pl[0] | ((unsigned long long)pl[1] << 8) |
					((unsigned long long)pl[2] << 16) | ((unsigned long long)pl[3] << 24);
			tj += "\"" + std::to_string(kv.first) + "\":{\"size\":" +
				std::to_string(pl.size()) + ",\"u32\":" + std::to_string(v) + "}";
		}
		tj += "}";
		f.push_back(tj);

		if (prep && steps > 0 && !threw) {
			Bridge::set_step_limit((uint64_t)steps);
			try { app.start(); ran = true; }
			catch (const std::exception& e) { threw = true; exmsg = e.what(); rc = 2; }
			catch (...) { threw = true; exmsg = "unknown exception"; rc = 2; }
			Bridge::set_step_limit(0);

			uint32_t pc = Bridge::get_pc();
			N("pc_after_run", (unsigned long long)pc);
			long long walked = (long long)pc - (long long)app.entry_point;
			f.push_back("\"entry_to_pc_delta\":" + std::to_string(walked));
			B("pc_left_entry_region", walked != 0);
		}

		B("executed", ran);
		B("threw", threw);
		S("exception", exmsg);
	}

	if (!screen_path.empty()) {
		try {
			graphic.update_screen();
			sf::Image img = graphic.screen_tex.copyToImage();
			B("screen_written", img.saveToFile(screen_path));
			S("screen_path", screen_path);
		} catch (const std::exception& e) {
			B("screen_written", false);
			S("screen_error", e.what());
		}
	}

	std::string out = "{";
	for (size_t i = 0; i < f.size(); i++) {
		if (i) out += ",";
		out += f[i];
	}
	out += "}";

	// Always to stdout so it is visible even when no --json was requested.
	std::cout << out << std::endl;
	if (!json_path.empty()) {
		std::ofstream fo(json_path, std::ios::binary);
		if (fo.is_open()) fo << out;
	}
	return rc;
}

// ---------------------------------------------------------------------------
// Watchable MCP mode
// ---------------------------------------------------------------------------
//   MREmu.exe <vxp> --mcp --mcp-window
//
// Plain --mcp is headless so it works on a machine with no display, but then a
// human cannot see the emulator at all, which makes an agent's steps hard to
// tell apart from a hang. This variant leaves the JSON-RPC server exactly as it
// is and adds a window mirroring the emulated screen plus a status line saying
// what the agent is doing (run state, VM call count, trace, fault).
//
// Threading: the window stays on this thread. Win32 delivers a window's messages
// to the thread that created it, so polling events from the pump thread would
// leave the window deaf to its own close button and to repaints. The protocol
// moves to a worker thread instead and the render loop runs on the main one.
static std::string mcp_status_line(const Mcp::Snapshot& s) {
	std::string t = "mcp ";
	t += s.run_state;
	t += "  calls=" + std::to_string(s.vm_calls);
	if (s.step_budget)
		t += "  step=" + std::to_string(s.stepped_instructions);
	if (s.trace_ready)
		t += "  trace";
	if (s.faulted)
		t += "  FAULT";
	// Unifont glyphs are 8-16px wide, so the window just clips a longer line.
	if (t.size() > 56) t.resize(56);
	return t;
}

static void run_mcp_window(MREngine::Graphic& graphic, Keyboard& keyboard, Touch& touch,
	Mcp::Control& ctl, const std::atomic<bool>& client_gone)
{
	sf::RenderWindow win(sf::VideoMode(graphic.width, graphic.height + 208),
		"MREmu (MCP)");
	win.setVerticalSyncEnabled(true);

	// Touch and the keyboard widget both map window coordinates back onto the
	// sprite, so they need the very sprite being drawn. Scale stays 1 here, which
	// makes window coordinates equal emulated screen coordinates.
	sf::Sprite sp(graphic.screen_tex);
	touch.screen = &sp;
	keyboard.screen = &sp;

	sf::Texture status_tex;
	std::string last_status, last_title;

	while (win.isOpen() && !client_gone.load() && !McpHost::quitting()) {
		sf::Event ev;
		while (win.pollEvent(ev)) {
			if (ev.type == sf::Event::Closed) {
				win.close();
				continue;
			}
			// Same input path the GUI uses, so a human can click and type in the
			// window while the agent works, and the app sees one consistent stream.
			keyboard.event(ev);
			touch.sf_event(ev);
		}

		keyboard.update();

		const Mcp::Snapshot snap = ctl.snapshot();

		// Same order as the GUI loop: refresh the texture from the framebuffer the
		// app thread writes, then draw it.
		graphic.update_screen();
		sp.setTexture(graphic.screen_tex, true);

		const std::string status = mcp_status_line(snap);
		win.clear(sf::Color::Black);
		win.draw(sp);

		// Status line sits in the band under the screen, where the GUI shows the
		// keyboard. Re-rasterise only when it changes: u16text_to_texture walks
		// glyphs out of the embedded unifont on every call.
		if (status != last_status) {
			status_tex = u16text_to_texture(
				std::u16string(status.begin(), status.end()),
				snap.faulted ? sf::Color(255, 96, 96)
					: (snap.run_state == "paused" ? sf::Color(255, 224, 96)
						: sf::Color::White));
			last_status = status;
		}
		if (status_tex.getSize().x) {
			sf::Sprite status_sp(status_tex);
			status_sp.setPosition(2.f, (float)graphic.height);
			win.draw(status_sp);
		}

		// Title as well, so the state is legible even when the window is behind
		// something else.
		const std::string title = "MREmu (MCP) - " + status;
		if (title != last_title) {
			win.setTitle(title);
			last_title = title;
		}

		win.display();
	}
}
#endif

int main(int argc, char** argv) {
    std::string app_path = "";
    bool path_is_local = false;

	Log::set_module("Main");
	spdlog::set_level(spdlog::level::debug);

#ifndef ANDROID
	cli::Parser parser(argc, argv);
	{
		parser.set_optional<std::string>("", "", "", "Path to vxp");
		parser.set_optional<bool>("l", "path_is_local", false, "Set to run from local filesystem");
		parser.set_optional<bool>("g", "gdb", false, "Set to run gdb server");
		parser.set_optional<int>("p", "gdb_port", 1234, "Port for gdb server");
		parser.set_optional<bool>("i", "inspect", false, "Headless mode: load and report the VXP, then exit (no window)");
		parser.set_optional<std::string>("j", "json", "", "Write the inspection report to this JSON file");
		parser.set_optional<std::string>("s", "screen", "", "Write the emulated screen to this PNG file");
		parser.set_optional<int>("n", "steps", 0, "With --inspect: execute at most N instructions (0 = do not execute)");
		parser.set_optional<bool>("w", "mcp-window", false, "With --mcp: also open the emulator window so a human can watch");
		parser.set_optional<bool>("m", "mcp", false, "MCP server over stdio: agent-controllable, headless unless --mcp-window");
	}
	parser.run_and_exit_if_error();
	app_path = parser.get<std::string>("");
	if (app_path.size() >= 2 && app_path.front() == '"' && app_path.back() == '"')
		app_path = app_path.substr(1, app_path.size() - 2);
	path_is_local = parser.get<bool>("l");

	// cmdparser keys get() on the SHORT name (that is why the existing code uses
	// "l" and "p" rather than the long names). Querying a long name misses the
	// map, inserts an undefined entry and then crashes casting it.
	bool inspect_mode = parser.get<bool>("i");
	std::string inspect_json = parser.get<std::string>("j");
	std::string inspect_screen = parser.get<std::string>("s");
	int inspect_steps = parser.get<int>("n");
	bool mcp_mode = parser.get<bool>("m");
	bool mcp_window = parser.get<bool>("w");
	// An MCP client is configured with a fixed argv, so MREMU_MCP_WINDOW is the
	// escape hatch for "always show the window" that needs no command-line edit.
	if (!mcp_window) {
		const char* env = std::getenv("MREMU_MCP_WINDOW");
		mcp_window = env && env[0] != 0 && env[0] != '0';
	}

#ifndef ANDROID
	// Claim stdout for JSON-RPC before anything logs. This must precede every
	// spdlog call, including the one below, because once spdlog's stdout sink is
	// live on fd 1 any log line corrupts the protocol stream.
	std::ostream* proto = nullptr;
	if (mcp_mode) {
		proto = Mcp::claim_stdout_for_protocol();
		if (!proto) {
			// Without this the protocol stream would be unusable: every log line
			// would corrupt a reply, so fail loudly instead of silently.
			spdlog::error("failed to reserve stdout for the MCP protocol stream");
			return 2;
		}
	}
#endif

	if (app_path.size())
		spdlog::info("Vxp path from args: {} {}", app_path, path_is_local ? "(local path)" : "");

	GDB::gdb_mode = parser.get<bool>("g");
	GDB::gdb_port = parser.get<int>("p");

	fs::current_path(fs::path(argv[0]).parent_path());
#else
    auto android_logger = spdlog::android_logger_mt("android_logger", "MREmu");

    spdlog::set_default_logger(android_logger);
#endif

    AppManager appManager;
    g_appManager = &appManager;

	if(GDB::gdb_mode)
		GDB::wait();

	Memory::init(32 * 1024 * 1024);
	Cpu::init();
	Bridge::init();

	MREngine::SIM::init();
	MREngine::System::init();
	MREngine::CharSet::init();
	MREngine::AppAudio::init();
	MREngine::SystemTextBox::init();
	MREngine::Graphic graphic;
	MREngine::NetworkManager nm;

	// Declared before the platform split: the Android main loop uses them, and the
	// headless modes below need them before the windowed code declares its own.
	Keyboard keyboard;
	Touch touch;

#ifndef ANDROID
	// Headless inspection runs before any window is created. SFML window creation
	// is the part that needs a display, and --inspect has to work on a machine
	// that has none.
	if (inspect_mode) {
		return run_inspection(graphic, app_path, path_is_local,
			inspect_json, inspect_screen, inspect_steps);
	}

	// MCP mode: the emulator is pumped on a worker thread while this thread
	// speaks MCP over stdio, so an agent can inspect and drive a running VXP on
	// a machine with no display.
	if (mcp_mode) {
		static Mcp::Control ctl;
		McpHost::setup(&appManager, &keyboard, &ctl, &graphic);
		appManager.start();
		// AppManager::loop drains the launch queue on its own thread, so queue the
		// argv VXP after start() and it gets picked up like the GUI path does.
		// With no path, fall back to the app selector exactly as the GUI does.
		// bootstrap_launch() returns on an empty path, so without this --mcp
		// started the emulator with nothing running at all: a black screen and an
		// empty emu_list_apps, with nothing to say a native app was expected.
		if (!app_path.empty())
			McpHost::bootstrap_launch(app_path, path_is_local);
		else
			appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);

		std::atomic<bool> stop{false};
		std::thread emu([&]() {
			while (!stop.load() && !McpHost::quitting()) {
				// Bounded slice per iteration: keeps the pump responsive to
				// commands while still making forward progress with no window.
				McpHost::pump(appManager);
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		});

		if (!mcp_window) {
			int rc = Mcp::serve(std::cin, *proto, ctl);
			McpHost::shutdown();
			stop.store(true);
			if (emu.joinable()) emu.join();
			appManager.stop();
			return rc;
		}

		// Watchable: protocol on a worker, window here. Returns when the human
		// closes the window or the client disconnects.
		std::atomic<bool> client_gone{false};
		std::atomic<int> serve_rc{0};
		std::thread server([&]() {
			serve_rc = Mcp::serve(std::cin, *proto, ctl);
			McpHost::shutdown();
			client_gone.store(true);
		});

		try {
			run_mcp_window(graphic, keyboard, touch, ctl, client_gone);
		}
		catch (const std::exception& e) {
			// No display, or the window could not be created. The agent still needs
			// its server, so keep serving headless and say why on stderr (fd 1 is
			// the protocol stream by now).
			std::cerr << "[mcp] window unavailable, continuing headless: "
				<< e.what() << std::endl;
			while (!client_gone.load() && !McpHost::quitting())
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}

		if (!client_gone.load()) {
			// The human closed the window while the protocol thread is parked in a
			// blocking read on stdin. Running static destructors would race that
			// read against iostream teardown, so leave immediately: the client sees
			// the pipe close and spawns a fresh server next time.
			McpHost::shutdown();
			std::_Exit(0);
		}

		stop.store(true);
		// Reached only when the client disconnected, so serve() has already
		// returned and this cannot block. It must be joined: a joinable std::thread
		// reaching the end of main terminates the process with 0xC0000409.
		if (server.joinable()) server.join();
		if (emu.joinable()) emu.join();
		appManager.stop();
		return serve_rc.load();
	}

	sf::RenderWindow win_debug(sf::VideoMode(1000, 600), "MREmu Debug");
	sf::RenderWindow win_device(sf::VideoMode(graphic.width, graphic.height + 208), "MREmu Device");
	ImGui::SFML::Init(win_debug);
	win_debug.setVerticalSyncEnabled(true);
	win_device.setVerticalSyncEnabled(true);
#else
	sf::RenderWindow win_device(sf::VideoMode::getDesktopMode(), "MREmu");
	win_device.setVerticalSyncEnabled(true);

	while (win_device.isOpen()) {
		sf::Event event;
		while (win_device.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				win_device.close();
			}
		}

		int perm_state = storage_permission_state.load();

		if (perm_state == 1)
			break;
		else if (perm_state == 0)
			win_device.clear(sf::Color::Black);
		else if (perm_state == -1)
			win_device.clear(sf::Color::Red);

		win_device.display();
	}
#endif

#ifndef ANDROID
	DragAndDrop::init(win_device, &win_debug);
#else
	DragAndDrop::init(win_device, nullptr);
#endif

	MREngine::IO::init();

	if (GDB::gdb_mode)
		GDB::cpu_state = GDB::Stop;

	appManager.start();

	if (app_path.size()) {
		if (fs::exists(app_path) || path_is_local) {
			appManager.add_app_for_launch(app_path, path_is_local);
		} else {
			error_message = "VXP file does not exist:\n" + app_path;
			show_error = true;
		}
	}
	else
		appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);


	int base_w = graphic.width;
	int base_h = graphic.height + 208;
	DragAndDrop::set_base_size(base_w, base_h);

// Options button state. Declared before repaint_device() because that is where the
	// button actually gets drawn (the main loop's own draws are discarded by
	// repaint_device(), which redraws and presents the device window).
#ifdef ANDROID
	sf::FloatRect options_btn;
	bool options_btn_pressed = false;
#endif

	float scale = 1.f;
	sf::Sprite screen_sp(graphic.screen_tex);
	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;

	auto repaint_device = [&]() {
		win_device.clear(sf::Color::Black);
		if (MREngine::SystemTextBox::is_open()) {
			MREngine::SystemTextBox::draw((VMUINT8*)graphic.screen.data(), graphic.width, graphic.height);
			graphic.screen_changed = true;
			graphic.update_screen();
		}
		screen_sp.setTexture(graphic.screen_tex, true);
		win_device.draw(screen_sp);

		if (show_warning) {
			float elapsed = warning_clock.getElapsedTime().asSeconds();
			if (elapsed < 4.f) {
				if (int(elapsed * 4.f) % 2 == 0) {
					sf::Texture warn_text_texture = u16text_to_texture(u"Invalid VXP file!", sf::Color(255, 64, 64));
					sf::Sprite warn_text_sprite(warn_text_texture);
					float tw = (float)warn_text_texture.getSize().x;
					float th = (float)warn_text_texture.getSize().y;
					float box_size_h = warn_text_texture.getSize().y;
					float box_size_w = warn_text_texture.getSize().x;
					float padding = 4.0f;

					float px = std::floor((win_device.getSize().x - box_size_w - 2 * padding) / 2.f);
					float py = std::floor((graphic.height * scale - box_size_h - 2 * padding) / 2.f);

					sf::RectangleShape bg(sf::Vector2f(box_size_w + 2 * padding, box_size_h + 2 * padding));
					bg.setFillColor(sf::Color(0, 0, 0, 0));
					bg.setOutlineColor(sf::Color(255, 64, 64, 230));
					bg.setOutlineThickness(1.f);
					bg.setPosition(px, py);

					warn_text_sprite.setOrigin(std::floor(tw / 2.f), std::floor(th / 2.f));
					warn_text_sprite.setPosition(px + std::floor(box_size_w / 2.f) + padding, py + std::floor(box_size_h / 2.f) + padding);

					win_device.draw(bg);
					win_device.draw(warn_text_sprite);
				}
			}
			else {
				show_warning = false;
			}
		}

		keyboard.draw(&win_device);

#ifdef ANDROID
		draw_options_button(win_device, options_btn, options_btn_pressed);
#endif

		win_device.display();
	};

	auto update_screen_size = [&](unsigned int new_w, unsigned int new_h) {
		static bool resizing = false;
		if (resizing)
			return;
		resizing = true;

		constexpr float scale_step = 0.05f;
		float scale_x = (float)new_w / (float)base_w;
		float scale_y = (float)new_h / (float)base_h;

		scale = std::round(std::max(scale_x, scale_y) / scale_step) * scale_step;
		if (scale < 1.0f)
			scale = 1.0f;

		unsigned int target_w = (unsigned int)std::round((float)base_w * scale);
		unsigned int target_h = (unsigned int)std::round((float)base_h * scale);

		if (win_device.getSize().x != target_w || win_device.getSize().y != target_h)
			win_device.setSize(sf::Vector2u(target_w, target_h));

		win_device.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)target_w, (float)target_h)));

		screen_sp.setScale(scale, scale);
		screen_sp.setPosition(0.f, 0.f);

		keyboard.update_resize(target_w, target_h);

		repaint_device();

		resizing = false;
	};

	DragAndDrop::set_callbacks(update_screen_size, repaint_device);

	DragAndDrop::restore_window_state();

#ifndef ANDROID
	win_debug.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)win_debug.getSize().x, (float)win_debug.getSize().y)));
#endif

	update_screen_size(win_device.getSize().x, win_device.getSize().y);

	// Recomputed every frame because the device window gets resized (and scaled) after startup.
#ifdef ANDROID
	options_btn = options_button_rect(
		win_device.getSize().x, win_device.getSize().y, keyboard.bottom());
#endif

	sf::Clock fps;

	sf::Clock deltaClock;
	sf::Event event;

	bool request_clear_user_data = false;

	while (win_device.isOpen()
#ifndef ANDROID
        && win_debug.isOpen()
#endif
    ) {
#ifdef ANDROID
		options_btn = options_button_rect(
			win_device.getSize().x, win_device.getSize().y, keyboard.bottom());
#endif
#ifdef ANDROID
		// Surface a pending fatal error to the Compose dialog. Android-only: the desktop
		// build renders the same message in an ImGui popup further down this loop, which
		// reads error_message directly. Draining it here would hand that popup an empty
		// string, so the take_error_message() call must stay on this side of the split.
		if (show_error) {
			show_error = false;
			std::string err = take_error_message();
			if (!err.empty())
				request_error_dialog(err);
		}
#endif

		if (g_request_hard_reset) {
			g_request_hard_reset = false;

			work = false;
			Cpu::stop();

			appManager.reset();

			if (request_clear_user_data) {
				request_clear_user_data = false;
				clear_user_data("fs");
			}

			Cpu::deinit();
			Memory::deinit();
			Memory::init(32 * 1024 * 1024);
			Cpu::init();
			Bridge::init();

			MREngine::SIM::init();
			MREngine::System::init();
			MREngine::CharSet::init();
			MREngine::AppAudio::init();
			MREngine::IO::init();
			MREngine::SystemTextBox::init();

			graphic.reset();

			keyboard.kc.pkey.clear();
			touch.touching = false;

			if (GDB::gdb_mode)
				GDB::cpu_state = GDB::Stop;

			appManager.add_app_for_launch("", false, &NativeApps::Menu::AppSelector::Conf);

			work = true;
		}

#ifndef ANDROID
		while (win_debug.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);
			switch (event.type) {
			case sf::Event::Closed:
				DragAndDrop::save_window_state();
				win_debug.close();
				win_device.close();
				break;
			case sf::Event::Resized:
				win_debug.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
				break;
			}
		}
#endif

		while (win_device.pollEvent(event)) {
			if (MREngine::SystemTextBox::is_open()) {
				if (MREngine::SystemTextBox::handle_sfml_event(event, graphic.width, graphic.height, scale)) {
					continue;
				}
			}
			keyboard.event(event);
			touch.sf_event(event);

			// Options button: taps land below the emulated screen, so Touch ignores
			// them. Handle them here before the rest of the input pipeline.
#ifdef ANDROID
			if (event.type == sf::Event::MouseButtonPressed &&
				event.mouseButton.button == sf::Mouse::Button::Left) {
				if (options_btn.contains((float)event.mouseButton.x, (float)event.mouseButton.y)) {
					options_btn_pressed = true;
					continue;
				}
			}
			else if (event.type == sf::Event::MouseButtonReleased &&
				event.mouseButton.button == sf::Mouse::Button::Left) {
				if (options_btn_pressed &&
					options_btn.contains((float)event.mouseButton.x, (float)event.mouseButton.y)) {
					trigger_options_haptic();
					request_options_dialog();
				}
				options_btn_pressed = false;
			}
			else if (event.type == sf::Event::TouchBegan) {
				if (options_btn.contains((float)event.touch.x, (float)event.touch.y))
					options_btn_pressed = true;
			}
			else if (event.type == sf::Event::TouchEnded) {
				if (options_btn_pressed &&
					options_btn.contains((float)event.touch.x, (float)event.touch.y)) {
					trigger_options_haptic();
					request_options_dialog();
				}
				options_btn_pressed = false;
			}
#endif

			switch (event.type) {
			case sf::Event::Closed:
				DragAndDrop::save_window_state();
				win_device.close();
#ifndef ANDROID
				win_debug.close();
#endif
				break;
			case sf::Event::Resized:
				update_screen_size(event.size.width, event.size.height);
				break;
			}
		}
#ifndef ANDROID
		ImGui::SFML::Update(win_debug, deltaClock.restart());

		if (show_error) {
			ImGui::OpenPopup("VXP Error");
			show_error = false; // Only call OpenPopup once
		}
		if (ImGui::BeginPopupModal("VXP Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			std::string current_err;
			{
				std::lock_guard<std::mutex> lock(g_error_mutex);
				current_err = error_message;
			}
			ImGui::TextUnformatted("The emulator encountered an error and was reset:");
			ImGui::Spacing();

			// Read-only multi-line text input with monospace-like selectable area
			ImVec2 text_size(520, std::min(300.f, std::max(120.f, ImGui::GetTextLineHeightWithSpacing() * 15)));
			ImGui::InputTextMultiline("##error_details", const_cast<char*>(current_err.c_str()),
				current_err.size() + 1, text_size, ImGuiInputTextFlags_ReadOnly);

			ImGui::Spacing();
			if (ImGui::Button("Copy to Clipboard", ImVec2(150, 0))) {
				ImGui::SetClipboardText(current_err.c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button("OK", ImVec2(100, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
#endif

		graphic.update_screen();

		{
			screen_sp.setTexture(graphic.screen_tex, true);
			win_device.draw(screen_sp);
		}

		keyboard.draw(&win_device);

#ifndef ANDROID
		if (ImGui::Begin("Fps")) {
			ImGui::Text("%1.3f", 1.f / fps.restart().asSeconds());
		}
		ImGui::End();

		keyboard.imgui_keyboard();
		graphic.imgui_screen();

		std::lock_guard lock(appManager.active_app_mutex);
		App* active_app = appManager.get_active_app();
		if (active_app) {
			active_app->graphic.imgui_layers();
			active_app->graphic.imgui_canvases();
		}

		ImGui::SetNextWindowPos(ImVec2(10, 255), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(175, 335), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Memory") && active_app) {
			float size = active_app->app_memory.get_memory_size();
			float free_size = active_app->app_memory.get_free_memory_size();
			float used_size = size - free_size;
			ImGui::Text("All:\n%1.0f bytes\n%1.1f kb\n %1.3f mb\n", 
				size, size / 1024.f, size / 1024.f / 1024.f);
			ImGui::Text("Free:\n%1.0f bytes\n%1.1f kb\n %1.3f mb\n", 
				free_size, free_size / 1024.f, free_size / 1024.f / 1024.f);
			ImGui::Text("Used:\n%1.0f bytes\n%1.1f kb\n %1.3f mb\n", 
				used_size, used_size / 1024.f, used_size / 1024.f / 1024.f);
			ImGui::Text("Used: %1.2f%%%", 100.f*used_size / size);
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(10, 190), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(175, 60), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Fps")) {
			ImGui::Text("%1.3f", 1.f / fps.restart().asSeconds());
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(175, 205), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Control")) {
			if (ImGui::Button("Hard Reset"))
				g_request_hard_reset = true;
			if (ImGui::Button("Clear user data")) {
				ImGui::OpenPopup("Clear User Data");
			}
			if (ImGui::Button("Open c:/ (fs/c)"))
				open_folder("fs/c");
			if (ImGui::Button("Open d:/ (fs/d)"))
				open_folder("fs/d");
			if (ImGui::Button("Open e:/ (fs/e)"))
				open_folder("fs/e");
			if (ImGui::Button("Open e:/mre (fs/e/mre)"))
				open_folder("fs/e/mre");

			if (ImGui::BeginPopupModal("Clear User Data", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Are you sure you want to clear all user data?\nThis will erase all saves, settings, and caches,\nwhile keeping all VXP files intact.");
				ImGui::Separator();
				if (ImGui::Button("Yes, Clear Data", ImVec2(130, 0))) {
					request_clear_user_data = true;
					g_request_hard_reset = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(100, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();

		if (show_warning && warning_clock.getElapsedTime().asSeconds() < 4.f) {
			ImGui::SetNextWindowPos(ImVec2(win_debug.getSize().x / 2.f, win_debug.getSize().y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			if (ImGui::Begin("Warning##VXP", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
				ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Imported file is not a valid VXP!");
			}
			ImGui::End();
		}
#endif

		keyboard.update();

		repaint_device();

#ifndef ANDROID
		Cpu::imgui_REG();

		keyboard.imgui_keyboard();
#endif

#ifndef ANDROID
		ImGui::SFML::Render(win_debug);
		win_debug.display();
		win_debug.clear();
		DragAndDrop::ensure_device_on_top();
#endif
	}

	DragAndDrop::cleanup();
 
	appManager.stop();
	work = false;

#ifndef ANDROID
	ImGui::SFML::Shutdown();
#endif
	return 0;
}