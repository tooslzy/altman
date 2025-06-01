#define _CRT_SECURE_NO_WARNINGS

#include "accounts_context_menu.h"
#include <shlobj_core.h>
#include <imgui.h>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <set>

#include "../../utils/roblox_api.h"
#include "../../utils/threading.h"
#include "../../utils/logging.hpp"
#include "../../utils/status.h"
#include "../../ui.h"
#include "../data.h"

static char g_edit_note_buffer_ctx[1024];
static int g_editing_note_for_account_id_ctx = -1;

using namespace ImGui;
using namespace std;

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <shlobj.h>

void LaunchBrowserWithCookie(const AccountData& account)
{
	// Get AppData folder path
	char appDataPath[MAX_PATH];
	if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath)))
	{
		LOG_ERROR("Failed to get AppData path");
		Status::Set("Failed to get AppData path");
		return;
	}

	// Create profile directory for this account
	filesystem::path profilesDir = filesystem::path(appDataPath) / "RobloxAccountManager" / "BrowserProfiles";
	filesystem::path profilePath = profilesDir / ("Account_" + to_string(account.id));

	try
	{
		filesystem::create_directories(profilePath);
	}
	catch (const exception& e)
	{
		LOG_ERROR("Failed to create profile directory: " + string(e.what()));
		Status::Set("Failed to create profile directory");
		return;
	}

	// Create a simple Chrome extension to inject the cookie
	filesystem::path extensionPath = profilePath / "cookie_injector";
	filesystem::create_directories(extensionPath);

	// Write manifest.json
	ofstream manifest(extensionPath / "manifest.json");
	manifest << R"({
  "manifest_version": 3,
  "name": "Roblox Cookie Injector",
  "version": "1.0",
  "permissions": ["cookies", "webNavigation"],
  "host_permissions": ["*://*.roblox.com/*"],
  "background": {
    "service_worker": "background.js"
  }
})";
	manifest.close();

	// Write background.js with the cookie
	ofstream background(extensionPath / "background.js");
	background << "chrome.webNavigation.onBeforeNavigate.addListener(\n"
		<< "  function(details) {\n"
		<< "    if (details.url.includes('roblox.com')) {\n"
		<< "      chrome.cookies.set({\n"
		<< "        url: 'https://www.roblox.com',\n"
		<< "        name: '.ROBLOSECURITY',\n"
		<< "        value: '" << account.cookie << "',\n"
		<< "        domain: '.roblox.com',\n"
		<< "        path: '/',\n"
		<< "        secure: true,\n"
		<< "        httpOnly: true,\n"
		<< "        sameSite: 'lax'\n"
		<< "      });\n"
		<< "    }\n"
		<< "  },\n"
		<< "  {url: [{hostContains: 'roblox.com'}]}\n"
		<< ");\n";
	background.close();

	// Find Chrome executable
	string chromePath;
	char programFiles[MAX_PATH];

	// Try multiple possible Chrome locations
	vector<string> chromePaths = {
		string(getenv("LOCALAPPDATA")) + "\\Google\\Chrome\\Application\\chrome.exe",
		string(getenv("PROGRAMFILES")) + "\\Google\\Chrome\\Application\\chrome.exe",
		string(getenv("PROGRAMFILES(X86)")) + "\\Google\\Chrome\\Application\\chrome.exe"
	};

	for (const auto& path : chromePaths)
	{
		if (filesystem::exists(path))
		{
			chromePath = path;
			break;
		}
	}

	if (chromePath.empty())
	{
		LOG_ERROR("Chrome not found");
		Status::Set("Chrome not found");
		return;
	}

	// Build command line with absolute paths
	string cmdLine = "\"" + chromePath + "\""
		" --user-data-dir=\"" + profilePath.string() + "\""
		" --profile-directory=\"Default\""
		" --load-extension=\"" + extensionPath.string() + "\""
		" --no-first-run"
		" --no-default-browser-check"
		" --disable-popup-blocking"
		" --disable-prompt-on-repost"
		" --force-new-profile"
		" \"https://www.roblox.com/users/\"" + account.userId + "/profile";

	LOG_INFO("Launching browser with command: " + cmdLine);
	LOG_INFO("Profile path: " + profilePath.string());

	// Use CreateProcess instead of system()
	STARTUPINFOA si = {0};
	PROCESS_INFORMATION pi = {nullptr};
	si.cb = sizeof(si);

	if (!CreateProcessA(
		nullptr,
		const_cast<char*>(cmdLine.c_str()),
		nullptr,
		nullptr,
		FALSE,
		CREATE_NEW_CONSOLE,
		nullptr,
		nullptr,
		&si,
		&pi))
	{
		DWORD error = GetLastError();
		LOG_ERROR("Failed to launch browser. Error code: " + to_string(error));
		Status::Set("Failed to launch browser");
	}
	else
	{
		LOG_INFO("Successfully launched browser for account: " + account.displayName);
		Status::Set("Launched browser for " + account.displayName);

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

void RenderAccountContextMenu(AccountData& account, const string& unique_context_menu_id)
{
	if (BeginPopupContextItem(unique_context_menu_id.c_str()))
	{
		Text("Account: %s", account.displayName.c_str());
		if (g_selectedAccountIds.contains(account.id))
		{
			SameLine();
			TextDisabled("(Selected)");
		}
		Separator();

		if (BeginMenu("Edit Note"))
		{
			if (g_editing_note_for_account_id_ctx != account.id)
			{
				strncpy_s(g_edit_note_buffer_ctx, account.note.c_str(), sizeof(g_edit_note_buffer_ctx) - 1);
				g_edit_note_buffer_ctx[sizeof(g_edit_note_buffer_ctx) - 1] = '\0';
				g_editing_note_for_account_id_ctx = account.id;
			}

			PushItemWidth(250.0f);
			InputTextMultiline("##EditNoteInput", g_edit_note_buffer_ctx, sizeof(g_edit_note_buffer_ctx),
			                   ImVec2(0, GetTextLineHeight() * 4));
			PopItemWidth();

			if (Button("Save##Note"))
			{
				if (g_editing_note_for_account_id_ctx == account.id)
				{
					account.note = g_edit_note_buffer_ctx;
					Data::SaveAccounts();
					printf("Note updated for account ID %d: %s\n", account.id, account.note.c_str());
					LOG_INFO("Note updated for account ID " + to_string(account.id) + ": " + account.note);
				}
				g_editing_note_for_account_id_ctx = -1;
				CloseCurrentPopup();
			}
			SameLine();
			if (Button("Cancel##Note"))
			{
				g_editing_note_for_account_id_ctx = -1;
				CloseCurrentPopup();
			}
			ImGui::EndMenu();
		}

		Separator();

		if (MenuItem("Copy UserID"))
		{
			SetClipboardText(account.userId.c_str());
			LOG_INFO("Copied UserID for account: " + account.displayName);
		}
		if (MenuItem("Copy Cookie"))
		{
			if (!account.cookie.empty())
			{
				SetClipboardText(account.cookie.c_str());
				LOG_INFO("Copied cookie for account: " + account.displayName);
			}
			else
			{
				printf("Info: Cookie for account ID %d (%s) is empty.\n", account.id, account.displayName.c_str());
				LOG_WARN(
					"Attempted to copy empty cookie for account: " + account.displayName + " (ID: " + to_string(account.
						id) + ")");
				SetClipboardText("");
			}
		}
		if (MenuItem("Copy Display Name"))
		{
			SetClipboardText(account.displayName.c_str());
			LOG_INFO("Copied Display Name for account: " + account.displayName);
		}
		if (MenuItem("Copy Username"))
		{
			SetClipboardText(account.username.c_str());
			LOG_INFO("Copied Username for account: " + account.displayName);
		}
		if (MenuItem("Copy Note"))
		{
			SetClipboardText(account.note.c_str());
			LOG_INFO("Copied Note for account: " + account.displayName);
		}

		Separator();

		if (MenuItem("Open In Browser"))
		{
			if (!account.cookie.empty())
			{
				LOG_INFO(
					"Opening browser for account: " + account.displayName + " (ID: " + to_string(account.id) + ")");
				Threading::newThread([account]()
				{
					LaunchBrowserWithCookie(account);
				});
			}
			else
			{
				LOG_WARN("Cannot open browser - cookie is empty for account: " + account.displayName);
				Status::Set("Cookie is empty for this account");
			}
		}

		if (MenuItem("Copy Launch Link"))
		{
			string acc_cookie = account.cookie;
			string place_id_str = join_value_buf;
			string job_id_str = join_jobid_buf;

			Threading::newThread(
				[acc_cookie, place_id_str, job_id_str, account_id = account.id, account_display_name = account.
					displayName]
				{
					LOG_INFO(
						"Generating launch link for account: " + account_display_name + " (ID: " + to_string(account_id)
						+ ") for place: " + place_id_str + (job_id_str.empty() ? "" : " job: " + job_id_str));
					bool hasJob = !job_id_str.empty();
					auto now_ms = chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now().time_since_epoch()
					).count();

					thread_local mt19937_64 rng{random_device{}()};
					static uniform_int_distribution<int> d1(100000, 130000), d2(100000, 900000);

					string browserTracker = to_string(d1(rng)) + to_string(d2(rng));
					string ticket = RobloxApi::fetchAuthTicket(acc_cookie);
					if (ticket.empty())
					{
						LOG_ERROR(
							"Failed to grab auth ticket for account ID " + to_string(account_id) +
							" while generating launch link.");
						return;
					}
					Status::Set("Got auth ticket");
					LOG_INFO("Successfully fetched auth ticket for account ID " + to_string(account_id));

					string placeLauncherUrl =
						"https://assetgame.roblox.com/game/PlaceLauncher.ashx?request=RequestGame%26placeId="
						+ place_id_str;
					if (hasJob) { placeLauncherUrl += "%26gameId=" + job_id_str; }

					string uri =
						string("roblox-player://1/1+launchmode:play")
						+ "+gameinfo:" + ticket
						+ "+launchtime:" + to_string(now_ms)
						+ "+browsertrackerid:" + browserTracker
						+ "+placelauncherurl:" + placeLauncherUrl
						+ "+robloxLocale:en_us+gameLocale:en_us";

					Status::Set("Copied link to clipboard!");
					SetClipboardText(uri.c_str());
					LOG_INFO("Launch link copied to clipboard for account ID " + to_string(account_id));
				});
		}


		Separator();

		if (MenuItem("Delete This Account"))
		{
			LOG_INFO("Attempting to delete account: " + account.displayName + " (ID: " + to_string(account.id) + ")");
			erase_if(
				g_accounts,
				[&](const AccountData& acc_data)
				{
					return acc_data.id == account.id;
				});
			g_selectedAccountIds.erase(account.id);
			Status::Set("Deleted account " + account.displayName);
			Data::SaveAccounts();
			LOG_INFO("Successfully deleted account: " + account.displayName + " (ID: " + to_string(account.id) + ")");
			CloseCurrentPopup();
		}

		if (!g_selectedAccountIds.empty() && g_selectedAccountIds.size() > 1 && g_selectedAccountIds.
			contains(account.id))
		{
			char buf[64];
			snprintf(buf, sizeof(buf), "Delete %zu Selected Account(s)", g_selectedAccountIds.size());
			if (MenuItem(buf))
			{
				LOG_INFO("Attempting to delete " + to_string(g_selectedAccountIds.size()) + " selected accounts.");
				erase_if(
					g_accounts,
					[&](const AccountData& acc_data)
					{
						return g_selectedAccountIds.contains(acc_data.id);
					});
				g_selectedAccountIds.clear();
				Data::SaveAccounts();
				Status::Set("Deleted selected accounts");
				LOG_INFO("Successfully deleted selected accounts.");
				CloseCurrentPopup();
			}
		}
		EndPopup();
	}
}
