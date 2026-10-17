#pragma once
#include <string>
#include <atomic>

extern std::string error_message;
extern std::atomic<bool> show_error;

void trigger_hard_reset_with_error(const std::string& err_msg);

// Take the pending error, clearing it. The GUI main loop reads error_message to
// raise a modal; headless tooling uses this instead so a fault is not silently lost.
std::string take_error_message();
