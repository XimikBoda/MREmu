#include "AppManager.h"
#include "Bridge.h"

void AppManager::add_app_for_launch(fs::path path, bool global)
{
	std::lock_guard lock(launch_queue_mutex);
	launch_queue.push({ path, global });
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

	if (launch_data.global) {
		if (!app.load_from_file(launch_data.path))
			return;
	}
	else
		return;

	if (!app.preparation())
		return;

	apps.push_back(app);

	active_app_id = apps.size() - 1;
	current_work_app_id = apps.size() - 1;

	apps[current_work_app_id].resources.file_context = &(apps[current_work_app_id].file_context); //todo

	apps[current_work_app_id].start();
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

void add_keyboard_event(int event, int keycode) {
	if (g_appManager)
		g_appManager->add_keyboard_event(event, keycode);
}