#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

#include "http.hpp"
#include "core/logging.hpp"
#include "status.h"


namespace Roblox {
	enum class BanCheckResult {
		InvalidCookie,
		Unbanned,
		Banned
	};

        static nlohmann::json getAuthenticatedUser(const std::string &cookie) {
                if (!canUseCookie(cookie))
                        return nlohmann::json::object();

                LOG_INFO("Fetching profile info");
                HttpClient::Response response = HttpClient::get(
                        "https://users.roblox.com/v1/users/authenticated",
                        {{"Cookie", ".ROBLOSECURITY=" + cookie}});

                if (response.status_code < 200 || response.status_code >= 300) {
                        LOG_ERROR("Failed to fetch user info: HTTP " + std::to_string(response.status_code));
                        return nlohmann::json::object();
                }

                return HttpClient::decode(response);
        }

        static BanCheckResult checkBanStatus(const std::string &cookie) {
                LOG_INFO("Checking moderation status");
                HttpClient::Response response = HttpClient::get(
                        "https://usermoderation.roblox.com/v1/not-approved",
                        {{"Cookie", ".ROBLOSECURITY=" + cookie}});

                if (response.status_code < 200 || response.status_code >= 300) {
                        LOG_ERROR("Failed moderation check: HTTP " + std::to_string(response.status_code));
                        return BanCheckResult::InvalidCookie;
                }

		auto j = HttpClient::decode(response);
                if (j.is_object() && j.contains("punishmentTypeDescription"))
                        return BanCheckResult::Banned;
                if (j.empty())
                        return BanCheckResult::Unbanned;
                return BanCheckResult::Unbanned;
        }

        static BanCheckResult cachedBanStatus(const std::string &cookie) {
                static std::mutex m;
                static std::unordered_map<std::string, BanCheckResult> cache;
                {
                        std::lock_guard<std::mutex> lock(m);
                        auto it = cache.find(cookie);
                        if (it != cache.end())
                                return it->second;
                }
                BanCheckResult status = checkBanStatus(cookie);
                {
                        std::lock_guard<std::mutex> lock(m);
                        cache[cookie] = status;
                }
                return status;
        }

        static bool canUseCookie(const std::string &cookie) {
                BanCheckResult status = cachedBanStatus(cookie);
                if (status == BanCheckResult::Banned) {
                        LOG_ERROR("Skipping request: cookie is banned");
                        return false;
                }
                if (status == BanCheckResult::InvalidCookie) {
                        LOG_ERROR("Skipping request: invalid cookie");
                        return false;
                }
                return true;
        }

        static bool isCookieValid(const std::string &cookie) {
                return cachedBanStatus(cookie) != BanCheckResult::InvalidCookie;
        }

        static std::string fetchAuthTicket(const std::string &cookie) {
                if (!canUseCookie(cookie))
                        return "";
                LOG_INFO("Fetching x-csrf token");
		std::cout << cookie;
		auto csrfResponse = HttpClient::post(
			"https://auth.roblox.com/v1/authentication-ticket",
			{{"Cookie", ".ROBLOSECURITY=" + cookie}});

                auto csrfToken = csrfResponse.headers.find("x-csrf-token");
                if (csrfToken == csrfResponse.headers.end()) {
                        std::cerr << "failed to get CSRF token\n";

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

                if (ticketResponse.status_code < 200 || ticketResponse.status_code >= 300) {
                        LOG_ERROR("Failed to fetch auth ticket: HTTP " + std::to_string(ticketResponse.status_code));
                        return "";
                }

                auto ticket = ticketResponse.headers.find("rbx-authentication-ticket");
                if (ticket == ticketResponse.headers.end()) {
                        std::cerr << "failed to get authentication ticket\n";
                        LOG_INFO("Failed to get authentication ticket");
                        return "";
                }

		return ticket->second;
	}

	static uint64_t getUserId(const std::string &cookie) {
		auto userJson = getAuthenticatedUser(cookie);
		return userJson.value("id", 0ULL);
	}

	static std::string getUsername(const std::string &cookie) {
		auto userJson = getAuthenticatedUser(cookie);
		return userJson.value("name", "");
	}

	static std::string getDisplayName(const std::string &cookie) {
		auto userJson = getAuthenticatedUser(cookie);
		return userJson.value("displayName", "");
	}
}
