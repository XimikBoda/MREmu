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

#ifdef  WIN32

#include <windows.h>

namespace fs = std::filesystem;

typedef int (_cdecl* dll_vm_entry)(void* vm_get_sym_entry_p);

class DLLApp : public App
{
public:
	dll_vm_entry entry_point = 0;

	HMODULE dll = 0;

	static bool check_format(fs::path path);

	bool preparation() override;
	void start() override;
};

#endif //  WIN32