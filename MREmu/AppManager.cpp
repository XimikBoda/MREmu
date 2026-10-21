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

extern AppManager* g_appManager; //TODO remove this

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

void AppManager::fix_mtone_wireless() {
	auto app = get_active_app();

	if (app->tags.get_dev_name() == u8"Mtone Wireless") {
#ifdef _WIN32
		auto f = fopen("NUL", "rb");
#else
		auto f = fopen("/dev/null", "rb");
#endif
		app->io.files.push(f);
	}
}

void AppManager::add_app_for_launch(fs::path path, bool local, const nativeapp_conf* conf)
{
	launch_queue.push(events_mutex, { path, local, conf });
	events_cv.notify_one();
}

void AppManager::process_launch_app(launch_el launch_data)
{
	std::shared_ptr<App> app;

	if (launch_data.conf) {
		app = std::make_shared<NativeApp>();

		((NativeApp*)app.get())->conf = *launch_data.conf;

		app->preparation();
	}
	else {
		if (ArmApp::check_format(launch_data.path, launch_data.local))
			app = std::make_shared<ArmApp>();

#ifdef WIN32
		if (DLLApp::check_format(launch_data.path, launch_data.local))
			app = std::make_shared<DLLApp>();
#endif // WIN32

		if (!app) {
			error_message = "VXP file format not recognized or unsupported:\n" + launch_data.path.string();
			show_error = true;
			return;
		}

		if (!app->load_from_file(launch_data.path, launch_data.local)) {
			error_message = "Failed to load VXP file (file not found or access denied):\n" + launch_data.path.string();
			show_error = true;
			return;
		}

		if (!app->preparation()) {
			error_message = "VXP preparation failed (corrupt file or invalid ELF):\n" + launch_data.path.string();
			show_error = true;
			return;
		}
	}

	apps.push_back(app);

	{
		std::lock_guard lock(active_app_mutex);

		active_app_id = apps.size() - 1;
		current_work_app_id = apps.size() - 1;

		apps[current_work_app_id]->resources.file_context = &(apps[current_work_app_id]->file_context); //todo
	}

	static int phandle = 0;
	phandle++;

	apps[current_work_app_id]->phandle = phandle;

	fix_mtone_wireless(); // file handle 0 fix

	apps[current_work_app_id]->start();

	add_system_event(phandle, VM_MSG_CREATE, 0);
	add_system_event(phandle, VM_MSG_PAINT, 0);
}

void AppManager::close_app(int id, bool soft)
{
	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->phandle == id) {
			auto& state = apps[i]->state;
			
			App::State close_state = soft ? App::SoftClosing : App::HardClosing;
			if (state < close_state)
				state = close_state;
		}
}

void AppManager::close_apps()
{
	for (int i = 0; i < apps.size(); ++i){
		auto& state = apps[i]->state;

		if (state == App::HardClosing) {
			std::lock_guard lock(active_app_mutex);

			apps.erase(apps.begin() + i);

			if (active_app_id > i)
				active_app_id--;
			else if (active_app_id == i)
				active_app_id = apps.size() - 1;

			--i;
		}
	}
}

void AppManager::add_message_event(int phandle, unsigned int msg_id,
	int wparam, int lparam, int phandle_sender)
{
	message_events_queue.push(events_mutex, { phandle, msg_id, wparam, lparam, phandle_sender });
	events_cv.notify_one(); // TODO: Do I need it here
}

int AppManager::find_app_id_by_phandle(int phandle) {
	int app_id = -1;

	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->phandle == phandle) {
			app_id = i;
			break;
		}

	return app_id;
}

void AppManager::process_message_event(message_event_el me)
{
	current_work_app_id = find_app_id_by_phandle(me.phandle);
	if (current_work_app_id == -1)
		return;

	App *cur_app = apps[current_work_app_id].get();
	if (cur_app && cur_app->system_callbacks.msg_proc)
		cur_app->run(cur_app->system_callbacks.msg_proc,
			me.phandle_sender, me.msg_id, me.wparam, me.lparam);
}

void AppManager::add_system_event(int phandle, int message, int param)
{
	system_events_queue.push(events_mutex, { phandle, message, param });
	events_cv.notify_one(); // TODO: here too
}

