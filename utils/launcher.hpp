#include "./http.hpp"
// #include "./status.h" // Status::Set is no longer used directly here, but Status::Get might be used by UI
#include <windows.h>
#include <iostream>
#include <chrono>
#include <sstream>

#include "logging.hpp"
#include "notifications.h"

using namespace std;
using namespace std::chrono;

static string urlEncode(const string &s) {
	ostringstream out;
	out << hex << uppercase;
	for (unsigned char c: s) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			out << c;
		else
			out << '%' << setw(2) << static_cast<int>(c);
	}
	return out.str();
}

inline HANDLE startRoblox(uint64_t placeId, const string &jobId, const string &cookie) {
	// Status::Set("Fetching x-csrf token"); // REMOVE THIS
	LOG_INFO("Fetching x-csrf token");
	auto csrfResponse = HttpClient::post(
		"https://auth.roblox.com/v1/authentication-ticket",
		{{"Cookie", ".ROBLOSECURITY=" + cookie}});

	auto csrfToken = csrfResponse.headers.find("x-csrf-token");
	if (csrfToken == csrfResponse.headers.end()) {
		cerr << "failed to get CSRF token\n";

		// Status::Set("Failed to get CSRF token"); // REMOVE THIS
		LOG_ERROR("Failed to get CSRF token"); // This will now set the status
		return nullptr;
	}

	// Status::Set("Fetching authentication ticket"); // REMOVE THIS
	LOG_INFO("Fetching authentication ticket"); // This will now set the status
	auto ticketResponse = HttpClient::post(
		"https://auth.roblox.com/v1/authentication-ticket",
		{
			{"Cookie", ".ROBLOSECURITY=" + cookie},
			{"Origin", "https://www.roblox.com"},
			{"Referer", "https://www.roblox.com/"},
			{"X-CSRF-TOKEN", csrfToken->second}
		});

	auto ticket = ticketResponse.headers.find("rbx-authentication-ticket");
	if (ticket == ticketResponse.headers.end()) {
		cerr << "failed to get authentication ticket\n";
		// Status::Set("Failed to get authentication ticket"); // REMOVE THIS
		LOG_ERROR("Failed to get authentication ticket"); // This will now set the status
		return nullptr;
	}

	auto nowMs = duration_cast<milliseconds>(
				system_clock::now().time_since_epoch())
			.count();
	ostringstream ts;
	ts << nowMs;

	string placeLauncherUrl =
			"https://assetgame.roblox.com/game/PlaceLauncher.ashx?"
			"request=RequestGameJob"
			"&browserTrackerId=147062882894"
			"&placeId=" +
			to_string(placeId) +
			"&gameId=" + jobId +
			"&isPlayTogetherGame=false"
			"+browsertrackerid:147062882894"
			"+robloxLocale:en_us"
			"+gameLocale:en_us"
			"+channel:";

	string protocolLaunchCommand =
			"roblox-player:1+launchmode:play"
			"+gameinfo:" +
			ticket->second +
			"+launchtime:" + ts.str() +
			"+placelauncherurl:" + urlEncode(placeLauncherUrl);


	string logMessage = "Attempting to launch Roblox for place ID: " + to_string(placeId) + (
		                    jobId.empty() ? "" : " with Job ID: " + jobId);
	LOG_INFO(logMessage);

	wstring notificationTitle = L"Launching";
	wostringstream notificationMessageStream;
	notificationMessageStream << L"Attempting to launch Roblox for place ID: " << placeId;
	if (!jobId.empty()) {
		notificationMessageStream << L" with Job ID: " << jobId.c_str();
	}
	Notifications::showNotification(notificationTitle.c_str(), notificationMessageStream.str().c_str());

	SHELLEXECUTEINFOA executionInfo{sizeof(executionInfo)};
	executionInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	executionInfo.lpVerb = "open";
	executionInfo.lpFile = protocolLaunchCommand.c_str();
	executionInfo.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteExA(&executionInfo)) {
		LOG_ERROR("ShellExecuteExA failed for Roblox launch. Error: " + to_string(GetLastError()));
		cerr << "ShellExecuteEx failed: " << GetLastError() << "\n";
		return nullptr;
	}

	LOG_INFO("Roblox process started successfully for place ID: " + to_string(placeId));
	return executionInfo.hProcess;
}
