#pragma once
#include "App.h"
#include <mutex>
#include <map>
#include <filesystem>
#include "Queue.h"
#include "History.h"
#include "NativeApps/Menu/AppSelector.h"

namespace fs = std::filesystem;

struct launch_el {
	fs::path path;
	bool local;
	const nativeapp_conf* conf = 0;
};

class AppManager {
	static AppManager* ptr;

	Queue<launch_el> launch_queue;
	Queue<Event::input> input_events_queue;
	Queue<Event::system> system_events_queue;
	Queue<Event::network> network_events_queue;

	std::mutex events_mutex;
	std::condition_variable events_cv;

	bool is_all_events_empty();

	bool running = false;
	std::thread loop_thread;

	std::map<int, std::shared_ptr<App>> apps;
	std::mutex foreground_app_mutex;
	History foreground_app_history;
	int foreground_app = -1; // can be acces from another theard
	int executing_app = -1; // only for app theard 
	int phandle_counter = 0;

	void fix_mtone_wireless(App* app);

	void run_sys_event(App* app, int event, int param);

	App* push_foreground_app(int phandle, bool fist_time = false);
	App* pop_foreground_app(int phandle);

	void process_launch_app(launch_el launch_data);

	void process_event(const Event::system& ev);
	void process_event(const Event::input& ev);
	void process_event(const Event::network& ev);

	sf::Time get_time_for_nearest_timer();

	void loop();
public:
	AppManager();
	~AppManager();
	static AppManager* getPtr();

	void start();
	void stop();

	void add_app_for_launch(fs::path path, bool local, const nativeapp_conf* conf = 0);

	void close_app(int phandle, bool soft);
	void close_apps();

	void push_event(const Event::system& ev);
	void push_event(const Event::input& ev);
	void push_event(const Event::network& ev);

	std::unique_lock<std::mutex> get_foreground_app_guard();

	App* get_foreground_app();
	App* get_executing_app();

	App* get_app_by_phandle(int phandle);
	App* get_app_by_mem_adr(size_t offset_mem);

	App* set_executing_app_by_phandle(int phandle);
	App* set_foreground_as_executing();
};