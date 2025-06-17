#define _CRT_SECURE_NO_WARNINGS
#include "servers.h"
#include "servers_utils.h"

#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include <imgui.h>
#include "imgui_internal.h"
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <thread>
#include <utility>
#include <cstdio>

#include "../components.h"
#include "network/roblox_api.h"
#include "core/status.h"
#include "system/launcher.hpp"
#include "ui/modal_popup.h"
#include "../../ui.h"

using namespace ImGui;
using namespace std;

enum class ServerSortMode {
    None = 0,
    PingAsc,
    PingDesc,
    PlayersAsc,
    PlayersDesc
};

static ServerSortMode g_serverSortMode = ServerSortMode::None;
static int g_serverSortComboIndex = 0;

static vector<PublicServerInfo> s_cachedServers;
static unordered_map<string, RobloxApi::ServerPage> g_pageCache;

static string g_currCursor_servers;
static string g_nextCursor_servers;
static string g_prevCursor_servers;

static char s_searchBuffer[64]{};
static char s_placeIdBuffer[32]{};

static uint64_t g_current_placeId_servers = 0;

static bool matchesQuery(const PublicServerInfo &srv, const string &qLower) {
    string alias = guidToName(srv.jobId);
    string hay = alias + ' ' + srv.jobId + ' ' + to_string(srv.currentPlayers) + '/' +
                 to_string(srv.maximumPlayers) + ' ' +
                 to_string(static_cast<int>(srv.averagePing + 0.5)) + "ms " +
                 to_string(static_cast<int>(srv.averageFps + 0.5));
    string lowerHay = toLower(hay);
    return lowerHay.find(qLower) != string::npos;
}

static void fetchPageServers(uint64_t placeId, const string &cursor = {}) {
    try {
        if (placeId != g_current_placeId_servers) {
            g_pageCache.clear();
            g_current_placeId_servers = placeId;
        }
        RobloxApi::ServerPage page;
        auto it_cache = g_pageCache.find(cursor);
        if (it_cache != g_pageCache.end()) {
            page = it_cache->second;
        } else {
            page = RobloxApi::getPublicServersPage(placeId, cursor);
            g_pageCache.emplace(cursor, page);
        }
        s_cachedServers = page.data;
        g_nextCursor_servers = page.nextCursor;
        g_prevCursor_servers = page.prevCursor;
        g_currCursor_servers = cursor;
        LOG_INFO(s_cachedServers.empty() ? "No servers found for this page" : "Fetched servers");
    } catch (const exception &ex) {
        LOG_INFO(string("Fetch error: ") + ex.what());
        s_cachedServers.clear();
        g_nextCursor_servers.clear();
        g_prevCursor_servers.clear();
    }
}

void ServerTab_SearchPlace(uint64_t placeId) {
    snprintf(s_placeIdBuffer, sizeof(s_placeIdBuffer), "%llu", placeId);
    fetchPageServers(placeId);
}

