#pragma once

#include "log_types.h"
#include <string>

// Parses a single log file and populates the LogInfo struct.
void parseLogFile(LogInfo& logInfo);

// Returns the path to the Roblox logs folder.
std::string logsFolder();
