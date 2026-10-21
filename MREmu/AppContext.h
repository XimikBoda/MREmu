#pragma once

class App;
class AppManager;

namespace MREngine {
	class AppSock;
}

namespace Event {
	struct network;
}

namespace AppContext {
	AppManager* get_app_manager();

	App* get_foreground_app();
	App* get_executing_app();

	int get_executing_app_phandle();
	MREngine::AppSock& get_executing_app_sock();

	void push_event(const Event::network& ev);
};