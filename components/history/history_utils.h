#pragma once

#include <string>
#include "log_types.h"

std::string ordSuffix(int day);

std::string friendlyTimestamp(const std::string &isoTimestamp);

std::string niceLabel(const LogInfo &logInfo);
