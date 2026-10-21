#pragma once
#include <filesystem>
#include <string>

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
	struct input;
}

namespace Memory {
	class MemoryManager;
}
namespace fs = std::filesystem;

namespace AppContext {
	AppManager* get_app_manager();

	App* get_foreground_app();
	App* get_executing_app();

	std::u8string_view get_foreground_app_name();

	int							get_executing_app_phandle();
	Memory::MemoryManager&		get_executing_app_memory();
	MREngine::SystemCallbacks&	get_executing_app_system_callbacks();
	MREngine::Resources&		get_executing_app_resources();
	MREngine::AppGraphic&		get_executing_app_graphic();
	MREngine::Timer&			get_executing_app_timer();
	MREngine::AppIO&			get_executing_app_io();
	MREngine::AppAudio&			get_executing_app_audio();
	MREngine::AppSock&			get_executing_app_sock();
	fs::path					get_executing_app_path();
	bool						is_executing_app_native();

	void push_event(const Event::network& ev);
	void push_event(const Event::input& ev);
};