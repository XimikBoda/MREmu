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
	static AppManager* ptr;

	Queue<launch_el> launch_queue;
	Queue<keyboard_event_el> keyboard_events_queue;
	Queue<pen_event_el> pen_events_queue;
	Queue<message_event_el> message_events_queue;
	Queue<system_event_el> system_events_queue;
	Queue<Event::network> network_events_queue;

	std::mutex events_mutex;
	std::condition_variable events_cv;

	bool is_all_events_empty();

	bool running = false;
	std::thread loop_thread;

	friend MreTags* get_tags_by_mem_adr(size_t offset_mem); //TODO: remove this

	std::vector<std::shared_ptr<App>> apps;
	int active_app_id = -1; // can be acces from another theard
	int current_work_app_id = -1; // only for app theard 

	void fix_mtone_wireless();

	void process_launch_app(launch_el launch_data);

	void process_keyboard_event(keyboard_event_el ke);
	void process_pen_event(pen_event_el pe);

	int find_app_id_by_phandle(int phandle);

	void process_message_event(message_event_el me);
	void process_system_event(system_event_el se);

	void process_event(const Event::network& ev);

	sf::Time get_time_for_nearest_timer();

	void loop();
public:
	AppManager();
	~AppManager();
	static AppManager* getPtr();

	void start();
	void stop();

	std::mutex active_app_mutex;

	void add_app_for_launch(fs::path path, bool local, const nativeapp_conf* conf = 0);

	void close_app(int id, bool soft);
	void close_apps();

	void add_keyboard_event(int event, int keycode);
	void add_pen_event(int event, int x, int y);

	void add_message_event(int phandle, unsigned int msg_id,
		int wparam, int lparam, int phandle_sender);

	void add_system_event(int phandle, int message, int param);

	void push_event(const Event::network& ev);

	App* get_active_app(); // TODO: rename it
	App* get_current_work_app_id();
	App* get_app_by_phandle(int phandle);
};