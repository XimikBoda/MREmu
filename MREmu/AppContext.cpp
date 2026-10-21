#include "AppContext.h"
#include "App.h"
#include "AppManager.h"

namespace AppContext {
	AppManager* get_app_manager() {
		return AppManager::getPtr();
	}

	App* get_foreground_app() {
		return get_app_manager()->get_active_app();
	}

	App* get_executing_app() {
		return get_app_manager()->get_current_work_app_id();
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

	void push_event(const Event::network& ev) {
		get_app_manager()->push_event(ev);
	}
}