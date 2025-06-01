#define CRT_SECURE_NO_WARNINGS
#define IDI_ICON_32 102

#include "accounts_context_menu.h"
#include <shlobj_core.h>
#include <imgui.h>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <set>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <thread>
#include <atomic>
#include <dwmapi.h>
#include <memory>

#include "../../utils/roblox_api.h"
#include "../../utils/threading.h"
#include "../../utils/logging.hpp"
#include "../../utils/status.h"
#include "../../ui.h"
#include "../data.h"

#pragma comment(lib, "Dwmapi.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

static char g_edit_note_buffer_ctx[1024];
static int g_editing_note_for_account_id_ctx = -1;

using namespace ImGui;
using namespace std;

class AccountWebViewWindow
{
private:
    HWND m_hwnd = nullptr;
    ComPtr<ICoreWebView2Controller> m_controller;
    ComPtr<ICoreWebView2> m_webview;
    AccountData m_account;
    static std::atomic<int> s_windowCount;
    static const wchar_t* s_windowClassName;
    bool m_initialized = false;

public:
    AccountWebViewWindow(const AccountData& account) : m_account(account) {}

    ~AccountWebViewWindow()
    {
        if (m_hwnd)
        {
            DestroyWindow(m_hwnd);
        }
    }

    bool Create(HINSTANCE hInstance)
    {
        // Register window class if needed
        static bool classRegistered = false;
        if (!classRegistered)
        {
            WNDCLASSEXW wc{ sizeof(wc) };
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.hInstance = hInstance;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_32));
            wc.lpszClassName = s_windowClassName;
            wc.lpfnWndProc = WndProc;
            wc.cbWndExtra = sizeof(void*);

            if (!RegisterClassExW(&wc))
            {
                LOG_ERROR("Failed to register window class");
                return false;
            }
            classRegistered = true;
        }

        std::wstring title = std::wstring(m_account.displayName.begin(), m_account.displayName.end());

        m_hwnd = CreateWindowExW(
            0, s_windowClassName, title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
            nullptr, nullptr, hInstance, this);


        if (!m_hwnd)
        {
            LOG_ERROR("Failed to create window");
            return false;
        }

        s_windowCount++;
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        CreateWebView();
        return true;
    }

	void ProcessMessages()
    {
    	MSG msg;
    	while (GetMessage(&msg, nullptr, 0, 0))
    	{
    		TranslateMessage(&msg);
    		DispatchMessage(&msg);
    	}
    }

private:
    void CreateWebView()
    {
        // Get AppData folder for WebView2 user data
        char appDataPath[MAX_PATH];
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath)))
        {
            LOG_ERROR("Failed to get AppData path");
            Status::Set("Failed to get AppData path");
            return;
        }

        // Create unique user data folder for this account
        std::filesystem::path userDataPath = std::filesystem::path(appDataPath) / "Altman" / "WebView2Profiles" / m_account.username;
        try
        {
            std::filesystem::create_directories(userDataPath);
        }
        catch (const exception& e)
        {
            LOG_ERROR("Failed to create user data directory: " + string(e.what()));
            return;
        }

        std::wstring userDataPathW = userDataPath.wstring();

        CreateCoreWebView2EnvironmentWithOptions(
            nullptr, userDataPathW.c_str(), nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT envHr, ICoreWebView2Environment* env) -> HRESULT
                {
                    if (FAILED(envHr))
                    {
                        LOG_ERROR("CreateCoreWebView2Environment failed");
                        return envHr;
                    }

                    env->CreateCoreWebView2Controller(
                        m_hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT ctrlHr, ICoreWebView2Controller* controller) -> HRESULT
                            {
                                if (FAILED(ctrlHr))
                                {
                                    LOG_ERROR("CreateCoreWebView2Controller failed");
                                    return ctrlHr;
                                }

                                m_controller = controller;
                                m_controller->get_CoreWebView2(&m_webview);

                                // Fill the client area
                                RECT rc{};
                                GetClientRect(m_hwnd, &rc);
                                m_controller->put_Bounds(rc);

                                // Inject cookie and navigate
                                InjectRobloxCookie();

                                // Navigate to user profile
                                std::wstring url = L"https://www.roblox.com/home";
                                m_webview->Navigate(url.c_str());

                                m_initialized = true;
                                LOG_INFO("Successfully created WebView2 for account: " + m_account.displayName);
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());
    }

    void InjectRobloxCookie()
    {
        ComPtr<ICoreWebView2_2> webview2_2;
        if (FAILED(m_webview.As(&webview2_2)) || !webview2_2)
        {
            LOG_ERROR("WebView2 runtime too old – missing cookie API");
            return;
        }

        ComPtr<ICoreWebView2CookieManager> mgr;
        if (FAILED(webview2_2->get_CookieManager(&mgr)) || !mgr)
        {
            LOG_ERROR("Cannot obtain CookieManager");
            return;
        }

        // Convert cookie to wide string
        std::wstring cookieValueW(m_account.cookie.begin(), m_account.cookie.end());

        // Create the cookie
        ComPtr<ICoreWebView2Cookie> cookie;
        if (FAILED(mgr->CreateCookie(
                L".ROBLOSECURITY", cookieValueW.c_str(),
                L".roblox.com", L"/",
                &cookie)) || !cookie)
        {
            LOG_ERROR("CreateCookie failed");
            return;
        }

        cookie->put_IsSecure(TRUE);
        cookie->put_IsHttpOnly(TRUE);
        cookie->put_SameSite(COREWEBVIEW2_COOKIE_SAME_SITE_KIND_LAX);

        // Set expiration to 10 years from now
        using namespace std::chrono;
        double expires = duration_cast<seconds>(
            system_clock::now().time_since_epoch() + hours(24 * 365 * 10)
        ).count();
        cookie->put_Expires(expires);

        if (FAILED(mgr->AddOrUpdateCookie(cookie.Get())))
        {
            LOG_ERROR("AddOrUpdateCookie failed");
            return;
        }

        LOG_INFO("Successfully injected cookie for account: " + m_account.displayName);
    }

    void ResizeWebView()
    {
        if (m_controller)
        {
            RECT rc{};
            GetClientRect(m_hwnd, &rc);
            m_controller->put_Bounds(rc);
        }
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        AccountWebViewWindow* pThis = nullptr;

        if (msg == WM_NCCREATE)
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<AccountWebViewWindow*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
        else
        {
            pThis = reinterpret_cast<AccountWebViewWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }

        if (pThis)
        {
            switch (msg)
            {
            case WM_SIZE:
                pThis->ResizeWebView();
                return 0;
            case WM_DESTROY:
                s_windowCount--;
                PostQuitMessage(0);
                return 0;
            }
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
};

std::atomic<int> AccountWebViewWindow::s_windowCount{0};
const wchar_t* AccountWebViewWindow::s_windowClassName = L"AltmanWebView2Window";

void LaunchBrowserWithCookie(const AccountData& account)
{
    if (account.cookie.empty())
    {
        LOG_WARN("Cannot open browser - cookie is empty for account: " + account.displayName);
        Status::Set("Cookie is empty for this account");
        return;
    }

    std::thread([account]()
    {
        LOG_INFO("Launching WebView2 browser for account: " + account.displayName);

        auto window = std::make_unique<AccountWebViewWindow>(account);

        if (window->Create(GetModuleHandle(nullptr)))
        {
            Status::Set("Launched browser for " + account.displayName);
            window->ProcessMessages();
        }
        else
        {
            LOG_ERROR("Failed to create WebView2 window for account: " + account.displayName);
            Status::Set("Failed to launch browser");
        }
    }).detach();
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
