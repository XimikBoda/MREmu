#include "DragAndDrop.h"
#include "AppManager.h"
#include "NativeApp.h"
#include "NativeApps/Menu/AppSelector.h"
#include "ArmApp.h"
#include "DLLApp.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <system_error>

extern AppManager* g_appManager;

namespace DragAndDrop {

bool show_warning = false;
sf::Clock warning_clock;

bool calculate_snap(const Rect& moving_rect, const Rect& target_rect, int threshold, Rect& out_snapped) {
	int vis_w = moving_rect.width();
	int vis_h = moving_rect.height();

	// If device window is inside or deeply overlapping the target window (>35% area), avoid snapping
	int overlap_l = std::max(moving_rect.left, target_rect.left);
	int overlap_r = std::min(moving_rect.right, target_rect.right);
	int overlap_t = std::max(moving_rect.top, target_rect.top);
	int overlap_b = std::min(moving_rect.bottom, target_rect.bottom);

	if (overlap_l < overlap_r && overlap_t < overlap_b) {
		int overlap_area = (overlap_r - overlap_l) * (overlap_b - overlap_t);
		int dev_area = vis_w * vis_h;
		if (dev_area > 0 && overlap_area > dev_area * 0.35f) {
			out_snapped = moving_rect;
			return false;
		}
	}

	out_snapped = moving_rect;
	bool snapped = false;

	bool v_near = (moving_rect.bottom >= target_rect.top - threshold) && (moving_rect.top <= target_rect.bottom + threshold);
	if (v_near) {
		if (std::abs(moving_rect.left - target_rect.right) <= threshold) {
			out_snapped.left = target_rect.right;
			out_snapped.right = out_snapped.left + vis_w;
			snapped = true;
		}
		else if (std::abs(moving_rect.right - target_rect.left) <= threshold) {
			out_snapped.right = target_rect.left;
			out_snapped.left = out_snapped.right - vis_w;
			snapped = true;
		}
		else if (std::abs(moving_rect.left - target_rect.left) <= threshold) {
			out_snapped.left = target_rect.left;
			out_snapped.right = out_snapped.left + vis_w;
			snapped = true;
		}
		else if (std::abs(moving_rect.right - target_rect.right) <= threshold) {
			out_snapped.right = target_rect.right;
			out_snapped.left = out_snapped.right - vis_w;
			snapped = true;
		}
	}

	bool h_near = (moving_rect.right >= target_rect.left - threshold) && (moving_rect.left <= target_rect.right + threshold);
	if (h_near) {
		if (std::abs(moving_rect.top - target_rect.bottom) <= threshold) {
			out_snapped.top = target_rect.bottom;
			out_snapped.bottom = out_snapped.top + vis_h;
			snapped = true;
		}
		else if (std::abs(moving_rect.bottom - target_rect.top) <= threshold) {
			out_snapped.bottom = target_rect.top;
			out_snapped.top = out_snapped.bottom - vis_h;
			snapped = true;
		}
		else if (std::abs(moving_rect.top - target_rect.top) <= threshold) {
			out_snapped.top = target_rect.top;
			out_snapped.bottom = out_snapped.top + vis_h;
			snapped = true;
		}
		else if (std::abs(moving_rect.bottom - target_rect.bottom) <= threshold) {
			out_snapped.bottom = target_rect.bottom;
			out_snapped.top = out_snapped.bottom - vis_h;
			snapped = true;
		}
	}

	return snapped;
}

bool check_is_docked(const Rect& a, const Rect& b, int tol) {
	bool touching_right = (std::abs(a.left - b.right) <= tol) &&
		(a.bottom >= b.top - tol) && (a.top <= b.bottom + tol);
	bool touching_left = (std::abs(a.right - b.left) <= tol) &&
		(a.bottom >= b.top - tol) && (a.top <= b.bottom + tol);
	bool touching_bottom = (std::abs(a.top - b.bottom) <= tol) &&
		(a.right >= b.left - tol) && (a.left <= b.right + tol);
	bool touching_top = (std::abs(a.bottom - b.top) <= tol) &&
		(a.right >= b.left - tol) && (a.left <= b.right + tol);

	return touching_right || touching_left || touching_bottom || touching_top;
}

void calculate_stepped_aspect_size(int cur_w, int cur_h, int base_w, int base_h, float scale_step, int& out_w, int& out_h) {
	if (base_w <= 0 || base_h <= 0) {
		out_w = cur_w;
		out_h = cur_h;
		return;
	}

	float scale_val = std::max((float)cur_w / (float)base_w, (float)cur_h / (float)base_h);
	scale_val = std::round(scale_val / scale_step) * scale_step;
	if (scale_val < 1.0f)
		scale_val = 1.0f;

	out_w = (int)std::round((float)base_w * scale_val);
	out_h = (int)std::round((float)base_h * scale_val);
}

void process_dropped_files(const std::vector<std::filesystem::path>& files,
						   bool is_move,
						   void* native_window_handle) {
	std::vector<std::filesystem::path> valid_files;
	for (const auto& src : files) {
		std::string ext = src.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		bool valid = false;
		if (ext == ".vxp" && std::filesystem::is_regular_file(src)) {
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

	if (valid_files.empty())
		return;

	bool should_move = false;
	if (is_move) {
#ifdef _WIN32
		HWND hwnd = (HWND)native_window_handle;
		int result = MessageBoxW(hwnd, L"Do you want to move the VXP file(s) to fs/e/mre instead of copying?", L"Move VXP", MB_YESNO | MB_ICONQUESTION);
		if (result == IDYES)
			should_move = true;
#else
		// For future Linux/POSIX platforms: use a GUI message box (e.g. Zenity/KDialog or ImGui modal) or default to move
		should_move = true;
#endif
	}

	std::filesystem::path dest_dir = std::filesystem::path("fs/e/mre").make_preferred();
	std::filesystem::create_directories(dest_dir);

	bool imported = false;
	for (const auto& src : valid_files) {
		std::error_code ec;
		std::filesystem::path target = dest_dir / src.filename();
		if (should_move) {
			std::filesystem::rename(src, target, ec);
			if (ec) {
				ec.clear();
				std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
				if (!ec)
					std::filesystem::remove(src, ec);
			}
		} else {
			std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
		}
		if (!ec)
			imported = true;
	}

	if (imported && g_appManager) {
		NativeApp* native_app = dynamic_cast<NativeApp*>(g_appManager->get_active_app());
		if (native_app && native_app->conf.entry == NativeApps::Menu::AppSelector::entry)
			NativeApps::Menu::AppSelector::rescan();
	}
}

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>

class WindowsPlatformBackend : public PlatformBackend {
public:
	static WindowsPlatformBackend* s_instance;

	WNDPROC old_wndproc_debug = NULL;
	WNDPROC old_wndproc_device = NULL;

	HWND hwnd_debug = NULL;
	HWND hwnd_device = NULL;

	int base_w = 240;
	int base_h = 528;

	std::function<void(unsigned int, unsigned int)> on_resize;
	std::function<void()> on_repaint;

	int dev_grab_x = 0;
	int dev_grab_y = 0;
	bool dev_is_moving = false;

	int dbg_last_x = 0;
	int dbg_last_y = 0;
	bool device_was_docked_to_debug = false;

	WindowsPlatformBackend() {
		s_instance = this;
	}

	~WindowsPlatformBackend() override {
		cleanup();
		if (s_instance == this)
			s_instance = nullptr;
	}

	void set_base_size(int w, int h) override {
		base_w = w;
		base_h = h;
	}

	void set_callbacks(std::function<void(unsigned int, unsigned int)> resize_cb,
					   std::function<void()> repaint_cb) override {
		on_resize = std::move(resize_cb);
		on_repaint = std::move(repaint_cb);
	}

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

	bool is_device_docked() {
		if (!hwnd_debug || !hwnd_device || !IsWindow(hwnd_debug) || !IsWindow(hwnd_device))
			return false;
		RECT dbg_vis, dev_vis;
		if (!get_visual_rect(hwnd_debug, &dbg_vis) || !get_visual_rect(hwnd_device, &dev_vis))
			return false;

		Rect a = { dev_vis.left, dev_vis.top, dev_vis.right, dev_vis.bottom };
		Rect b = { dbg_vis.left, dbg_vis.top, dbg_vis.right, dbg_vis.bottom };
		return check_is_docked(a, b, 4);
	}

	void snap_device(RECT* r) {
		if (!hwnd_debug || !IsWindow(hwnd_debug) || !IsWindowVisible(hwnd_debug))
			return;

		RECT dbg_vis;
		if (!get_visual_rect(hwnd_debug, &dbg_vis))
			return;

		int w = r->right - r->left;
		int h = r->bottom - r->top;

		int ideal_left = r->left;
		int ideal_top = r->top;
		if (dev_is_moving) {
			POINT pt;
			GetCursorPos(&pt);
			ideal_left = pt.x - dev_grab_x;
			ideal_top = pt.y - dev_grab_y;
		}

		int m_l, m_t, m_r, m_b;
		get_shadow_margins(hwnd_device, m_l, m_t, m_r, m_b);

		Rect ideal_vis = { ideal_left + m_l, ideal_top + m_t, ideal_left + w - m_r, ideal_top + h - m_b };
		Rect dbg_rect = { dbg_vis.left, dbg_vis.top, dbg_vis.right, dbg_vis.bottom };
		Rect snapped_vis;

		calculate_snap(ideal_vis, dbg_rect, 16, snapped_vis);

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

		std::vector<std::filesystem::path> files;
		for (UINT i = 0; i < count; ++i) {
			UINT len = DragQueryFileW(hDrop, i, NULL, 0);
			std::wstring path(len, L'\0');
			DragQueryFileW(hDrop, i, &path[0], len + 1);
			files.push_back(std::filesystem::path(path));
		}
		DragFinish(hDrop);

		process_dropped_files(files, is_shift, (void*)hwnd);
	}

	static LRESULT CALLBACK debug_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto* self = s_instance;
		if (!self)
			return DefWindowProcW(hwnd, msg, wParam, lParam);

		if (msg == WM_DROPFILES) {
			handle_drop((HDROP)wParam, hwnd);
			return 0;
		}
		if (msg == WM_ENTERSIZEMOVE) {
			RECT dbg_rect;
			GetWindowRect(hwnd, &dbg_rect);
			self->dbg_last_x = dbg_rect.left;
			self->dbg_last_y = dbg_rect.top;
			self->device_was_docked_to_debug = self->is_device_docked();
		}
		if (msg == WM_MOVING) {
			RECT* r = (RECT*)lParam;
			int dx = r->left - self->dbg_last_x;
			int dy = r->top - self->dbg_last_y;
			self->dbg_last_x = r->left;
			self->dbg_last_y = r->top;

			if (self->device_was_docked_to_debug && self->hwnd_device && IsWindow(self->hwnd_device) && (dx != 0 || dy != 0)) {
				RECT dev_rect;
				GetWindowRect(self->hwnd_device, &dev_rect);
				SetWindowPos(self->hwnd_device, NULL,
					dev_rect.left + dx, dev_rect.top + dy,
					0, 0,
					SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return CallWindowProcW(self->old_wndproc_debug, hwnd, msg, wParam, lParam);
		}
		if (msg == WM_EXITSIZEMOVE) {
			self->device_was_docked_to_debug = false;
		}
		return CallWindowProcW(self->old_wndproc_debug, hwnd, msg, wParam, lParam);
	}

	static LRESULT CALLBACK device_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto* self = s_instance;
		if (!self)
			return DefWindowProcW(hwnd, msg, wParam, lParam);

		if (msg == WM_DROPFILES) {
			handle_drop((HDROP)wParam, hwnd);
			return 0;
		}
		if (msg == WM_ENTERSIZEMOVE) {
			POINT pt;
			GetCursorPos(&pt);
			RECT win_rect;
			GetWindowRect(hwnd, &win_rect);
			self->dev_grab_x = pt.x - win_rect.left;
			self->dev_grab_y = pt.y - win_rect.top;
			self->dev_is_moving = true;
		}
		if (msg == WM_MOVING) {
			RECT* r = (RECT*)lParam;
			self->snap_device(r);
			return TRUE;
		}
		if (msg == WM_EXITSIZEMOVE) {
			self->dev_is_moving = false;
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

			int stepped_w = 0, stepped_h = 0;
			calculate_stepped_aspect_size(cur_w, cur_h, self->base_w, self->base_h, 0.05f, stepped_w, stepped_h);

			int new_w = stepped_w + bw;
			int new_h = stepped_h + bh;

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
			LRESULT res = CallWindowProcW(self->old_wndproc_device, hwnd, msg, wParam, lParam);
			if (self->on_resize && wParam != SIZE_MINIMIZED) {
				self->on_resize(LOWORD(lParam), HIWORD(lParam));
			}
			return res;
		}
		if (msg == WM_PAINT) {
			LRESULT res = CallWindowProcW(self->old_wndproc_device, hwnd, msg, wParam, lParam);
			if (self->on_repaint)
				self->on_repaint();
			return res;
		}
		return CallWindowProcW(self->old_wndproc_device, hwnd, msg, wParam, lParam);
	}

	void init(sf::RenderWindow& win_device, sf::RenderWindow* win_debug) override {
		if (win_debug) {
			hwnd_debug = (HWND)win_debug->getSystemHandle();
			DragAcceptFiles(hwnd_debug, TRUE);
			old_wndproc_debug = (WNDPROC)SetWindowLongPtrW(hwnd_debug, GWLP_WNDPROC, (LONG_PTR)debug_proc);
		}
		hwnd_device = (HWND)win_device.getSystemHandle();
		DragAcceptFiles(hwnd_device, TRUE);
		old_wndproc_device = (WNDPROC)SetWindowLongPtrW(hwnd_device, GWLP_WNDPROC, (LONG_PTR)device_proc);
	}

	void cleanup() override {
		if (hwnd_debug && old_wndproc_debug) {
			SetWindowLongPtrW(hwnd_debug, GWLP_WNDPROC, (LONG_PTR)old_wndproc_debug);
			old_wndproc_debug = NULL;
		}
		if (hwnd_device && old_wndproc_device) {
			SetWindowLongPtrW(hwnd_device, GWLP_WNDPROC, (LONG_PTR)old_wndproc_device);
			old_wndproc_device = NULL;
		}
		on_resize = nullptr;
		on_repaint = nullptr;
	}
};

WindowsPlatformBackend* WindowsPlatformBackend::s_instance = nullptr;

#endif // _WIN32

// Linux / POSIX platform backend stub (X11 / Wayland)
class LinuxPlatformBackend : public PlatformBackend {
public:
	int base_w = 240;
	int base_h = 528;
	std::function<void(unsigned int, unsigned int)> on_resize;
	std::function<void()> on_repaint;

	void set_base_size(int w, int h) override {
		base_w = w;
		base_h = h;
	}

	void set_callbacks(std::function<void(unsigned int, unsigned int)> resize_cb,
					   std::function<void()> repaint_cb) override {
		on_resize = std::move(resize_cb);
		on_repaint = std::move(repaint_cb);
	}

	void init(sf::RenderWindow& win_device, sf::RenderWindow* win_debug) override {
		// On Linux X11:
		// 1. Obtain Window handles via win_device.getSystemHandle() and win_debug->getSystemHandle().
		// 2. Register for XDND (X Drag-and-Drop) protocol:
		//    - Set XdndAware property on the X11 window.
		//    - Filter ClientMessage events for XdndEnter, XdndPosition, XdndStatus, XdndDrop.
		//    - On drop, retrieve text/uri-list from selection, convert to std::vector<std::filesystem::path>,
		//      and invoke DragAndDrop::process_dropped_files(paths, is_move, native_window).
		// 3. For window snapping and aspect-ratio resizing:
		//    - Intercept ConfigureNotify / MotionNotify events.
		//    - Apply calculate_snap() and calculate_stepped_aspect_size().
		//
		// On Linux Wayland:
		// - Listen to wl_data_device.data_offer and wl_data_device.drop events,
		//   then forward parsed file paths to DragAndDrop::process_dropped_files().
	}

	void cleanup() override {
		on_resize = nullptr;
		on_repaint = nullptr;
	}
};

// Fallback for unsupported and headless platforms
class GenericPlatformBackend : public PlatformBackend {
public:
	void set_base_size(int, int) override {}
	void set_callbacks(std::function<void(unsigned int, unsigned int)>, std::function<void()>) override {}
	void init(sf::RenderWindow&, sf::RenderWindow*) override {}
	void cleanup() override {}
};

static std::unique_ptr<PlatformBackend> g_backend;

static PlatformBackend& get_backend() {
	if (!g_backend) {
#if defined(_WIN32)
		g_backend = std::make_unique<WindowsPlatformBackend>();
#elif defined(__linux__)
		g_backend = std::make_unique<LinuxPlatformBackend>();
#else
		g_backend = std::make_unique<GenericPlatformBackend>();
#endif
	}
	return *g_backend;
}

void init(sf::RenderWindow& win_device, sf::RenderWindow* win_debug) {
	get_backend().init(win_device, win_debug);
}

void set_base_size(int base_w, int base_h) {
	get_backend().set_base_size(base_w, base_h);
}

void set_callbacks(std::function<void(unsigned int, unsigned int)> on_resize,
				   std::function<void()> on_repaint) {
	get_backend().set_callbacks(std::move(on_resize), std::move(on_repaint));
}

void cleanup() {
	if (g_backend) {
		g_backend->cleanup();
		g_backend.reset();
	}
}

} // namespace DragAndDrop
