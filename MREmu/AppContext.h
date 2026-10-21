#pragma once

class App;
class AppManager;

namespace MREngine {
	class AppSock;
}

struct sock_event;
struct dns_event;

namespace AppContext {
	AppManager* get_app_manager();

	App* get_foreground_app();
	App* get_executing_app();

	int get_executing_app_phandle();
	MREngine::AppSock& get_executing_app_sock();

	void push_event(const sock_event& ev);
	void push_event(const dns_event& ev);
};