#include "AppManager.h"
#include "Bridge.h"
#include "ArmApp.h"
#include "DLLApp.h"
#include "NativeApp.h"
#include "GDB.h"
#include <string>
#include <vmcert.h>

extern std::string error_message;
extern bool show_error;

AppManager* AppManager::ptr = nullptr;

AppManager::AppManager() {
	if (ptr != nullptr)
		throw std::runtime_error("Network already exists!");

	ptr = this;
}

AppManager::~AppManager() {
	ptr = nullptr;
}

AppManager* AppManager::getPtr() {
	return ptr;
}

void AppManager::fix_mtone_wireless(App* app) {
	if (app->tags.get_dev_name() == u8"Mtone Wireless") {
#ifdef _WIN32
		auto f = fopen("NUL", "rb");
#else
		auto f = fopen("/dev/null", "rb");
#endif
		app->io.files.push(f);
	}
}

void AppManager::run_sys_event(App* app, int event, int param) { // TODO
	if (auto handler = app->system_callbacks.sysevt)
		app->run(handler, event, param);
}

App* AppManager::push_foreground_app(int phandle, bool fist_time) {
	auto lock = get_foreground_app_guard();

	if (foreground_app != -1) {
		App* app = get_foreground_app();
		run_sys_event(app, VM_MSG_INACTIVE, 0); // TODO baground apps
		app->timer.set_active(false);
	}

	App* app = get_app_by_phandle(phandle);
	if (app) {
		foreground_app = foreground_app_history.push(phandle);
		if (!fist_time) {
			run_sys_event(app, VM_MSG_ACTIVE, 0);
			app->timer.set_active(true);
			push_event(Event::system{ phandle, Event::system_msg{ VM_MSG_PAINT, 0} });
		}
	}
	return app;
}

App* AppManager::pop_foreground_app(int phandle) {
	auto lock = get_foreground_app_guard();

	foreground_app = foreground_app_history.pop(phandle);
	App* app = get_foreground_app();
	if (app) {
		run_sys_event(app, VM_MSG_ACTIVE, 0);
		app->timer.set_active(true);
		push_event(Event::system{ phandle, Event::system_msg{ VM_MSG_PAINT, 0} });
	}
	return app;
}

void AppManager::close_app(int phandle, bool soft) {
	App* app = get_app_by_phandle(phandle);
	if (app) {
		auto& state = app->state;

		App::State close_state = soft ? App::SoftClosing : App::HardClosing;
		if (state < close_state)
			state = close_state;
	}
}

void AppManager::close_apps() {
	for (auto it = apps.begin(); it != apps.end();) {
		auto& state = it->second->state;

		if (state == App::HardClosing) {
			pop_foreground_app(it->first);
			
			it = apps.erase(it);
		}
		else
			++it;
	}
}

void AppManager::push_event(const Event::launch& ev) {
	launch_events_queue.push(events_mutex, ev);
	events_cv.notify_one();
}

void AppManager::push_event(const Event::system& ev) {
	system_events_queue.push(events_mutex, ev);
	events_cv.notify_one();
}

void AppManager::push_event(const Event::input& ev) {
	input_events_queue.push(events_mutex, ev);
	events_cv.notify_one();
}

void AppManager::push_event(const Event::network& ev) {
	network_events_queue.push(events_mutex, ev);
	events_cv.notify_one();
}

std::unique_lock<std::mutex> AppManager::get_foreground_app_guard() {
	return std::unique_lock<std::mutex>(foreground_app_mutex);
}

void AppManager::process_event(const Event::launch& ev) {
	std::shared_ptr<App> app;

	if (const auto* native_app = std::get_if<Event::native_app>(&ev.data)) {
		app = std::make_shared<NativeApp>();

		((NativeApp*)app.get())->conf = *native_app;

		app->preparation();
	}
	else{ 
		app = std::make_shared<App>();

		bool ok = false;

		if (const auto* path_app = std::get_if<Event::path_app>(&ev.data))
			ok = app->load(path_app->path, path_app->local);
		else if (const auto* fd_app = std::get_if<Event::fd_app>(&ev.data))
			ok = app->load(fd_app->fd, fd_app->path);

		if (!ok) {
			error_message = "Failed to load VXP file (file not found or access denied):\n" + app->real_path.string();
			show_error = true;
			return;
		}

		if (ArmApp::check_format(app.get()))
			app = std::make_shared<ArmApp>(app.get());

#ifdef WIN32
		if (DLLApp::check_format(app.get()))
			app = std::make_shared<DLLApp>(app.get());
#endif // WIN32

		if (!app) {
			error_message = "VXP file format not recognized or unsupported:\n" + app->real_path.string();
			show_error = true;
			return;
		}

		if (!app->preparation()) {
			error_message = "VXP preparation failed (corrupt file or invalid ELF):\n" + app->real_path.string();
			show_error = true;
			return;
		}
	}

	fix_mtone_wireless(app.get()); // file handle 0 fix

	int app_phandle = ++phandle_counter;

	app->phandle = app_phandle;
	apps[app_phandle] = app;

	set_executing_app_by_phandle(app_phandle);
	push_foreground_app(app_phandle, true);
	app->start();

	run_sys_event(app.get(), VM_MSG_CREATE, 0); // now
	push_event(Event::system{ app_phandle, Event::system_msg{ VM_MSG_PAINT, 0} }); // later

}

