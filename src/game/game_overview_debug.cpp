#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace GAME_OVERVIEW_DEBUG {
    namespace {
        bool Valid_Player_(int16_t player) {
            return MOX::player != nullptr && player >= 0 &&
                   player < std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        }

        void Draw_Count_(const char* label, int32_t value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", (int)value);
        }

        void Draw_Value_(const char* label, const char* value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value);
        }

        void Draw_Stat_Table_(const char* id, const char* left_label,
                              int32_t left_value, const char* right_label,
                              int32_t right_value) {
            if (ImGui::BeginTable(id, 4, ImGuiTableFlags_BordersInnerV |
                                            ImGuiTableFlags_RowBg |
                                            ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(left_label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", (int)left_value);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(right_label);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", (int)right_value);
                ImGui::EndTable();
            }
        }

        void Draw_Game_Stats_() {
            const int16_t player_count =
                std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            int16_t active_players = 0;
            if (MOX::player != nullptr) {
                for (int16_t player = 0; player < player_count; ++player) {
                    if (MOX::player[player].eliminated == 0) ++active_players;
                }
            }

            char stardate[32];
            snprintf(stardate, sizeof(stardate), "%d.%d",
                     (int)(MOX::stardate / 10),
                     (int)(MOX::stardate % 10));

            if (ImGui::BeginTable("game_stats", 2,
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                Draw_Value_("Stardate", stardate);
                Draw_Count_("Active players", active_players);
                Draw_Count_("Player slots", player_count);
                Draw_Count_("Stars", std::clamp<int16_t>(MOX::NUM_STARS, 0, MAX_STARS));
                Draw_Count_("Planets", std::clamp<int16_t>(MOX::planet_count, 0, MAX_PLANETS));
                Draw_Count_("Colonies", std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES));
                Draw_Count_("Ships", std::clamp<int16_t>(MOX::NUM_SHIPS, 0, MAX_SHIPS));
                ImGui::EndTable();
            }
        }

        void Draw_Network_Stats_() {
            const s_network& network = NETMOX::net;
            const char* role = "Offline";
            if (network.is_network_active != 0) {
                role = network.local_net_id == network.net_host_id ? "Host" : "Client";
            }

            if (ImGui::BeginTable("network_stats", 2,
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                Draw_Value_("Role", role);
                Draw_Count_("Connected players", network.num_connected_players);
                Draw_Count_("Bytes received", (int32_t)network.total_bytes_received);
                Draw_Count_("Sync state", network.sync_state);
                ImGui::EndTable();
            }
        }

        void Draw_Current_Player_(int16_t player_index) {
            if (!Valid_Player_(player_index)) {
                ImGui::TextDisabled("No current player is loaded.");
                return;
            }

            const s_player& player = MOX::player[player_index];
            char player_name[sizeof(player.name) + 1];
            char race_name[sizeof(player.race_name) + 1];
            strlcpy(player_name, player.name, sizeof(player_name));
            strlcpy(race_name, player.race_name, sizeof(race_name));

            ImGui::Text("%s [%d]", player_name[0] == '\0' ? "Player" : player_name,
                        (int)player_index);
            ImGui::SameLine();
            ImGui::TextDisabled("%s%s", race_name,
                                player.eliminated != 0 ? " - eliminated" : "");

            ImGui::SeparatorText("Empire");
            Draw_Stat_Table_("player_empire_1", "Population", player.total_pop,
                             "Colonies", player.total_colonies);
            Draw_Stat_Table_("player_empire_2", "Ships", player.total_ships,
                             "Treasury", player.bc);

            ImGui::SeparatorText("Turn economy");
            Draw_Stat_Table_("player_economy_1", "Food", player.food_produced,
                             "Industry", player.industry_produced);
            Draw_Stat_Table_("player_economy_2", "Research", player.research_produced,
                             "BC", player.bc_produced);
            Draw_Stat_Table_("player_economy_3", "Food surplus", player.surplus_food,
                             "BC surplus", player.surplus_bc);
            Draw_Stat_Table_("player_economy_4", "Maintenance", player.total_maintenance,
                             "Research stored", player.research_accumulated);

            ImGui::SeparatorText("Logistics");
            Draw_Stat_Table_("player_logistics_1", "Command points", player.command_points,
                             "Command used", player.command_points_used);
            Draw_Stat_Table_("player_logistics_2", "Freighters", player.n_freighters,
                             "Freighters free", player.surplus_freighters);
            Draw_Stat_Table_("player_logistics_3", "Food freighted", player.food_freighted,
                             "Settlers freighted", player.settlers_freighted);

            if (ImGui::Button("Open current player details")) {
                debug_game_editors::Open_Player(player_index);
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            if (ImGui::BeginTable("overview_columns", 2,
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Network", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Game");
                Draw_Game_Stats_();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted("Network");
                Draw_Network_Stats_();
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Current player");
            Draw_Current_Player_(context.player_id);
        }

        debug_overlay::s_global_pane_registration g_registration("Game", Draw_);
    } // namespace
} // namespace GAME_OVERVIEW_DEBUG
#endif