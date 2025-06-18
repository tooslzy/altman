#pragma once

#include <string>
#include <vector>
#include <atomic>

#include "network/roblox.h"
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
		Roblox::FriendDetail &outFriendDetail,
		std::atomic<bool> &loadingFlag);
}
