#pragma once
#include <filesystem>
#include <string>

class App;
class AppManager;

namespace MREngine {
	class AppSock;
	class SystemCallbacks;
	class Resources;
	class Tags;
	class AppGraphic;
	class Timer;
	class AppIO;
	class AppAudio;
}

namespace Event {
	struct launch;
	struct network;
	struct input;
	struct system;
}

namespace Memory {
	class MemoryManager;
}

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
	std::filesystem::path		get_executing_app_path();
	bool						is_executing_app_native();

	void close_executing_app();

	MREngine::Tags* get_tags_by_mem_adr(size_t offset_mem);

	void push_event(const Event::launch& ev);
	void push_event(const Event::network& ev);
	void push_event(const Event::input& ev);
	void push_event(const Event::system& ev);
};