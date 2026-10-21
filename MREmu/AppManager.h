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

	friend MreTags* get_tags_by_mem_adr(size_t offset_mem); //TODO: remove this

	std::vector<std::shared_ptr<App>> apps;
	int active_app_id = -1; // can be acces from another theard
	int current_work_app_id = -1; // only for app theard 

	void fix_mtone_wireless();

	void process_launch_app(launch_el launch_data);

	int find_app_id_by_phandle(int phandle);

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

	std::mutex active_app_mutex;

	void add_app_for_launch(fs::path path, bool local, const nativeapp_conf* conf = 0);

	void close_app(int id, bool soft);
	void close_apps();

	void push_event(const Event::system& ev);
	void push_event(const Event::input& ev);
	void push_event(const Event::network& ev);

	App* get_active_app(); // TODO: rename it
	App* get_current_work_app_id();
	App* get_app_by_phandle(int phandle);
	App* get_app_by_mem_adr(size_t offset_mem);
	App* set_executing_app_by_phandle(int phandle);
	App* set_foreground_as_executing();
};