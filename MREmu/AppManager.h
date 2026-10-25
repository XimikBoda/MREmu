#pragma once
#include "App.h"
#include <mutex>
#include <map>
#include <filesystem>
#include "Queue.h"
#include "History.h"
#include "AppManagerEvents.h"

namespace fs = std::filesystem;

class AppManager {
	static AppManager* ptr;

	// Events
	Queue<Event::launch> launch_events_queue;
	Queue<Event::input> input_events_queue;
	Queue<Event::system> system_events_queue;
	Queue<Event::network> network_events_queue;

	std::mutex events_mutex;
	std::condition_variable events_cv;

	void process_event(const Event::launch& ev);
	void process_event(const Event::system& ev);
	void process_event(const Event::input& ev);
	void process_event(const Event::network& ev);

	void run_sys_event(App* app, int event, int param);

	bool is_all_events_empty();
	sf::Time get_time_for_nearest_timer();

	// Loop
	bool running = false;
	std::thread loop_thread;

	void loop();

	// Apps
	std::map<int, std::shared_ptr<App>> apps;
	std::mutex foreground_app_mutex;
	History foreground_app_history;
	int foreground_app = -1; // can be acces from another theard
	int executing_app = -1; // only for app theard 
	int phandle_counter = 1;

	void fix_mtone_wireless(App* app);

	App* push_foreground_app(int phandle, bool fist_time = false);
	App* pop_foreground_app(int phandle);

public:
	AppManager();
	~AppManager();
	static AppManager* getPtr();

	void start();
	void stop();

	void close_app(int phandle, bool soft);
	void close_apps();

	void push_event(const Event::launch& ev);
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