void AppManager::process_event(const Event::system& ev) {
	App* app = set_executing_app_by_phandle(ev.phandle);

	if (app)
		std::visit([app](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (std::is_same_v<T, Event::system_msg>) {
			if (auto handler = app->system_callbacks.sysevt)
				app->run(handler, arg.message, arg.param);
		}
		else if constexpr (std::is_same_v<T, Event::message>) {
			if (auto handler = app->system_callbacks.msg_proc)
				app->run(handler, arg.phandle_sender, arg.msg_id, arg.wparam, arg.lparam);
		}
			}, ev.data);
}

void AppManager::process_event(const Event::input& ev) {
	App* app = set_foreground_as_executing();

	if (app)
		std::visit([app](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (std::is_same_v<T, Event::keyboard>) {
			if (auto handler = app->io.key_handler)
				app->run(handler, arg.event, arg.keycode);
		}
		else if constexpr (std::is_same_v<T, Event::pen>) {
			if (auto handler = app->io.pen_handler)
				app->run(handler, arg.event, arg.x, arg.y);
		}
			}, ev.data);
}

void AppManager::process_event(const Event::network& ev) {
	App* app = set_executing_app_by_phandle(ev.phandle);

	if (app)
		std::visit([app](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, Event::sock>) {
				if (arg.callback) 
					app->run(arg.callback, arg.handle, arg.event);
			}
			else if constexpr (std::is_same_v<T, Event::dns>) {
				static vm_soc_dns_result* g_result = (vm_soc_dns_result*)Memory::shared_malloc(sizeof(vm_soc_dns_result));
				*g_result = arg.result;

				if (arg.callback)
					app->run(arg.callback, g_result);
			}
			}, ev.data);
}

sf::Time AppManager::get_time_for_nearest_timer()
{
	sf::Time nearst = sf::milliseconds(INT32_MAX);

	for (const auto& it : apps) {
		sf::Time nearst_for_app = it.second->timer.get_time_for_nearest();

		if (nearst > nearst_for_app)
			nearst = nearst_for_app;
	}

	return nearst;
}

bool AppManager::is_all_events_empty() {
	return launch_events_queue.empty()
		&& input_events_queue.empty()
		&& system_events_queue.empty()
		&& network_events_queue.empty();
}

template<typename T, typename F>
static void run_queue(Queue<T>& q, std::unique_lock<std::mutex>& lock, F f) {
	if (q.empty())
		return;

	Queue<T> copy_q;
	std::swap(copy_q, q);

	lock.unlock();
	while (!copy_q.empty())
		f(copy_q.front());
	lock.lock();
}

void AppManager::loop() {
	while (running) {
#ifdef BUILTIN_GDB
		GDB::update(); //TODO
#endif
		bool event = false;

		auto wait_time_ms = get_time_for_nearest_timer().asMicroseconds();
		auto wait_time = std::chrono::microseconds(wait_time_ms);

		std::unique_lock lock(events_mutex);
		if(wait_time_ms > 0)
			event = events_cv.wait_for(lock, wait_time, [this] { return !is_all_events_empty() || !running; });

		run_queue(launch_events_queue, lock, [this](Event::launch data) { process_event(data); });
		run_queue(system_events_queue, lock, [this](Event::system data) { process_event(data); });
		run_queue(input_events_queue, lock, [this](Event::input data) { process_event(data); });
		run_queue(network_events_queue, lock, [this](Event::network data) { process_event(data); });
		
		lock.unlock();

		for (const auto& it : apps) {
			executing_app = it.first;
			App* app = it.second.get();

			app->timer.update(app);
		}

		close_apps();
	}
}

void AppManager::start() {
	running = true;
	loop_thread = std::thread([this]() { loop(); });
}

void AppManager::stop() {
	running = false;
	events_cv.notify_one();
	loop_thread.join();
}

App* AppManager::get_foreground_app() {
	return get_app_by_phandle(foreground_app);
}

App* AppManager::get_executing_app() {
	return get_app_by_phandle(executing_app);
}

App* AppManager::get_app_by_phandle(int phandle) {
	if (phandle == -1)
		return nullptr;

	auto it = apps.find(phandle);
	if (it != apps.end())
		return it->second.get();
	return nullptr;
}

App* AppManager::get_app_by_mem_adr(size_t offset_mem) {
	for (const auto& it : apps)
		if (it.second->offset_mem == offset_mem)
			return it.second.get();

	return 0;
}

App* AppManager::set_executing_app_by_phandle(int phandle) {
	executing_app = phandle;

	return get_executing_app();
}

App* AppManager::set_foreground_as_executing() {
	executing_app = foreground_app;

	return get_executing_app();
}