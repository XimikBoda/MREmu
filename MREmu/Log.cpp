#include "Log.h"

static std::string g_current_module = "Main";

void Log::set_module(const std::string_view str) {
	g_current_module = str;
	spdlog::set_pattern(fmt::format("[{}] [%^%l%$] %v", str));
}

void Log::set_module(const std::u8string_view str) {
	set_module(std::string_view((char*)str.data(), str.size()));
}

std::string Log::get_module() {
	return g_current_module;
}
