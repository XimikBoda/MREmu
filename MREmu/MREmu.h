#pragma once
#include <string>
#include <atomic>

extern std::string error_message;
extern std::atomic<bool> show_error;

void trigger_hard_reset_with_error(const std::string& err_msg);
