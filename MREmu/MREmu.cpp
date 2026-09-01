#include <iostream>
#include <thread>

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
#include <cmdparser.hpp>

#include "NativeApps/Menu/AppSelector.h"
#include "NativeApp.h"
#include "ArmApp.h"
#include "DLLApp.h"

AppManager* g_appManager = 0;

sf::Texture u16text_to_texture(std::u16string str, sf::Color c);

std::u16string warning_text_u16;
sf::Clock warning_clock;
bool show_warning = false;

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <functional>

static WNDPROC old_wndproc_debug = NULL;
static WNDPROC old_wndproc_device = NULL;

static int dev_base_w = 240;
static int dev_base_h = 528;

static HWND g_hwnd_debug = NULL;
static HWND g_hwnd_device = NULL;

static std::function<void(unsigned int, unsigned int)> g_on_device_resize;
static std::function<void()> g_repaint_device;

static int g_dev_grab_x = 0;
static int g_dev_grab_y = 0;
static bool g_dev_is_moving = false;

static int g_dbg_last_x = 0;
static int g_dbg_last_y = 0;
static bool g_device_was_docked_to_debug = false;

static bool get_visual_rect(HWND hwnd, RECT* out_rect) {
	if (!hwnd || !IsWindow(hwnd))
		return false;
	if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, out_rect, sizeof(RECT))))
		return true;
	return GetWindowRect(hwnd, out_rect) != 0;
}

static void get_shadow_margins(HWND hwnd, int& m_left, int& m_top, int& m_right, int& m_bottom) {
	RECT win_rect, vis_rect;
	GetWindowRect(hwnd, &win_rect);
	if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &vis_rect, sizeof(RECT)))) {
		m_left = vis_rect.left - win_rect.left;
		m_top = vis_rect.top - win_rect.top;
		m_right = win_rect.right - vis_rect.right;
		m_bottom = win_rect.bottom - vis_rect.bottom;
	} else {
		m_left = m_top = m_right = m_bottom = 0;
	}
}

static bool is_device_docked_to_debug() {
	if (!g_hwnd_debug || !g_hwnd_device || !IsWindow(g_hwnd_debug) || !IsWindow(g_hwnd_device))
		return false;
	RECT dbg_vis, dev_vis;
	if (!get_visual_rect(g_hwnd_debug, &dbg_vis) || !get_visual_rect(g_hwnd_device, &dev_vis))
		return false;

	const int tol = 4;
	bool touching_right = (std::abs(dev_vis.left - dbg_vis.right) <= tol) &&
		(dev_vis.bottom >= dbg_vis.top - tol) && (dev_vis.top <= dbg_vis.bottom + tol);
	bool touching_left = (std::abs(dev_vis.right - dbg_vis.left) <= tol) &&
		(dev_vis.bottom >= dbg_vis.top - tol) && (dev_vis.top <= dbg_vis.bottom + tol);
	bool touching_bottom = (std::abs(dev_vis.top - dbg_vis.bottom) <= tol) &&
		(dev_vis.right >= dbg_vis.left - tol) && (dev_vis.left <= dbg_vis.right + tol);
	bool touching_top = (std::abs(dev_vis.bottom - dbg_vis.top) <= tol) &&
		(dev_vis.right >= dbg_vis.left - tol) && (dev_vis.left <= dbg_vis.right + tol);

	return touching_right || touching_left || touching_bottom || touching_top;
}

