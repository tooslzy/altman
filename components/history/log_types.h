#pragma once

#include <string>
#include <vector>

struct LogInfo
{
	std::string fileName;
	std::string fullPath;
	std::string timestamp; // ISO UTC
	std::string version;
	std::string channel;
	std::string joinTime; // raw string
	std::string jobId;
	std::string placeId;
	std::string universeId;
	std::string serverIp;
	std::string serverPort;
	std::string userId; // Parsed from log, if available
	std::vector<std::string> outputLines; // captured [FLog::Output] lines
};
