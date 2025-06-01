#define WIN32_LEAN_AND_MEAN
#include "data.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <windows.h>
#include <dpapi.h>

#include "../utils/base64.h"
#include "../utils/logging.hpp"

#pragma comment(lib, "Crypt32.lib")

using namespace std;
using json = nlohmann::json;

vector<AccountData> g_accounts;
set<int> g_selectedAccountIds;

vector<FavoriteGame> g_favorites;
vector<FriendInfo> g_friends;

int g_defaultAccountId = -1;
array<char, 128> s_jobIdBuffer = {};
array<char, 128> s_playerBuffer = {};

vector<BYTE> encryptData(const string& plainText)
{
	DATA_BLOB DataIn;
	DATA_BLOB DataOut;

	auto szDescription = L"User Cookie Data";

	DataIn.pbData = (BYTE*)plainText.c_str();
	DataIn.cbData = plainText.length() + 1;

	if (CryptProtectData(&DataIn, szDescription, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &DataOut))
	{
		vector<BYTE> encrypted(DataOut.pbData, DataOut.pbData + DataOut.cbData);
		LocalFree(DataOut.pbData);
		return encrypted;
	}
	LOG_ERROR("CryptProtectData failed. Error code: " + std::to_string(GetLastError()));
	throw runtime_error("Encryption failed");
}

string decryptData(const vector<BYTE>& encryptedText)
{
	DATA_BLOB DataIn;
	DATA_BLOB DataOut;
	LPWSTR szDescriptionOut = nullptr;

	DataIn.pbData = (BYTE*)encryptedText.data();
	DataIn.cbData = encryptedText.size();

	if (CryptUnprotectData(&DataIn, &szDescriptionOut, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &DataOut))
	{
		string decrypted((char*)DataOut.pbData, DataOut.cbData);
		LocalFree(DataOut.pbData);
		if (szDescriptionOut)
			LocalFree(szDescriptionOut);

		if (!decrypted.empty() && decrypted.back() == '\0')
		{
			decrypted.pop_back();
		}
		return decrypted;
	}
	LOG_ERROR("CryptUnprotectData failed. Error code: " + std::to_string(GetLastError()));

	if (GetLastError() == ERROR_INVALID_DATA || GetLastError() == 0x8009000B)
	{
		LOG_ERROR("Could not decrypt data. It might be from a different user/machine or corrupted.");
	}

	return "";
}

namespace Data
{
	void LoadAccounts(const string& filename)
	{
		ifstream fileStream{filename};
		if (!fileStream.is_open())
		{
			LOG_INFO("No "+ filename + ", starting fresh");
			return;
		}
		json dataArray;
		try
		{
			fileStream >> dataArray;
		}
		catch (const json::parse_error& exception)
		{
			LOG_ERROR("Failed to parse " + filename + ": " + exception.what());

			return;
		}

		g_accounts.clear();
		for (auto& item : dataArray)
		{
			AccountData account;
			account.id = item.value("id", 0);
			account.displayName = item.value("displayName", "");
			account.username = item.value("username", "");
			account.userId = item.value("userId", "");
			account.status = item.value("status", "");
			account.note = item.value("note", "");
			account.isFavorite = item.value("isFavorite", false);

			if (item.contains("encryptedCookie"))
			{
				string b64EncryptedCookie = item.value("encryptedCookie", "");
				if (!b64EncryptedCookie.empty())
				{
					try
					{
						vector<BYTE> encryptedCookieBytes = base64_decode(b64EncryptedCookie);
						account.cookie = decryptData(encryptedCookieBytes);
						if (account.cookie.empty() && !encryptedCookieBytes.empty())
						{
							LOG_ERROR(
								"Failed to decrypt cookie for account ID " + std::to_string(account.id) +
								". Cookie will be empty. User might need to re-authenticate.");
						}
					}
					catch (const exception& e)
					{
						LOG_ERROR(
							"Exception during cookie decryption for account ID " + std::to_string(account.id) + ": " + e
							.what());
						account.cookie = "";
					}
				}
				else
				{
					account.cookie = "";
				}
			}
			else if (item.contains("cookie"))
			{
				account.cookie = item.value("cookie", "");
				LOG_INFO(
					"Account ID " + std::to_string(account.id) +
					" has an unencrypted cookie. It will be encrypted on next save.");
			}

			g_accounts.push_back(move(account));
		}
		LOG_INFO("Loaded " + std::to_string(g_accounts.size()) + " accounts");
	}