static void snap_device_to_debug(HWND hwnd, RECT* r) {
	if (!g_hwnd_debug || !IsWindow(g_hwnd_debug) || !IsWindowVisible(g_hwnd_debug))
		return;

	RECT dbg_vis;
	if (!get_visual_rect(g_hwnd_debug, &dbg_vis))
		return;

	int w = r->right - r->left;
	int h = r->bottom - r->top;

	int ideal_left = r->left;
	int ideal_top = r->top;
	if (g_dev_is_moving) {
		POINT pt;
		GetCursorPos(&pt);
		ideal_left = pt.x - g_dev_grab_x;
		ideal_top = pt.y - g_dev_grab_y;
	}

	int m_l, m_t, m_r, m_b;
	get_shadow_margins(hwnd, m_l, m_t, m_r, m_b);

	RECT ideal_vis;
	ideal_vis.left = ideal_left + m_l;
	ideal_vis.top = ideal_top + m_t;
	ideal_vis.right = ideal_left + w - m_r;
	ideal_vis.bottom = ideal_top + h - m_b;

	int vis_w = ideal_vis.right - ideal_vis.left;
	int vis_h = ideal_vis.bottom - ideal_vis.top;

	int overlap_l = std::max(ideal_vis.left, dbg_vis.left);
	int overlap_r = std::min(ideal_vis.right, dbg_vis.right);
	int overlap_t = std::max(ideal_vis.top, dbg_vis.top);
	int overlap_b = std::min(ideal_vis.bottom, dbg_vis.bottom);

	if (overlap_l < overlap_r && overlap_t < overlap_b) {
		int overlap_area = (overlap_r - overlap_l) * (overlap_b - overlap_t);
		int dev_area = vis_w * vis_h;
		if (overlap_area > dev_area * 0.35f) {
			r->left = ideal_left;
			r->top = ideal_top;
			r->right = ideal_left + w;
			r->bottom = ideal_top + h;
			return;
		}
	}

	const int threshold = 16;
	RECT snapped_vis = ideal_vis;

	bool v_near = (ideal_vis.bottom >= dbg_vis.top - threshold) && (ideal_vis.top <= dbg_vis.bottom + threshold);
	if (v_near) {
		if (std::abs(ideal_vis.left - dbg_vis.right) <= threshold) {
			snapped_vis.left = dbg_vis.right;
			snapped_vis.right = snapped_vis.left + vis_w;
		}
		else if (std::abs(ideal_vis.right - dbg_vis.left) <= threshold) {
			snapped_vis.right = dbg_vis.left;
			snapped_vis.left = snapped_vis.right - vis_w;
		}
		else if (std::abs(ideal_vis.left - dbg_vis.left) <= threshold) {
			snapped_vis.left = dbg_vis.left;
			snapped_vis.right = snapped_vis.left + vis_w;
		}
		else if (std::abs(ideal_vis.right - dbg_vis.right) <= threshold) {
			snapped_vis.right = dbg_vis.right;
			snapped_vis.left = snapped_vis.right - vis_w;
		}
	}

	bool h_near = (ideal_vis.right >= dbg_vis.left - threshold) && (ideal_vis.left <= dbg_vis.right + threshold);
	if (h_near) {
		if (std::abs(ideal_vis.top - dbg_vis.bottom) <= threshold) {
			snapped_vis.top = dbg_vis.bottom;
			snapped_vis.bottom = snapped_vis.top + vis_h;
		}
		else if (std::abs(ideal_vis.bottom - dbg_vis.top) <= threshold) {
			snapped_vis.bottom = dbg_vis.top;
			snapped_vis.top = snapped_vis.bottom - vis_h;
		}
		else if (std::abs(ideal_vis.top - dbg_vis.top) <= threshold) {
			snapped_vis.top = dbg_vis.top;
			snapped_vis.bottom = snapped_vis.top + vis_h;
		}
		else if (std::abs(ideal_vis.bottom - dbg_vis.bottom) <= threshold) {
			snapped_vis.bottom = dbg_vis.bottom;
			snapped_vis.top = snapped_vis.bottom - vis_h;
		}
	}

	r->left = snapped_vis.left - m_l;
	r->top = snapped_vis.top - m_t;
	r->right = r->left + w;
	r->bottom = r->top + h;
}

