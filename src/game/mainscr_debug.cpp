#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace MAINSCR_DEBUG {
    namespace {
        void Race_Name_(int16_t owner, char* name, size_t name_size) {
            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if ((owner >= 0 && owner < player_count) ||
                (owner >= MAX_PLAYERS && owner <= NONPLAYER_SHIP_TYPE_HYDRA)) {
                HACCESS::Race_Name(owner, name, (uint32_t)name_size);
            } else {
                strlcpy(name, "None", name_size);
            }
        }

        int16_t Selected_Star_() {
            const int16_t star_count = std::clamp<int16_t>(MOX::NUM_STARS, 0, MAX_STARS);
            if (MOX::system_star_id >= 0 && MOX::system_star_id < star_count) {
                return MOX::system_star_id;
            }
            return -1;
        }

        void Draw_Planet_(int16_t planet_index) {
            const int16_t planet_count = std::clamp<int16_t>(MOX::planet_count, 0, MAX_PLANETS);
            if (MOX::planet == nullptr || planet_index < 0 || planet_index >= planet_count) {
                return;
            }

            const s_planet_data& planet = MOX::planet[planet_index];
            char name[64];
            const int16_t star_count = std::min<int16_t>(MOX::NUM_STARS, MAX_STARS);
            if (MOX::star != nullptr && planet.star_index >= 0 && planet.star_index < star_count) {
                HACCESS::Do_Get_Planet_Name(planet_index, name, 1);
            } else {
                snprintf(name, sizeof(name), "Planet %d", (int)planet_index);
            }
            char label[112];
            const int16_t colony_count = std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES);
            if (MOX::colony != nullptr && planet.colony_index >= 0 && planet.colony_index < colony_count) {
                char race_name[32];
                Race_Name_(MOX::colony[planet.colony_index].owner, race_name, sizeof(race_name));
                snprintf(label, sizeof(label), "%s [Colony: %s]", name, race_name);
            } else {
                strlcpy(label, name, sizeof(label));
            }
            ImGui::PushID((int32_t)planet_index);
            if (ImGui::Selectable(label)) {
                debug_game_editors::Open_Planet(planet_index);
            }
            ImGui::PopID();
        }

        void Draw_Star_(int16_t star_index, int16_t selected_star, bool reveal_selection) {
            s_star_data& star = MOX::star[star_index];
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
                Draw_Planet_(star.planet_index[orbit]);
            }
            ImGui::TreePop();
        }

        void Draw_Star_List_(const debug_overlay::s_panel_context& context) {
            static int16_t previous_selected_star = -1;
            static ImGuiTextFilter star_filter;
            const int16_t selected_star = Selected_Star_();
            const bool reveal_selection = context.screen_entered || selected_star != previous_selected_star;

            ImGui::Text("Stars: %d | Selected: %d | Hovered: %d",
                (int)MOX::NUM_STARS, (int)selected_star,
                (int)MAINSCR::main_screen_scanned_star);
            star_filter.Draw("Filter by name", -1.0f);
            ImGui::Separator();
            if (MOX::star == nullptr || MOX::NUM_STARS <= 0) {
                ImGui::TextUnformatted("No galaxy is loaded.");
            } else {
                if (ImGui::BeginChild("stars")) {
                    const int16_t star_count = std::min<int16_t>(MOX::NUM_STARS, MAX_STARS);
                    for (int16_t star_index = 0; star_index < star_count; ++star_index) {
                        char star_name[sizeof(MOX::star[star_index].name) + 1];
                        strlcpy(star_name, MOX::star[star_index].name, sizeof(star_name));
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
            ImGui::Text("Ships: %d", (int)MOX::NUM_SHIPS);
            ImGui::SameLine();
            ImGui::Checkbox("Current race only", &current_player_only);
            ship_filter.Draw("Filter ships", -1.0f);
            ImGui::Separator();
            if (MOX::ship == nullptr || MOX::NUM_SHIPS <= 0) {
                ImGui::TextUnformatted("No ships are loaded.");
            } else {
                if (ImGui::BeginChild("ships")) {
                    const int16_t ship_count = std::clamp<int16_t>(MOX::NUM_SHIPS, 0, MAX_SHIPS);
                    for (int16_t ship_index = 0; ship_index < ship_count; ++ship_index) {
                        const s_ship_data& ship = MOX::ship[ship_index];
                        if (current_player_only && ship.owner != context.player_id) {
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
                Race_Name_(context.player_id, race_name, sizeof(race_name));
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