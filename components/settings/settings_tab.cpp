#include "settings.h"
#include <imgui.h>
#include <vector>
#include <string>

#include "../components.h"
#include "../data.h"

using namespace ImGui;
using namespace std;

void RenderSettingsTab()
{
	if (!g_accounts.empty())
	{
		Text("Default Account:");

		vector<const char*> names;
		names.reserve(g_accounts.size());
		int current_default_idx = -1;

		for (size_t i = 0; i < g_accounts.size(); ++i)
		{
			names.push_back(g_accounts[i].displayName.c_str());
			if (g_accounts[i].id == g_defaultAccountId)
			{
				current_default_idx = static_cast<int>(i);
			}
		}

		if (current_default_idx == -1 && !g_accounts.empty())
		{
		}

		int combo_idx = current_default_idx;
		if (Combo("##defaultAccountCombo", &combo_idx, names.data(), static_cast<int>(names.size())))
		{
			if (combo_idx >= 0 && combo_idx < static_cast<int>(g_accounts.size()))
			{
				g_defaultAccountId = g_accounts[combo_idx].id;

				g_selectedAccountIds.clear();
				g_selectedAccountIds.insert(g_defaultAccountId);

				Data::SaveSettings("settings.json");
			}
		}
	}
	else
	{
		TextDisabled("No accounts available to set a default.");
	}
}