static void handle_drop(HDROP hDrop, HWND hwnd) {
	bool is_shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

	UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
	if (count == 0) {
		DragFinish(hDrop);
		return;
	}

	std::vector<fs::path> valid_files;
	for (UINT i = 0; i < count; ++i) {
		UINT len = DragQueryFileW(hDrop, i, NULL, 0);
		std::wstring path(len, L'\0');
		DragQueryFileW(hDrop, i, &path[0], len + 1);

		fs::path src(path);
		std::string ext = src.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		bool valid = false;
		if (ext == ".vxp" && fs::is_regular_file(src)) {
			if (ArmApp::check_format(src, false))
				valid = true;
#ifdef WIN32
			else if (DLLApp::check_format(src, false))
				valid = true;
#endif
		}

		if (valid) {
			valid_files.push_back(src);
		} else {
			warning_clock.restart();
			show_warning = true;
		}
	}

	if (valid_files.empty()) {
		DragFinish(hDrop);
		return;
	}

	bool should_move = false;
	if (is_shift) {
		int result = MessageBoxW(hwnd, L"Do you want to move the VXP file(s) to fs/e/mre instead of copying?", L"Move VXP", MB_YESNO | MB_ICONQUESTION);
		if (result == IDYES)
			should_move = true;
	}

	fs::path dest_dir = fs::path("fs/e/mre").make_preferred();
	fs::create_directories(dest_dir);

	bool imported = false;
	for (const auto& src : valid_files) {
		std::error_code ec;
		fs::path target = dest_dir / src.filename();
		if (should_move) {
			fs::rename(src, target, ec);
			if (ec) {
				ec.clear();
				fs::copy_file(src, target, fs::copy_options::overwrite_existing, ec);
				if (!ec)
					fs::remove(src, ec);
			}
		} else {
			fs::copy_file(src, target, fs::copy_options::overwrite_existing, ec);
		}
		if (!ec)
			imported = true;
	}
	DragFinish(hDrop);

	if (imported && g_appManager) {
		NativeApp* native_app = dynamic_cast<NativeApp*>(g_appManager->get_active_app());
		if (native_app && native_app->conf.entry == NativeApps::Menu::AppSelector::entry)
			NativeApps::Menu::AppSelector::rescan();
	}
}

