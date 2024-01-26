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

class AppManager {
	std::queue<launch_el> launch_queue;
	std::mutex launch_queue_mutex;
public:
	std::vector<App> apps;
	int active_app_id = -1;
	int current_work_app_id = -1;

	void add_app_for_launch(fs::path path, bool global);
	void launch_apps();

	App* get_active_app();
	App* get_current_work_app_id();
};