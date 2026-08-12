#include "Log.h"
#include "../Log.h"
#include <vmlog.h>

spdlog::level::level_enum map_vm_level(int vm_level) {
    switch (vm_level) {
    case VM_DEBUG_LEVEL: return spdlog::level::debug;
    case VM_INFO_LEVEL: return spdlog::level::info;
    case VM_WARN_LEVEL: return spdlog::level::warn;
    case VM_ERROR_LEVEL: return spdlog::level::err;
    case VM_FATAL_LEVEL: return spdlog::level::critical;
    default: return spdlog::level::trace;
    }
}

spdlog::level::level_enum map_rust_level(const std::string& level_str) {
    if (level_str == "TRACE") return spdlog::level::trace;
    if (level_str == "DEBUG") return spdlog::level::debug;
    if (level_str == "INFO")  return spdlog::level::info;
    if (level_str == "WARN")  return spdlog::level::warn;
    if (level_str == "ERROR") return spdlog::level::err;
    return spdlog::level::info;
}

void vm_app_log(char* str) {
    std::string line = str;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }

    if (line.empty()) return;

    if (line.find('\t') != std::string::npos) {
        std::istringstream iss(line);
        std::string timestamp, level_str, source, message;

        if (std::getline(iss, timestamp, '\t') &&
            std::getline(iss, level_str, '\t') &&
            std::getline(iss, source, '\t') &&
            std::getline(iss, message)) {

            int vm_level = 4;
            try { vm_level = std::stoi(level_str); }
            catch (...) {}
            auto spdlog_level = map_vm_level(vm_level);

            spdlog::log(spdlog_level, "{} - {}", source, message);
            return;
        }
    }

    if (line[0] == '[') {
        size_t end_bracket = line.find(']');
        if (end_bracket != std::string::npos) {
            std::string level_str = line.substr(1, end_bracket - 1);
            auto spdlog_level = map_rust_level(level_str);

            size_t msg_start = end_bracket + 1;
            if (msg_start < line.size() && line[msg_start] == ' ') {
                msg_start++;
            }
            std::string message = line.substr(msg_start);

            spdlog::log(spdlog_level, "{}", message);
            return;
        }
    }

    spdlog::info("{}", line);
}