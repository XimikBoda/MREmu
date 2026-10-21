#pragma once

class App;
class AppManager;

namespace MREngine {
	class AppSock;
	class SystemCallbacks;
	class Resources;
	class AppGraphic;
	class Timer;
	class AppIO;
	class AppAudio;
}

namespace Event {
	struct network;
}

namespace Memory {
	class MemoryManager;
}

namespace AppContext {
	AppManager* get_app_manager();

	App* get_foreground_app();
	App* get_executing_app();

	int							get_executing_app_phandle();
	Memory::MemoryManager&		get_executing_app_memory();
	MREngine::SystemCallbacks&	get_executing_app_system_callbacks();
	MREngine::Resources&		get_executing_app_resources();
	MREngine::AppGraphic&		get_executing_app_graphic();
	MREngine::Timer&			get_executing_app_timer();
	MREngine::AppIO&			get_executing_app_io();
	MREngine::AppAudio&			get_executing_app_audio();
	MREngine::AppSock&			get_executing_app_sock();

	void push_event(const Event::network& ev);
};