static LRESULT CALLBACK drop_wndproc_debug(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_DROPFILES) {
		handle_drop((HDROP)wParam, hwnd);
		return 0;
	}
	if (msg == WM_ENTERSIZEMOVE) {
		RECT dbg_rect;
		GetWindowRect(hwnd, &dbg_rect);
		g_dbg_last_x = dbg_rect.left;
		g_dbg_last_y = dbg_rect.top;
		g_device_was_docked_to_debug = is_device_docked_to_debug();
	}
	if (msg == WM_MOVING) {
		RECT* r = (RECT*)lParam;
		int dx = r->left - g_dbg_last_x;
		int dy = r->top - g_dbg_last_y;
		g_dbg_last_x = r->left;
		g_dbg_last_y = r->top;

		if (g_device_was_docked_to_debug && g_hwnd_device && IsWindow(g_hwnd_device) && (dx != 0 || dy != 0)) {
			RECT dev_rect;
			GetWindowRect(g_hwnd_device, &dev_rect);
			SetWindowPos(g_hwnd_device, NULL,
				dev_rect.left + dx, dev_rect.top + dy,
				0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		return CallWindowProcW(old_wndproc_debug, hwnd, msg, wParam, lParam);
	}
	if (msg == WM_EXITSIZEMOVE) {
		g_device_was_docked_to_debug = false;
	}
	return CallWindowProcW(old_wndproc_debug, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK drop_wndproc_device(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_DROPFILES) {
		handle_drop((HDROP)wParam, hwnd);
		return 0;
	}
	if (msg == WM_ENTERSIZEMOVE) {
		POINT pt;
		GetCursorPos(&pt);
		RECT win_rect;
		GetWindowRect(hwnd, &win_rect);
		g_dev_grab_x = pt.x - win_rect.left;
		g_dev_grab_y = pt.y - win_rect.top;
		g_dev_is_moving = true;
	}
	if (msg == WM_MOVING) {
		RECT* r = (RECT*)lParam;
		snap_device_to_debug(hwnd, r);
		return TRUE;
	}
	if (msg == WM_EXITSIZEMOVE) {
		g_dev_is_moving = false;
	}
	if (msg == WM_SIZING) {
		RECT* r = (RECT*)lParam;
		RECT cr = { 0, 0, 100, 100 };
		RECT wr = cr;
		AdjustWindowRect(&wr, GetWindowLong(hwnd, GWL_STYLE), FALSE);
		int bw = (wr.right - wr.left) - 100;
		int bh = (wr.bottom - wr.top) - 100;

		int cur_w = (r->right - r->left) - bw;
		int cur_h = (r->bottom - r->top) - bh;

		constexpr float scale_step = 0.05f;
		float scale_val = std::max((float)cur_w / (float)dev_base_w, (float)cur_h / (float)dev_base_h);
		scale_val = std::round(scale_val / scale_step) * scale_step;
		if (scale_val < 1.0f)
			scale_val = 1.0f;

		int new_w = (int)std::round((float)dev_base_w * scale_val) + bw;
		int new_h = (int)std::round((float)dev_base_h * scale_val) + bh;

		switch (wParam) {
		case WMSZ_LEFT:
		case WMSZ_TOPLEFT:
		case WMSZ_BOTTOMLEFT:
			r->left = r->right - new_w;
			break;
		default:
			r->right = r->left + new_w;
			break;
		}

		switch (wParam) {
		case WMSZ_TOP:
		case WMSZ_TOPLEFT:
		case WMSZ_TOPRIGHT:
			r->top = r->bottom - new_h;
			break;
		default:
			r->bottom = r->top + new_h;
			break;
		}

		return TRUE;
	}
	if (msg == WM_SIZE) {
		LRESULT res = CallWindowProcW(old_wndproc_device, hwnd, msg, wParam, lParam);
		if (g_on_device_resize && wParam != SIZE_MINIMIZED) {
			g_on_device_resize(LOWORD(lParam), HIWORD(lParam));
		}
		return res;
	}
	if (msg == WM_PAINT) {
		LRESULT res = CallWindowProcW(old_wndproc_device, hwnd, msg, wParam, lParam);
		if (g_repaint_device)
			g_repaint_device();
		return res;
	}
	return CallWindowProcW(old_wndproc_device, hwnd, msg, wParam, lParam);
}
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

sf::Clock global_clock;

bool work = true;

std::string error_message = "";
bool show_error = false;

#ifdef ANDROID
#include <spdlog/sinks/android_sink.h>

std::atomic<int> storage_permission_state{0};

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_notifyPermissionState(JNIEnv *env, jobject thiz, jboolean granted) {
    storage_permission_state = granted ? 1 : -1;
}

extern "C" JNIEXPORT void JNICALL
Java_com_ximikboda_mremu_MainActivity_nativeLoadVxpFile(JNIEnv *env, jobject thiz, jstring j_path) {
    const char *path_cstr = env->GetStringUTFChars(j_path, nullptr);

    while(!g_appManager)
        sf::sleep(sf::seconds(0.2));

    g_appManager->add_app_for_launch(path_cstr, false);

    env->ReleaseStringUTFChars(j_path, path_cstr);
}
#endif


void mre_main(AppManager* appManager_p) {
	AppManager& appManager = *appManager_p;

	sf::Clock deltaClock;
	while (work) {
		uint32_t delta_ms = deltaClock.restart().asMilliseconds();

		GDB::update();
		appManager.update(delta_ms);

		sf::sleep(sf::milliseconds(1000 / 120));
	}
}

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
	}
	parser.run_and_exit_if_error();
	app_path = parser.get<std::string>("");
	path_is_local = parser.get<bool>("l");

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
	MREngine::Graphic graphic;

#ifndef ANDROID
	sf::RenderWindow win_debug(sf::VideoMode(1000, 600), "MREmu Debug");
	sf::RenderWindow win_device(sf::VideoMode(graphic.width, graphic.height + 208), "MREmu Device");
	ImGui::SFML::Init(win_debug);
	win_debug.setFramerateLimit(60);
	win_device.setFramerateLimit(60);
	//win_debug.setVerticalSyncEnabled(true);
#else
	sf::RenderWindow win_device(sf::VideoMode::getDesktopMode(), "MREmu");
	win_device.setFramerateLimit(60);

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

#ifdef _WIN32
#ifndef ANDROID
	g_hwnd_debug = (HWND)win_debug.getSystemHandle();
	DragAcceptFiles(g_hwnd_debug, TRUE);
	old_wndproc_debug = (WNDPROC)SetWindowLongPtrW(g_hwnd_debug, GWLP_WNDPROC, (LONG_PTR)drop_wndproc_debug);
#endif
	g_hwnd_device = (HWND)win_device.getSystemHandle();
	DragAcceptFiles(g_hwnd_device, TRUE);
	old_wndproc_device = (WNDPROC)SetWindowLongPtrW(g_hwnd_device, GWLP_WNDPROC, (LONG_PTR)drop_wndproc_device);
#endif

	MREngine::IO::init();

	if (GDB::gdb_mode)
		GDB::cpu_state = GDB::Stop;

	std::thread second_thread(mre_main, &appManager);

	Keyboard keyboard;
	Touch touch;

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


	dev_base_w = graphic.width;
	dev_base_h = graphic.height + 208;

	float scale = 1.f;
	sf::Sprite screen_sp(graphic.screen_tex);
	touch.screen = &screen_sp;
	keyboard.screen = &screen_sp;

	auto repaint_device = [&]() {
		win_device.clear(sf::Color::Black);
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
		win_device.display();
	};

	auto update_screen_size = [&](unsigned int new_w, unsigned int new_h) {
		static bool resizing = false;
		if (resizing)
			return;
		resizing = true;

		constexpr float scale_step = 0.05f;
		float scale_x = (float)new_w / (float)dev_base_w;
		float scale_y = (float)new_h / (float)dev_base_h;

		scale = std::round(std::max(scale_x, scale_y) / scale_step) * scale_step;
		if (scale < 1.0f)
			scale = 1.0f;

		unsigned int target_w = (unsigned int)std::round((float)dev_base_w * scale);
		unsigned int target_h = (unsigned int)std::round((float)dev_base_h * scale);

		if (win_device.getSize().x != target_w || win_device.getSize().y != target_h)
			win_device.setSize(sf::Vector2u(target_w, target_h));

		win_device.setView(sf::View(sf::FloatRect(0.f, 0.f, (float)target_w, (float)target_h)));

		screen_sp.setScale(scale, scale);
		screen_sp.setPosition(0.f, 0.f);

		keyboard.update_resize(target_w, target_h);

		repaint_device();

		resizing = false;
	};

#ifdef _WIN32
	g_repaint_device = [&]() {
		repaint_device();
	};
	g_on_device_resize = [&](unsigned int w, unsigned int h) {
		update_screen_size(w, h);
	};
#endif

	update_screen_size(win_device.getSize().x, win_device.getSize().y);

	sf::Clock fps;

	sf::Clock deltaClock;
	sf::Event event;

	while (win_device.isOpen()
#ifndef ANDROID
        && win_debug.isOpen()
#endif
    ) {
#ifndef ANDROID
		while (win_debug.pollEvent(event)) {
			ImGui::SFML::ProcessEvent(event);
			switch (event.type) {
			case sf::Event::Closed:
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
			keyboard.event(event);
			touch.sf_event(event);
			switch (event.type) {
			case sf::Event::Closed:
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
			ImGui::Text("%s", error_message.c_str());
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
#endif

		graphic.update_screen();

#ifndef ANDROID
		graphic.imgui_screen();
		App* active_app = appManager.get_active_app();
		if (active_app) {
			active_app->graphic.imgui_layers();
			active_app->graphic.imgui_canvases();
		}

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

		if (ImGui::Begin("Fps")) {
			ImGui::Text("%1.3f", 1.f / fps.restart().asSeconds());
		}
		ImGui::End();

		if (ImGui::Begin("Control")) {
			if (ImGui::Button("Open c:/ (fs/c)"))
				open_folder("fs/c");
			if (ImGui::Button("Open d:/ (fs/d)"))
				open_folder("fs/d");
			if (ImGui::Button("Open e:/ (fs/e)"))
				open_folder("fs/e");
			if (ImGui::Button("Open e:/mre (fs/e/mre)"))
				open_folder("fs/e/mre");
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

		repaint_device();

#ifndef ANDROID
		Cpu::imgui_REG();

		keyboard.imgui_keyboard();
#endif

#ifndef ANDROID
		ImGui::SFML::Render(win_debug);
		win_debug.display();
		win_debug.clear();
#endif
	}

#ifdef _WIN32
	g_repaint_device = nullptr;
	g_on_device_resize = nullptr;
#endif

	work = false;
	second_thread.join();

#ifndef ANDROID
	ImGui::SFML::Shutdown();
#endif
	return 0;
}