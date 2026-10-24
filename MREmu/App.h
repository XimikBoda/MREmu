#pragma once
#include "Memory.h"
#include "MREngine/System.h"
#include "MREngine/Resources.h"
#include "MREngine/Tags.h"
#include "MREngine/Graphic.h"
#include "MREngine/Timer.h"
#include "MREngine/IO.h"
#include "MREngine/Sock.h"
#include "MREngine/Audio.h"
#include <filesystem>
#include <vector>
#include "Bridge.h"
#include "Log.h"

namespace fs = std::filesystem;

class App
{
public: // TODO make private
	enum State{
		Runned,
		SoftClosing,
		HardClosing
	};

	std::vector<unsigned char> file_context;

	fs::path path;
	fs::path real_path;
	bool path_is_local = false;

	void* mem_location = 0;
	size_t offset_mem;
	size_t mem_size;
	size_t segments_size;

	State state = Runned; // todo

	int phandle = -1;

	std::u8string app_name;

	Memory::MemoryManager app_memory;
	MREngine::SystemCallbacks system_callbacks;
	MREngine::Resources resources;
	MREngine::Tags tags;
	MREngine::AppGraphic graphic;
	MREngine::Timer timer;
	MREngine::AppIO io;
	MREngine::AppSock sock;
	MREngine::AppAudio audio;

	bool load(std::ifstream& in);
	bool load(fs::path path, bool local);
	bool load(int fd, fs::path path);

	void init_from(App* app);

	virtual bool preparation() { return false; };
	virtual void start() {};

	virtual bool is_native() { return true; }

	template <bool from_hook = false, typename Func, typename... Args>
	auto run(Func func, Args... args) {
		Log::set_module(app_name);

		if (state != HardClosing) // TODO: potential issue here
			return Bridge::run<from_hook>(is_native(), func, std::forward<Args>(args)...);
		else {
			using RetType = decltype(func(std::forward<Args>(args)...));
			return RetType();
		}
	}
};