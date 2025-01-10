#pragma once
#include "MreTags.h"
#include "Memory.h"
#include "MREngine/System.h"
#include "MREngine/Resources.h"
#include "MREngine/Graphic.h"
#include "MREngine/Timer.h"
#include "MREngine/IO.h"
#include "MREngine/Sock.h"
#include "MREngine/Audio.h"
#include <filesystem>
#include <vector>
#include "Bridge.h"

namespace fs = std::filesystem;

class App
{
public:
	std::vector<unsigned char> file_context;

	fs::path path;
	fs::path real_path;
	bool path_is_local = false;

	void* mem_location = 0;
	size_t offset_mem;
	size_t mem_size;
	size_t segments_size;

	MreTags tags;

	Memory::MemoryManager app_memory;
	MREngine::SystemCallbacks system_callbacks;
	MREngine::Resources resources;
	MREngine::AppGraphic graphic;
	MREngine::Timer timer;
	MREngine::AppIO io;
	MREngine::AppSock sock;
	MREngine::AppAudio audio;

	virtual bool preparation() { return false; };
	virtual void start() {};
	virtual bool load_from_file(fs::path path, bool local) { return false; };//tmp

	virtual bool is_native() { return true; }

	template <typename Func, typename... Args>
	auto run(Func func, Args... args) {
		if (is_native())
			return func(std::forward<Args>(args)...);
		else {
			int n = sizeof...(args);

			return (decltype(func(std::forward<Args>(args)...)))Bridge::run_cpu(FUNC_TO_UINT32(func), n, args...);
		}
	}
};