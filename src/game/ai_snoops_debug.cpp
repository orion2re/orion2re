#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace AI_SNOOPS_DEBUG {
    namespace {
        int g_player = 0;
        int g_opponent = 0;
        int g_star = 0;
        int g_source_star = 0;
        int g_dest_star = 0;
        int g_ship = 0;

        const ai_state::s_snapshot* Snapshot_() {
            const ai_state::s_snapshot* snapshot = ai_state::Get_Completed();
            if (snapshot == nullptr) snapshot = ai_state::Get_Active();
            return ai_state::Is_Valid(snapshot) ? snapshot : nullptr;
        }

        const char* Objective_Name_(int objective) {
            switch (objective) {
            case PLAYER_OBJECTIVE_DIPLOMATIC: return "Diplomatic";
            case PLAYER_OBJECTIVE_MILITARIST: return "Militarist";
            case PLAYER_OBJECTIVE_EXPANSIONIST: return "Expansionist";
            case PLAYER_OBJECTIVE_TECHNOLOGIST: return "Technologist";
            case PLAYER_OBJECTIVE_INDUSTRIALIST: return "Industrialist";
            case PLAYER_OBJECTIVE_ECOLOGIST: return "Ecologist";
            case PLAYER_OBJECTIVE_HUMAN: return "Human";
            default: return "Unknown";
            }
        }

        void Draw_Colony_Assessments_(const ai_state::s_snapshot* snapshot) {
            if (ImGui::BeginTable("colony_assessments", 8,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 410.0f))) {
                ImGui::TableSetupColumn("Colony");
                ImGui::TableSetupColumn("Race");
                ImGui::TableSetupColumn("Pop limit");
                ImGui::TableSetupColumn("Threatened");
                ImGui::TableSetupColumn("Flagged pops");
                ImGui::TableSetupColumn("Food buildings applicable");
                ImGui::TableSetupColumn("Unused 0x3");
                ImGui::TableSetupColumn("Pollution controls applicable");
                ImGui::TableHeadersRow();
                for (int colony = 0; colony < snapshot->colony_count; ++colony) {
                    const s_ai_colony_info& info = (*snapshot->ai_colony_info)[colony];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(colony);
                    if (ImGui::Selectable(std::to_string(colony).c_str())) {
                        debug_game_editors::Open_Colony((int16_t)colony);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", (int)info.race);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)info.pop_limit);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(info.threatened ? "yes" : "no");
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", (int)info.n_flagged_pops);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%d", (int)info.food_buildings_applicable);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%d", (int)info.unused_0x3);
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%d", (int)info.pollution_controls_applicable);
                }
                ImGui::EndTable();
            }
        }

        const char* Player_Name_(int player, char* buffer, size_t buffer_size) {
            if (MOX::player == nullptr || player < 0 || player >= MAX_PLAYERS) {
                snprintf(buffer, buffer_size, "Player %d", player);
            } else {
                char race_name[sizeof(MOX::player[0].race_name) + 1];
                strlcpy(race_name, MOX::player[player].race_name, sizeof(race_name));
                snprintf(buffer, buffer_size, "%d: %s", player,
                         race_name[0] == '\0' ? "Player" : race_name);
            }
            return buffer;
        }

        const char* Star_Name_(int star, char* buffer, size_t buffer_size) {
            if (MOX::star == nullptr || star < 0 || star >= MAX_STARS) {
                snprintf(buffer, buffer_size, "Star %d", star);
            } else {
                char name[sizeof(MOX::star[0].name) + 1];
                strlcpy(name, MOX::star[star].name, sizeof(name));
                snprintf(buffer, buffer_size, "%d: %s", star,
                         name[0] == '\0' ? "Star" : name);
            }
            return buffer;
        }

        bool Player_Selector_(const char* label, int* player,
                              const ai_state::s_snapshot* snapshot) {
            if (snapshot->player_count <= 0) return false;
            *player = std::clamp(*player, 0, (int)snapshot->player_count - 1);
            char preview[64];
            bool changed = false;
            if (ImGui::BeginCombo(label, Player_Name_(*player, preview, sizeof(preview)))) {
                for (int index = 0; index < snapshot->player_count; ++index) {
                    char name[64];
                    if (ImGui::Selectable(Player_Name_(index, name, sizeof(name)),
                                          index == *player)) {
                        *player = index;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        void Stat_(const char* label, int64_t value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%lld", (long long)value);
        }

        void Draw_Snapshot_Header_(const ai_state::s_snapshot* snapshot) {
            ImGui::Text("AI snapshot: %d.%d | %.1f MiB | %s",
                        (int)(snapshot->stardate / 10),
                        (int)(snapshot->stardate % 10),
                        (double)snapshot->allocated_bytes / (1024.0 * 1024.0),
                        ai_state::Get_Completed() == snapshot ? "completed" : "computing");
        }

        void Draw_Build_Needs_(const ai_state::s_snapshot* snapshot, int player) {
            const s_player_ship_sizes& ships = snapshot->player_ship_sizes[player];
            if (ImGui::BeginTable("build_needs", 2,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                Stat_("War level", AIBUILD::Player_War_Level((int16_t)player));
                Stat_("Combat ships", ships.combat_ship_count);
                Stat_("Weighted ship size", ships.weighted_ship_size);
                Stat_("Target ship size", ships.target_ship_size);
                Stat_("Command points used", ships.combat_command_points_used);
                Stat_("Active command points", ships.active_combat_command_points_used);
                Stat_("Strength vs ships", snapshot->player_total_strength_vs_ships[player][0]);
                Stat_("Strength wanted", snapshot->player_total_strength_vs_ships[player][2]);
                Stat_("Strength vs colonies", snapshot->player_total_strength_vs_colonies[player]);
                Stat_("Transports current", snapshot->player_n_transports[player][0]);
                Stat_("Transports wanted", snapshot->player_n_transports[player][1]);
                Stat_("Colony ships current", snapshot->player_n_colony_ships[player][0]);
                Stat_("Colony ships wanted", snapshot->player_n_colony_ships[player][1]);
                Stat_("Colony ships assigned", snapshot->player_n_colony_ships[player][2]);
                Stat_("Spies current", snapshot->player_n_spies[player][0]);
                Stat_("Spies wanted", snapshot->player_n_spies[player][1]);
                Stat_("Freighter groups wanted", snapshot->player_n_freighter_groups_wanted[player]);
                ImGui::EndTable();
            }
            ImGui::TextUnformatted("Ships by hull class:");
            for (int size = 0; size < HULL_CLASS_COUNT; ++size) {
                if (size != 0) ImGui::SameLine();
                ImGui::Text("%d: %d", size, (int)ships.ship_counts[size]);
            }
        }

        void Draw_Empire_() {
            const ai_state::s_snapshot* snapshot = Snapshot_();
            if (snapshot == nullptr) {
                ImGui::TextDisabled("No AI turn snapshot is available yet.");
            } else {
                Draw_Snapshot_Header_(snapshot);
                Player_Selector_("Player", &g_player, snapshot);
                const int player = g_player;
                const s_ai_player_info& info = (*snapshot->ai_player_info)[player];
                const s_ai_tech_info_player& tech = snapshot->aitech->players[player];
                const s_player* live = MOX::player == nullptr ? nullptr : &MOX::player[player];

                if (ImGui::BeginTabBar("empire_tabs")) {
                        if (ImGui::BeginTabItem("Summary")) {
                            ImGui::Text("Objective: %s", live == nullptr ? "Unknown" :
                                        Objective_Name_(live->objectives));
                            if (ImGui::BeginTable("empire_summary", 2,
                                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                                Stat_("Colonies", info.colony_count);
                                Stat_("Colony deficit", info.colony_deficit);
                                Stat_("Blockaded colonies", info.colony_blockaded_count);
                                Stat_("Colonized stars", info.n_stars_with_colony);
                                Stat_("Blockaded stars", info.n_blockaded_stars);
                                Stat_("Contacts", info.n_contacts);
                                Stat_("Previous contacts", info.prev_n_contacts);
                                Stat_("Reachable players", info.n_reachable_players);
                                ImGui::EndTable();
                            }
                            ImGui::SeparatorText("Reachability and estimated strength");
                            if (ImGui::BeginTable("empire_opponents", 3,
                                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                        ImGuiTableFlags_SizingStretchProp)) {
                                ImGui::TableSetupColumn("Player");
                                ImGui::TableSetupColumn("Reachable");
                                ImGui::TableSetupColumn("Ship strength");
                                ImGui::TableHeadersRow();
                                for (int other = 0; other < snapshot->player_count; ++other) {
                                    char name[64];
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    ImGui::TextUnformatted(Player_Name_(other, name, sizeof(name)));
                                    ImGui::TableSetColumnIndex(1);
                                    ImGui::TextUnformatted(info.reachable_players[other] ? "yes" : "no");
                                    ImGui::TableSetColumnIndex(2);
                                    ImGui::Text("%d", (int)info.total_ship_strength[other]);
                                }
                                ImGui::EndTable();
                            }
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Build needs")) {
                            Draw_Build_Needs_(snapshot, player);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Tech assessment")) {
                            if (ImGui::BeginTable("tech_assessment", 2,
                                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                                Stat_("Best shield", tech.best_shield);
                                Stat_("Shield delta", tech.best_shield_delta);
                                Stat_("Best armor", tech.best_armor);
                                Stat_("Armor delta", tech.armor_delta);
                                Stat_("Best computer", tech.best_computer);
                                Stat_("Computer delta", tech.best_computer_delta);
                                Stat_("Best fuel", tech.best_fuel);
                                Stat_("Best drive", tech.best_drive);
                                Stat_("Best beam score", tech.best_weapon1);
                                Stat_("Beam delta", tech.best_weapon1_delta);
                                Stat_("Best missile score", tech.best_weapon2);
                                Stat_("Missile delta", tech.best_weapon2_delta);
                                Stat_("Planet shield", tech.best_planet_shield);
                                Stat_("Planet shield delta", tech.planet_shield_delta);
                                Stat_("Ground bonus", tech.ground_bonus);
                                Stat_("Ground delta", tech.ground_bonus_delta);
                                Stat_("Contact count", tech.contact_count);
                                Stat_("Colonization idle", tech.colonization_idle);
                                ImGui::EndTable();
                            }
                            if (live != nullptr) {
                                ImGui::Text("Selected research field/application: %d / %d",
                                            (int)live->current_research_field,
                                            (int)live->current_research_application);
                            }
                            ImGui::EndTabItem();
                        }
                    ImGui::EndTabBar();
                }
                if (ImGui::Button("Open player details")) {
                    debug_game_editors::Open_Player((int16_t)player);
                }
            }
        }

        void Draw_Route_Query_(const ai_state::s_snapshot* snapshot) {
            if (snapshot->star_count <= 0 || snapshot->player_count <= 0) return;
            g_source_star = std::clamp(g_source_star, 0, (int)snapshot->star_count - 1);
            g_dest_star = std::clamp(g_dest_star, 0, (int)snapshot->star_count - 1);
            ImGui::SliderInt("Source star", &g_source_star, 0, snapshot->star_count - 1);
            ImGui::SliderInt("Destination star", &g_dest_star, 0, snapshot->star_count - 1);
            const size_t route_index = snapshot->Route_Index(
                g_source_star, g_dest_star, g_player);
            const uint8_t turns = snapshot->n_turns_from_to[route_index];
            const AIDATA::s_turns_from_to_intermediate_cache_entry& route =
                snapshot->n_turns_from_to_intermediate[route_index];
            ImGui::Text("Direct turns: %s", turns == 0xFF ? "unreachable" :
                        std::to_string(turns).c_str());
            ImGui::Text("Normal: %u via %d | extended: %u via %d | computed: %d",
                        (unsigned)route.turns, (int)route.intermediate_star,
                        (unsigned)route.extended_turns,
                        (int)route.extended_intermediate_star, (int)route.computed);
        }

        void Draw_Star_State_(const ai_state::s_snapshot* snapshot, int player) {
            if (snapshot->star_count <= 0) return;
            g_star = std::clamp(g_star, 0, (int)snapshot->star_count - 1);
            ImGui::SliderInt("Star", &g_star, 0, snapshot->star_count - 1);
            char star_name[64];
            ImGui::TextUnformatted(Star_Name_(g_star, star_name, sizeof(star_name)));
            const s_ai_star_info& info = (*snapshot->ai_star_info)[g_star];
            const s_ai_star_player_block& block = info.per_player[player];
            const uint8_t normal_mask = (*snapshot->star_in_range_of_player)[g_star];
            const uint8_t extended_mask = (*snapshot->star_in_extended_range_of_player)[g_star];
            if (ImGui::BeginTable("star_state", 2,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                Stat_("Planets present", info.n_planets_present);
                Stat_("Normal range mask", normal_mask);
                Stat_("Extended range mask", extended_mask);
                Stat_("Enemy count", block.enemy_count);
                Stat_("Neutral count", block.neutral_count);
                Stat_("Alliance count", block.alliance_count);
                Stat_("Out-of-range count", block.out_of_range_count);
                Stat_("Nearest ETA accumulator", block.nearest_eta);
                Stat_("Enemy enroute/threat", block.enemy_enroute_or_threat);
                Stat_("Base stage weight", (*snapshot->base_star_stage_weights)[g_star][player]);
                Stat_("Stage weight", (*snapshot->star_stage_weights)[g_star][player]);
                Stat_("First combat ship", (*snapshot->i_all_combat_ships)[player][g_star]);
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Strength by turn");
            if (ImGui::BeginTable("star_strength", 5,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Turn");
                ImGui::TableSetupColumn("Possible ours");
                ImGui::TableSetupColumn("Possible theirs");
                ImGui::TableSetupColumn("Their enroute");
                ImGui::TableSetupColumn("Ship present");
                ImGui::TableHeadersRow();
                for (int turn = 0; turn < AIMOVE::ENEMY_STRENGTH_TURN_WINDOW; ++turn) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", turn);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", (int)(*snapshot->possible_ship_strengths)
                        [g_star][player][g_opponent][turn]);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)(*snapshot->possible_ship_strengths)
                        [g_star][g_opponent][player][turn]);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", (int)(*snapshot->enroute_ship_strengths)
                        [g_star][g_opponent][player][turn]);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", (int)(*snapshot->enroute_ships)
                        [g_star][g_opponent][turn]);
                }
                ImGui::EndTable();
            }
            if (ImGui::Button("Open star details")) {
                debug_game_editors::Open_Star((int16_t)g_star);
            }
        }

        void Draw_Planet_Values_(const ai_state::s_snapshot* snapshot, int player) {
            static bool valued_only = true;
            ImGui::Checkbox("Valued only", &valued_only);
            if (ImGui::BeginTable("planet_values", 5,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 360.0f))) {
                ImGui::TableSetupColumn("Planet");
                ImGui::TableSetupColumn("Star");
                ImGui::TableSetupColumn("Base");
                ImGui::TableSetupColumn("Contextual");
                ImGui::TableSetupColumn("Stage");
                ImGui::TableHeadersRow();
                for (int planet = 0; planet < snapshot->planet_count; ++planet) {
                    const uint16_t base = (*snapshot->base_planet_values)[player][planet];
                    const uint16_t contextual = (*snapshot->contextual_planet_values)
                        [player * MAX_PLANETS + planet];
                    const int16_t stage = (*snapshot->planet_stage_weights)[planet][player];
                    if (valued_only && base == 0 && contextual == 0 && stage == 0) continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(planet);
                    if (ImGui::Selectable(std::to_string(planet).c_str(), false,
                                          ImGuiSelectableFlags_SpanAllColumns)) {
                        debug_game_editors::Open_Planet((int16_t)planet);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", MOX::planet == nullptr ? -1 :
                                (int)MOX::planet[planet].star_index);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", (unsigned)base);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%u", (unsigned)contextual);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", (int)stage);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Worlds_() {
            const ai_state::s_snapshot* snapshot = Snapshot_();
            if (snapshot == nullptr) {
                ImGui::TextDisabled("No AI turn snapshot is available yet.");
            } else {
                Draw_Snapshot_Header_(snapshot);
                Player_Selector_("Player", &g_player, snapshot);
                Player_Selector_("Opponent", &g_opponent, snapshot);
                if (ImGui::BeginTabBar("world_tabs")) {
                        if (ImGui::BeginTabItem("Star threats")) {
                            Draw_Star_State_(snapshot, g_player);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Planet values")) {
                            Draw_Planet_Values_(snapshot, g_player);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Colony assessments")) {
                            Draw_Colony_Assessments_(snapshot);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Route cache")) {
                            Draw_Route_Query_(snapshot);
                            ImGui::EndTabItem();
                        }
                    ImGui::EndTabBar();
                }
            }
        }

        void Draw_Fleet_Table_(const ai_state::s_snapshot* snapshot) {
            static bool refits_only = false;
            ImGui::Checkbox("Refit candidates only", &refits_only);
            if (ImGui::BeginTable("fleet_state", 7,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 390.0f))) {
                ImGui::TableSetupColumn("Ship");
                ImGui::TableSetupColumn("Owner");
                ImGui::TableSetupColumn("Location");
                ImGui::TableSetupColumn("Vs opponent");
                ImGui::TableSetupColumn("Nebula");
                ImGui::TableSetupColumn("Bomb 0-3");
                ImGui::TableSetupColumn("Refit");
                ImGui::TableHeadersRow();
                for (int index = 0; index < snapshot->n_all_ships; ++index) {
                    const int ship = (*snapshot->all_ships)[index];
                    if (ship < 0 || ship >= snapshot->ship_count) continue;
                    const int desirability = (*snapshot->ship_refit_desirabilities)[ship];
                    if (refits_only && desirability == 0) continue;
                    const s_ship_data* live = MOX::ship == nullptr ? nullptr : &MOX::ship[ship];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(ship);
                    if (ImGui::Selectable(std::to_string(ship).c_str())) {
                        g_ship = ship;
                        debug_game_editors::Open_Ship((int16_t)ship);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", live == nullptr ? -1 : (int)live->owner);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", live == nullptr ? -1 : (int)live->location);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%u", (unsigned)(*snapshot->ship_strengths)[ship][g_opponent]);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", (unsigned)(*snapshot->ship_strengths_in_nebula)[ship][g_opponent]);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%u %u %u %u",
                                (unsigned)(*snapshot->ship_bomb_strengths)[ship][0],
                                (unsigned)(*snapshot->ship_bomb_strengths)[ship][1],
                                (unsigned)(*snapshot->ship_bomb_strengths)[ship][2],
                                (unsigned)(*snapshot->ship_bomb_strengths)[ship][3]);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%d", desirability);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Ship_Route_(const ai_state::s_snapshot* snapshot) {
            if (snapshot->ship_count <= 0 || snapshot->star_count <= 0) return;
            g_ship = std::clamp(g_ship, 0, (int)snapshot->ship_count - 1);
            g_dest_star = std::clamp(g_dest_star, 0, (int)snapshot->star_count - 1);
            ImGui::SliderInt("Ship", &g_ship, 0, snapshot->ship_count - 1);
            ImGui::SliderInt("Destination", &g_dest_star, 0, snapshot->star_count - 1);
            const size_t route_index = snapshot->Ship_Route_Index(g_ship, g_dest_star);
            const int8_t turns = snapshot->ship_n_turns_to[route_index];
            const AIDATA::s_ship_n_turns_to_intermediate_cache_entry& route =
                snapshot->ship_n_turns_to_intermediate[route_index];
            ImGui::Text("Turns: %d | cached turns: %u | via: %d | computed: %d",
                        (int)turns, (unsigned)route.turns,
                        (int)route.intermediate_star, (int)route.computed);
        }

        void Draw_Refit_Queue_(const ai_state::s_snapshot* snapshot) {
            Player_Selector_("Player", &g_player, snapshot);
            const s_ai_refit_player& refits = (*snapshot->airefit)[g_player];
            const int count = std::clamp<int>(refits.n_entries, 0, MAX_SHIPS);
            ImGui::Text("Remaining refit candidates: %d", count);
            if (ImGui::BeginTable("refit_queue", 3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 390.0f))) {
                ImGui::TableSetupColumn("Priority");
                ImGui::TableSetupColumn("Ship");
                ImGui::TableSetupColumn("Desirability");
                ImGui::TableHeadersRow();
                for (int index = count - 1; index >= 0; --index) {
                    const s_ai_refit_entry& entry = refits.entry[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", count - index);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(index);
                    if (ImGui::Selectable(std::to_string(entry.ship_idx).c_str())) {
                        debug_game_editors::Open_Ship(entry.ship_idx);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)entry.desirability);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Fleets_() {
            const ai_state::s_snapshot* snapshot = Snapshot_();
            if (snapshot == nullptr) {
                ImGui::TextDisabled("No AI turn snapshot is available yet.");
            } else {
                Draw_Snapshot_Header_(snapshot);
                Player_Selector_("Opponent perspective", &g_opponent, snapshot);
                if (ImGui::BeginTabBar("fleet_tabs")) {
                        if (ImGui::BeginTabItem("Strength and refits")) {
                            Draw_Fleet_Table_(snapshot);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Ship route cache")) {
                            Draw_Ship_Route_(snapshot);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Refit queues")) {
                            Draw_Refit_Queue_(snapshot);
                            ImGui::EndTabItem();
                        }
                    ImGui::EndTabBar();
                }
            }
        }

        void Draw_Colony_Decisions_(const ai_state::s_snapshot* snapshot, int player) {
            const ai_state::s_player_decisions& decisions = snapshot->players[player];
            if (!decisions.colony_phase_ran) {
                ImGui::TextDisabled("The colony phase did not run for this player.");
                return;
            }
            if (ImGui::BeginTable("colony_decisions", 8,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 390.0f))) {
                ImGui::TableSetupColumn("Colony");
                ImGui::TableSetupColumn("Blockaded");
                ImGui::TableSetupColumn("Farmers");
                ImGui::TableSetupColumn("Workers");
                ImGui::TableSetupColumn("Assignable");
                ImGui::TableSetupColumn("Pop end");
                ImGui::TableSetupColumn("Pollution");
                ImGui::TableSetupColumn("Worker/Farmer scores");
                ImGui::TableHeadersRow();
                for (size_t index = 0; index < decisions.colonies.size(); ++index) {
                    const ai_state::s_colony_decision& colony = decisions.colonies[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID((int)index);
                    if (ImGui::Selectable(std::to_string(colony.colony_idx).c_str())) {
                        debug_game_editors::Open_Colony(colony.colony_idx);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(colony.blockaded ? "yes" : "no");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", (unsigned)colony.n_farmers);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%u", (unsigned)colony.n_workers);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", (unsigned)colony.first_assignable_pop_idx);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%u", (unsigned)colony.pop_end_idx);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%u%s", (unsigned)colony.pollution_at_start,
                                colony.eats_minerals ? " (lithovore)" : "");
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%d / %d", (int)colony.best_worker_to_farmer_score,
                                (int)colony.worst_worker_to_farmer_score);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Movement_Decisions_(const ai_state::s_snapshot* snapshot, int player) {
            const ai_state::s_movement_decision& movement = snapshot->players[player].movement;
            if (!snapshot->players[player].movement_phase_ran) {
                ImGui::TextDisabled("The movement phase did not run for this player.");
                return;
            }
            ImGui::Text("Pre-assignment colonization candidates: %zu | avoided stars: %zu | monsters: %zu",
                        movement.colonizable_planets.size(), movement.flee_stars.size(),
                        movement.nonplayer_stars.size());
            if (ImGui::BeginTable("movement_candidates", 3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 330.0f))) {
                ImGui::TableSetupColumn("Rank");
                ImGui::TableSetupColumn("Planet");
                ImGui::TableSetupColumn("Contextual value");
                ImGui::TableHeadersRow();
                for (size_t index = 0; index < movement.colonizable_planets.size(); ++index) {
                    const s_ai_colonizable_planet& planet = movement.colonizable_planets[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", index + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID((int)index);
                    if (ImGui::Selectable(std::to_string(planet.planet_idx).c_str())) {
                        debug_game_editors::Open_Planet(planet.planet_idx);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)planet.contextual_value);
                }
                ImGui::EndTable();
            }
            if (!movement.flee_stars.empty()) {
                ImGui::TextUnformatted("Avoided stars:");
                for (size_t index = 0; index < movement.flee_stars.size(); ++index) {
                    if (index != 0) ImGui::SameLine();
                    ImGui::Text("%d", (int)movement.flee_stars[index]);
                }
            }
            if (!movement.nonplayer_stars.empty()) {
                ImGui::TextUnformatted("Monster-occupied stars:");
                for (size_t index = 0; index < movement.nonplayer_stars.size(); ++index) {
                    if (index != 0) ImGui::SameLine();
                    ImGui::Text("%d", (int)movement.nonplayer_stars[index]);
                }
            }
        }

        void Draw_Decisions_() {
            const ai_state::s_snapshot* snapshot = Snapshot_();
            if (snapshot == nullptr) {
                ImGui::TextDisabled("No AI turn snapshot is available yet.");
            } else {
                Draw_Snapshot_Header_(snapshot);
                Player_Selector_("Player", &g_player, snapshot);
                if (ImGui::BeginTabBar("decision_tabs")) {
                        if (ImGui::BeginTabItem("Colony allocation")) {
                            Draw_Colony_Decisions_(snapshot, g_player);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Movement and expansion")) {
                            Draw_Movement_Decisions_(snapshot, g_player);
                            ImGui::EndTabItem();
                        }
                    ImGui::EndTabBar();
                }
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            if (ImGui::BeginTabBar("ai_snoop_tabs")) {
                if (ImGui::BeginTabItem("Empire")) {
                    Draw_Empire_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Worlds")) {
                    Draw_Worlds_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Fleets")) {
                    Draw_Fleets_();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Decisions")) {
                    Draw_Decisions_();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        debug_overlay::s_global_pane_registration g_registration("AI", Draw_);
    } // namespace
} // namespace AI_SNOOPS_DEBUG
#endif