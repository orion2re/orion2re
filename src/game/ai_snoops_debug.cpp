#include "pch.h"

#include "legacy_world.h"

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

        const AIDATA::s_state* State_() {
            return AIDATA::state.initialized ? &AIDATA::state : nullptr;
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

        void Draw_Colony_Assessments_(const AIDATA::s_state* ai) {
            if (ImGui::BeginTable("colony_assessments", 7,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 410.0f))) {
                ImGui::TableSetupColumn("Colony");
                ImGui::TableSetupColumn("Race");
                ImGui::TableSetupColumn("Pop limit");
                ImGui::TableSetupColumn("Other-player arrivals");
                ImGui::TableSetupColumn("Conquered population");
                ImGui::TableSetupColumn("Food buildings relevant");
                ImGui::TableSetupColumn("Pollution controls relevant");
                ImGui::TableHeadersRow();
                for (int colony = 0; colony < MOX::NUM_COLONIES; ++colony) {
                    auto entry = ai->colonies.find(legacy_world::Colony_Id(colony));
                    if (entry == ai->colonies.end()) continue;
                    const AIDATA::s_colony_assessment& info = entry->second;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(colony);
                    if (ImGui::Selectable(std::to_string(colony).c_str())) {
                        debug_game_editors::Open_Colony((int16_t)colony);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%u", legacy_world::Slot(info.dominant_race));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)info.population_limit);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(info.other_player_arrivals ? "yes" : "no");
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", (int)info.conquered_population);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%d", (int)info.food_buildings_relevant);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%d", (int)info.pollution_controls_relevant);
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

        bool Player_Selector_(const char* label, int* player) {
            if (MOX::NUM_PLAYERS <= 0) return false;
            *player = std::clamp(*player, 0, (int)MOX::NUM_PLAYERS - 1);
            char preview[64];
            bool changed = false;
            if (ImGui::BeginCombo(label, Player_Name_(*player, preview, sizeof(preview)))) {
                for (int index = 0; index < MOX::NUM_PLAYERS; ++index) {
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

        void Draw_State_Header_(const AIDATA::s_state* ai) {
            ImGui::Text("AI state: %d.%d | %s",
                        (int)(ai->stardate / 10),
                        (int)(ai->stardate % 10),
                        ai->complete ? "completed" : "computing");
            ImGui::Text("Assessments: %zu ships, %zu stars, %zu planets, %zu colonies",
                ai->ships.size(), ai->stars.size(),
                ai->planets.size(), ai->colonies.size());
        }

        void Draw_Build_Needs_(const AIDATA::s_build_needs* needs, int player) {
            const AIDATA::s_fleet_composition& ships = needs->ship_sizes;
            if (ImGui::BeginTable("build_needs", 2,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                Stat_("War level", AIBUILD::Player_War_Level(&game_world::current, legacy_world::Player_Id(player)));
                Stat_("Combat ships", ships.combat_ship_count);
                Stat_("Weighted ship size", ships.weighted_ship_size);
                Stat_("Target ship size", ships.target_ship_size);
                Stat_("Command points used", ships.combat_command_points_used);
                Stat_("Active command points", ships.active_combat_command_points_used);
                Stat_("Strength vs ships", needs->combat_strength.total);
                Stat_("Strength wanted", needs->combat_strength.additional_wanted);
                Stat_("Strength vs colonies", needs->total_strength_vs_colonies);
                Stat_("Transports current", needs->transports.current);
                Stat_("Transports wanted", needs->transports.additional_wanted);
                Stat_("Colony ships current", needs->colony_ships.current);
                Stat_("Colony ships wanted", needs->colony_ships.additional_wanted);
                Stat_("Colony ships en route", needs->colony_ships.enroute);
                Stat_("Spies current", needs->spies.current);
                Stat_("Spies wanted", needs->spies.additional_wanted);
                Stat_("Freighter groups wanted", needs->freighter_groups_wanted);
                ImGui::EndTable();
            }
            ImGui::TextUnformatted("Ships by hull class:");
            for (int size = 0; size < HULL_CLASS_COUNT; ++size) {
                if (size != 0) ImGui::SameLine();
                ImGui::Text("%d: %d", size, (int)ships.ship_counts[size]);
            }
        }

        void Draw_Empire_() {
            const AIDATA::s_state* ai = State_();
            if (ai == nullptr) {
                ImGui::TextDisabled("No AI turn has run yet.");
            } else {
                Draw_State_Header_(ai);
                Player_Selector_("Player", &g_player);
                const int player = g_player;
                const AIDATA::s_empire_assessment& info = ai->players[player].empire;
                const AIDATA::s_technology_assessment& tech = ai->players[player].technology;
                const s_player* live = MOX::player == nullptr ? nullptr : &MOX::player[player];

                if (ImGui::BeginTabBar("empire_tabs")) {
                        if (ImGui::BeginTabItem("Summary")) {
                            ImGui::Text("Objective: %s", live == nullptr ? "Unknown" :
                                        Objective_Name_(live->objectives));
                            if (ImGui::BeginTable("empire_summary", 2,
                                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                                Stat_("Colonies", info.colony_count);
                                Stat_("Colony deficit", info.colony_deficit);
                                Stat_("Blockaded colonies", info.blockaded_colony_count);
                                Stat_("Colonized stars", info.colonized_star_count);
                                Stat_("Blockaded stars", info.blockaded_star_count);
                                Stat_("Contacts", info.contact_count);
                                Stat_("Reachable players", info.reachable_player_count);
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
                                for (int other = 0; other < MOX::NUM_PLAYERS; ++other) {
                                    char name[64];
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    ImGui::TextUnformatted(Player_Name_(other, name, sizeof(name)));
                                    ImGui::TableSetColumnIndex(1);
                                    ImGui::TextUnformatted(info.reachable_players[other] ? "yes" : "no");
                                    ImGui::TableSetColumnIndex(2);
                                    ImGui::Text("%d", (int)info.strength_vs_player[other]);
                                }
                                ImGui::EndTable();
                            }
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Build needs")) {
                            Draw_Build_Needs_(&ai->players[player].build, player);
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
                                Stat_("Best beam score", tech.best_beam_score);
                                Stat_("Beam delta", tech.beam_score_delta);
                                Stat_("Best missile score", tech.best_missile_score);
                                Stat_("Missile delta", tech.missile_score_delta);
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

        void Draw_Route_Result_(const char* label, const std::optional<AIDATA::s_route>* route) {
            if (*route) {
                ImGui::Text("%s: %u turns via %u", label,
                    (unsigned)(*route)->turns, legacy_world::Slot((*route)->next_star));
            } else {
                ImGui::Text("%s: unreachable", label);
            }
        }

        void Draw_Route_Query_(const AIDATA::s_route_cache* routes) {
            if (MOX::NUM_STARS <= 0 || MOX::NUM_PLAYERS <= 0) return;
            g_source_star = std::clamp(g_source_star, 0, (int)MOX::NUM_STARS - 1);
            g_dest_star = std::clamp(g_dest_star, 0, (int)MOX::NUM_STARS - 1);
            ImGui::SliderInt("Source star", &g_source_star, 0, MOX::NUM_STARS - 1);
            ImGui::SliderInt("Destination star", &g_dest_star, 0, MOX::NUM_STARS - 1);
            const auto turns = AIDATA::N_Turns_From_To(routes, legacy_world::Star_Id(g_source_star), legacy_world::Star_Id(g_dest_star), legacy_world::Player_Id(g_player));
            ImGui::Text("Direct turns: %s", turns ? std::to_string(*turns).c_str() : "unreachable");
            auto entry = routes->intermediate.find({legacy_world::Player_Id(g_player),
                legacy_world::Star_Id(g_source_star), legacy_world::Star_Id(g_dest_star)});
            if (entry == routes->intermediate.end()) {
                ImGui::TextDisabled("Intermediate route not computed.");
                return;
            }
            const auto& route = entry->second;
            Draw_Route_Result_("Normal", &route.normal);
            Draw_Route_Result_("Extended", &route.extended);
        }

        void Draw_Star_State_(const AIDATA::s_state* ai, int player) {
            if (MOX::NUM_STARS <= 0) return;
            g_star = std::clamp(g_star, 0, (int)MOX::NUM_STARS - 1);
            ImGui::SliderInt("Star", &g_star, 0, MOX::NUM_STARS - 1);
            char star_name[64];
            ImGui::TextUnformatted(Star_Name_(g_star, star_name, sizeof(star_name)));
            auto entry = ai->stars.find(legacy_world::Star_Id(g_star));
            if (entry == ai->stars.end()) return;
            const auto& star = entry->second;
            const AIDATA::s_star_neighborhood& info = star.neighborhood;
            const AIDATA::s_star_player_assessment& block = info.per_player[player];
            const uint8_t normal_mask = star.in_range;
            const uint8_t extended_mask = star.in_extended_range;
            if (ImGui::BeginTable("star_state", 2,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                Stat_("Planets present", info.planet_count);
                Stat_("Normal range mask", normal_mask);
                Stat_("Extended range mask", extended_mask);
                Stat_("Treaty partners in range", block.treaty_partners_in_range);
                Stat_("Neutral players in range", block.neutral_players_in_range);
                Stat_("Hostile players in range", block.hostile_players_in_range);
                Stat_("Players only in extended range", block.players_only_in_extended_range);
                Stat_("Covering colonies", block.covering_colony_count);
                Stat_("Other-player arrivals", block.other_player_arrivals);
                Stat_("Base stage weight", star.base_stage_weights[player]);
                Stat_("Stage weight", star.stage_weights[player]);
                const auto* group = ai->players[player].Find_Combat_Ships(legacy_world::Star_Id(g_star));
                if (group) Stat_("Combat ship cursor", group->next);
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
                    ImGui::Text("%d", (int)star.possible_strengths[player][g_opponent][turn]);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)star.possible_strengths[g_opponent][player][turn]);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", (int)star.enroute_strengths[g_opponent][player][turn]);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", (int)star.enroute_ships[g_opponent][turn]);
                }
                ImGui::EndTable();
            }
            if (ImGui::Button("Open star details")) {
                debug_game_editors::Open_Star((int16_t)g_star);
            }
        }

        void Draw_Planet_Values_(const AIDATA::s_state* ai, int player) {
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
                for (int planet = 0; planet < MOX::planet_count; ++planet) {
                    auto entry = ai->planets.find(legacy_world::Planet_Id(planet));
                    if (entry == ai->planets.end()) continue;
                    const uint16_t base = entry->second.base_values[player];
                    const uint16_t contextual = entry->second.contextual_values[player];
                    const int16_t stage = entry->second.stage_weights[player];
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
            const AIDATA::s_state* ai = State_();
            if (ai == nullptr) {
                ImGui::TextDisabled("No AI turn has run yet.");
            } else {
                Draw_State_Header_(ai);
                Player_Selector_("Player", &g_player);
                Player_Selector_("Opponent", &g_opponent);
                if (ImGui::BeginTabBar("world_tabs")) {
                        if (ImGui::BeginTabItem("Star threats")) {
                            Draw_Star_State_(ai, g_player);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Planet values")) {
                            Draw_Planet_Values_(ai, g_player);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Colony assessments")) {
                            Draw_Colony_Assessments_(ai);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Route cache")) {
                            Draw_Route_Query_(&ai->routes);
                            ImGui::EndTabItem();
                        }
                    ImGui::EndTabBar();
                }
            }
        }

        void Draw_Fleet_Table_(const AIDATA::s_state* ai) {
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
                for (game_world::t_ship_id ship_id : ai->combat_ships) {
                    const uint32_t ship = legacy_world::Slot(ship_id);
                    if (ship < 0 || ship >= MOX::NUM_SHIPS) continue;
                    auto entry = ai->ships.find(legacy_world::Ship_Id(ship));
                    if (entry == ai->ships.end()) continue;
                    const auto& assessment = entry->second;
                    const int desirability = assessment.refit_desirability;
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
                    ImGui::Text("%u", (unsigned)assessment.strengths[g_opponent]);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", (unsigned)assessment.strengths_in_nebula[g_opponent]);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%u %u %u %u",
                                (unsigned)assessment.bomb_strengths[0],
                                (unsigned)assessment.bomb_strengths[1],
                                (unsigned)assessment.bomb_strengths[2],
                                (unsigned)assessment.bomb_strengths[3]);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%d", desirability);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Ship_Route_(const AIDATA::s_route_cache* routes) {
            if (MOX::NUM_SHIPS <= 0 || MOX::NUM_STARS <= 0) return;
            g_ship = std::clamp(g_ship, 0, (int)MOX::NUM_SHIPS - 1);
            g_dest_star = std::clamp(g_dest_star, 0, (int)MOX::NUM_STARS - 1);
            ImGui::SliderInt("Ship", &g_ship, 0, MOX::NUM_SHIPS - 1);
            ImGui::SliderInt("Destination", &g_dest_star, 0, MOX::NUM_STARS - 1);
            const auto turns = AIDATA::Ship_N_Turns_To(routes, legacy_world::Ship_Id(g_ship), legacy_world::Star_Id(g_dest_star));
            ImGui::Text("Direct turns: %s", turns ? std::to_string(*turns).c_str() : "unreachable");
            auto entry = routes->ship_intermediate.find(AIDATA::Ship_Route_Key(legacy_world::Ship_Id(g_ship), legacy_world::Star_Id(g_dest_star)));
            if (entry == routes->ship_intermediate.end()) {
                ImGui::TextDisabled("Intermediate route not computed.");
                return;
            }
            const auto& route = entry->second;
            Draw_Route_Result_("Intermediate", &route);
        }

        void Draw_Refit_Queue_(const AIDATA::s_state* ai) {
            Player_Selector_("Player", &g_player);
            const auto& refits = ai->players[g_player].refits;
            const int count = (int)refits.size();
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
                    const auto& entry = refits[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", count - index);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(index);
                    if (ImGui::Selectable(std::to_string(legacy_world::Slot(entry.ship_id)).c_str())) {
                        auto ship_index = legacy_world::Index(entry.ship_id);
                        if (ship_index) debug_game_editors::Open_Ship(*ship_index);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)entry.desirability);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Fleets_() {
            const AIDATA::s_state* ai = State_();
            if (ai == nullptr) {
                ImGui::TextDisabled("No AI turn has run yet.");
            } else {
                Draw_State_Header_(ai);
                Player_Selector_("Opponent perspective", &g_opponent);
                if (ImGui::BeginTabBar("fleet_tabs")) {
                        if (ImGui::BeginTabItem("Strength and refits")) {
                            Draw_Fleet_Table_(ai);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Ship route cache")) {
                            Draw_Ship_Route_(&ai->routes);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Refit queues")) {
                            Draw_Refit_Queue_(ai);
                            ImGui::EndTabItem();
                        }
                    ImGui::EndTabBar();
                }
            }
        }

        void Draw_Colony_Decisions_(const AIDATA::s_colony_workspace* work) {
            const auto& decisions = *work;
            if (!decisions.complete) {
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
                for (uint32_t index = 0; index < decisions.colonies.size(); ++index) {
                    const AIDATA::s_colony_assignment& colony = decisions.colonies[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID((int)index);
                    if (ImGui::Selectable(std::to_string(legacy_world::Slot(colony.colony_id)).c_str())) {
                        auto colony_index = legacy_world::Index(colony.colony_id);
                        if (colony_index) debug_game_editors::Open_Colony(*colony_index);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(index < decisions.blockaded_count ? "yes" : "no");
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
                    ImGui::Text("%d / %d", (int)colony.best_reassignment_score,
                                (int)colony.worst_reassignment_score);
                }
                ImGui::EndTable();
            }
        }

        void Draw_Movement_Decisions_(const AIDATA::s_movement_workspace* work) {
            const auto& movement = *work;
            if (!movement.complete) {
                ImGui::TextDisabled("The movement phase did not run for this player.");
                return;
            }
            ImGui::Text("Remaining colonization candidates: %zu", movement.colonizable.size());
            if (ImGui::BeginTable("movement_candidates", 3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0.0f, 330.0f))) {
                ImGui::TableSetupColumn("Rank");
                ImGui::TableSetupColumn("Planet");
                ImGui::TableSetupColumn("Contextual value");
                ImGui::TableHeadersRow();
                for (uint32_t index = 0; index < movement.colonizable.size(); ++index) {
                    const AIDATA::s_colonization_candidate& planet = movement.colonizable[index];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%u", index + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID((int)index);
                    if (ImGui::Selectable(std::to_string(legacy_world::Slot(planet.planet_id)).c_str())) {
                        auto planet_index = legacy_world::Index(planet.planet_id);
                        if (planet_index) debug_game_editors::Open_Planet(*planet_index);
                    }
                    ImGui::PopID();
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", (int)planet.contextual_value);
                }
                ImGui::EndTable();
            }
            if (!movement.flee_stars.empty()) {
                ImGui::TextUnformatted("Avoided stars:");
                for (int star = 0; star < MOX::NUM_STARS; ++star) {
                    auto entry = movement.flee_stars.find(legacy_world::Star_Id(star));
                    if (entry == movement.flee_stars.end() || entry->second[AIDATA::LAST_FORECAST_TURN] == 0) continue;
                    ImGui::SameLine();
                    ImGui::Text("%d", star);
                }
            }
            if (!movement.nonplayer_here.empty()) {
                ImGui::TextUnformatted("Monster-occupied stars:");
                for (int star = 0; star < MOX::NUM_STARS; ++star) {
                    auto entry = movement.nonplayer_here.find(legacy_world::Star_Id(star));
                    if (entry == movement.nonplayer_here.end() || entry->second == legacy_world::Player_Id(0)) continue;
                    ImGui::SameLine();
                    ImGui::Text("%d", star);
                }
            }
        }

        void Draw_Decisions_() {
            const AIDATA::s_state* ai = State_();
            if (ai == nullptr) {
                ImGui::TextDisabled("No AI turn has run yet.");
            } else {
                Draw_State_Header_(ai);
                Player_Selector_("Player", &g_player);
                if (ImGui::BeginTabBar("decision_tabs")) {
                        if (ImGui::BeginTabItem("Colony allocation")) {
                            Draw_Colony_Decisions_(&ai->players[g_player].colony_work);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Movement and expansion")) {
                            Draw_Movement_Decisions_(&ai->players[g_player].movement);
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