#pragma once

#include <SFML/Graphics.hpp>
#include <filesystem>
#include <functional>
#include <vector>

namespace DragAndDrop {

extern bool show_warning;
extern sf::Clock warning_clock;

// Platform-independent rectangle representation for window geometry
struct Rect {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;

	int width() const { return right - left; }
	int height() const { return bottom - top; }
};

// Pure geometric window snapping & docking algorithms (reusable across all platforms)
bool calculate_snap(const Rect& moving_rect, const Rect& target_rect, int threshold, Rect& out_snapped);
bool check_is_docked(const Rect& a, const Rect& b, int tolerance = 4);
void calculate_stepped_aspect_size(int cur_w, int cur_h, int base_w, int base_h, float scale_step, int& out_w, int& out_h);

// Core cross-platform handling of dropped files (validates VXPs, handles shift-move vs copy, updates filesystem & menu)
void process_dropped_files(const std::vector<std::filesystem::path>& files,
						   bool is_move,
						   void* native_window_handle = nullptr);

// Abstract platform interface for window events, drag-and-drop, and magnetic snapping
class PlatformBackend {
public:
	virtual ~PlatformBackend() = default;
	virtual void init(sf::RenderWindow& win_device, sf::RenderWindow* win_debug) = 0;
	virtual void cleanup() = 0;
	virtual void set_base_size(int base_w, int base_h) = 0;
	virtual void set_callbacks(std::function<void(unsigned int, unsigned int)> on_resize,
							   std::function<void()> on_repaint) = 0;
};

// High-level public API used by MREmu.cpp
void init(sf::RenderWindow& win_device, sf::RenderWindow* win_debug = nullptr);
void set_base_size(int base_w, int base_h);
void set_callbacks(std::function<void(unsigned int, unsigned int)> on_resize,
				   std::function<void()> on_repaint);
void cleanup();

} // namespace DragAndDrop
