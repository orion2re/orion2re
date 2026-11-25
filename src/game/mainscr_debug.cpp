#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace MAINSCR_DEBUG {
    namespace {
        void Race_Name_(game::owner_slot owner, char* name, size_t name_size) {
            const int16_t player_count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
            if ((owner.code >= 0 && owner.code < player_count) ||
                (owner.code >= MAX_PLAYERS && owner.code <= NONPLAYER_SHIP_TYPE_HYDRA)) {
                HACCESS::Race_Name(owner, name, (uint32_t)name_size);
            } else {
                strlcpy(name, "None", name_size);
            }
        }

        int16_t Selected_Star_() {
            const int16_t star_count = std::clamp<int16_t>(std::ssize(game::current_state.stars), 0, MAX_STARS);
            if (const auto position = game::Star_Row(game::current_state, MOX::system_star_id); position >= 0) {
                return position;
            }
            return -1;
        }

        void Draw_Planet_(game::id_planet planet_index) {
            if (!game::current_state.planets.contains(planet_index)) {
                return;
            }

            const s_planet_data& planet = game::current_state.planets.at(planet_index);
            const auto id_label = std::to_string(planet_index.id);
            char name[64];
            const int16_t star_count = std::min<int16_t>(std::ssize(game::current_state.stars), MAX_STARS);
            if (game::Star_Row(game::current_state, planet.star_id) >= 0 && game::Star_Row(game::current_state, planet.star_id) < star_count) {
                strlcpy(name, HACCESS::Get_Planet_Name(game::current_state, planet_index), sizeof(name));
            } else {
                snprintf(name, sizeof(name), "Planet %s", id_label.c_str());
            }
            char label[112];
            if (game::current_state.colonies.contains(planet.colony_id)) {
                char race_name[32];
                Race_Name_(factions::From_Player(game::current_state.colonies.at(planet.colony_id).owner), race_name, sizeof(race_name));
                snprintf(label, sizeof(label), "%s [Colony: %s]", name, race_name);
            } else {
                strlcpy(label, name, sizeof(label));
            }
            ImGui::PushID(id_label.c_str());
            if (ImGui::Selectable(label)) {
                debug_game_editors::Open_Planet(planet_index);
            }
            ImGui::PopID();
        }

        void Draw_Star_(int16_t star_index, int16_t selected_star, bool reveal_selection) {
            s_star_data& star = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_index));
            char star_name[sizeof(star.name) + 1];
            strlcpy(star_name, star.name, sizeof(star_name));
            const bool selected = star_index == selected_star;
            if (selected && reveal_selection) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_OpenOnArrow;
            if (selected) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            const bool open = ImGui::TreeNodeEx(
                (void*)(intptr_t)(star_index + 1),
                flags,
                "%s [%d]",
                star_name,
                (int)star_index);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
                debug_game_editors::Open_Star(star_index);
            }
            if (selected && reveal_selection) {
                ImGui::SetScrollHereY(0.5f);
            }

            if (!open) {
                return;
            }

            for (int32_t orbit = 0; orbit < 5; ++orbit) {
                Draw_Planet_(star.orbit_planet_ids[orbit]);
            }
            ImGui::TreePop();
        }

        void Draw_Star_List_(const debug_overlay::s_panel_context& context) {
            static int16_t previous_selected_star = -1;
            static ImGuiTextFilter star_filter;
            const int16_t selected_star = Selected_Star_();
            const bool reveal_selection = context.screen_entered || selected_star != previous_selected_star;

            ImGui::Text("Stars: %d | Selected: %d | Hovered: %d",
                (int)std::ssize(game::current_state.stars), (int)selected_star,
                (int)MAINSCR::main_screen_scanned_star);
            star_filter.Draw("Filter by name", -1.0f);
            ImGui::Separator();
            if (std::ssize(game::current_state.stars) <= 0) {
                ImGui::TextUnformatted("No galaxy is loaded.");
            } else {
                if (ImGui::BeginChild("stars")) {
                    const int16_t star_count = std::min<int16_t>(std::ssize(game::current_state.stars), MAX_STARS);
                    for (int16_t star_index = 0; star_index < star_count; ++star_index) {
                        char star_name[sizeof(game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_index)).name) + 1];
                        strlcpy(star_name, game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_index)).name, sizeof(star_name));
                        if (!star_filter.PassFilter(star_name)) {
                            continue;
                        }
                        Draw_Star_(star_index, selected_star, reveal_selection);
                    }
                }
                ImGui::EndChild();
            }
            previous_selected_star = selected_star;
        }

        void Draw_Ship_List_(const debug_overlay::s_panel_context& context) {
            static ImGuiTextFilter ship_filter;
            static bool current_player_only = false;
            ImGui::Text("Ships: %zu", game::current_state.ships.size());
            ImGui::SameLine();
            ImGui::Checkbox("Current race only", &current_player_only);
            ship_filter.Draw("Filter ships", -1.0f);
            ImGui::Separator();
            if (game::current_state.ships.empty()) {
                ImGui::TextUnformatted("No ships are loaded.");
            } else {
                if (ImGui::BeginChild("ships")) {
                    const int16_t ship_count = std::clamp<int16_t>(static_cast<int16_t>(game::current_state.ship_slots.size()), 0, MAX_SHIPS);
                    for (int16_t ship_index = 0; ship_index < ship_count; ++ship_index) {
                        if (game::current_state.ship_slots[ship_index] == game::NO_SHIP) continue;
                        const s_ship_data& ship = ship_slots::At(game::current_state, ship_index);
                        if (current_player_only && (context.player_id == game::NO_PLAYER || factions::Player(ship.owner) != context.player_id)) {
                            continue;
                        }
                        char ship_name[sizeof(ship.d.name) + 1];
                        strlcpy(ship_name, ship.d.name, sizeof(ship_name));
                        char race_name[32];
                        Race_Name_(ship.owner, race_name, sizeof(race_name));
                        char label[112];
                        snprintf(label, sizeof(label), "%s [%d] - %s", ship_name, (int)ship_index, race_name);
                        if (!ship_filter.PassFilter(label)) {
                            continue;
                        }
                        ImGui::PushID((int32_t)ship_index);
                        if (ImGui::Selectable(label)) {
                            debug_game_editors::Open_Ship(ship_index);
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            const int16_t selected_star = Selected_Star_();
            const float width = std::min(316.0f, ImGui::GetIO().DisplaySize.x - 16.0f);
            const float height = std::min(464.0f, ImGui::GetIO().DisplaySize.y - 16.0f);
            ImGui::SetNextWindowPos(
                ImVec2(ImGui::GetIO().DisplaySize.x - width - 8.0f, 8.0f),
                ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Main Screen Overview")) {
                char race_name[32];
                Race_Name_(factions::From_Player(context.player_id), race_name, sizeof(race_name));
                ImGui::Text("Race: %s", race_name);
                if (selected_star >= 0 && ImGui::Button("Edit selected star")) {
                    debug_game_editors::Open_Star(selected_star);
                }
                if (ImGui::BeginTabBar("main_screen_debug_tabs")) {
                    if (ImGui::BeginTabItem("Stars")) {
                        Draw_Star_List_(context);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Ships")) {
                        Draw_Ship_List_(context);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
        }

        debug_overlay::s_screen_panel_registration g_registration(SCREEN_MAIN, Draw_);
    } // namespace
} // namespace MAINSCR_DEBUG
#endif
