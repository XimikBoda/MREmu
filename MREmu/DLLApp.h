#pragma once
#include "App.h"
#include <filesystem>

#ifdef  WIN32

#include <windows.h>

namespace fs = std::filesystem;

typedef int (_cdecl* dll_vm_entry)(void* vm_get_sym_entry_p);

class DLLApp : public App
{
public:
	dll_vm_entry entry_point = 0;

	HMODULE dll = 0;

	static bool check_format(fs::path path, bool local);

	bool preparation() override;
	void start() override;
};

#endif //  WIN32