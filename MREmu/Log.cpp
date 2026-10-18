#include "Log.h"

void Log::set_module(const std::string_view str) {
	spdlog::set_pattern(fmt::format("[{}] [%^%l%$] %v", str));
}

void Log::set_module(const std::u8string_view str) {
	set_module(std::string_view((char*)str.data(), str.size()));
}