#pragma once

#include <iostream>
#include <random>
#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "http.hpp"
#include "logging.hpp"
#include "status.h"
#include "threading.h"

#include "../components/components.h"

using namespace std;

static ImVec4 getStatusColor(string statusCode)
{
	if (statusCode == "Online")
	{
		return ImVec4(0.6f, 0.8f, 0.95f, 1.0f);
	}
	if (statusCode == "InGame")
	{
		return ImVec4(0.6f, 0.9f, 0.7f, 1.0f);
	}
	if (statusCode == "InStudio")
	{
		return ImVec4(1.0f, 0.85f, 0.7f, 1.0f);
	}
	if (statusCode == "Invisible")
	{
		return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
	}
	return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

static std::string generateSessionId()
{
	static auto hex = "0123456789abcdef";
	std::random_device rd;
	std::mt19937 gen(rd());
	uniform_int_distribution<> dis(0, 15);

	std::string uuid(36, ' ');
	for (int i = 0; i < 36; i++)
	{
		switch (i)
		{
		case 8:
		case 13:
		case 18:
		case 23:
			uuid[i] = '-';
			break;
		case 14:
			uuid[i] = '4';
			break;
		case 19:
			uuid[i] = hex[(dis(gen) & 0x3) | 0x8];
			break;
		default:
			uuid[i] = hex[dis(gen)];
		}
	}
	return uuid;
}

static std::string presenceTypeToString(int type)
{
	switch (type)
	{
	case 1:
		return "Online";
	case 2:
		return "InGame";
	case 3:
		return "InStudio";
	case 4:
		return "Invisible";
	default:
		return "Offline";
	}
}

namespace RobloxApi
{
	static std::vector<FriendInfo> getFriends(const std::string& userId, const std::string& cookie)
	{
		LOG_INFO("Fetching friends list");

		HttpClient::Response resp = HttpClient::get(
			"https://friends.roblox.com/v1/users/" + userId + "/friends",
			{{"Cookie", ".ROBLOSECURITY=" + cookie}});

		if (resp.status_code != 200)
		{
			LOG_INFO("Failed to fetch friends: HTTP " + std::to_string(resp.status_code));
			throw std::runtime_error("Failed to fetch friends: HTTP " + std::to_string(resp.status_code));
		}

		nlohmann::json j = HttpClient::decode(resp);
		std::vector<FriendInfo> friends;
		for (const auto& item : j.at("data"))
		{
			FriendInfo f;
			f.id = item.value("id", 0ULL);
			f.isDeleted = item.value("isDeleted", false);
			f.displayName = item.value("displayName", "");
			f.username = item.value("name", "");

			friends.push_back(f);
		}
		return friends;
	}

	static FriendInfo getUserInfo(const std::string& userId)
	{
		LOG_INFO("Fetching user info");
		HttpClient::Response resp = HttpClient::get(
			"https://users.roblox.com/v1/users/" + userId,
			{{"Accept", "application/json"}});

		if (resp.status_code != 200)
		{
			LOG_INFO("Failed to fetch user info: HTTP " + std::to_string(resp.status_code));
			throw std::runtime_error("Failed to fetch user info: HTTP " + std::to_string(resp.status_code));
		}

		nlohmann::json j = HttpClient::decode(resp);
		FriendInfo f;
		f.id = j.value("id", 0ULL);
		f.username = j.value("name", "");
		f.displayName = j.value("displayName", "");
		return f;
	}

	struct GameDetail
	{
		std::string genre;
		std::string description;
		uint64_t visits = 0;
		int maxPlayers = 0;
		std::string createdIso;
		std::string updatedIso;

		std::string creatorName;
		bool creatorVerified = false;
	};

	inline GameDetail getGameDetail(uint64_t universeId)
	{
		using nlohmann::json;
		const std::string url =
			"https://games.roblox.com/v1/games?universeIds=" + std::to_string(universeId);

		HttpClient::Response resp = HttpClient::get(url);
		if (resp.status_code != 200)
			throw std::runtime_error("detail fetch failed");

		json j = json::parse(resp.text)["data"].at(0);

		GameDetail d;
		d.genre = j.value("genre", "");
		d.description = j.value("description", "");
		d.visits = j.value("visits", 0ULL);
		d.maxPlayers = j.value("maxPlayers", 0);
		d.createdIso = j.value("created", "");
		d.updatedIso = j.value("updated", "");

		const auto& c = j["creator"];
		d.creatorName = c.value("name", "");
		d.creatorVerified = c.value("hasVerifiedBadge", false);

		return d;
	}

	struct ServerPage
	{
		std::vector<PublicServerInfo> data;
		std::string nextCursor;
		std::string prevCursor;
	};

	static ServerPage getPublicServersPage(uint64_t placeId,
	                                       const std::string& cursor = {})
	{
		std::string url =
			"https://games.roblox.com/v1/games/" + std::to_string(placeId) +
			"/servers/Public?sortOrder=Asc&limit=100" +
			(cursor.empty() ? "" : "&cursor=" + cursor);

		HttpClient::Response resp = HttpClient::get(url);
		if (resp.status_code != 200)
			throw std::runtime_error("Failed to fetch servers: HTTP " +
				std::to_string(resp.status_code));

		auto json = HttpClient::decode(resp);

		ServerPage page;
		page.nextCursor = json.at("nextPageCursor").is_null()
			                  ? std::string{}
			                  : json.at("nextPageCursor").get<std::string>();

		page.prevCursor = json.at("previousPageCursor").is_null()
			                  ? std::string{}
			                  : json.at("previousPageCursor").get<std::string>();

		for (auto& e : json.at("data"))
		{
			PublicServerInfo s;
			s.jobId = e.at("id").get<std::string>();
			s.currentPlayers = e.at("playing").get<int>();
			s.maximumPlayers = e.at("maxPlayers").get<int>();
			s.averagePing = e.at("ping").get<double>();
			s.averageFps = e.at("fps").get<double>();
			page.data.push_back(std::move(s));
		}
		return page;
	}

	static std::vector<GameInfo> searchGames(const std::string& query)
	{
		const std::string sessionId = generateSessionId();
		auto resp = HttpClient::get(
			"https://apis.roblox.com/search-api/omni-search",
			{{"Accept", "application/json"}},
			cpr::Parameters{
				{"searchQuery", query},
				{"pageToken", ""},
				{"sessionId", sessionId},
				{"pageType", "all"}
			});

		std::vector<GameInfo> out;
		if (resp.status_code != 200)
		{
			return out;
		}

		auto j = HttpClient::decode(resp);

		for (auto& group : j["searchResults"])
		{
			if (group.value("contentGroupType", "") != "Game")
				continue;

			for (auto& g : group["contents"])
			{
				GameInfo info;
				info.name = g.value("name", "");
				info.universeId = g.value("universeId", 0ULL);
				info.placeId = g.value("rootPlaceId", 0ULL);
				info.playerCount = g.value("playerCount", 0);
				info.upVotes = g.value("totalUpVotes", 0);
				info.downVotes = g.value("totalDownVotes", 0);
				info.creatorName = g.value("creatorName", "");
				info.creatorVerified = g.value("creatorHasVerifiedBadge", false);
				out.push_back(std::move(info));
			}
		}

		return out;
	}

	static nlohmann::json getAuthenticatedUser(const string& cookie)
	{
		LOG_INFO("Fetching profile info");
		HttpClient::Response response = HttpClient::get(
			"https://users.roblox.com/v1/users/authenticated",
			{{"Cookie", ".ROBLOSECURITY=" + cookie}});

		if (response.status_code != 200)
		{
			LOG_INFO("Failed to fetch user info: HTTP " + to_string(response.status_code));

			throw runtime_error(
				"Failed to fetch user info: HTTP " +
				to_string(response.status_code));
		}

		return HttpClient::decode(response);
	}

	static string fetchAuthTicket(const string& cookie)
	{
		LOG_INFO("Fetching x-csrf token");
		cout << cookie;
		auto csrfResponse = HttpClient::post(
			"https://auth.roblox.com/v1/authentication-ticket",
			{{"Cookie", ".ROBLOSECURITY=" + cookie}});

		auto csrfToken = csrfResponse.headers.find("x-csrf-token");
		if (csrfToken == csrfResponse.headers.end())
		{
			cerr << "failed to get CSRF token\n";

			LOG_INFO("Failed to get CSRF token");
			return "";
		}

		LOG_INFO("Fetching authentication ticket");
		auto ticketResponse = HttpClient::post(
			"https://auth.roblox.com/v1/authentication-ticket",
			{
				{"Cookie", ".ROBLOSECURITY=" + cookie},
				{"Origin", "https://www.roblox.com"},
				{"Referer", "https://www.roblox.com/"},
				{"X-CSRF-TOKEN", csrfToken->second}
			});

		auto ticket = ticketResponse.headers.find("rbx-authentication-ticket");
		if (ticket == ticketResponse.headers.end())
		{
			cerr << "failed to get authentication ticket\n";
			LOG_INFO("Failed to get authentication ticket");
			return "";
		}

		return ticket->second;
	}

	static uint64_t getUserId(const string& cookie)
	{
		auto userJson = getAuthenticatedUser(cookie);
		return userJson.at("id").get<uint64_t>();
	}

	static string getUsername(const string& cookie)
	{
		auto userJson = getAuthenticatedUser(cookie);
		return userJson.at("name").get<string>();
	}

	static string getDisplayName(const string& cookie)
	{
		auto userJson = getAuthenticatedUser(cookie);
		return userJson.at("displayName").get<string>();
	}

	static string presenceTypeToString(int type)
	{
		switch (type)
		{
		case 1:
			return "Online";
		case 2:
			return "InGame";
		case 3:
			return "InStudio";
		case 4:
			return "Invisible";
		default:
			return "Offline";
		}
	}

	static string getPresence(
		const string& cookie,
		uint64_t userId)
	{
		LOG_INFO("Fetching user presence");
		nlohmann::json payload = {{"userIds", {userId}}};
		HttpClient::Response response = HttpClient::post(
			"https://presence.roproxy.com/v1/presence/users",
			{{"Cookie", ".ROBLOSECURITY=" + cookie}},
			payload.dump());
		if (response.status_code != 200)
		{
			LOG_INFO("Presence lookup failed: HTTP " +
				to_string(response.status_code));

			// If we get HTTP 403, it likely means the user is banned or inaccessible
			if (response.status_code == 403)
			{
				return "Banned";
			}

			throw runtime_error(
				"Presence lookup failed: HTTP " +
				to_string(response.status_code));
		}

		// Print the raw response for debugging
		OutputDebugStringA(("Raw response: " + response.text).c_str());
		LOG_INFO("Raw response body: " + response.text);

		auto json = HttpClient::decode(response);

		// Print the parsed JSON for debugging
		OutputDebugStringA(("Parsed JSON: " + json.dump()).c_str());
		LOG_INFO("Parsed JSON: " + json.dump());

		auto& jsonData = json.at("userPresences");
		if (jsonData.empty())
		{
			throw runtime_error("No presence data returned");
		}
		OutputDebugStringA(jsonData.dump().c_str());

		try
		{
			int typeInt = jsonData.front().at("userPresenceType").get<int>();
			LOG_INFO("Got user presence for " + to_string(userId));
			return presenceTypeToString(typeInt);
		}
		catch (const exception& e)
		{
			// Check if the error from presenceTypeToString contains "Invalid UserID"
			string errorMsg = e.what();
			if (errorMsg.find("Invalid UserID") != string::npos)
			{
				return "Banned";
			}
			throw; // Re-throw if it's a different error
		}
	}

	struct FriendDetail
	{
		uint64_t id = 0;
		std::string username;
		std::string displayName;
		std::string description;
		std::string createdIso;
		int followers = 0;
		int following = 0;
		int placeVisits = 0;
		std::string presence;
	};

	static FriendDetail getUserDetails(const std::string& userId,
	                                   const std::string& cookie)
	{
		FriendDetail d;
		std::mutex m;
		std::condition_variable cv;
		int remaining = 3;

		auto signalDone = [&]
		{
			std::lock_guard<std::mutex> lk(m);
			if (--remaining == 0)
				cv.notify_one();
		};

		Threading::newThread([&, userId]
		{
			auto resp = HttpClient::get(
				"https://users.roblox.com/v1/users/" + userId,
				{{"Accept", "application/json"}}
			);
			if (resp.status_code == 200)
			{
				nlohmann::json j = HttpClient::decode(resp);
				d.id = j.value("id", 0ULL);
				d.username = j.value("name", "");
				d.displayName = j.value("displayName", "");
				d.description = j.value("description", "");
				d.createdIso = j.value("created", "");
			}
			signalDone();
		});

		Threading::newThread([&, userId]
		{
			auto resp = HttpClient::get(
				"https://friends.roblox.com/v1/users/" + userId + "/followers/count",
				{}
			);
			if (resp.status_code == 200)
			{
				d.followers = nlohmann::json::parse(resp.text).value("count", 0);
			}
			signalDone();
		});

		Threading::newThread([&, userId]
		{
			auto resp = HttpClient::get(
				"https://friends.roblox.com/v1/users/" + userId + "/followings/count",
				{}
			);
			if (resp.status_code == 200)
			{
				d.following = nlohmann::json::parse(resp.text).value("count", 0);
			}
			signalDone();
		});

		std::unique_lock<std::mutex> lk(m);
		cv.wait(lk, [&]
		{
			return remaining == 0;
		});

		return d;
	}

	static std::unordered_map<uint64_t, std::string>
	getPresences(const std::vector<uint64_t>& userIds, const std::string& cookie)
	{
		LOG_INFO("Fetching presences in batch");

		nlohmann::json payload = {{"userIds", userIds}};

		HttpClient::Response resp = HttpClient::post(
			"https://presence.roblox.com/v1/presence/users",
			{{"Cookie", ".ROBLOSECURITY=" + cookie}},
			payload.dump());
		if (resp.status_code != 200)
		{
			LOG_INFO("Batch presence lookup failed: HTTP " + std::to_string(resp.status_code));
			throw std::runtime_error("Batch presence failed: HTTP " + std::to_string(resp.status_code));
		}

		nlohmann::json j = HttpClient::decode(resp);
		std::unordered_map<uint64_t, std::string> out;
		for (auto& up : j.at("userPresences"))
		{
			uint64_t uid = up.value("userId", 0ULL);
			int typeInt = up.value("userPresenceType", 0);
			out[uid] = presenceTypeToString(typeInt);
		}
		return out;
	}
}
