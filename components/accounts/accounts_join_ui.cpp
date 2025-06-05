#include "accounts_join_ui.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <stdexcept>

#include "../data.h"
#include "../../ui.h"
#include "../../utils/launcher.hpp"
#include "../../utils/status.h"
#include "../../utils/logging.hpp"
#include "../../utils/modal_popup.h"

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
            if (g_selectedAccountIds.empty()) {
                    ModalPopup::Add("Select an account first.");
            } else {
                    for (int id: g_selectedAccountIds) {
			auto it = find_if(g_accounts.begin(), g_accounts.end(),
			                  [id](auto &a) {
				                  return a.id == id;
			                  });
			if (it == g_accounts.end())
				continue;

			uint64_t placeId_val = 0;
			string jobId_str;
			try {
				if (join_type_combo_index == 0) {
					placeId_val = stoull(join_value_buf);
					LOG_INFO(
						"Attempting to join Place ID: " + std::string(join_value_buf) + " for account ID: " + std::
						to_string(it->id));
				} else if (join_type_combo_index == 1) {
					placeId_val = stoull(join_value_buf);
					jobId_str = join_jobid_buf;
					LOG_INFO(
						"Attempting to join Place ID: " + std::string(join_value_buf) + " with Job ID: " + jobId_str +
						" for account ID: " + std::to_string(it->id));
				} else {
					printf("[Error] Join type %d not supported for direct launch\n", join_type_combo_index);
					LOG_ERROR(
						"Join type " + std::to_string(join_type_combo_index) +
						" not supported for direct launch for account ID: " + std::to_string(it->id));
                                        Status::Error("Error: Join type not supported for direct launch");
					continue;
				}
			} catch (const invalid_argument &ia) {
				printf("[Error] Invalid numeric input: '%s' or '%s'. Details: %s\n", join_value_buf, join_jobid_buf,
				       ia.what());
				LOG_ERROR(
					"Invalid numeric input for join: PlaceID='" + std::string(join_value_buf) + "', JobID='" + std::
					string(join_jobid_buf) + "'. Details: " + ia.what());
                                Status::Error("Error: Invalid input for Place ID");
				continue;
			}
			catch (const out_of_range &oor) {
				printf("[Error] Numeric input out of range: '%s' or '%s'. Details: %s\n", join_value_buf,
				       join_jobid_buf, oor.what());
				LOG_ERROR(
					"Numeric input out of range for join: PlaceID='" + std::string(join_value_buf) + "', JobID='" + std
					::string(join_jobid_buf) + "'. Details: " + oor.what());
                                Status::Error("Error: Input number too large for Place ID");
				continue;
			}

			thread([placeId_val, jobId_str, cookie = it->cookie, account_id = it->id]() {
						Status::Set("Attempting to launch Roblox...");
						LOG_INFO(
							"Launching Roblox for account ID: " + std::to_string(account_id) + " PlaceID: " + std::
							to_string
							(placeId_val) + (jobId_str.empty() ? "" : " JobID: " + jobId_str));

						HANDLE proc = startRoblox(placeId_val, jobId_str, cookie);
						if (proc) {
							WaitForInputIdle(proc, INFINITE);
							CloseHandle(proc);
							Status::Set("Roblox launched (process ended or became idle).");
							LOG_INFO(
								"Roblox launched successfully for account ID: " + std::to_string(account_id) +
								" PlaceID: "
								+ std::to_string(placeId_val) + (jobId_str.empty() ? "" : " JobID: " + jobId_str));
						} else {
                                                        Status::Error("Failed to start Roblox.");
							LOG_ERROR(
								"Failed to start Roblox for account ID: " + std::to_string(account_id) + " PlaceID: " +
								std
								::to_string(placeId_val) + (jobId_str.empty() ? "" : " JobID: " + jobId_str));
						}
					})
					.detach();
		}
	}
}

