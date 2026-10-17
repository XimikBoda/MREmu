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

struct input_event_el {
	uint32_t callback;
	int state;
	uint32_t text_emu_addr;
};

class AppManager {
	Queue<launch_el> launch_queue;
	Queue<keyboard_event_el> keyboard_events_queue;
	Queue<pen_event_el> pen_events_queue;
	Queue<message_event_el> message_events_queue;
	Queue<system_event_el> system_events_queue;
	Queue<sock_event> sock_events_queue;
	Queue<dns_event> dns_events_queue;

	std::mutex events_mutex;
	std::condition_variable events_cv;
	// Set by wake() before it notifies, cleared by the loop once it has woken. Part
	// of the wait predicate so a wake cannot be lost in the window between the loop
	// deciding to sleep and actually sleeping.
	std::atomic<bool> wake_flag{false};

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

	void process_sock_event(sock_event se);
	void process_dns_event(dns_event de);

	sf::Time get_time_for_nearest_timer();

	void loop();
public:
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

	Queue<input_event_el> input_events_queue;

	void add_input_event(uint32_t callback, int state, uint32_t text_emu_addr);
	void process_input_event(input_event_el ie);

	void reset();

	// Nudge the loop out of its wait so it re-checks timers and event queues now.
	// Needed by external tooling: the loop sleeps until the nearest timer is due, so
	// a timer made overdue from another thread would not be noticed until then.
	// Safe to call without holding events_mutex.
	void wake() {
		wake_flag.store(true, std::memory_order_relaxed);
		events_cv.notify_one();
	}

	static void add_sock_event(sock_event se);
	static void add_dns_event(dns_event de);

	App* get_active_app();
	App* get_current_work_app_id();

	// Read-only views for external tooling (the MCP host). Exposing accessors
	// rather than the containers keeps the launch queue and the mutex-guarded
	// active-app pointer encapsulated.
	std::size_t app_count() const { return apps.size(); }
	App* app_at(std::size_t index) const {
		return index < apps.size() ? apps[index].get() : nullptr;
	}
	App* current_app() {
		if (current_work_app_id < 0 || (std::size_t)current_work_app_id >= apps.size())
			return nullptr;
		return apps[current_work_app_id].get();
	}
};

void add_input_event(uint32_t callback, int state, uint32_t text_emu_addr);