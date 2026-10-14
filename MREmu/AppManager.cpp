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

	static int ph_app_id = 0;
	ph_app_id++;

	apps[current_work_app_id]->system_callbacks.ph_app_id = ph_app_id;

	fix_mtone_wireless(); // file handle 0 fix

	apps[current_work_app_id]->start();

	add_system_event(ph_app_id, VM_MSG_CREATE, 0);
	add_system_event(ph_app_id, VM_MSG_PAINT, 0);
}

void AppManager::close_app(int id, bool soft)
{
	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->system_callbacks.ph_app_id == id) {
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

void AppManager::add_keyboard_event(int event, int keycode)
{
	keyboard_events_queue.push(events_mutex, { event, keycode });
	events_cv.notify_one();
}

void AppManager::process_keyboard_event(keyboard_event_el ke)
{
	current_work_app_id = active_app_id;

	if (current_work_app_id < 0 || current_work_app_id >= apps.size())
		return;

	App& cur_app = *apps[current_work_app_id];
	if (cur_app.io.key_handler)
		cur_app.run(cur_app.io.key_handler, ke.event, ke.keycode);
}

void AppManager::add_pen_event(int event, int x, int y)
{
	pen_events_queue.push(events_mutex, { event, x, y });
	events_cv.notify_one();
}

void AppManager::process_pen_event(pen_event_el pe)
{
	current_work_app_id = active_app_id;

	if (current_work_app_id < 0 || current_work_app_id >= apps.size())
		return;

	App& cur_app = *apps[current_work_app_id];
	if (cur_app.io.pen_handler)
		cur_app.run(cur_app.io.pen_handler, pe.event, pe.x, pe.y);
}

void AppManager::add_message_event(int phandle, unsigned int msg_id,
	int wparam, int lparam, int phandle_sender)
{
	message_events_queue.push(events_mutex, { phandle, msg_id, wparam, lparam, phandle_sender });
	events_cv.notify_one(); // TODO: Do I need it here
}

void AppManager::process_message_event(message_event_el me)
{
	int app_id = -1;

	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->system_callbacks.ph_app_id == me.phandle) {
			app_id = i;
			break;
		}

	if (app_id == -1)
		return;

	current_work_app_id = app_id;

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
	int app_id = -1;

	for (int i = 0; i < apps.size(); ++i)
		if (apps[i]->system_callbacks.ph_app_id == se.phandle) {
			app_id = i;
			break;
		}

	if (app_id == -1)
		return;

	current_work_app_id = app_id;

	App& cur_app = *apps[current_work_app_id];
	if (cur_app.system_callbacks.sysevt)
		cur_app.run(cur_app.system_callbacks.sysevt, se.message, se.param);
}

sf::Time AppManager::get_time_for_nearest_timer()
{
	sf::Time nearst = sf::milliseconds(INT32_MAX);

	for (int i = 0; i < apps.size(); ++i) {
		App* cur_app = apps[current_work_app_id].get();

		if (!cur_app)
			continue;

		sf::Time nearst_for_app = apps[i]->timer.get_time_for_nearest();

		if (nearst > nearst_for_app)
			nearst = nearst_for_app;
	}

	return nearst;
}

bool AppManager::is_all_events_empty() {
	return launch_queue.empty() &&
		keyboard_events_queue.empty() && pen_events_queue.empty() &&
		message_events_queue.empty() && system_events_queue.empty();
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

		if (event) {
			run_queue(launch_queue, lock, [this](launch_el data) { process_launch_app(data); });
			run_queue(system_events_queue, lock, [this](system_event_el data) { process_system_event(data); });
			run_queue(message_events_queue, lock, [this](message_event_el data) { process_message_event(data); });
			run_queue(keyboard_events_queue, lock, [this](keyboard_event_el data) { process_keyboard_event(data); });
			run_queue(pen_events_queue, lock, [this](pen_event_el data) { process_pen_event(data); });
		}

		lock.unlock();

		for (int i = 0; i < apps.size(); ++i) {
			current_work_app_id = i;
			bool active = active_app_id == current_work_app_id;
			App* cur_app = apps[current_work_app_id].get();

			if (!cur_app)
				continue;

			apps[i]->timer.update(cur_app);
			apps[i]->sock.update(cur_app);
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
		return &*apps[active_app_id];
}

App* AppManager::get_current_work_app_id()
{
	if (current_work_app_id < 0 || current_work_app_id >= apps.size())
		return 0;
	else
		return &*apps[current_work_app_id];
}

extern AppManager* g_appManager;

App* get_cur_app() {
	App* curr_app = 0;
	if (g_appManager && (curr_app = g_appManager->get_current_work_app_id()))
		return curr_app;
	else
		abort();
}

bool cur_app_is_native() {
	return get_cur_app()->is_native();
}

Memory::MemoryManager& get_current_app_memory() { 
	return get_cur_app()->app_memory;
}

MREngine::SystemCallbacks& get_current_app_system_callbacks() {
	return get_cur_app()->system_callbacks;
}

MREngine::Resources& get_current_app_resources() {
	return get_cur_app()->resources;
}

MREngine::AppGraphic& get_current_app_graphic() {
	return get_cur_app()->graphic;
}

MREngine::Timer& get_current_app_timer() {
	return get_cur_app()->timer;
}

MREngine::AppIO& get_current_app_io() {
	return get_cur_app()->io;
}

MREngine::AppSock& get_current_app_sock() {
	return get_cur_app()->sock;
}

MREngine::AppAudio& get_current_app_audio() {
	return get_cur_app()->audio;
}

fs::path get_current_app_path() {
	return get_cur_app()->path;
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

void add_keyboard_event(int event, int keycode) {
	if (g_appManager)
		g_appManager->add_keyboard_event(event, keycode);
}

void add_pen_event(int event, int x, int y) {
	if (g_appManager)
		g_appManager->add_pen_event(event, x, y);
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