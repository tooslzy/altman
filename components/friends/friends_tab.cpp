#include "friends.h"
#include <imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>


#include "../data.h"
#include "../../utils/roblox_api.h"
#include "../../utils/launcher.hpp"
#include "../../utils/threading.h"
#include "./friends_actions.h"
#include "../utils/webview.hpp"
#include "../games/games_utils.h"

using namespace ImGui;
using namespace std;

static int g_selectedFriendIdx = -1;
static RobloxApi::FriendDetail g_selectedFriend;
static atomic<bool> g_friendDetailsLoading{false};
static atomic<bool> g_friendsLoading{false};

static int g_lastAcctIdForFriends = -1;

static auto ICON_TOOL = "\xEF\x82\xAD ";
static auto ICON_PERSON = "\xEF\x80\x87 ";
static auto ICON_CONTROLLER = "\xEF\x84\x9B ";
static auto ICON_REFRESH = "\xEF\x8B\xB1 ";
static auto ICON_OPEN_LINK = "\xEF\x8A\xBB ";
static auto ICON_INVENTORY = "\xEF\x8A\x90 ";
static auto ICON_JOIN = "\xEF\x8B\xB6 ";

static const char *presenceIcon(const string &p) {
    if (p == "InStudio")
        return ICON_TOOL;
    if (p == "InGame")
        return ICON_CONTROLLER;
    if (p == "Online")
        return ICON_PERSON;
    return "";
}


