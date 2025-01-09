#pragma once
#include "MreTags.h"
#include "Memory.h"
#include "App.h"
#include "MREngine/System.h"
#include "MREngine/Resources.h"
#include "MREngine/Graphic.h"
#include "MREngine/Timer.h"
#include "MREngine/IO.h"
#include "MREngine/Sock.h"
#include "MREngine/Audio.h"
#include <filesystem>
#include <vector>

#include <windows.h>

namespace fs = std::filesystem;

typedef int (_cdecl* dll_vm_entry)(void* vm_get_sym_entry_p);

class DLLApp : public App
{
public:
	dll_vm_entry entry_point = 0;

	HMODULE dll = 0;

	bool preparation() override;
	void start() override;
	bool load_from_file(fs::path path, bool local) override;//tmp
};