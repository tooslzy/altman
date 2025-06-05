#include "accounts_join_ui.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <stdexcept>
#include <utility>

#include "../data.h"
#include "../../ui.h"
#include "../../utils/launcher.hpp"
#include "../../utils/status.h"
#include "../../utils/logging.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace ImGui;
using namespace std;

static const char *join_types_local[] = {
	"Place ID",
	"PlaceId + JobId",
};

static const char *GetJoinHintLocal(int idx) {
	switch (idx) {
		case 0:
			return "Enter Place ID...";
		case 2:
			return "Enter Private Server Link...";
		case 3:
			return "Enter Username...";
		default:
			return "";
	}
}

void RenderJoinOptions() {
	Spacing();
	Text("Join Options");
	Spacing();
	Combo(" Join Type", &join_type_combo_index, join_types_local, IM_ARRAYSIZE(join_types_local));
	if (join_type_combo_index == 1) {
		InputTextWithHint("##JoinPlaceId", "Enter Place ID...", join_value_buf, IM_ARRAYSIZE(join_value_buf));
		InputTextWithHint("##JoinJobId", "Enter Job ID...", join_jobid_buf, IM_ARRAYSIZE(join_jobid_buf));
	} else {
		InputTextWithHint("##JoinValue", GetJoinHintLocal(join_type_combo_index), join_value_buf,
		                  IM_ARRAYSIZE(join_value_buf));
	}

        Separator();
        if (Button(" \xEF\x8B\xB6  Join ")) {
                uint64_t placeId_val = 0;
                string jobId_str;
                try {
                        if (join_type_combo_index == 0) {
                                placeId_val = stoull(join_value_buf);
                        } else if (join_type_combo_index == 1) {
                                placeId_val = stoull(join_value_buf);
                                jobId_str = join_jobid_buf;
                        } else {
                                Status::Set("Error: Join type not supported for direct launch");
                                return;
                        }
                } catch (const invalid_argument &ia) {
                        Status::Set("Error: Invalid input for Place ID");
                        LOG_ERROR("Invalid numeric input for join: " + string(ia.what()));
                        return;
                } catch (const out_of_range &oor) {
                        Status::Set("Error: Input number too large for Place ID");
                        LOG_ERROR("Numeric input out of range for join: " + string(oor.what()));
                        return;
                }

                vector<pair<int, string>> accounts;
                for (int id: g_selectedAccountIds) {
                        auto it = find_if(g_accounts.begin(), g_accounts.end(), [id](auto &a) { return a.id == id; });
                        if (it != g_accounts.end())
                                accounts.emplace_back(it->id, it->cookie);
                }
                if (accounts.empty())
                        return;

                thread([placeId_val, jobId_str, accounts]() {
                        Status::Set("Attempting to launch Roblox...");
                        launchRobloxSequential(placeId_val, jobId_str, accounts);
                        Status::Set("Roblox launched (process ended or became idle).");
                }).detach();
        }
}
