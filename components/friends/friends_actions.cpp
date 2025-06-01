#include "friends_actions.h"
#include "../utils/roblox_api.h"
#include "../utils/status.h"
#include <algorithm>
#include <vector>
#include <string>

using namespace std;

static int presencePriority(const string& p)
{
	if (p == "InGame")
		return 0;
	if (p == "InStudio")
		return 1;
	if (p == "Online")
		return 2;
	return 3;
}

namespace FriendsActions
{
	void RefreshFullFriendsList(
		const string& userId,
		const string& cookie,
		vector<FriendInfo>& outFriendsList,
		atomic<bool>& loadingFlag)
	{
		loadingFlag = true;
		LOG_INFO("Fetching friends list...");

		auto list = RobloxApi::getFriends(userId, cookie);

		vector<uint64_t> ids;
		ids.reserve(list.size());
		for (const auto& f : list)
		{
			ids.push_back(f.id);
		}

		for (auto& f : list)
		{
			f.presence = "Offline";
		}

		LOG_INFO("Fetching friend presences...");

		for (size_t i = 0; i < ids.size(); i += 100)
		{
			size_t batchEnd = (min)(ids.size(), i + 100);
			vector batch_ids(ids.begin() + i, ids.begin() + batchEnd);

			if (batch_ids.empty())
				continue;

			auto presMap = RobloxApi::getPresences(batch_ids, cookie);

			for (const auto& [userIdFromPresence, presenceStr] : presMap)
			{
				auto it = find_if(list.begin(), list.end(),
				                  [&](const FriendInfo& f_item)
				                  {
					                  return f_item.id == userIdFromPresence;
				                  });
				if (it != list.end())
				{
					it->presence = presenceStr;
				}
			}
		}

		sort(list.begin(), list.end(), [](const FriendInfo& a, const FriendInfo& b)
		{
			int pa = presencePriority(a.presence);
			int pb = presencePriority(b.presence);
			if (pa != pb) return pa < pb;

			const string& nameA_ref = a.displayName.empty() || a.displayName == a.username ? a.username : a.displayName;
			const string& nameB_ref = b.displayName.empty() || b.displayName == b.username ? b.username : b.displayName;

			if (nameA_ref.empty() && !nameB_ref.empty()) return false;
			if (!nameA_ref.empty() && nameB_ref.empty()) return true;
			if (nameA_ref.empty() && nameB_ref.empty()) return a.id < b.id;

			return nameA_ref < nameB_ref;
		});

		outFriendsList = move(list);
		loadingFlag = false;
		LOG_INFO("Friends list updated.");
	}

	void FetchFriendDetails(
		const string& friendId,
		const string& cookie,
		RobloxApi::FriendDetail& outFriendDetail,
		atomic<bool>& loadingFlag)
	{
		loadingFlag = true;
		LOG_INFO("Fetching friend details...");
		outFriendDetail = RobloxApi::getUserDetails(friendId, cookie);
		loadingFlag = false;
		LOG_INFO("Friend details loaded.");
	}
}