void AppManager::process_system_event(system_event_el se)
{
	current_work_app_id = find_app_id_by_phandle(se.phandle);
	if (current_work_app_id == -1)
		return;

	App& cur_app = *apps[current_work_app_id];
	if (cur_app.system_callbacks.sysevt)
		cur_app.run(cur_app.system_callbacks.sysevt, se.message, se.param);
}

void AppManager::push_event(const Event::input& ev) {
	input_events_queue.push(events_mutex, ev);
	events_cv.notify_one();
}

void AppManager::push_event(const Event::network& ev) {
	network_events_queue.push(events_mutex, ev);
	events_cv.notify_one();
}

void AppManager::process_event(const Event::input& ev) {
	App* app = set_foreground_as_executing();

	if (app)
		std::visit([app](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (std::is_same_v<T, Event::keyboard>) {
			if (app->io.key_handler)
				app->run(app->io.key_handler, arg.event, arg.keycode);
		}
		else if constexpr (std::is_same_v<T, Event::pen>) {
			if (app->io.pen_handler)
				app->run(app->io.pen_handler, arg.event, arg.x, arg.y);
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

	for (int i = 0; i < apps.size(); ++i) {
		sf::Time nearst_for_app = apps[i]->timer.get_time_for_nearest();

		if (nearst > nearst_for_app)
			nearst = nearst_for_app;
	}

	return nearst;
}

bool AppManager::is_all_events_empty() {
	return launch_queue.empty() &&
		input_events_queue.empty() &&
		message_events_queue.empty() && system_events_queue.empty() &&
		network_events_queue.empty();
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
		GDB::update(); //TODO
		bool event = false;

		auto wait_time_ms = get_time_for_nearest_timer().asMicroseconds();
		auto wait_time = std::chrono::microseconds(wait_time_ms);

		std::unique_lock lock(events_mutex);
		if(wait_time_ms > 0)
			event = events_cv.wait_for(lock, wait_time, [this] { return !is_all_events_empty() || !running; });

		run_queue(launch_queue, lock, [this](launch_el data) { process_launch_app(data); });
		run_queue(system_events_queue, lock, [this](system_event_el data) { process_system_event(data); });
		run_queue(message_events_queue, lock, [this](message_event_el data) { process_message_event(data); });
		run_queue(input_events_queue, lock, [this](Event::input data) { process_event(data); });
		run_queue(network_events_queue, lock, [this](Event::network data) { process_event(data); });
		
		lock.unlock();

		for (int i = 0; i < apps.size(); ++i) {
			current_work_app_id = i;
			bool active = active_app_id == current_work_app_id;
			App* cur_app = apps[current_work_app_id].get();

			if (!cur_app)
				continue;

			apps[i]->timer.update(cur_app);
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

App* AppManager::get_active_app()
{
	if (active_app_id < 0 || active_app_id >= apps.size())
		return 0;
	else
		return apps[active_app_id].get();
}

App* AppManager::get_current_work_app_id()
{
	if (current_work_app_id < 0 || current_work_app_id >= apps.size())
		return 0;
	else
		return apps[current_work_app_id].get();
}

App* AppManager::get_app_by_phandle(int phandle)
{
	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->phandle == phandle)
			return apps[i].get();

	return 0;
}

App* AppManager::set_executing_app_by_phandle(int phandle)
{
	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->phandle == phandle) {
			current_work_app_id = i;
			return apps[i].get();
		}

	return 0;
}

App* AppManager::set_foreground_as_executing()
{
	if (active_app_id < 0 || active_app_id >= apps.size())
		return 0;
	else {
		current_work_app_id = active_app_id;
		return apps[active_app_id].get();
	}
}

MreTags* get_tags_by_mem_adr(size_t offset_mem) {
	if (!g_appManager)
		return 0;

	auto &apps = g_appManager->apps;
	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->offset_mem == offset_mem)
			return &apps[i]->tags;

	return 0;
}

void add_message_event(int phandle, unsigned int msg_id,
	int wparam, int lparam, int phandle_sender) 
{
	if (g_appManager)
		g_appManager->add_message_event(phandle, msg_id, wparam, 
			lparam, phandle_sender);
}

void add_system_event(int phandle, int message, int param)
{
	if (g_appManager)
		g_appManager->add_system_event(phandle, message, param);
}
