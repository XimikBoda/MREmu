#include "AppContext.h"
#include "App.h"
#include "AppManager.h"

namespace AppContext {
	AppManager* get_app_manager() {
		return AppManager::getPtr();
	}

	App* get_foreground_app() {
		return get_app_manager()->get_foreground_app();
	}

	App* get_executing_app() {
		return get_app_manager()->get_executing_app();
	}

	std::u8string_view get_foreground_app_name() {
		App* app = get_foreground_app();
		if (app)
			return get_foreground_app()->app_name;
		else
			return u8"";
	}

	int get_executing_app_phandle() {
		return get_executing_app()->phandle;
	}

	Memory::MemoryManager& get_executing_app_memory() {
		return get_executing_app()->app_memory;
	}

	MREngine::SystemCallbacks& get_executing_app_system_callbacks() {
		return get_executing_app()->system_callbacks;
	}

	MREngine::Resources& get_executing_app_resources() {
		return get_executing_app()->resources;
	}

	MREngine::AppGraphic& get_executing_app_graphic() {
		return get_executing_app()->graphic;
	}

	MREngine::Timer& get_executing_app_timer() {
		return get_executing_app()->timer;
	}

	MREngine::AppIO& get_executing_app_io() {
		return get_executing_app()->io;
	}

	MREngine::AppAudio& get_executing_app_audio() {
		return get_executing_app()->audio;
	}

	MREngine::AppSock& get_executing_app_sock() {
		return get_executing_app()->sock;
	}

	fs::path get_executing_app_path() {
		return get_executing_app()->path;
	}

	bool is_executing_app_native() {
		return get_executing_app()->is_native();
	}

	void close_executing_app() {
		get_app_manager()->close_app(get_executing_app()->phandle, false);
	}

	MREngine::Tags* get_tags_by_mem_adr(size_t offset_mem) {
		App* app = get_app_manager()->get_app_by_mem_adr(offset_mem);
		if (app)
			return &app->tags;
		return 0;
	}

	void push_event(const Event::launch& ev) {
		get_app_manager()->push_event(ev);
	}

	void push_event(const Event::network& ev) {
		get_app_manager()->push_event(ev);
	}

	void push_event(const Event::input& ev) {
		get_app_manager()->push_event(ev);
	}

	void push_event(const Event::system& ev) {
		get_app_manager()->push_event(ev);
	}
}