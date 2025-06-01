#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace Console
{
	void Log(const std::string& message);
	void RenderConsoleTab();
	std::vector<std::string> GetLogs(); // Added for potential external access
	std::string GetLatestLogMessageForStatus();
}
