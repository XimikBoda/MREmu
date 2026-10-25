#pragma once
#include "App.h"
#include <filesystem>

#ifdef  WIN32

#include <windows.h>

namespace fs = std::filesystem;

typedef int (_cdecl* dll_vm_entry)(void* vm_get_sym_entry_p);

class DLLApp : public App
{
	dll_vm_entry entry_point = 0;

	HMODULE dll = 0;

public:
	DLLApp(App* app);

	static bool check_format(App* app);

	bool preparation() override;
	void start() override;
};

#endif //  WIN32