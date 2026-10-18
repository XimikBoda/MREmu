#pragma once
#include <spdlog/spdlog.h>

class Log {
public:
	static void set_module(const std::string_view str);
	static void set_module(const std::u8string_view str);
};