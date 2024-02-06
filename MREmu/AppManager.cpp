#include "AppManager.h"
#include "Bridge.h"

void AppManager::add_app_for_launch(fs::path path, bool local)
{
	std::lock_guard lock(launch_queue_mutex);
	launch_queue.push({ path, local });
}

void AppManager::launch_apps()
{
	if (!launch_queue.size())
		return;

	launch_el launch_data;
	{
		std::lock_guard lock(launch_queue_mutex);
		launch_data = launch_queue.front();
		launch_queue.pop();
	}

	App app; 

	if (!app.load_from_file(launch_data.path, launch_data.local))
		return;


	if (!app.preparation())
		return;

	apps.push_back(app);

	active_app_id = apps.size() - 1;
	current_work_app_id = apps.size() - 1;

	apps[current_work_app_id].resources.file_context = &(apps[current_work_app_id].file_context); //todo

	static int ph_app_id = 0;
	ph_app_id++;

	apps[current_work_app_id].system_callbacks.ph_app_id = ph_app_id;

	apps[current_work_app_id].start();

	add_system_event(ph_app_id, VM_MSG_CREATE, 0);
	add_system_event(ph_app_id, VM_MSG_PAINT, 0);
}

void AppManager::add_keyboard_event(int event, int keycode)
{
	std::lock_guard lock(keyboard_events_queue_mutex);
	keyboard_events_queue.push({ event, keycode });
}

void AppManager::process_keyboard_events()
{
	while (keyboard_events_queue.size()) {
		keyboard_event_el ke;
		{
			std::lock_guard lock(keyboard_events_queue_mutex);
			ke = keyboard_events_queue.front();
			keyboard_events_queue.pop();
		}

		current_work_app_id = active_app_id;

		if (current_work_app_id < 0 || current_work_app_id >= apps.size())
			return;

		App& cur_app = apps[current_work_app_id];
		if (cur_app.io.key_handler)
			Bridge::run_cpu(cur_app.io.key_handler, 2, ke.event, ke.keycode);
	}
}

void AppManager::add_message_event(int phandle, unsigned int msg_id,
	int wparam, int lparam, int phandle_sender)
{
	std::lock_guard lock(message_events_queue_mutex);
	message_events_queue.push({ phandle, msg_id, wparam, lparam, phandle_sender });
}

void AppManager::process_message_events()
{
	while (message_events_queue.size()) {
		message_event_el me;
		{
			std::lock_guard lock(message_events_queue_mutex);
			me = message_events_queue.front();
			message_events_queue.pop();
		}

		int app_id = -1;

		for (int i = 0; i < apps.size(); ++i)
			if (apps[i].system_callbacks.ph_app_id == me.phandle) {
				app_id = i;
				break;
			}

		if (app_id == -1)
			continue;

		current_work_app_id = app_id;

		App& cur_app = apps[current_work_app_id];
		if (cur_app.system_callbacks.msg_proc)
			Bridge::run_cpu(cur_app.system_callbacks.msg_proc, 4,
				me.phandle_sender, me.msg_id, me.wparam, me.lparam);
	}
}

void AppManager::add_system_event(int phandle, int message, int param)
{
	std::lock_guard lock(system_events_queue_mutex);
	system_events_queue.push({ phandle, message, param });
}

void AppManager::process_system_events()
{
	while (system_events_queue.size()) {
		system_event_el se;
		{
			std::lock_guard lock(system_events_queue_mutex);
			se = system_events_queue.front();
			system_events_queue.pop();
		}

		int app_id = -1;

		for (int i = 0; i < apps.size(); ++i)
			if (apps[i].system_callbacks.ph_app_id == se.phandle) {
				app_id = i;
				break;
			}

		if (app_id == -1)
			continue;

		current_work_app_id = app_id;

		App& cur_app = apps[current_work_app_id];
		if (cur_app.system_callbacks.sysevt)
			Bridge::run_cpu(cur_app.system_callbacks.sysevt, 2, se.message, se.param);
	}
}

App* AppManager::get_active_app()
{
	if (active_app_id < 0 || active_app_id >= apps.size())
		return 0;
	else
		return &apps[active_app_id];
}

App* AppManager::get_current_work_app_id()
{
	if (current_work_app_id < 0 || current_work_app_id >= apps.size())
		return 0;
	else
		return &apps[current_work_app_id];
}

extern AppManager* g_appManager;

App* get_cur_app() {
	App* curr_app = 0;
	if (g_appManager && (curr_app = g_appManager->get_current_work_app_id()))
		return curr_app;
	else
		abort();
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

fs::path get_current_app_path() {
	return get_cur_app()->path;
}

MreTags* get_tags_by_mem_adr(size_t offset_mem) {
	if (!g_appManager)
		return 0;

	auto &apps = g_appManager->apps;
	for (int i = 0; i < apps.size(); ++i)
		if (apps[i].offset_mem == offset_mem)
			return &apps[i].tags;

	return 0;
}

void add_keyboard_event(int event, int keycode) {
	if (g_appManager)
		g_appManager->add_keyboard_event(event, keycode);
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