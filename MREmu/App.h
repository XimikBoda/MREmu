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
#include <elfio/elfio.hpp>
#include <elfio/elf_types.hpp>

namespace fs = std::filesystem;

class App 
{
public:
	std::vector<unsigned char> file_context;

	fs::path path;
	fs::path real_path;
	bool path_is_local = false;

	bool is_arm;

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
};