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

	MREngine::AppSock& get_executing_app_sock() {
		return get_executing_app()->sock;
	}

	void push_event(const Event::network& ev) {
		get_app_manager()->push_event(ev);
	}
}