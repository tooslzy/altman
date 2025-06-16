#pragma once

#include <string>
#include <vector>
#include <atomic>

#include "../utils/roblox_api.h"
#include "../data.h"

namespace FriendsActions {
	void RefreshFullFriendsList(
		int accountId,
		const std::string &userId,
		const std::string &cookie,
		std::vector<FriendInfo> &outFriendsList,
		std::atomic<bool> &loadingFlag);

	void FetchFriendDetails(
		const std::string &friendId,
		const std::string &cookie,
		RobloxApi::FriendDetail &outFriendDetail,
		std::atomic<bool> &loadingFlag);
}
