#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace INTERNAL_STATE_DEBUG {
    namespace {
        constexpr const char* k_leader_types[] = {"Ship", "Colony"};
        constexpr const char* k_leader_statuses[] = {
            "Dead", "Unowned", "Pool", "Assigned", "Limbo", "Owned (3)",
            "For hire", "Marooned", "Special"};
        constexpr const char* k_event_statuses[] = {
            "Inactive", "Triggered new", "Active", "Expired", "Triggered",
            "Finishing", "Just started"};
        constexpr const char* k_event_names[EVENT_COUNT] = {
            "Derelict",
            "Climatic change",
            "Comet",
            "Computer virus",
            "Diplomatic blunder",
            "Diplomatic marriage",
            "Donation",
            "Earthquake",
            "Freak accident",
            "Hyperspace flux",
            "Industrial accident",
            "Mineral depletion",
            "Mineral deposit",
            "Mutiny",
            "Pirate activity",
            "Pirate raid",
            "Plague",
            "Population boom",
            "Serendipity",
            "Space amoeba",
            "Space crystal",
            "Space dragon",
            "Space eel",
            "Space hydra",
            "Supernova",
            "Time-space anomaly",
            "Warp beast",
            "Warp funnel",
            "Unstable wormhole",
            "Empire destroyed",
            "Empire grown in strength",
            "Empires ranking",
            "Orion visited",
            "Antarans defeated",
            "Empire surrenders",
            "Rebel assimilation"};

        enum e_event_target_kind {
            EVENT_TARGET_RAW,
            EVENT_TARGET_PLAYER,
            EVENT_TARGET_STAR,
            EVENT_TARGET_PLANET,
            EVENT_TARGET_COLONY,
            EVENT_TARGET_SHIP,
            EVENT_TARGET_TECH,
        };

        int16_t g_selected_leader = -1;
        int16_t g_selected_event = -1;
        s_leader_data g_leader_baseline = {};
        s_leader_data g_leader_draft = {};
        s_event_data g_event_baseline = {};
        s_event_data g_event_draft = {};
        int8_t g_leader_player_filter = -2;
        int8_t g_event_player_filter = -2;
        int8_t g_transfer_player = -1;
        int16_t g_target_star = -1;
        int16_t g_target_colony = -1;
        int16_t g_target_ship = -1;
        int8_t g_assignment_eta = 0;
        int8_t g_new_assignment_eta = 0;
        int16_t g_xp_amount = 100;
        bool g_transfer_requested = false;
        bool g_kill_requested = false;
        const char* g_leader_status = nullptr;
        const char* g_event_status = nullptr;

        bool Network_Game_() {
            return MOX::game_type == 2 || MOX::game_type == 3 ||
                NETMOX::net.is_network_active != 0;
        }

        bool Input_S8_(const char* label, int8_t* value) {
            const int8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S8, value, &step, nullptr, "%d");
        }

        bool Input_U8_(const char* label, uint8_t* value) {
            const uint8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U8, value, &step, nullptr, "%u");
        }

        bool Input_S16_(const char* label, int16_t* value) {
            const int16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S16, value, &step, nullptr, "%d");
        }

        bool Input_U16_(const char* label, uint16_t* value) {
            const uint16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U16, value, &step, nullptr, "%u");
        }

        bool Input_U32_(const char* label, uint32_t* value) {
            const uint32_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U32, value, &step, nullptr, "%u");
        }

        const char* Leader_Status_Name_(int8_t status) {
            const int16_t index = (int16_t)status + 2;
            return index >= 0 && index < (int16_t)(sizeof(k_leader_statuses) / sizeof(k_leader_statuses[0]))
                ? k_leader_statuses[index]
                : "Unknown";
        }

        const char* Player_Name_(int16_t player) {
            static thread_local char name[sizeof(MOX::player[0].race_name) + 1];
            const int16_t player_count = std::clamp<int16_t>(
                MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if (MOX::player == nullptr || player < 0 || player >= player_count) {
                return "None";
            }
            strlcpy(name, MOX::player[player].race_name, sizeof(name));
            return name;
        }

        const char* Leader_Name_(int16_t leader) {
            static thread_local char name[sizeof(MOX::leaders[0].name) + 1];
            if (MOX::leaders == nullptr || leader < 0 || leader >= MAX_LEADERS) {
                return "leader";
            }
            strlcpy(name, MOX::leaders[leader].name, sizeof(name));
            return name;
        }

        bool Valid_Leader_(int16_t leader) {
            return MOX::leaders != nullptr && leader >= 0 && leader < MAX_LEADERS;
        }

        bool Valid_Player_(int16_t player) {
            return MOX::player != nullptr && player >= 0 &&
                player < std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        }

        bool Valid_Star_(int16_t star) {
            return MOX::star != nullptr && star >= 0 &&
                star < std::clamp<int16_t>(MOX::NUM_STARS, 0, MAX_STARS);
        }

        bool Valid_Colony_(int16_t colony) {
            return MOX::colony != nullptr && colony >= 0 &&
                colony < std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES);
        }

        bool Valid_Ship_(int16_t ship) {
            return MOX::ship != nullptr && ship >= 0 &&
                ship < std::clamp<int16_t>(MOX::NUM_SHIPS, 0, MAX_SHIPS);
        }

        void Request_Redraw_() {
            SYS::Clear_System_Display_Loaded_Flag();
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            MOX::draw_main_screen_to_back = 1;
            video::Set_Refresh_Stencil();
        }

        void Refresh_Leader_State_() {
            COLCALC::Compute_All_Star_Colony_Stuff();
            COLCALC::All_Colony_Calculations();
            SHIPMOVE::Recalculate_All_Moving_Fleets();
            SHIPS::Build_Ship_Icons();
            Request_Redraw_();
        }

        void Load_Leader_(int16_t leader) {
            if (!Valid_Leader_(leader)) {
                g_selected_leader = -1;
                return;
            }
            g_selected_leader = leader;
            g_leader_baseline = MOX::leaders[leader];
            g_leader_draft = g_leader_baseline;
            g_transfer_player = g_leader_draft.player_index;
            g_assignment_eta = std::max<int8_t>(g_leader_draft.eta, 0);
            g_new_assignment_eta = g_assignment_eta;
            g_target_star = -1;
            g_target_colony = -1;
            g_target_ship = -1;
            g_leader_status = nullptr;
        }

        bool Leader_Metadata_Dirty_() {
            if (!Valid_Leader_(g_selected_leader)) {
                return false;
            }
            return memcmp(g_leader_draft.name, g_leader_baseline.name, sizeof(g_leader_draft.name)) != 0 ||
                memcmp(g_leader_draft.title, g_leader_baseline.title, sizeof(g_leader_draft.title)) != 0 ||
                g_leader_draft.type != g_leader_baseline.type ||
                g_leader_draft.xp != g_leader_baseline.xp ||
                g_leader_draft.general_skills != g_leader_baseline.general_skills ||
                g_leader_draft.special_skills != g_leader_baseline.special_skills ||
                memcmp(g_leader_draft.tech_application, g_leader_baseline.tech_application, sizeof(g_leader_draft.tech_application)) != 0 ||
                g_leader_draft.pict_num != g_leader_baseline.pict_num ||
                g_leader_draft.skill_value != g_leader_baseline.skill_value ||
                g_leader_draft.level != g_leader_baseline.level ||
                g_leader_draft.display_level_popup != g_leader_baseline.display_level_popup;
        }

        bool Leader_Metadata_Valid_() {
            if (g_leader_draft.name[sizeof(g_leader_draft.name) - 1] != '\0' ||
                g_leader_draft.title[sizeof(g_leader_draft.title) - 1] != '\0' ||
                g_leader_draft.type >= LEADER_TYPE_COUNT) {
                return false;
            }
            for (uint8_t application : g_leader_draft.tech_application) {
                if (application >= TECH_APP_COUNT) {
                    return false;
                }
            }
            if (g_leader_draft.level >= 6) {
                return false;
            }
            return g_leader_draft.type == g_leader_baseline.type ||
                g_leader_baseline.status != 1;
        }

        bool Leader_Metadata_Conflict_() {
            if (!Valid_Leader_(g_selected_leader)) {
                return true;
            }
            const s_leader_data& live = MOX::leaders[g_selected_leader];
#define LEADER_METADATA_CONFLICT(field) \
            (memcmp(&g_leader_draft.field, &g_leader_baseline.field, sizeof(g_leader_draft.field)) != 0 && \
                memcmp(&live.field, &g_leader_baseline.field, sizeof(live.field)) != 0 && \
                memcmp(&live.field, &g_leader_draft.field, sizeof(live.field)) != 0)
            return LEADER_METADATA_CONFLICT(name) ||
                LEADER_METADATA_CONFLICT(title) ||
                LEADER_METADATA_CONFLICT(type) ||
                LEADER_METADATA_CONFLICT(xp) ||
                LEADER_METADATA_CONFLICT(general_skills) ||
                LEADER_METADATA_CONFLICT(special_skills) ||
                LEADER_METADATA_CONFLICT(tech_application) ||
                LEADER_METADATA_CONFLICT(pict_num) ||
                LEADER_METADATA_CONFLICT(skill_value) ||
                LEADER_METADATA_CONFLICT(level) ||
                LEADER_METADATA_CONFLICT(display_level_popup);
#undef LEADER_METADATA_CONFLICT
        }

        void Save_Leader_Metadata_() {
            s_leader_data& live = MOX::leaders[g_selected_leader];
#define SAVE_LEADER_METADATA(field) \
            if (memcmp(&g_leader_draft.field, &g_leader_baseline.field, sizeof(g_leader_draft.field)) != 0) { \
                memcpy(&live.field, &g_leader_draft.field, sizeof(live.field)); \
            }
            SAVE_LEADER_METADATA(name);
            SAVE_LEADER_METADATA(title);
            SAVE_LEADER_METADATA(type);
            SAVE_LEADER_METADATA(xp);
            SAVE_LEADER_METADATA(general_skills);
            SAVE_LEADER_METADATA(special_skills);
            SAVE_LEADER_METADATA(tech_application);
            SAVE_LEADER_METADATA(pict_num);
            SAVE_LEADER_METADATA(skill_value);
            SAVE_LEADER_METADATA(level);
            SAVE_LEADER_METADATA(display_level_popup);
#undef SAVE_LEADER_METADATA
            Refresh_Leader_State_();
            Load_Leader_(g_selected_leader);
            g_leader_status = "Leader metadata saved.";
        }

        void Clear_Hire_References_(int16_t leader) {
            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            for (int16_t player = 0; player < player_count; ++player) {
                if (MOX::player[player].officer_for_hire == leader) {
                    MOX::player[player].officer_for_hire = -1;
                }
            }
        }

        bool Star_Has_Player_Colony_(int16_t star, int8_t player) {
            if (!Valid_Star_(star) || !Valid_Player_(player)) {
                return false;
            }
            for (int16_t orbit = 0; orbit < 5; ++orbit) {
                const int16_t planet = MOX::star[star].planet_index[orbit];
                const int16_t planet_count = std::clamp<int16_t>(
                    MOX::planet_count, 0, MAX_PLANETS);
                if (planet < 0 || planet >= planet_count) {
                    continue;
                }
                const int16_t colony = MOX::planet[planet].colony_index;
                if (Valid_Colony_(colony) && MOX::colony[colony].owner == player) {
                    return true;
                }
            }
            return false;
        }

        int16_t Colony_Star_(int16_t colony) {
            if (!Valid_Colony_(colony)) {
                return -1;
            }
            const int16_t planet = MOX::colony[colony].planet;
            const int16_t planet_count = std::clamp<int16_t>(
                MOX::planet_count, 0, MAX_PLANETS);
            return planet >= 0 && planet < planet_count
                ? MOX::planet[planet].star_index
                : -1;
        }

        bool Player_Has_Leader_Slot_(int16_t player, int8_t type) {
            if (!Valid_Player_(player) || type < 0 || type >= LEADER_TYPE_COUNT) {
                return false;
            }
            if (OFFICER::leaders_no_limit != 0) {
                return true;
            }
            int16_t count = 0;
            for (int16_t leader = 0; leader < MAX_LEADERS; ++leader) {
                const s_leader_data& data = MOX::leaders[leader];
                if (data.player_index == player && data.type == type &&
                    data.status >= 0 && data.status <= 4) {
                    ++count;
                }
            }
            return count < 4;
        }

        bool Leader_Relationship_Consistent_(const s_leader_data& leader) {
            if (leader.status != 1) {
                return leader.location == -1;
            }
            if (!Valid_Player_(leader.player_index)) {
                return false;
            }
            const int16_t leader_index = (int16_t)(&leader - MOX::leaders);
            if (leader.type == LEADER_TYPE_COLONY) {
                return Valid_Star_(leader.location) &&
                    MOX::star[leader.location].officer_index[leader.player_index] ==
                        leader_index;
            }
            return leader.type == LEADER_TYPE_SHIP && Valid_Ship_(leader.location) &&
                MOX::ship[leader.location].officer_index == leader_index;
        }

        void Assign_Leader_To_Star_(int16_t star) {
            s_leader_data& leader = MOX::leaders[g_selected_leader];
            OFFICER::Assign_Leader_To_Star(
                &MOX::player[leader.player_index],
                &leader,
                &MOX::star[star],
                g_assignment_eta);
            Refresh_Leader_State_();
            Load_Leader_(g_selected_leader);
            g_leader_status = "Leader assigned to star.";
        }

        void Assign_Leader_To_Ship_() {
            s_leader_data& leader = MOX::leaders[g_selected_leader];
            OFFICER::Assign_Leader_To_Ship(
                &MOX::player[leader.player_index],
                &leader,
                &MOX::ship[g_target_ship],
                g_assignment_eta);
            Refresh_Leader_State_();
            Load_Leader_(g_selected_leader);
            g_leader_status = "Leader assigned to ship.";
        }

        void Transfer_Leader_() {
            s_leader_data& leader = MOX::leaders[g_selected_leader];
            if (leader.status == 1) {
                OFFICER::Move_Officer_To_Pool(g_selected_leader);
            } else if (leader.status == 2) {
                leader.status = 0;
                leader.eta = 0;
                leader.location = -1;
            }
            Clear_Hire_References_(g_selected_leader);
            OFFICER::Set_Officer_To_Player(g_selected_leader, g_transfer_player, 1);
            Refresh_Leader_State_();
            Load_Leader_(g_selected_leader);
            g_leader_status = "Leader moved to another player.";
        }

        void Draw_Player_Filter_(const char* label, int8_t* value) {
            const char* preview = *value == -2 ? "All" : (*value == -1 ? "Unowned / global" : Player_Name_(*value));
            if (ImGui::BeginCombo(label, preview)) {
                if (ImGui::Selectable("All", *value == -2)) {
                    *value = -2;
                }
                if (ImGui::Selectable("Unowned / global", *value == -1)) {
                    *value = -1;
                }
                const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
                for (int16_t player = 0; player < player_count; ++player) {
                    if (ImGui::Selectable(Player_Name_(player), *value == player)) {
                        *value = (int8_t)player;
                    }
                }
                ImGui::EndCombo();
            }
        }

        void Draw_Leader_List_() {
            static ImGuiTextFilter filter;
            Draw_Player_Filter_("Player filter", &g_leader_player_filter);
            filter.Draw("Filter leaders", -1.0f);
            const float list_width = ImGui::GetContentRegionAvail().x * 0.4f;
            if (ImGui::BeginChild("leader_list", ImVec2(list_width, 0.0f), true)) {
                if (MOX::leaders == nullptr) {
                    ImGui::TextUnformatted("No leader data is loaded.");
                } else {
                    for (int16_t leader = 0; leader < MAX_LEADERS; ++leader) {
                        const s_leader_data& data = MOX::leaders[leader];
                        if (g_leader_player_filter != -2 && data.player_index != g_leader_player_filter) {
                            continue;
                        }
                        char label[96];
                        snprintf(
                            label,
                            sizeof(label),
                            "%s [%d] - %s, %s",
                            Leader_Name_(leader),
                            (int)leader,
                            data.type == LEADER_TYPE_SHIP ? "ship" : "colony",
                            Leader_Status_Name_(data.status));
                        if (!filter.PassFilter(label)) {
                            continue;
                        }
                        if (ImGui::Selectable(label, g_selected_leader == leader)) {
                            Load_Leader_(leader);
                        }
                    }
                }
            }
            ImGui::EndChild();
        }

        void Draw_Leader_Relationships_() {
            const s_leader_data& live = MOX::leaders[g_selected_leader];
            ImGui::Text("Player: %s (%d)", Player_Name_(live.player_index), (int)live.player_index);
            ImGui::Text("Status: %s (%d)", Leader_Status_Name_(live.status), (int)live.status);
            ImGui::Text("Location: %d", (int)live.location);
            ImGui::Text("Assignment ETA: %d", (int)live.eta);
            ImGui::Text("Calculated level: %d", (int)OFFICER::Get_Officer_Level(g_selected_leader));
            if (Valid_Player_(live.player_index)) {
                ImGui::Text(
                    "Cost: %d | Maintenance: %d",
                    (int)OFFICER::Officer_Cost(live.player_index, g_selected_leader),
                    (int)OFFICER::Officer_Maintenance(live.player_index, g_selected_leader));
            }
        }

        void Draw_Leader_Assignment_() {
            s_leader_data& live = MOX::leaders[g_selected_leader];
            const bool blocked = Network_Game_() || Leader_Metadata_Dirty_();
            const bool can_assign = Valid_Player_(live.player_index) &&
                (live.status == 0 || live.status == 1 || live.status == 2) &&
                Leader_Relationship_Consistent_(live);
            Input_S8_("ETA for new assignment", &g_assignment_eta);
            if (live.type == LEADER_TYPE_COLONY) {
                debug_game_widgets::Star_Reference("Target star", &g_target_star, false);
                const bool star_valid = Star_Has_Player_Colony_(g_target_star, live.player_index);
                ImGui::BeginDisabled(blocked || !can_assign || !star_valid || g_assignment_eta < 0);
                if (ImGui::Button("Assign to star")) {
                    Assign_Leader_To_Star_(g_target_star);
                }
                ImGui::EndDisabled();

                debug_game_widgets::Colony_Reference("Target colony", &g_target_colony, false);
                const int16_t colony_star = Colony_Star_(g_target_colony);
                const bool colony_valid = Valid_Colony_(g_target_colony) &&
                    MOX::colony[g_target_colony].owner == live.player_index &&
                    Star_Has_Player_Colony_(colony_star, live.player_index);
                ImGui::BeginDisabled(blocked || !can_assign || !colony_valid || g_assignment_eta < 0);
                if (ImGui::Button("Assign to colony's star")) {
                    Assign_Leader_To_Star_(colony_star);
                }
                ImGui::EndDisabled();
            } else {
                Input_S16_("Target ship", &g_target_ship);
                const bool ship_valid = Valid_Ship_(g_target_ship) &&
                    MOX::ship[g_target_ship].owner == live.player_index &&
                    MOX::ship[g_target_ship].status < 5;
                ImGui::BeginDisabled(blocked || !can_assign || !ship_valid || g_assignment_eta < 0);
                if (ImGui::Button("Assign to ship")) {
                    Assign_Leader_To_Ship_();
                }
                ImGui::EndDisabled();
            }

            if (live.status == 1) {
                Input_S8_("New current ETA", &g_new_assignment_eta);
                ImGui::BeginDisabled(
                    blocked || !Leader_Relationship_Consistent_(live) ||
                    g_new_assignment_eta < 0);
                if (ImGui::Button("Update assignment ETA")) {
                    live.eta = g_new_assignment_eta;
                    Refresh_Leader_State_();
                    Load_Leader_(g_selected_leader);
                    g_leader_status = "Assignment ETA updated.";
                }
                ImGui::EndDisabled();
            }

            ImGui::BeginDisabled(
                blocked || !Valid_Player_(live.player_index) || live.status == -2 ||
                !Leader_Relationship_Consistent_(live));
            if (ImGui::Button("Move to player's pool")) {
                Clear_Hire_References_(g_selected_leader);
                OFFICER::Move_Officer_To_Pool(g_selected_leader);
                Refresh_Leader_State_();
                Load_Leader_(g_selected_leader);
                g_leader_status = "Leader moved to the officer pool.";
            }
            ImGui::EndDisabled();
            if (!Leader_Relationship_Consistent_(live)) {
                ImGui::TextUnformatted("Assignment reciprocal is inconsistent; repair it through the related star or ship editor first.");
            }
        }

        void Draw_Leader_Ownership_() {
            s_leader_data& live = MOX::leaders[g_selected_leader];
            debug_game_widgets::Player_Reference("Move to player", &g_transfer_player, false);
            const bool transferable_status = live.status >= -1 && live.status <= 4;
            const bool can_transfer = Valid_Player_(g_transfer_player) &&
                g_transfer_player != live.player_index && transferable_status &&
                Player_Has_Leader_Slot_(g_transfer_player, live.type);
            ImGui::BeginDisabled(
                Network_Game_() || Leader_Metadata_Dirty_() || !can_transfer);
            if (ImGui::Button("Move leader to player")) {
                g_transfer_requested = true;
            }
            ImGui::EndDisabled();

            Input_S16_("XP to grant", &g_xp_amount);
            const int32_t xp_room = std::max<int32_t>(0, 30000 - live.xp);
            ImGui::BeginDisabled(
                Network_Game_() || Leader_Metadata_Dirty_() ||
                !Valid_Player_(live.player_index) || g_xp_amount <= 0 ||
                g_xp_amount > xp_room || live.status == -2);
            if (ImGui::Button("Grant XP")) {
                OFFICER::Give_Officer_XP(
                    live.player_index, g_selected_leader, g_xp_amount);
                Refresh_Leader_State_();
                Load_Leader_(g_selected_leader);
                g_leader_status = "Leader XP granted.";
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(
                Network_Game_() || Leader_Metadata_Dirty_() || live.status == -2 ||
                !Leader_Relationship_Consistent_(live));
            if (ImGui::Button("Kill leader")) {
                g_kill_requested = true;
            }
            ImGui::EndDisabled();
        }

        void Draw_Leader_Editor_() {
            if (!Valid_Leader_(g_selected_leader)) {
                ImGui::TextUnformatted("Select a leader to inspect or edit.");
                return;
            }
            Draw_Leader_Relationships_();
            ImGui::SeparatorText("Metadata");
            ImGui::InputText("Name", g_leader_draft.name, sizeof(g_leader_draft.name));
            ImGui::InputText("Title", g_leader_draft.title, sizeof(g_leader_draft.title));
            int8_t leader_type = (int8_t)g_leader_draft.type;
            if (debug_game_widgets::Enum(
                    "Type", &leader_type, k_leader_types, LEADER_TYPE_COUNT)) {
                g_leader_draft.type = (uint8_t)leader_type;
            }
            Input_S16_("Experience", &g_leader_draft.xp);
            Input_U32_("General skill flags", &g_leader_draft.general_skills);
            Input_U32_("Type-specific skill flags", &g_leader_draft.special_skills);
            for (int32_t index = 0; index < 3; ++index) {
                ImGui::PushID(index);
                Input_U8_("Granted technology", &g_leader_draft.tech_application[index]);
                ImGui::PopID();
            }
            Input_U8_("Picture", &g_leader_draft.pict_num);
            Input_S16_("Stored skill value", &g_leader_draft.skill_value);
            Input_U8_("Stored level", &g_leader_draft.level);
            int8_t display_level_popup = (int8_t)g_leader_draft.display_level_popup;
            if (Input_S8_("Display level popup", &display_level_popup)) {
                g_leader_draft.display_level_popup = (char)display_level_popup;
            }

            const bool dirty = Leader_Metadata_Dirty_();
            const bool valid = Leader_Metadata_Valid_();
            const bool conflict = Leader_Metadata_Conflict_();
            ImGui::BeginDisabled(!dirty || !valid || conflict || Network_Game_());
            if (ImGui::Button("Save metadata")) {
                Save_Leader_Metadata_();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Reload live")) {
                Load_Leader_(g_selected_leader);
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(
                conflict ? "Conflict with live metadata"
                         : (!valid ? "Invalid metadata" : (dirty ? "Unsaved metadata" : "Metadata unchanged")));

            ImGui::SeparatorText("Assignment");
            Draw_Leader_Assignment_();
            ImGui::SeparatorText("Ownership and lifecycle");
            Draw_Leader_Ownership_();
            if (Network_Game_()) {
                ImGui::TextUnformatted("Leader mutations are disabled in network games.");
            }
            if (g_leader_status != nullptr) {
                ImGui::TextWrapped("%s", g_leader_status);
            }
        }

        void Draw_Leader_Modals_() {
            if (g_transfer_requested) {
                ImGui::OpenPopup("Move leader to another player?");
                g_transfer_requested = false;
            }
            if (ImGui::BeginPopupModal(
                    "Move leader to another player?",
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text(
                    "Move %s to %s? Current assignment will be cleared.",
                    Leader_Name_(g_selected_leader),
                    Player_Name_(g_transfer_player));
                const bool valid = Valid_Leader_(g_selected_leader) &&
                    Valid_Player_(g_transfer_player) && !Network_Game_() &&
                    g_transfer_player != MOX::leaders[g_selected_leader].player_index &&
                    MOX::leaders[g_selected_leader].status >= -1 &&
                    MOX::leaders[g_selected_leader].status <= 4 &&
                    Leader_Relationship_Consistent_(MOX::leaders[g_selected_leader]) &&
                    Player_Has_Leader_Slot_(
                        g_transfer_player, MOX::leaders[g_selected_leader].type);
                ImGui::BeginDisabled(!valid);
                if (ImGui::Button("Move")) {
                    Transfer_Leader_();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (g_kill_requested) {
                ImGui::OpenPopup("Kill leader?");
                g_kill_requested = false;
            }
            if (ImGui::BeginPopupModal(
                    "Kill leader?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text(
                    "Permanently kill %s?",
                    Leader_Name_(g_selected_leader));
                const bool valid = Valid_Leader_(g_selected_leader) && !Network_Game_() &&
                    MOX::leaders[g_selected_leader].status != -2 &&
                    Valid_Player_(MOX::leaders[g_selected_leader].player_index) &&
                    Leader_Relationship_Consistent_(MOX::leaders[g_selected_leader]);
                ImGui::BeginDisabled(!valid);
                if (ImGui::Button("Kill")) {
                    Clear_Hire_References_(g_selected_leader);
                    OFFICER::Kill_Officer(g_selected_leader);
                    Refresh_Leader_State_();
                    Load_Leader_(g_selected_leader);
                    g_leader_status = "Leader killed.";
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void Draw_Leaders_() {
            Draw_Leader_List_();
            ImGui::SameLine();
            if (ImGui::BeginChild("leader_editor", ImVec2(0.0f, 0.0f), true)) {
                Draw_Leader_Editor_();
            }
            ImGui::EndChild();
            Draw_Leader_Modals_();
        }

        bool Event_Is_Global_(int16_t event) {
            return event == EVENT_HYPERSPACE_FLUX ||
                event == EVENT_PIRATE_ACTIVITY ||
                (event >= EVENT_SPACE_AMOEBA && event <= EVENT_SUPERNOVA) ||
                event == EVENT_EMPIRES_RANKING ||
                event == EVENT_ANTARANS_DEFEATED;
        }

        bool Event_Trigger_Supported_(int16_t event) {
            return event >= 0 && event < EVENT_COUNT &&
                event != EVENT_MUTINY && event != EVENT_WARP_FUNNEL &&
                event != EVENT_REBEL_ASSIMILATION;
        }

        bool Event_Is_Monster_(int16_t event) {
            return event >= EVENT_SPACE_AMOEBA && event <= EVENT_SPACE_HYDRA;
        }

        e_event_target_kind Event_Target_Kind_(int16_t event) {
            switch (event) {
                case EVENT_DERELICT: return EVENT_TARGET_TECH;
                case EVENT_CLIMATIC_CHANGE:
                case EVENT_INDUSTRIAL_ACCIDENT:
                case EVENT_MINERAL_DEPLETION:
                case EVENT_MINERAL_DEPOSIT: return EVENT_TARGET_PLANET;
                case EVENT_COMET:
                case EVENT_EARTHQUAKE:
                case EVENT_PLAGUE:
                case EVENT_POPULATION_BOOM:
                case EVENT_REBEL_ASSIMILATION: return EVENT_TARGET_COLONY;
                case EVENT_DIPLOMATIC_BLUNDER:
                case EVENT_DIPLOMATIC_MARRIAGE:
                case EVENT_EMPIRE_SURRENDERS: return EVENT_TARGET_PLAYER;
                case EVENT_FREAK_ACCIDENT:
                case EVENT_MUTINY:
                case EVENT_WARP_FUNNEL: return EVENT_TARGET_SHIP;
                case EVENT_PIRATE_ACTIVITY:
                case EVENT_PIRATE_RAID:
                case EVENT_SPACE_AMOEBA:
                case EVENT_SPACE_CRYSTAL:
                case EVENT_SPACE_DRAGON:
                case EVENT_SPACE_EEL:
                case EVENT_SPACE_HYDRA:
                case EVENT_SUPERNOVA:
                case EVENT_TIME_SPACE_ANOMALY:
                case EVENT_UNSTABLE_WORMHOLE: return EVENT_TARGET_STAR;
                default: return EVENT_TARGET_RAW;
            }
        }

        void Initialize_Event_Record_(int16_t event) {
            memset(&g_event_draft, 0, sizeof(g_event_draft));
            g_event_draft.player_idx = Event_Is_Global_(event)
                ? -1
                : (MOX::PLAYER_NUM >= 0 && MOX::PLAYER_NUM < MOX::NUM_PLAYERS
                          ? (int8_t)MOX::PLAYER_NUM
                          : -1);
            g_event_draft.status = 0;
            g_event_draft.target_id = -1;
            g_event_draft.amount = -1;
            g_event_draft.max_amount = -1;
            g_event_draft.turn_count = 0;
        }

        int16_t Planet_For_Colony_(int16_t colony) {
            return Valid_Colony_(colony) ? MOX::colony[colony].planet : -1;
        }

        bool Planet_Relationship_Valid_(int16_t planet) {
            if (MOX::planet == nullptr || planet < 0 ||
                planet >= std::clamp<int16_t>(MOX::planet_count, 0, MAX_PLANETS)) {
                return false;
            }
            const int16_t colony = MOX::planet[planet].colony_index;
            return colony == -1 ||
                (Valid_Colony_(colony) && MOX::colony[colony].planet == planet);
        }

        bool Colony_Relationship_Valid_(int16_t colony) {
            if (!Valid_Colony_(colony)) {
                return false;
            }
            const int16_t planet = MOX::colony[colony].planet;
            return Planet_Relationship_Valid_(planet) &&
                MOX::planet[planet].colony_index == colony &&
                Valid_Star_(MOX::planet[planet].star_index);
        }

        bool Star_Relationship_Valid_(int16_t star) {
            if (!Valid_Star_(star)) {
                return false;
            }
            for (int16_t orbit = 0; orbit < 5; ++orbit) {
                const int16_t planet = MOX::star[star].planet_index[orbit];
                if (planet != -1 && (!Planet_Relationship_Valid_(planet) ||
                        MOX::planet[planet].star_index != star)) {
                    return false;
                }
            }
            return true;
        }

        bool Ship_Relationship_Valid_(int16_t ship) {
            if (!Valid_Ship_(ship)) {
                return false;
            }
            const int16_t leader = MOX::ship[ship].officer_index;
            return leader == -1 ||
                (Valid_Leader_(leader) && MOX::leaders[leader].type == LEADER_TYPE_SHIP &&
                    MOX::leaders[leader].status == 1 &&
                    MOX::leaders[leader].location == ship);
        }

        void Suggest_Event_Defaults_() {
            Initialize_Event_Record_(g_selected_event);
            const int16_t player = MOX::player != nullptr &&
                    MOX::PLAYER_NUM >= 0 &&
                    MOX::PLAYER_NUM < std::clamp<int16_t>(
                        MOX::NUM_PLAYERS, 0, MAX_PLAYERS)
                ? MOX::PLAYER_NUM
                : -1;
            if (player < 0 && !Event_Is_Global_(g_selected_event)) {
                g_event_status = "Load a game before generating player-scoped event defaults.";
                return;
            }
            int16_t colony = -1;
            switch (g_selected_event) {
                case EVENT_DERELICT:
                    g_event_draft.target_id = TECH_APP_ACHILLES_TARGETING_UNIT;
                    g_event_draft.amount = TECH_APP_NO_TECH;
                    break;
                case EVENT_CLIMATIC_CHANGE:
                    colony = EVENTS::Get_Event_Climate_Change_Colony(player);
                    g_event_draft.target_id = Planet_For_Colony_(colony);
                    break;
                case EVENT_COMET:
                    g_event_draft.target_id = EVENTS::Pick_Random_Colony_No_Capitol(player);
                    g_event_draft.amount = 100;
                    g_event_draft.max_amount = 100;
                    g_event_draft.turn_count = 10;
                    break;
                case EVENT_COMPUTER_VIRUS:
                    g_event_draft.target_id = 0;
                    g_event_draft.amount = 0;
                    break;
                case EVENT_DIPLOMATIC_BLUNDER:
                case EVENT_DIPLOMATIC_MARRIAGE:
                    g_event_draft.target_id = EVENTS::Pick_Random_Player_In_Contact(player);
                    break;
                case EVENT_DONATION:
                    g_event_draft.target_id = EVENTS::Donation_Amount(
                        std::max<int32_t>(0, (MOX::stardate - 35000) / 10));
                    break;
                case EVENT_EARTHQUAKE:
                case EVENT_PLAGUE:
                    colony = EVENTS::Pick_Random_Colony_No_Capitol(player);
                    g_event_draft.target_id = colony;
                    if (g_selected_event == EVENT_PLAGUE && Valid_Colony_(colony)) {
                        g_event_draft.amount = std::max<int16_t>(
                            1, EVENTS::Colony_Research_Points(colony));
                        g_event_draft.max_amount = g_event_draft.amount;
                    }
                    break;
                case EVENT_FREAK_ACCIDENT:
                    g_event_draft.target_id = EVENTS::Pick_Random_Ship(player);
                    break;
                case EVENT_INDUSTRIAL_ACCIDENT:
                    colony = EVENTS::Get_Event_Industrial_Accident_Colony(player);
                    g_event_draft.target_id = Planet_For_Colony_(colony);
                    break;
                case EVENT_MINERAL_DEPLETION:
                    colony = EVENTS::Get_Event_Mineral_Depletion_Colony(player);
                    g_event_draft.target_id = Planet_For_Colony_(colony);
                    break;
                case EVENT_MINERAL_DEPOSIT:
                    colony = EVENTS::Get_Event_Mineral_Deposit_Colony(player);
                    g_event_draft.target_id = Planet_For_Colony_(colony);
                    break;
                case EVENT_PIRATE_ACTIVITY:
                    g_event_draft.target_id = EVENTS::Pick_Completely_Random_Star();
                    if (Valid_Star_(g_event_draft.target_id)) {
                        g_event_draft.amount = std::max<int16_t>(
                            1, EVENTS::Event_Pirate_Activity_Strength(
                                   g_event_draft.target_id));
                        g_event_draft.max_amount = g_event_draft.amount;
                    }
                    break;
                case EVENT_PIRATE_RAID:
                    g_event_draft.target_id = EVENTS::Pick_Random_Star(player);
                    g_event_draft.amount = 100;
                    break;
                case EVENT_POPULATION_BOOM:
                    g_event_draft.target_id = EVENTS::Pick_Random_Colony_No_Outpost(player);
                    break;
                case EVENT_SERENDIPITY:
                    g_event_draft.target_id = MOX::player[player].current_research_field;
                    break;
                case EVENT_SPACE_AMOEBA:
                case EVENT_SPACE_CRYSTAL:
                case EVENT_SPACE_DRAGON:
                case EVENT_SPACE_EEL:
                case EVENT_SPACE_HYDRA:
                    g_event_draft.target_id = EVENTS::Pick_Completely_Random_Star();
                    break;
                case EVENT_SUPERNOVA:
                    g_event_draft.target_id = EVENTS::Pick_Completely_Random_Star();
                    g_event_draft.turn_count = 10;
                    if (Valid_Star_(g_event_draft.target_id)) {
                        g_event_draft.amount = EVENTS::Supernova_Research_Requirement(
                            g_event_draft.target_id, g_event_draft.turn_count);
                        g_event_draft.max_amount = g_event_draft.amount;
                    }
                    break;
                case EVENT_TIME_SPACE_ANOMALY:
                    g_event_draft.target_id = EVENTS::Pick_Random_Star(player);
                    break;
                case EVENT_UNSTABLE_WORMHOLE:
                    g_event_draft.target_id = EVENTS::Pick_Completely_Random_Star();
                    g_event_draft.amount = 1;
                    break;
                case EVENT_EMPIRE_SURRENDERS:
                    g_event_draft.target_id = player == 0 && MOX::NUM_PLAYERS > 1 ? 1 : 0;
                    break;
                default:
                    break;
            }
        }

        void Load_Event_(int16_t event) {
            if (event < 0 || event >= EVENT_COUNT) {
                g_selected_event = -1;
                return;
            }
            g_selected_event = event;
            g_event_baseline = EVENTS::event_data[event];
            g_event_draft = g_event_baseline;
            g_event_status = nullptr;
        }

        bool Event_Record_Dirty_() {
            return g_selected_event >= 0 &&
                memcmp(&g_event_draft, &g_event_baseline, sizeof(g_event_draft)) != 0;
        }

        bool Event_Target_Valid_() {
            switch (Event_Target_Kind_(g_selected_event)) {
                case EVENT_TARGET_PLAYER: return Valid_Player_(g_event_draft.target_id);
                case EVENT_TARGET_STAR:
                    return Star_Relationship_Valid_(g_event_draft.target_id);
                case EVENT_TARGET_PLANET:
                    return Planet_Relationship_Valid_(g_event_draft.target_id);
                case EVENT_TARGET_COLONY:
                    return Colony_Relationship_Valid_(g_event_draft.target_id);
                case EVENT_TARGET_SHIP:
                    return Ship_Relationship_Valid_(g_event_draft.target_id);
                case EVENT_TARGET_TECH:
                    return g_event_draft.target_id >= 0 && g_event_draft.target_id < TECH_APP_COUNT;
                default: return true;
            }
        }

        bool Event_Record_Valid_(bool for_trigger) {
            const bool player_valid = Event_Is_Global_(g_selected_event)
                ? g_event_draft.player_idx == -1
                : Valid_Player_(g_event_draft.player_idx);
            const bool base_valid = g_selected_event >= 0 && g_selected_event < EVENT_COUNT &&
                g_event_draft.status >= 0 && g_event_draft.status <= 6 &&
                player_valid && (!for_trigger || Event_Target_Valid_());
            if (!base_valid || !for_trigger) {
                return base_valid;
            }
            switch (g_selected_event) {
                case EVENT_DERELICT:
                    return g_event_draft.amount >= 0 &&
                        g_event_draft.amount < TECH_APP_COUNT;
                case EVENT_COMET:
                case EVENT_PIRATE_ACTIVITY:
                case EVENT_PLAGUE:
                case EVENT_SUPERNOVA:
                    return g_event_draft.amount > 0 && g_event_draft.max_amount > 0 &&
                        g_event_draft.turn_count > 0;
                case EVENT_PIRATE_RAID:
                    return g_event_draft.amount >= 0;
                case EVENT_UNSTABLE_WORMHOLE:
                    return g_event_draft.amount > 0;
                case EVENT_DIPLOMATIC_BLUNDER:
                case EVENT_DIPLOMATIC_MARRIAGE:
                case EVENT_EMPIRE_SURRENDERS:
                    return g_event_draft.target_id != g_event_draft.player_idx;
                default:
                    return true;
            }
        }

        bool Event_Status_Is_Executable_() {
            return g_event_draft.status == 1 || g_event_draft.status == 2 ||
                g_event_draft.status == 4 || g_event_draft.status == 5 ||
                g_event_draft.status == 6;
        }

        bool Event_Record_Saveable_() {
            if (!Event_Record_Valid_(false)) {
                return false;
            }
            if (!Event_Status_Is_Executable_()) {
                return true;
            }
            if (!Event_Trigger_Supported_(g_selected_event) ||
                !Event_Record_Valid_(true)) {
                return false;
            }
            if (Event_Is_Monster_(g_selected_event)) {
                return g_event_draft.status == 1 || g_event_draft.status == 6;
            }
            return true;
        }

        void Save_Event_Record_(int8_t status, const char* message) {
            g_event_draft.status = status;
            EVENTS::event_data[g_selected_event] = g_event_draft;
            g_event_baseline = g_event_draft;
            g_event_status = message;
            Request_Redraw_();
        }

        void Clear_Event_() {
            Initialize_Event_Record_(g_selected_event);
            EVENTS::event_data[g_selected_event] = g_event_draft;
            g_event_baseline = g_event_draft;
            g_event_status = "Event record cleared.";
            Request_Redraw_();
        }

        const char* Event_Owner_Name_(const s_event_data& event) {
            return event.player_idx < 0 ? "Global" : Player_Name_(event.player_idx);
        }

        void Draw_Event_List_() {
            static ImGuiTextFilter filter;
            static bool include_inactive = true;
            Draw_Player_Filter_("Scope filter", &g_event_player_filter);
            ImGui::Checkbox("Include inactive / expired", &include_inactive);
            filter.Draw("Filter events", -1.0f);
            const float list_width = ImGui::GetContentRegionAvail().x * 0.4f;
            if (ImGui::BeginChild("event_list", ImVec2(list_width, 0.0f), true)) {
                for (int16_t event = 0; event < EVENT_COUNT; ++event) {
                    const s_event_data& data = EVENTS::event_data[event];
                    if (!include_inactive && (data.status == 0 || data.status == 3)) {
                        continue;
                    }
                    if (g_event_player_filter != -2 && data.player_idx != g_event_player_filter) {
                        continue;
                    }
                    char label[128];
                    snprintf(
                        label,
                        sizeof(label),
                        "%s [%d] - %s, %s",
                        k_event_names[event],
                        (int)event,
                        Event_Owner_Name_(data),
                        data.status >= 0 && data.status <= 6
                            ? k_event_statuses[(int)data.status]
                            : "Invalid");
                    if (!filter.PassFilter(label)) {
                        continue;
                    }
                    if (ImGui::Selectable(label, g_selected_event == event)) {
                        Load_Event_(event);
                    }
                }
            }
            ImGui::EndChild();
        }

        void Draw_Event_Target_() {
            switch (Event_Target_Kind_(g_selected_event)) {
                case EVENT_TARGET_PLAYER: {
                    int8_t player = (int8_t)g_event_draft.target_id;
                    if (debug_game_widgets::Player_Reference("Target player", &player, false)) {
                        g_event_draft.target_id = player;
                    }
                    break;
                }
                case EVENT_TARGET_STAR:
                    debug_game_widgets::Star_Reference(
                        "Target star", &g_event_draft.target_id, false);
                    break;
                case EVENT_TARGET_PLANET:
                    debug_game_widgets::Planet_Reference(
                        "Target planet", &g_event_draft.target_id, false);
                    break;
                case EVENT_TARGET_COLONY:
                    debug_game_widgets::Colony_Reference(
                        "Target colony", &g_event_draft.target_id, false);
                    break;
                case EVENT_TARGET_SHIP:
                    Input_S16_("Target ship", &g_event_draft.target_id);
                    break;
                case EVENT_TARGET_TECH:
                    Input_S16_("Target technology", &g_event_draft.target_id);
                    break;
                default:
                    Input_S16_("Target / value", &g_event_draft.target_id);
                    break;
            }
        }

        void Draw_Event_Editor_() {
            if (g_selected_event < 0 || g_selected_event >= EVENT_COUNT) {
                ImGui::TextUnformatted("Select an event to inspect, queue, or trigger.");
                return;
            }
            ImGui::Text("%s [%d]", k_event_names[g_selected_event], (int)g_selected_event);
            ImGui::TextWrapped(
                "Queue uses status 1 and starts through normal event processing. Trigger uses status 4 and is applied on the next event update. It does not process all events immediately.");
            if (Event_Is_Global_(g_selected_event)) {
                ImGui::TextUnformatted("Scope: Global");
                g_event_draft.player_idx = -1;
            } else {
                debug_game_widgets::Player_Reference(
                    "Player", &g_event_draft.player_idx, false);
            }
            Draw_Event_Target_();
            Input_S16_("Amount", &g_event_draft.amount);
            Input_S16_("Maximum amount", &g_event_draft.max_amount);
            Input_S8_("Turn count", &g_event_draft.turn_count);
            debug_game_widgets::Enum(
                "Status", &g_event_draft.status, k_event_statuses, 7);

            if (ImGui::Button("Suggest valid defaults")) {
                Suggest_Event_Defaults_();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload live")) {
                Load_Event_(g_selected_event);
            }

            const bool record_valid = Event_Record_Saveable_();
            const bool trigger_valid = Event_Record_Valid_(true) &&
                Event_Trigger_Supported_(g_selected_event);
            ImGui::BeginDisabled(
                !Event_Record_Dirty_() || !record_valid || Network_Game_());
            if (ImGui::Button("Save record")) {
                Save_Event_Record_(g_event_draft.status, "Event record saved.");
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!trigger_valid || Network_Game_());
            if (ImGui::Button("Queue event")) {
                Save_Event_Record_(1, "Event queued for normal processing.");
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(Event_Is_Monster_(g_selected_event));
            if (ImGui::Button("Trigger next update")) {
                Save_Event_Record_(4, "Event will trigger on the next event update.");
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(Network_Game_());
            if (ImGui::Button("Clear event")) {
                Clear_Event_();
            }
            ImGui::EndDisabled();

            if (!Event_Trigger_Supported_(g_selected_event)) {
                ImGui::TextUnformatted("This event has no safe generic trigger path; edit/save its record only.");
            } else if (Event_Is_Monster_(g_selected_event)) {
                ImGui::TextUnformatted("Monster events must be queued so normal processing can spawn them at a star.");
            } else if (!Event_Target_Valid_()) {
                ImGui::TextUnformatted("Select a valid target before queueing or triggering.");
            }
            if (Network_Game_()) {
                ImGui::TextUnformatted("Event mutations are disabled in network games.");
            }
            if (g_event_status != nullptr) {
                ImGui::TextWrapped("%s", g_event_status);
            }
        }

        void Draw_Event_Globals_() {
            if (!ImGui::CollapsingHeader("Event scheduler state")) {
                return;
            }
            ImGui::BeginDisabled(Network_Game_());
            Input_U8_("Current event", &EVENTS::current_event);
            Input_U32_("Last event year", &EVENTS::last_event_year);
            Input_S16_("Event delay count", &EVENTS::event_delay_count);
            int16_t forced_event = EVENTS::force_event_type;
            if (Input_S16_("Forced event type (0=none, 1-36=event)", &forced_event)) {
                EVENTS::force_event_type = (uint8_t)std::clamp<int16_t>(
                    forced_event, 0, EVENT_COUNT);
            }
            Input_U8_("All-events delay", &EVENTS::all_events_delay);
            Input_U8_("Between-events delay", &EVENTS::between_events_delay);
            ImGui::EndDisabled();
        }

        void Draw_Events_() {
            Draw_Event_Globals_();
            ImGui::Separator();
            Draw_Event_List_();
            ImGui::SameLine();
            if (ImGui::BeginChild("event_editor", ImVec2(0.0f, 0.0f), true)) {
                Draw_Event_Editor_();
            }
            ImGui::EndChild();
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            if (ImGui::Button("Open current player editor")) {
                const int16_t player = std::clamp<int16_t>(
                    MOX::PLAYER_NUM,
                    0,
                    std::max<int16_t>(MOX::NUM_PLAYERS - 1, 0));
                debug_game_editors::Open_Player(player);
            }
            ImGui::SameLine();
            if (ImGui::Button("Open Antaran editor")) {
                debug_game_editors::Open_Antaran();
            }
            if (ImGui::BeginTabBar("internal_state_tabs")) {
                if (ImGui::BeginTabItem("Leaders")) {
                    Draw_Leaders_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Events")) {
                    Draw_Events_();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        debug_overlay::s_global_pane_registration g_registration("State", Draw_);
    } // namespace
} // namespace INTERNAL_STATE_DEBUG
#endif