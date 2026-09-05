#include "Log.h"

static std::string g_current_module = "Main";

void Log::set_module(const std::string str) {
	g_current_module = str;
	spdlog::set_pattern(fmt::format("[{}] [%^%l%$] %v", str));
}

std::string Log::get_module() {
	return g_current_module;
}