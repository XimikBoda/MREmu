#pragma once
#include <spdlog/spdlog.h>

namespace Log {
	void set_module(const std::string_view str);
	void set_module(const std::u8string_view str);
}