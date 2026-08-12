#include "Log.h"

void Log::set_module(const std::string str) {
	spdlog::set_pattern(fmt::format("[{}] [%^%l%$] %v", str));
}