void RenderServersTab() {
    if (g_targetPlaceId_ServersTab != 0) {
        snprintf(s_placeIdBuffer, sizeof(s_placeIdBuffer), "%llu", g_targetPlaceId_ServersTab);
        fetchPageServers(g_targetPlaceId_ServersTab);
        g_targetPlaceId_ServersTab = 0;
    }

    ImGuiStyle &style = GetStyle();
    float fetchButtonWidth = CalcTextSize("Fetch Servers").x + style.FramePadding.x * 2.0f;
    float prevButtonWidth = CalcTextSize("\xEF\x81\x93 Prev Page").x + style.FramePadding.x * 2.0f;
    float nextButtonWidth = CalcTextSize("Next Page \xEF\x81\x94").x + style.FramePadding.x * 2.0f;
    float buttons_total_width = fetchButtonWidth + prevButtonWidth + nextButtonWidth + style.ItemSpacing.x * 2;
    float inputWidth = GetContentRegionAvail().x - buttons_total_width - style.ItemSpacing.x;
    if (inputWidth < 100.0f)
        inputWidth = 100.0f;
    PushItemWidth(inputWidth);
    InputTextWithHint("##placeid_servers", "Place Id", s_placeIdBuffer, sizeof(s_placeIdBuffer));
    PopItemWidth();
    SameLine(0, style.ItemSpacing.x);
    if (Button("Fetch Servers", ImVec2(fetchButtonWidth, 0))) {
        string raw_pid{s_placeIdBuffer};
        erase_if(raw_pid, ::isspace);
        if (raw_pid.empty() || !all_of(raw_pid.begin(), raw_pid.end(), ::isdigit)) {
            LOG_INFO("Place ID must be all digits.");
        } else {
            try {
                uint64_t pid_val = stoull(raw_pid);
                g_currCursor_servers.clear();
                fetchPageServers(pid_val);
            } catch (const out_of_range &oor) {
                LOG_INFO(string("Place ID is too large: ") + oor.what());
            }
            catch (const invalid_argument &ia) {
                LOG_INFO(string("Invalid Place ID format: ") + ia.what());
            }
        }
    }
    SameLine(0, style.ItemSpacing.x);
    BeginDisabled(g_prevCursor_servers.empty());
    if (Button("\xEF\x81\x93 Prev Page", ImVec2(prevButtonWidth, 0)))
        fetchPageServers(g_current_placeId_servers, g_prevCursor_servers);
    EndDisabled();
    SameLine(0, style.ItemSpacing.x);
    BeginDisabled(g_nextCursor_servers.empty());
    if (Button("Next Page \xEF\x81\x94", ImVec2(nextButtonWidth, 0)))
        fetchPageServers(g_current_placeId_servers, g_nextCursor_servers);
    EndDisabled();

    Separator();
    const char *sortOptions[] = {
        "None",
        "Ping (Asc)",
        "Ping (Desc)",
        "Players (Asc)",
        "Players (Desc)"
    };

    float comboWidth = CalcTextSize("Players (Desc)").x + style.FramePadding.x * 7.0f;
    float searchInputWidth = GetContentRegionAvail().x - comboWidth - style.ItemSpacing.x;
    if (searchInputWidth < 100.0f)
        searchInputWidth = 100.0f;
    PushItemWidth(searchInputWidth);
    InputTextWithHint("##search_servers", "Search...", s_searchBuffer, sizeof(s_searchBuffer));
    PopItemWidth();
    SameLine(0, style.ItemSpacing.x);
    PushItemWidth(comboWidth);
    if (Combo("##server_filter", &g_serverSortComboIndex, sortOptions, IM_ARRAYSIZE(sortOptions))) {
        g_serverSortMode = static_cast<ServerSortMode>(g_serverSortComboIndex);
    }
    PopItemWidth();

    string qLower = toLower(s_searchBuffer);
    bool isSearching = !qLower.empty();
    vector<PublicServerInfo> displayList;
    if (isSearching) {
        for (const auto &pair_cache: g_pageCache) {
            for (const auto &srv: pair_cache.second.data) {
                if (matchesQuery(srv, qLower))
                    displayList.push_back(srv);
            }
        }
    } else {
        displayList = s_cachedServers;
    }

    auto sortServers = [&](ServerSortMode mode) {
        switch (mode) {
            case ServerSortMode::PingAsc:
                sort(displayList.begin(), displayList.end(), [](const PublicServerInfo &a, const PublicServerInfo &b) {
                    return a.averagePing < b.averagePing;
                });
                break;
            case ServerSortMode::PingDesc:
                sort(displayList.begin(), displayList.end(), [](const PublicServerInfo &a, const PublicServerInfo &b) {
                    return a.averagePing > b.averagePing;
                });
                break;
            case ServerSortMode::PlayersAsc:
                sort(displayList.begin(), displayList.end(), [](const PublicServerInfo &a, const PublicServerInfo &b) {
                    return a.currentPlayers < b.currentPlayers;
                });
                break;
            case ServerSortMode::PlayersDesc:
                sort(displayList.begin(), displayList.end(), [](const PublicServerInfo &a, const PublicServerInfo &b) {
                    return a.currentPlayers > b.currentPlayers;
                });
                break;
            case ServerSortMode::None:
            default:
                break;
        }
    };

    if (g_serverSortMode == ServerSortMode::None && isSearching) {
        sort(displayList.begin(), displayList.end(), [](const PublicServerInfo &a, const PublicServerInfo &b) {
            return guidToName(a.jobId) < guidToName(b.jobId);
        });
    } else {
        sortServers(g_serverSortMode);
    }

    constexpr int columnCount = 5;
    ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable;

    if (BeginTable("ServersTable", columnCount, table_flags, ImVec2(0, GetContentRegionAvail().y))) {
        TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        TableSetupColumn("Job ID", ImGuiTableColumnFlags_WidthStretch);
        TableSetupColumn("Players", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        TableSetupColumn("FPS", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        TableSetupScrollFreeze(0, 1);
        TableNextRow(ImGuiTableRowFlags_Headers);
        TableNextColumn();
        TextUnformatted("Name");
        TableNextColumn();
        TextUnformatted("Job ID");
        TableNextColumn();
        TextUnformatted("Players");
        TableNextColumn();
        TextUnformatted("Ping");
        TableNextColumn();
        TextUnformatted("FPS");

        float row_interaction_height = GetFrameHeight();
        if (row_interaction_height <= 0)
            row_interaction_height = GetTextLineHeightWithSpacing();
        if (row_interaction_height <= 0)
            row_interaction_height = 19.0f;
        float text_visual_height = GetTextLineHeight();
        float vertical_padding = (row_interaction_height - text_visual_height) * 0.5f;
        vertical_padding = ImMax(0.0f, vertical_padding);

        for (const auto &srv: displayList) {
            TableNextRow();
            PushID(srv.jobId.c_str());

            TableNextColumn();
            float cell1_start_y = GetCursorPosY();
            SetCursorPosY(cell1_start_y + vertical_padding);
            string serverNameStr = guidToName(srv.jobId);
            TextUnformatted(serverNameStr.c_str());
            SetCursorPosY(cell1_start_y + row_interaction_height);

            TableNextColumn();
            float cell2_start_y = GetCursorPosY();

            char selectable_widget_id[128];
            snprintf(selectable_widget_id, sizeof(selectable_widget_id), "##JobIDSelectable_%s", srv.jobId.c_str());

            if (Selectable(selectable_widget_id, false,
                           ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
                           ImVec2(0, row_interaction_height))) {
                if (!g_selectedAccountIds.empty()) {
                    vector<pair<int, string> > accounts;
                    for (int id: g_selectedAccountIds) {
                        auto it = find_if(g_accounts.begin(), g_accounts.end(),
                                          [&](const AccountData &a) { return a.id == id; });
                        if (it != g_accounts.end())
                            accounts.emplace_back(it->id, it->cookie);
                    }
                    if (!accounts.empty()) {
                        LOG_INFO("Joining server (left-click)...");
                        thread([accounts, pId = g_current_placeId_servers, jId = srv.jobId]() {
                                    launchRobloxSequential(pId, jId, accounts);
                                })
                                .detach();
                    } else {
                        LOG_INFO("Selected account not found.");
                    }
                } else {
                    LOG_INFO("No account selected to join server.");
                    Status::Error("No account selected to join server.");
                    ModalPopup::Add("Select an account first.");
                }
            }

            if (BeginPopupContextItem("ServerRowContextMenu")) {
                if (MenuItem("Copy Name")) {
                    SetClipboardText(serverNameStr.c_str());
                }
                if (MenuItem("Copy Job ID")) {
                    SetClipboardText(srv.jobId.c_str());
                }
                if (MenuItem("Copy Place ID")) {
                    SetClipboardText(to_string(g_current_placeId_servers).c_str());
                }
                if (BeginMenu("Copy Launch Method")) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "roblox://placeId=%llu&gameInstanceId=%s",
                             (unsigned long long) g_current_placeId_servers, srv.jobId.c_str());
                    if (MenuItem("Deep Link")) SetClipboardText(buf);
                    string js = "Roblox.GameLauncher.joinGameInstance(" + to_string(g_current_placeId_servers) + ", \""
                                + srv.jobId + "\")";
                    if (MenuItem("JavaScript")) SetClipboardText(js.c_str());
                    string luau = "game:GetService(\"TeleportService\"):TeleportToPlaceInstance(" + to_string(
                                      g_current_placeId_servers) + ", \"" + srv.jobId + "\")";
                    if (MenuItem("ROBLOX Luau")) SetClipboardText(luau.c_str());
                    ImGui::EndMenu();
                }
                if (MenuItem("Generate Invite Link")) {
                    string link = "https://www.roblox.com/games/start?placeId=" + to_string(g_current_placeId_servers) +
                                  "&gameInstanceId=" + srv.jobId;
                    SetClipboardText(link.c_str());
                }
                Separator();
                if (MenuItem("Join Server")) {
                    if (!g_selectedAccountIds.empty()) {
                        vector<pair<int, string> > accounts;
                        for (int id: g_selectedAccountIds) {
                            auto it = find_if(g_accounts.begin(), g_accounts.end(),
                                              [&](const AccountData &a) { return a.id == id; });
                            if (it != g_accounts.end())
                                accounts.emplace_back(it->id, it->cookie);
                        }
                        if (!accounts.empty()) {
                            LOG_INFO("Joining server (context menu)...");
                            thread([accounts, pId = g_current_placeId_servers, jId = srv.jobId]() {
                                        launchRobloxSequential(pId, jId, accounts);
                                    })
                                    .detach();
                        } else {
                            LOG_INFO("Selected account not found.");
                        }
                    } else {
                        LOG_INFO("No account selected to join server.");
                        Status::Error("No account selected to join server.");
                        ModalPopup::Add("Select an account first.");
                    }
                }
                EndPopup();
            }

            SetCursorPosY(cell2_start_y + vertical_padding);
            TextUnformatted(srv.jobId.c_str());
            SetCursorPosY(cell2_start_y + row_interaction_height);

            TableNextColumn();
            float cell3_start_y = GetCursorPosY();
            SetCursorPosY(cell3_start_y + vertical_padding);
            char playersBuf[16];
            snprintf(playersBuf, sizeof(playersBuf), "%d/%d", srv.currentPlayers, srv.maximumPlayers);
            TextUnformatted(playersBuf);
            SetCursorPosY(cell3_start_y + row_interaction_height);

            TableNextColumn();
            float cell4_start_y = GetCursorPosY();
            SetCursorPosY(cell4_start_y + vertical_padding);
            char pingBuf[16];
            snprintf(pingBuf, sizeof(pingBuf), "%.0f ms", srv.averagePing);
            TextUnformatted(pingBuf);
            SetCursorPosY(cell4_start_y + row_interaction_height);

            TableNextColumn();
            float cell5_start_y = GetCursorPosY();
            SetCursorPosY(cell5_start_y + vertical_padding);
            char fpsBuf[16];
            snprintf(fpsBuf, sizeof(fpsBuf), "%.0f", srv.averageFps);
            TextUnformatted(fpsBuf);
            SetCursorPosY(cell5_start_y + row_interaction_height);

            PopID();
        }
        EndTable();
    }
}