	void SaveAccounts(const string& filename)
	{
		ofstream out{filename};
		if (!out.is_open())
		{
			LOG_ERROR("Could not open '" + filename + "' for writing");
			return;
		}

		json dataArray = json::array();
		for (auto& account : g_accounts)
		{
			string b64EncryptedCookie;
			if (!account.cookie.empty())
			{
				try
				{
					vector<BYTE> encryptedCookieBytes = encryptData(account.cookie);
					b64EncryptedCookie = base64_encode(encryptedCookieBytes);
				}
				catch (const exception& exception)
				{
					LOG_ERROR(
						"Exception during cookie encryption for account ID " + std::to_string(account.id) + ": " +
						exception.what() + ". Cookie will not be saved.");
					b64EncryptedCookie = "";
				}
			}

			dataArray.push_back({
				{"id", account.id},
				{"displayName", account.displayName},
				{"username", account.username},
				{"userId", account.userId},
				{"status", account.status},
				{"note", account.note},
				{"encryptedCookie", b64EncryptedCookie},
				{"isFavorite", account.isFavorite}
			});
		}
		out << dataArray.dump(4);
		LOG_INFO("Saved " + std::to_string(g_accounts.size()) + " accounts");
	}

	void LoadFavorites(const std::string& filename)
	{
		std::ifstream fin{filename};
		if (!fin.is_open())
		{
			LOG_INFO("No " + filename + ", starting with 0 favourites");
			return;
		}

		g_favorites.clear();
		try
		{
			json arr;
			fin >> arr;
			for (auto& j : arr)
			{
				g_favorites.push_back(FavoriteGame{
					j.value("name", ""),
					j.value("universeId", 0ULL),
					j.value("placeId", j.value("universeId", 0ULL))
				});
			}
			LOG_INFO("Loaded " + std::to_string(g_favorites.size()) + " favourites");
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Could not parse " + filename + ": " + e.what());
		}
	}

	void SaveFavorites(const std::string& filename)
	{
		std::ofstream out{filename};
		if (!out.is_open())
		{
			LOG_ERROR("Could not open '" + filename + "' for writing");
			return;
		}

		json arr = json::array();
		for (auto& f : g_favorites)
		{
			arr.push_back({
				{"universeId", f.universeId},
				{"placeId", f.placeId},
				{"name", f.name}
			});
		}

		out << arr.dump(4);
		LOG_INFO("Saved " + std::to_string(g_favorites.size()) + " favourites");
	}

	void LoadSettings(const std::string& filename)
	{
		std::ifstream fin{filename};
		if (!fin.is_open())
		{
			LOG_INFO("No " + filename + ", using no default account");
			return;
		}

		try
		{
			nlohmann::json j;
			fin >> j;
			g_defaultAccountId = j.value("defaultAccountId", -1);
			LOG_INFO("Default account ID = " + std::to_string(g_defaultAccountId));
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Failed to parse " + filename + ": " + e.what());
		}
	}

	void SaveSettings(const std::string& filename)
	{
		nlohmann::json j;
		j["defaultAccountId"] = g_defaultAccountId;
		std::ofstream out{filename};
		if (!out.is_open())
		{
			LOG_ERROR("Could not open " + filename + " for writing");
			return;
		}
		out << j.dump(4);
		LOG_INFO("Saved defaultAccountId=" + std::to_string(g_defaultAccountId));
	}
}
