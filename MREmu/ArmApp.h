#pragma once
#include "App.h"
#include <filesystem>

namespace fs = std::filesystem;

class ArmApp : public App {
	uint32_t entry_point;

	bool is_ads;
	bool is_zipped;

	uint32_t rw_size = 0;
	uint32_t zi_size = 0;

public:
	ArmApp(App* app);

	static bool check_format(App* app);

	bool preparation() override;
	void start() override;

	bool is_native() { return false; }
};