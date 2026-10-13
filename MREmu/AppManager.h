#pragma once
#include "App.h"
#include <mutex>
#include <filesystem>
#include "Queue.h"
#include "NativeApps/Menu/AppSelector.h"

namespace fs = std::filesystem;

struct launch_el {
	fs::path path;
	bool local;
	const nativeapp_conf* conf = 0;
};

struct keyboard_event_el {
	int event;
	int keycode;
};

struct pen_event_el {
	int event;
	int x, y;
};

struct message_event_el {
	int phandle;
	unsigned int msg_id;
	int wparam;
	int lparam;
	int phandle_sender;
};

struct system_event_el {
	int phandle;
	int message;
	int param;
};

class AppManager {
	Queue<launch_el> launch_queue;
	Queue<keyboard_event_el> keyboard_events_queue;
	Queue<pen_event_el> pen_events_queue;
	Queue<message_event_el> message_events_queue;
	Queue<system_event_el> system_events_queue;

	friend MreTags* get_tags_by_mem_adr(size_t offset_mem); //TODO: remove this

	std::vector<std::shared_ptr<App>> apps;
	int active_app_id = -1; // can be acces from another theard
	int current_work_app_id = -1; // only for app theard 

	void fix_mtone_wireless();
public:
	std::mutex active_app_mutex;

	void add_app_for_launch(fs::path path, bool local, const nativeapp_conf* conf = 0);
	void launch_apps();

	void close_app(int id, bool soft);
	void close_apps();

	void add_keyboard_event(int event, int keycode);
	void process_keyboard_events();

	void add_pen_event(int event, int x, int y);
	void process_pen_events();

	void add_message_event(int phandle, unsigned int msg_id,
		int wparam, int lparam, int phandle_sender);
	void process_message_events();

	void add_system_event(int phandle, int message, int param);
	void process_system_events();

	void update(size_t delta_ms);

	App* get_active_app();
	App* get_current_work_app_id();
};