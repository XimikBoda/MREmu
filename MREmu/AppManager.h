#pragma once
#include "App.h"
#include <mutex>
#include <filesystem>
#include <queue>

namespace fs = std::filesystem;

struct launch_el {
	fs::path path; 
	bool global;
};

struct keyboard_event_el {
	int event; 
	int keycode;
};

class AppManager {
	std::queue<launch_el> launch_queue;
	std::mutex launch_queue_mutex;


	std::queue<keyboard_event_el> keyboard_events_queue;
	std::mutex keyboard_events_queue_mutex;
public:
	std::vector<App> apps;
	int active_app_id = -1;
	int current_work_app_id = -1;

	void add_app_for_launch(fs::path path, bool global);
	void launch_apps();

	void add_keyboard_event(int event, int keycode);
	void process_keyboard_events();

	App* get_active_app();
	App* get_current_work_app_id();
};