void RenderFriendsTab() {
    if (g_selectedAccountIds.empty()) {
        TextDisabled("Select an account in the Accounts tab to view its friends.");
        return;
    }
    int currentAcctId = *g_selectedAccountIds.begin();
    auto it = find_if(g_accounts.begin(), g_accounts.end(),
                      [&](auto &a) {
                          return a.id == currentAcctId;
                      });
    if (it == g_accounts.end()) {
        TextDisabled("Selected account not found.");
        return;
    }
    const AccountData &acct = *it;

    if (currentAcctId != g_lastAcctIdForFriends) {
        g_friends.clear();
        g_selectedFriendIdx = -1;
        g_selectedFriend = {};
        g_friendsLoading = false;
        g_friendDetailsLoading = false;
        g_lastAcctIdForFriends = currentAcctId;

        if (!acct.userId.empty()) {
            Threading::newThread(FriendsActions::RefreshFullFriendsList, acct.userId, acct.cookie, ref(g_friends),
                                 ref(g_friendsLoading));
        }
    }

    BeginDisabled(g_friendsLoading.load());
    if (Button((string(ICON_REFRESH) + "Refresh").c_str()) && !acct.userId.empty()) {
        g_selectedFriendIdx = -1;
        g_selectedFriend = {};
        Threading::newThread(FriendsActions::RefreshFullFriendsList, acct.userId, acct.cookie, ref(g_friends),
                             ref(g_friendsLoading));
    }
    EndDisabled();

    float friendsListWidth = 300.0f;

    BeginChild("##FriendsList", ImVec2(friendsListWidth, 0), true);
    if (g_friendsLoading.load() && g_friends.empty()) {
        Text("Loading friends...");
    } else {
        for (size_t i = 0; i < g_friends.size(); ++i) {
            const auto &f = g_friends[i];
            PushID(static_cast<int>(i));

            string label;
            const char *icon = presenceIcon(f.presence);
            if (*icon) label += icon;
            label += (f.displayName == f.username || f.displayName.empty())
                         ? f.username
                         : (f.displayName + " (" + f.username + ")");

            ImVec4 txtCol = getStatusColor(f.presence);
            PushStyleColor(ImGuiCol_Text, txtCol);

            bool clicked = Selectable(label.c_str(),
                                      g_selectedFriendIdx == static_cast<int>(i),
                                      ImGuiSelectableFlags_SpanAllColumns);

            PopStyleColor();
            if (f.presence == "InGame" && !f.lastLocation.empty()) {
                const float indent = GetStyle().FramePadding.x * 4.0f;
                Indent(indent);
                ImVec4 gameCol = txtCol;
                gameCol.x *= 0.75f;
                gameCol.y *= 0.75f;
                gameCol.z *= 0.75f;
                gameCol.w *= 0.65f;

                PushStyleColor(ImGuiCol_Text, gameCol);
                TextUnformatted(string("\xEF\x83\x9A  " + f.lastLocation).c_str());
                PopStyleColor();

                Unindent(indent);
            }

            if (clicked) {
                g_selectedFriendIdx = static_cast<int>(i);
                if (g_selectedFriend.id != f.id) {
                    g_selectedFriend = {};
                    Threading::newThread(FriendsActions::FetchFriendDetails,
                                         to_string(f.id),
                                         acct.cookie,
                                         ref(g_selectedFriend),
                                         ref(g_friendDetailsLoading));
                }
            }
            PopID();
        }
    }
    EndChild();

    SameLine();

    float desiredTextIndent = 8.0f;
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    BeginChild("##FriendDetails", ImVec2(0, 0), true);
    PopStyleVar();

    if (g_selectedFriendIdx < 0 || g_selectedFriendIdx >= static_cast<int>(g_friends.size())) {
        Indent(desiredTextIndent);
        TextWrapped("Click a friend to see more details or take action.");
        Unindent(desiredTextIndent);
    } else if (g_friendDetailsLoading.load()) {
        Indent(desiredTextIndent);
        Text("Loading full details...");
        Unindent(desiredTextIndent);
    } else {
        const auto &D = g_selectedFriend;
        if (D.id == 0 && g_friends[g_selectedFriendIdx].id != 0) {
            Indent(desiredTextIndent);
            Text("Fetching details for %s...", g_friends[g_selectedFriendIdx].username.c_str());

            Unindent(desiredTextIndent);
        } else if (D.id == 0) {
            Indent(desiredTextIndent);
            TextWrapped("Details not available or selection issue.");
            Unindent(desiredTextIndent);
        } else {
            ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_SizingFixedFit;
            PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 4.0f));
            if (BeginTable("FriendInfoTable", 2, tableFlags)) {
                TableSetupColumn("##friendlabel", ImGuiTableColumnFlags_WidthFixed, 120.f);
                TableSetupColumn("##friendvalue", ImGuiTableColumnFlags_WidthStretch);

                auto addFriendDataRow = [&](const char *label, const string &value, bool isWrapped = false) {
                    TableNextRow();
                    TableSetColumnIndex(0);
                    Indent(desiredTextIndent);
                    Spacing();
                    TextUnformatted(label);
                    Spacing();
                    Unindent(desiredTextIndent);

                    TableSetColumnIndex(1);
                    Indent(desiredTextIndent);
                    Spacing();
                    PushID(label);
                    if (isWrapped) {
                        TextWrapped("%s", value.c_str());
                    } else {
                        TextUnformatted(value.c_str());
                    }
                    if (BeginPopupContextItem("CopyFriendValue")) {
                        if (MenuItem("Copy")) {
                            SetClipboardText(value.c_str());
                        }
                        EndPopup();
                    }
                    PopID();
                    Spacing();
                    Unindent(desiredTextIndent);
                };

                auto addFriendDataRowInt = [&](const char *label, int value) {
                    addFriendDataRow(label, to_string(value));
                };

                addFriendDataRow("User ID:", to_string(D.id));
                addFriendDataRow("Username:", D.username);
                addFriendDataRow("Display Name:", D.displayName.empty() ? D.username : D.displayName);
                addFriendDataRow("Created:", formatPrettyDate(D.createdIso));
                addFriendDataRowInt("Followers:", D.followers);
                addFriendDataRowInt("Following:", D.following);

                TableNextRow();
                TableSetColumnIndex(0);
                Indent(desiredTextIndent);
                Spacing();
                TextUnformatted("Description:");
                Spacing();
                Unindent(desiredTextIndent);

                TableSetColumnIndex(1);
                Indent(desiredTextIndent);
                Spacing();

                ImGuiStyle &style = GetStyle();

                float spaceForBottomSpacingInCell = style.ItemSpacing.y;

                float spaceForSeparator = style.ItemSpacing.y;

                float spaceForButtons = GetFrameHeightWithSpacing();

                float reservedHeightBelowDescContent = spaceForBottomSpacingInCell + spaceForSeparator +
                                                       spaceForButtons;

                float availableHeightForDescAndBelow = GetContentRegionAvail().y;

                float descChildHeight = availableHeightForDescAndBelow - reservedHeightBelowDescContent;

                float minDescHeight = GetTextLineHeightWithSpacing() * 3.0f;
                if (descChildHeight < minDescHeight) {
                    descChildHeight = minDescHeight;
                }

                const string descStr = D.description.empty() ? "(No description)" : D.description;
                PushID("FriendDesc");
                BeginChild("##FriendDescScroll", ImVec2(0, descChildHeight - 4), false,
                           ImGuiWindowFlags_HorizontalScrollbar);
                TextWrapped("%s", descStr.c_str());
                if (BeginPopupContextItem("CopyFriendDesc")) {
                    if (MenuItem("Copy")) {
                        SetClipboardText(descStr.c_str());
                    }
                    EndPopup();
                }
                EndChild();
                PopID();

                Spacing();
                Unindent(desiredTextIndent);

                EndTable();
            }
            PopStyleVar();

            Separator();

            Indent(desiredTextIndent / 2);
            const FriendInfo &row = g_friends[g_selectedFriendIdx];

            bool canJoin = (row.presence == "InGame" && row.placeId && !row.gameId.empty());
            BeginDisabled(!canJoin);
            if (Button((string(ICON_JOIN) + " Join Game").c_str()) && canJoin) {
                startRoblox(row.placeId, row.gameId, acct.cookie);
            }

            EndDisabled();
            SameLine();
            if (Button((string(ICON_OPEN_LINK) + " Open Profile").c_str())) {
                if (D.id)
                    LaunchWebview(
                        "https://www.roblox.com/users/" + to_string(D.id) + "/profile",
                        "Roblox Profile", acct.cookie);
            }
            SameLine();
            if (Button((string(ICON_INVENTORY) + " Open Inventory").c_str())) {
                if (D.id)
                    LaunchWebview(
                        "https://www.roblox.com/users/" + to_string(D.id) + "/inventory/#!/accessories",
                        "Roblox Inventory", acct.cookie);
            }
            Unindent(desiredTextIndent / 2);
        }
    }

    EndChild();
}
