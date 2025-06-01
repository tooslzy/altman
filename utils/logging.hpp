#pragma once
#include "../components/console/console.h"
#include <string>

#define LOG(msg) Console::Log(msg)
#define LOG_INFO(msg) Console::Log(std::string("[INFO] ") + (msg))
#define LOG_WARN(msg) Console::Log(std::string("[WARN] ") + (msg))
#define LOG_ERROR(msg) Console::Log(std::string("[ERROR] ") + (msg))
