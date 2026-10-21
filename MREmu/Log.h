#pragma once
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>

class Log {
public:
	static void set_module(const std::string_view str);
	static void set_module(const std::u8string_view str);
	// The module the running app set, for the GDB helper. Kept alongside the
	// pattern setter because the two are set together.
	static std::string get_module();
};
