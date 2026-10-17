#pragma once
#include <spdlog/spdlog.h>
#include <string>

class Log {
public:
	static void set_module(const std::string str);
	static std::string get_module();
};