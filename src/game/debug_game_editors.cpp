#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace debug_game_editors {
    namespace {
        constexpr const char* k_star_sizes[] = {"Small", "Medium", "Large"};
        constexpr const char* k_picture_types[] = {"Type 0", "Type 1", "Type 2"};

        struct s_star_editor_state {
            bool open;
            bool focus_requested;
            bool discard_requested;
            int16_t index;
            int16_t pending_index;
            s_star_data baseline;
            s_star_data draft;
        };

        struct s_planet_editor_state {
            bool open;
            bool focus_requested;
            bool discard_requested;
            game::id_planet index = game::NO_PLANET;
            game::id_planet pending_index = game::NO_PLANET;
            s_planet_data baseline;
            s_planet_data draft;
        };

        s_star_editor_state g_star_editor = {};
        s_planet_editor_state g_planet_editor = {};
        int16_t g_pending_planet_operation = -1;
        game::id_planet g_pending_planet_source = game::NO_PLANET;
        int16_t g_pending_planet_star = -1;
        int16_t g_pending_planet_orbit = -1;
        int8_t g_pending_colony_player = -1;
        bool g_planet_operation_popup_requested = false;
        const char* g_planet_operation_status = nullptr;

        bool Valid_Star(int16_t index) {
            return MOX::star != nullptr && index >= 0 &&
                index < std::clamp<int16_t>(MOX::NUM_STARS, 0, MAX_STARS);
        }

        bool Valid_Planet(game::s_world_state* world_state, game::id_planet id) {
            return game::Find_Planet(world_state, id) != nullptr;
        }

        void Request_Redraw() {
            SYS::Clear_System_Display_Loaded_Flag();
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            MOX::draw_main_screen_to_back = 1;
            video::Set_Refresh_Stencil();
        }

        void Request_Relationship_Refresh() {
            COLCALC::Compute_All_Star_Colony_Stuff();
            MAINSCR::Update_Star_Owners();
            Request_Redraw();
        }

        e_planet_operation_result Create_Colony(::game::s_world_state* world_state, ::game::id_planet planet_index, int8_t player_index) {
            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            if (!Valid_Planet(world_state, planet_index) || MOX::colony == nullptr ||
                player_index < 0 || player_index >= player_count) {
                return PLANET_OPERATION_INVALID_RELATION;
            }

            s_planet_data& planet = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index)));
            if (planet.planet_type != PLANET_TYPE_PLANET || planet.colony_index != -1 ||
                !Valid_Star(planet.star_index) || planet.orbit < 0 || planet.orbit >= 5 ||
                MOX::star[planet.star_index].planet_index[planet.orbit] != planet_index) {
                return PLANET_OPERATION_INVALID_RELATION;
            }

            const int16_t colony_count = std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES);
            int16_t colony_index = -1;
            for (int16_t index = 0; index < MAX_COLONIES; ++index) {
                if (MOX::colony[index].allocated_to == player_index) {
                    const bool free = index >= colony_count || MOX::colony[index].planet == ::game::NO_PLANET;
                    if (!free) {
                        return PLANET_OPERATION_INVALID_RELATION;
                    }
                    colony_index = index;
                    break;
                }
            }
            if (colony_index < 0) {
                for (int16_t index = 0; index < MAX_COLONIES; ++index) {
                    if (index >= colony_count || MOX::colony[index].planet == ::game::NO_PLANET) {
                        colony_index = index;
                        break;
                    }
                }
            }
            if (colony_index < 0) {
                return PLANET_OPERATION_NO_COLONY_CAPACITY;
            }

            const int8_t previous_allocation = MOX::colony[colony_index].allocated_to;
            MOX::colony[colony_index].allocated_to = player_index;
            COLONIZE::Make_New_Colony(legacy_world::Planet_Id(planet_index), player_index);
            if (planet.colony_index != colony_index ||
                MOX::colony[colony_index].planet != planet_index ||
                MOX::colony[colony_index].owner != player_index) {
                MOX::colony[colony_index].allocated_to = previous_allocation;
                return PLANET_OPERATION_INVALID_RELATION;
            }

            Request_Relationship_Refresh();
            return PLANET_OPERATION_OK;
        }

        bool Planet_In_Reports(::game::id_planet planet_index) {
            const int16_t count = std::clamp<int16_t>(REPORT::n_reports, 0, MAX_REPORTS);
            for (int16_t index = 0; index < count; ++index) {
                const s_report& report = REPORT::reports[index];
                if (((report.type == 1 || report.type == 2) && report.action == 1 &&
                        ((report.type == 1 && report.d.sabotage.planet_idx == planet_index) ||
                            (report.type == 2 && report.d.enemy_sabotage.planet_idx == planet_index))) ||
                    (report.type == 3 && report.d.artifacts.planet_idx == planet_index)) {
                    return true;
                }
            }
            return false;
        }

        bool Planet_In_Messages(::game::id_planet planet_index) {
            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            for (int16_t player = 0; player < player_count; ++player) {
                const int32_t first = MSG_::first_msg[player];
                int32_t count = std::min<int32_t>(MSG_::n_msgs[player], MSG_::max_msgs[player]);
                if (first < 0 || first >= MAX_MESSAGES) {
                    continue;
                }
                count = std::clamp<int32_t>(count, 0, MAX_MESSAGES - first);
                for (int32_t offset = 0; offset < count; ++offset) {
                    const s_MSG& message = MSG_::msgs[first + offset];
                    if ((message.msg_type == MSG_TYPE_COLONY &&
                            message.u.colony.planet_idx == planet_index) ||
                        (message.msg_type == MSG_TYPE_LOST_COLONY &&
                            message.u.lost_colony.planet_idx == planet_index)) {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Planet_Has_External_Reference(game::s_world_state* world_state, ::game::id_planet planet_index) {
            const int16_t star_count = std::clamp<int16_t>(MOX::NUM_STARS, 0, MAX_STARS);
            int32_t star_reference_count = 0;
            for (int16_t star = 0; star < star_count; ++star) {
                for (int16_t orbit = 0; orbit < 5; ++orbit) {
                    if (MOX::star[star].planet_index[orbit] == planet_index) {
                        ++star_reference_count;
                    }
                }
            }
            if (star_reference_count != 1) {
                return true;
            }

            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            for (int16_t player = 0; player < player_count; ++player) {
                const s_player& data = MOX::player[player];
                if (data.home_planet_id == planet_index || data.sneak_attack_planet == planet_index) {
                    return true;
                }
                for (int16_t other = 0; other < player_count; ++other) {
                    if (data.opportunity_planet_num[other] == planet_index) {
                        return true;
                    }
                }
                const auto found = world_state->settlers.find(legacy_world::Player_Id(player));
                if (found != world_state->settlers.end()) {
                    for (const auto& settler : found->second) if (settler.destination_planet == planet_index) return true;
                }
            }

            const int16_t colony_count = std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES);
            for (int16_t colony = 0; colony < colony_count; ++colony) {
                if (MOX::colony[colony].planet == planet_index) {
                    return true;
                }
            }

            if (NETMOX::net.combat_planet_id == planet_index) {
                return true;
            }
            for (const s_net_player_info& info : NETMOX::net.player_net_info) {
                if (info.combat_planet_id == planet_index) {
                    return true;
                }
            }
            if (NETMOX::net.combat_results_data != nullptr) {
                const s_battle_results& results = *NETMOX::net.combat_results_data;
                if (results.planet_id == planet_index ||
                    results.player_info.combat_planet_id == planet_index ||
                    results.info.captured_tech_planet_id == planet_index) {
                    return true;
                }
            }
            return Planet_In_Reports(legacy_world::Planet_Id(planet_index)) || Planet_In_Messages(legacy_world::Planet_Id(planet_index));
        }

        void Initialize_Unused_Planet(s_planet_data* planet) {
            memset(planet, 0, sizeof(*planet));
            planet->colony_index = -1;
            planet->star_index = -1;
        }

        bool Input_S8(const char* label, int8_t* value) {
            const int8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S8, value, &step, nullptr, "%d");
        }

        bool Input_U8(const char* label, uint8_t* value) {
            const uint8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U8, value, &step, nullptr, "%u");
        }

        bool Input_S16(const char* label, int16_t* value) {
            const int16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S16, value, &step, nullptr, "%d");
        }

        void Boolean_Byte(const char* label, uint8_t* value) {
            bool checked = *value != 0;
            if (ImGui::Checkbox(label, &checked)) {
                *value = checked ? 1 : 0;
            }
        }

        void Player_State(s_star_data& star) {
            if (!ImGui::CollapsingHeader("Player bitmaps")) {
                return;
            }

            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            for (int16_t player = 0; player < player_count; ++player) {
                ImGui::PushID((int32_t)player);
                char player_name[16];
                if (MOX::player != nullptr) {
                    strlcpy(player_name, MOX::player[player].race_name, sizeof(player_name));
                } else {
                    strlcpy(player_name, "Player", sizeof(player_name));
                }
                if (ImGui::TreeNode(player_name)) {
                    debug_game_widgets::Player_Bit("Blockaded", &star.blockaded, player);
                    debug_game_widgets::Player_Bit("Visited", &star.visited, player);
                    debug_game_widgets::Player_Bit("Just visited", &star.just_visited, player);
                    debug_game_widgets::Player_Bit("Ignore colony ships", &star.ignore_colony_ships, player);
                    debug_game_widgets::Player_Bit("Ignore combat ships", &star.ignore_combat_ships, player);
                    debug_game_widgets::Player_Bit("Has colony", &star.has_colony, player);
                    debug_game_widgets::Player_Bit(
                        "Has warp field interdictor", &star.has_warp_field_interdictor, player);
                    debug_game_widgets::Player_Bit("Has tachyon", &star.has_tachyon, player);
                    debug_game_widgets::Player_Bit("Has subspace", &star.has_subspace, player);
                    debug_game_widgets::Player_Bit("Has stargate", &star.has_stargate, player);
                    debug_game_widgets::Player_Bit("Has jumpgate", &star.has_jumpgate, player);
                    debug_game_widgets::Player_Bit("Has Artemis net", &star.has_artemis_net, player);
                    debug_game_widgets::Player_Bit(
                        "Has dimensional portal", &star.has_dimensional_portal, player);
                    ImGui::SeparatorText("Details");
                    Input_S8("Last planet selected", &star.last_orbit_selected[player]);
                    debug_game_widgets::Player_Bitmap("Blockaded by", &star.blockaded_by[player]);
                    debug_game_widgets::Officer_Reference(&game::world, "Officer", &star.officer_index[player]);
                    debug_game_widgets::Star_Reference("Relocate ships to", &star.relocate_ship_to[player]);
                    debug_game_widgets::Player_Reference("Surrender to", &star.surrender_to[player]);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        void Draw_Star_Form(s_star_data& star) {
            ImGui::InputText("Name", star.name, sizeof(star.name));
            Input_S16("X", &star.x);
            Input_S16("Y", &star.y);
            debug_game_widgets::Enum("Size", &star.size, k_star_sizes, 3);
            debug_game_widgets::Player_Reference("Owner", &star.owner, true, true);
            debug_game_widgets::Enum("Picture type", &star.pict_type, k_picture_types, 3);
            debug_game_widgets::Enum(
                "Spectral class", &star.spectral_class, MOX::spectral_class_string, STAR_CLASS_COUNT);
            debug_game_widgets::Enum(
                "System special", &star.system_special, MOX::planet_special_string, SYSTEM_SPECIAL_COUNT);
            debug_game_widgets::Star_Reference("Wormhole destination", &star.wormhole_star_id);
            debug_game_widgets::Player_Reference("Colonize player", &star.colonize_player);
            Boolean_Byte("Stage point", &star.is_stagepoint);
            Boolean_Byte("In nebula", &star.in_nebula);
            Boolean_Byte("Artifacts gave application", &star.artifacts_gave_app);

            if (ImGui::CollapsingHeader("Planet slots")) {
                const bool draft_dirty = memcmp(
                                             &star,
                                             &g_star_editor.baseline,
                                             sizeof(star)) != 0;
                ImGui::TextUnformatted("Indexes are read-only. Use relationship operations below.");
                if (draft_dirty) {
                    ImGui::TextUnformatted("Save or discard star changes before changing planet relationships.");
                }
                for (int32_t orbit = 0; orbit < 5; ++orbit) {
                    ImGui::PushID(orbit);
                    char label[24];
                    snprintf(label, sizeof(label), "Orbit %d", orbit + 1);
                    ImGui::BeginDisabled();
                    debug_game_widgets::Planet_Reference(&::game::world, label, &star.planet_index[orbit]);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (star.planet_index[orbit] != ::game::NO_PLANET) {
                        if (ImGui::SmallButton("Open")) {
                            Open_Planet(legacy_world::Planet_Id(star.planet_index[orbit]));
                        }
                        ImGui::SameLine();
                        ImGui::BeginDisabled(draft_dirty);
                        if (ImGui::SmallButton("Duplicate")) {
                            g_pending_planet_operation = 1;
                            g_planet_operation_popup_requested = true;
                            g_pending_planet_source = star.planet_index[orbit];
                            g_pending_planet_star = g_star_editor.index;
                        }
                        ImGui::EndDisabled();
                    } else {
                        ImGui::BeginDisabled(draft_dirty);
                        if (ImGui::SmallButton("Add planet")) {
                            g_pending_planet_operation = 0;
                            g_planet_operation_popup_requested = true;
                            g_pending_planet_source = game::NO_PLANET;
                            g_pending_planet_star = g_star_editor.index;
                            g_pending_planet_orbit = (int16_t)orbit;
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::PopID();
                }
            }

            Player_State(star);

            if (ImGui::CollapsingHeader("Advanced / internal")) {
                ImGui::TextUnformatted("These fields may be derived or used by runtime bookkeeping.");
                debug_game_widgets::Star_Reference("Next WFI in list", &star.next_wfi_in_list);
                Input_S8("Twinkling 1", &star.twinkling1);
                Input_S8("Twinkling 2", &star.twinkling2);
                Input_U8("Unused byte", &star.not_used3);
                if (ImGui::TreeNode("Black-hole blocked stars")) {
                    debug_game_widgets::Star_Bitmap(
                        "black_hole_blocks",
                        star.black_hole_blocks,
                        sizeof(star.black_hole_blocks),
                        2);
                    ImGui::TreePop();
                }
            }
        }

        void Draw_Planet_Form(s_planet_data& planet) {
            ImGui::TextUnformatted("Relationships are read-only to preserve reciprocal links.");
            const bool draft_dirty = memcmp(
                                         &planet,
                                         &g_planet_editor.baseline,
                                         sizeof(planet)) != 0;
            const int16_t colony_count = std::clamp<int16_t>(MOX::NUM_COLONIES, 0, MAX_COLONIES);
            const bool has_colony = MOX::colony != nullptr && planet.colony_index >= 0 &&
                planet.colony_index < colony_count;
            if (has_colony) {
                char race_name[32];
                const int8_t owner = MOX::colony[planet.colony_index].owner;
                const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
                if (owner >= 0 && owner < player_count) {
                    HACCESS::Race_Name(owner, race_name, sizeof(race_name));
                } else {
                    strlcpy(race_name, "Invalid owner", sizeof(race_name));
                }
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "Colony: %s", race_name);
            }
            ImGui::BeginDisabled();
            debug_game_widgets::Colony_Reference("Colony", &planet.colony_index);
            debug_game_widgets::Star_Reference("Star", &planet.star_index);
            Input_S8("Orbit", &planet.orbit);
            ImGui::EndDisabled();
            if (Valid_Star(planet.star_index) && ImGui::Button("Open related star")) {
                Open_Star(planet.star_index);
            }
            if (has_colony) {
                ImGui::SameLine();
                if (ImGui::Button("Open colony")) {
                    Open_Colony(planet.colony_index);
                }
            } else if (planet.planet_type == PLANET_TYPE_PLANET) {
                ImGui::SameLine();
                ImGui::BeginDisabled(draft_dirty);
                if (ImGui::Button("Create colony")) {
                    g_pending_planet_operation = 3;
                    g_pending_planet_source = g_planet_editor.index;
                    g_pending_colony_player = MOX::PLAYER_NUM >= 0 && MOX::PLAYER_NUM < MOX::NUM_PLAYERS
                        ? (int8_t)MOX::PLAYER_NUM
                        : 0;
                    g_planet_operation_popup_requested = true;
                }
                ImGui::EndDisabled();
            }
            debug_game_widgets::Enum(
                "Type",
                &planet.planet_type,
                MOX::planet_type_string,
                4,
                PLANET_TYPE_ASTEROID);
            debug_game_widgets::Enum("Size", &planet.size, MOX::planet_size_string, PLANET_SIZE_COUNT);
            debug_game_widgets::Enum(
                "Gravity", &planet.gravity_class, MOX::planet_gravity_string, PLANET_GRAVITY_COUNT);
            debug_game_widgets::Enum("Group", &planet.group, MOX::planet_group_string, PLANET_GROUP_COUNT);
            debug_game_widgets::Enum(
                "Climate", &planet.climate, MOX::planet_climate_string, PLANET_CLIMATE_COUNT);
            debug_game_widgets::Enum(
                "Atmosphere", &planet.climate_bg_type, MOX::planet_atmosphere_string, 3);
            debug_game_widgets::Enum(
                "Minerals", &planet.mineral_class, MOX::mineral_class_string, MINERAL_RESOURCE_COUNT);
            debug_game_widgets::Enum(
                "Special", &planet.planet_special, MOX::planet_special_string, SYSTEM_SPECIAL_COUNT);

            if (ImGui::CollapsingHeader("Derived / advanced")) {
                ImGui::TextUnformatted("Saving these values may bypass normal game recalculation.");
                Input_U8("Food per farmer", &planet.food_per_farmer);
                Input_U8("Terraform count", &planet.n_terraforms);
                Input_U8("Max farms", &planet.max_farms);
                Input_U8("Max population", &planet.max_population);
                Input_S8("Environmental alterations", &planet.environmental_alterations);
            }
        }

        template <typename T>
        bool Dirty(const T& draft, const T& baseline) {
            return memcmp(&draft, &baseline, sizeof(T)) != 0;
        }

        template <typename T>
        bool Merge_Field(const T& baseline, const T& draft, const T& live, T* merged) {
            if (memcmp(&draft, &baseline, sizeof(T)) == 0) {
                return true;
            }
            if (memcmp(&live, &baseline, sizeof(T)) != 0 &&
                memcmp(&live, &draft, sizeof(T)) != 0) {
                return false;
            }
            memcpy(merged, &draft, sizeof(T));
            return true;
        }

        bool Merge_Star(
            const s_star_data& baseline,
            const s_star_data& draft,
            const s_star_data& live,
            s_star_data* merged) {
            *merged = live;
            bool valid = true;
#define MERGE_STAR_FIELD(field) valid = Merge_Field(baseline.field, draft.field, live.field, &merged->field) && valid
            MERGE_STAR_FIELD(name);
            MERGE_STAR_FIELD(x);
            MERGE_STAR_FIELD(y);
            MERGE_STAR_FIELD(size);
            MERGE_STAR_FIELD(owner);
            MERGE_STAR_FIELD(pict_type);
            MERGE_STAR_FIELD(spectral_class);
            MERGE_STAR_FIELD(last_orbit_selected);
            MERGE_STAR_FIELD(black_hole_blocks);
            MERGE_STAR_FIELD(system_special);
            MERGE_STAR_FIELD(wormhole_star_id);
            MERGE_STAR_FIELD(blockaded);
            MERGE_STAR_FIELD(blockaded_by);
            MERGE_STAR_FIELD(visited);
            MERGE_STAR_FIELD(just_visited);
            MERGE_STAR_FIELD(ignore_colony_ships);
            MERGE_STAR_FIELD(ignore_combat_ships);
            MERGE_STAR_FIELD(colonize_player);
            MERGE_STAR_FIELD(has_colony);
            MERGE_STAR_FIELD(has_warp_field_interdictor);
            MERGE_STAR_FIELD(next_wfi_in_list);
            MERGE_STAR_FIELD(has_tachyon);
            MERGE_STAR_FIELD(has_subspace);
            MERGE_STAR_FIELD(has_stargate);
            MERGE_STAR_FIELD(has_jumpgate);
            MERGE_STAR_FIELD(has_artemis_net);
            MERGE_STAR_FIELD(has_dimensional_portal);
            MERGE_STAR_FIELD(is_stagepoint);
            MERGE_STAR_FIELD(officer_index);
            MERGE_STAR_FIELD(planet_index);
            MERGE_STAR_FIELD(relocate_ship_to);
            MERGE_STAR_FIELD(twinkling1);
            MERGE_STAR_FIELD(twinkling2);
            MERGE_STAR_FIELD(not_used3);
            MERGE_STAR_FIELD(surrender_to);
            MERGE_STAR_FIELD(in_nebula);
            MERGE_STAR_FIELD(artifacts_gave_app);
#undef MERGE_STAR_FIELD
            return valid;
        }

        bool Merge_Planet(
            const s_planet_data& baseline,
            const s_planet_data& draft,
            const s_planet_data& live,
            s_planet_data* merged) {
            *merged = live;
            bool valid = true;
#define MERGE_PLANET_FIELD(field) valid = Merge_Field(baseline.field, draft.field, live.field, &merged->field) && valid
            MERGE_PLANET_FIELD(colony_index);
            MERGE_PLANET_FIELD(star_index);
            MERGE_PLANET_FIELD(orbit);
            MERGE_PLANET_FIELD(planet_type);
            MERGE_PLANET_FIELD(size);
            MERGE_PLANET_FIELD(gravity_class);
            MERGE_PLANET_FIELD(group);
            MERGE_PLANET_FIELD(climate);
            MERGE_PLANET_FIELD(climate_bg_type);
            MERGE_PLANET_FIELD(mineral_class);
            MERGE_PLANET_FIELD(food_per_farmer);
            MERGE_PLANET_FIELD(n_terraforms);
            MERGE_PLANET_FIELD(max_farms);
            MERGE_PLANET_FIELD(max_population);
            MERGE_PLANET_FIELD(planet_special);
            MERGE_PLANET_FIELD(environmental_alterations);
#undef MERGE_PLANET_FIELD
            return valid;
        }

        bool Valid_Player_Reference(int8_t value, bool allow_contested = false) {
            if (value == -1 || (allow_contested && value == -2)) {
                return true;
            }
            return value >= 0 && value < std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        }

        bool Validate_Star(const s_star_data& star) {
            const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
            bool planet_selections_valid = true;
            for (int16_t player = 0; player < player_count; ++player) {
                planet_selections_valid = planet_selections_valid &&
                    star.last_orbit_selected[player] >= -1 &&
                    star.last_orbit_selected[player] < 5;
            }
            return planet_selections_valid && star.name[sizeof(star.name) - 1] == '\0' &&
                star.size >= 0 && star.size < 3 &&
                star.pict_type >= 0 && star.pict_type < 3 &&
                star.spectral_class >= 0 && star.spectral_class < STAR_CLASS_COUNT &&
                star.system_special >= 0 && star.system_special < SYSTEM_SPECIAL_COUNT &&
                Valid_Player_Reference(star.owner, true) &&
                Valid_Player_Reference(star.colonize_player);
        }

        bool Validate_Planet(const s_planet_data& planet) {
            return planet.planet_type >= PLANET_TYPE_ASTEROID &&
                planet.planet_type <= PLANET_TYPE_PLANET &&
                planet.size >= 0 && planet.size < PLANET_SIZE_COUNT &&
                planet.gravity_class >= 0 && planet.gravity_class < PLANET_GRAVITY_COUNT &&
                planet.group >= 0 && planet.group < PLANET_GROUP_COUNT &&
                planet.climate >= 0 && planet.climate < PLANET_CLIMATE_COUNT &&
                planet.climate_bg_type >= 0 && planet.climate_bg_type < 3 &&
                planet.mineral_class >= 0 && planet.mineral_class < MINERAL_RESOURCE_COUNT &&
                planet.planet_special >= 0 && planet.planet_special < SYSTEM_SPECIAL_COUNT;
        }

        void Load_Star(int16_t index) {
            g_star_editor.index = index;
            g_star_editor.pending_index = -1;
            g_star_editor.baseline = MOX::star[index];
            g_star_editor.draft = g_star_editor.baseline;
            g_star_editor.open = true;
            g_star_editor.focus_requested = true;
        }

        void Load_Planet(::game::s_world_state* world_state, game::id_planet index) {
            g_planet_editor.index = index;
            g_planet_editor.pending_index = game::NO_PLANET;
            g_planet_editor.baseline = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(index)));
            g_planet_editor.draft = g_planet_editor.baseline;
            g_planet_editor.open = true;
            g_planet_editor.focus_requested = true;
        }

        template <typename Id, typename Load>
        void Draw_Discard_Popup(
            const char* popup_id,
            bool* discard_requested,
            bool* editor_open,
            Id* pending_index,
            Id none,
            Load load_pending) {
            if (*discard_requested) {
                ImGui::OpenPopup(popup_id);
                *discard_requested = false;
            }
            if (ImGui::BeginPopupModal(popup_id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Discard unsaved changes?");
                if (ImGui::Button("Discard")) {
                    const Id next = *pending_index;
                    ImGui::CloseCurrentPopup();
                    if (next != none) {
                        load_pending(next);
                    } else {
                        *editor_open = false;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep editing")) {
                    *pending_index = none;
                    *editor_open = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void Draw_Star_Editor() {
            if (!g_star_editor.open) {
                return;
            }
            if (!Valid_Star(g_star_editor.index)) {
                g_star_editor.open = false;
                return;
            }

            if (g_star_editor.focus_requested) {
                ImGui::SetNextWindowFocus();
                g_star_editor.focus_requested = false;
            }
            ImGui::SetNextWindowSize(ImVec2(330.0f, 440.0f), ImGuiCond_FirstUseEver);
            char title[48];
            snprintf(title, sizeof(title), "Star Editor [%d]", (int)g_star_editor.index);
            bool open = g_star_editor.open;
            if (ImGui::Begin(title, &open)) {
                Draw_Star_Form(g_star_editor.draft);
                ImGui::Separator();
                const bool dirty = Dirty(g_star_editor.draft, g_star_editor.baseline);
                const bool valid = Validate_Star(g_star_editor.draft);
                s_star_data merged;
                const bool mergeable = Merge_Star(
                    g_star_editor.baseline,
                    g_star_editor.draft,
                    MOX::star[g_star_editor.index],
                    &merged);
                ImGui::BeginDisabled(!dirty || !valid || !mergeable);
                if (ImGui::Button("Save")) {
                    MOX::star[g_star_editor.index] = merged;
                    g_star_editor.baseline = merged;
                    g_star_editor.draft = merged;
                    Request_Redraw();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    if (dirty) {
                        g_star_editor.pending_index = -1;
                        g_star_editor.discard_requested = true;
                    } else {
                        open = false;
                    }
                }
                ImGui::SameLine();
                if (!mergeable) {
                    ImGui::TextUnformatted("Conflict with live game state");
                    if (ImGui::Button("Reload live data")) {
                        Load_Star(g_star_editor.index);
                    }
                } else {
                    ImGui::TextUnformatted(!valid ? "Invalid values" : (dirty ? "Unsaved changes" : "No changes"));
                }
            }
            ImGui::End();
            const bool dirty = Dirty(g_star_editor.draft, g_star_editor.baseline);
            if (!open && dirty) {
                g_star_editor.pending_index = -1;
                g_star_editor.discard_requested = true;
                open = true;
            }
            g_star_editor.open = open;
            Draw_Discard_Popup(
                "Discard star changes?",
                &g_star_editor.discard_requested,
                &g_star_editor.open,
                &g_star_editor.pending_index,
                int16_t{-1},
                Load_Star);
        }

        void Draw_Planet_Operation_Popup(::game::s_world_state* world_state) {
            if (g_planet_operation_popup_requested) {
                ImGui::OpenPopup("Confirm planet operation");
                g_planet_operation_popup_requested = false;
            }
            if (ImGui::BeginPopupModal(
                    "Confirm planet operation",
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
                const char* action = g_pending_planet_operation == 0
                    ? "add a new generated planet"
                    : (g_pending_planet_operation == 1
                              ? "duplicate this planet into the first free orbit"
                              : (g_pending_planet_operation == 2
                                        ? "delete this planet"
                                        : "create a colony on this planet"));
                ImGui::Text("Really %s?", action);
                if (g_pending_planet_operation == 3) {
                    debug_game_widgets::Player_Reference(
                        "Colonize for", &g_pending_colony_player, false);
                }
                const bool colony_player_valid = g_pending_planet_operation != 3 ||
                    (g_pending_colony_player >= 0 && g_pending_colony_player < MOX::NUM_PLAYERS);
                ImGui::BeginDisabled(!colony_player_valid);
                if (ImGui::Button("Confirm")) {
                    auto result_planet = game::NO_PLANET;
                    e_planet_operation_result result = PLANET_OPERATION_INVALID_RELATION;
                    if (g_pending_planet_operation == 0) {
                        result = Add_Planet(world_state, 
                            g_pending_planet_star,
                            g_pending_planet_orbit,
                            &result_planet);
                    } else if (g_pending_planet_operation == 1) {
                        result = Duplicate_Planet(world_state, 
                            legacy_world::Planet_Id(g_pending_planet_source),
                            g_pending_planet_star,
                            -1,
                            &result_planet);
                    } else if (g_pending_planet_operation == 2) {
                        result = Delete_Planet(world_state, legacy_world::Planet_Id(g_pending_planet_source));
                    } else {
                        result = Create_Colony(world_state, 
                            legacy_world::Planet_Id(g_pending_planet_source),
                            g_pending_colony_player);
                        result_planet = g_pending_planet_source;
                    }
                    g_planet_operation_status = Planet_Operation_Message(result);
                    if (result == PLANET_OPERATION_OK) {
                        if (g_pending_planet_operation == 2) {
                            g_planet_editor.open = false;
                        } else if (g_pending_planet_operation == 3) {
                            Load_Planet(world_state, result_planet);
                            Open_Colony(::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(result_planet))).colony_index);
                        } else {
                            Open_Planet(legacy_world::Planet_Id(result_planet));
                        }
                        if (Valid_Star(g_star_editor.index)) {
                            Load_Star(g_star_editor.index);
                        }
                    }
                    g_pending_planet_operation = -1;
                    g_pending_colony_player = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    g_pending_planet_operation = -1;
                    g_pending_colony_player = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void Draw_Planet_Editor(::game::s_world_state* world_state) {
            if (!g_planet_editor.open) {
                return;
            }
            if (!Valid_Planet(world_state, g_planet_editor.index)) {
                g_planet_editor.open = false;
                return;
            }

            if (g_planet_editor.focus_requested) {
                ImGui::SetNextWindowFocus();
                g_planet_editor.focus_requested = false;
            }
            ImGui::SetNextWindowSize(ImVec2(315.0f, 410.0f), ImGuiCond_FirstUseEver);
            char title[48];
            snprintf(title, sizeof(title), "Planet Editor [%u]", g_planet_editor.index.id);
            bool open = g_planet_editor.open;
            if (ImGui::Begin(title, &open)) {
                Draw_Planet_Form(g_planet_editor.draft);
                ImGui::Separator();
                const bool dirty = Dirty(g_planet_editor.draft, g_planet_editor.baseline);
                const bool valid = Validate_Planet(g_planet_editor.draft);
                s_planet_data merged;
                const bool mergeable = Merge_Planet(
                    g_planet_editor.baseline,
                    g_planet_editor.draft,
                    ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(g_planet_editor.index))),
                    &merged);
                ImGui::BeginDisabled(!dirty || !valid || !mergeable);
                if (ImGui::Button("Save")) {
                    ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(g_planet_editor.index))) = merged;
                    g_planet_editor.baseline = merged;
                    g_planet_editor.draft = merged;
                    Request_Redraw();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    if (dirty) {
                        g_planet_editor.pending_index = game::NO_PLANET;
                        g_planet_editor.discard_requested = true;
                    } else {
                        open = false;
                    }
                }
                ImGui::SameLine();
                if (!mergeable) {
                    ImGui::TextUnformatted("Conflict with live game state");
                    if (ImGui::Button("Reload live data")) {
                        Load_Planet(world_state, g_planet_editor.index);
                    }
                } else {
                    ImGui::TextUnformatted(!valid ? "Invalid values" : (dirty ? "Unsaved changes" : "No changes"));
                }
                ImGui::Separator();
                ImGui::BeginDisabled(dirty);
                if (ImGui::Button("Delete planet")) {
                    g_pending_planet_operation = 2;
                    g_planet_operation_popup_requested = true;
                    g_pending_planet_source = g_planet_editor.index;
                }
                ImGui::EndDisabled();
                if (dirty) {
                    ImGui::TextUnformatted("Save or discard changes before deleting.");
                }
            }
            ImGui::End();
            const bool dirty = Dirty(g_planet_editor.draft, g_planet_editor.baseline);
            if (!open && dirty) {
                g_planet_editor.pending_index = game::NO_PLANET;
                g_planet_editor.discard_requested = true;
                open = true;
            }
            g_planet_editor.open = open;
            Draw_Discard_Popup(
                "Discard planet changes?",
                &g_planet_editor.discard_requested,
                &g_planet_editor.open,
                &g_planet_editor.pending_index,
                game::NO_PLANET,
                [](game::id_planet index) { Load_Planet(&game::world, index); });
        }

        void Draw(const debug_overlay::s_panel_context& context) {
            (void)context;
            Draw_Star_Editor();
            Draw_Planet_Editor(&::game::world);
            Draw_Planet_Operation_Popup(&::game::world);
            if (g_planet_operation_status != nullptr) {
                ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Appearing);
                if (ImGui::Begin("Planet operation result")) {
                    ImGui::TextWrapped("%s", g_planet_operation_status);
                    if (ImGui::Button("Dismiss")) {
                        g_planet_operation_status = nullptr;
                    }
                }
                ImGui::End();
            }
        }

        debug_overlay::s_screen_panel_registration g_registration(
            debug_overlay::k_all_screens,
            Draw);
    } // namespace

    e_planet_operation_result Add_Planet(game::s_world_state* world_state, int16_t star_index, int16_t orbit, game::id_planet* result) {
        if (!Valid_Star(star_index) || orbit < 0 || orbit >= 5 || MOX::star[star_index].planet_index[orbit] != game::NO_PLANET) return PLANET_OPERATION_INVALID_RELATION;
        auto id = game::Create_Planet(world_state, s_planet_data{});
        if (!id) return PLANET_OPERATION_NO_CAPACITY;
        HOMEGEN::Make_One_Planet(world_state, star_index, *id, orbit, 0);
        if (result != nullptr) *result = *id;
        Request_Relationship_Refresh();
        return PLANET_OPERATION_OK;
    }

    e_planet_operation_result Duplicate_Planet(game::s_world_state* world_state, game::id_planet source, int16_t star_index, int16_t orbit, game::id_planet* result) {
        const auto* original = game::Find_Planet(world_state, source);
        if (original == nullptr || !Valid_Star(star_index)) return PLANET_OPERATION_INVALID_RELATION;
        if (orbit < 0) {
            for (int16_t candidate = 0; candidate < 5; ++candidate) {
                if (MOX::star[star_index].planet_index[candidate] == game::NO_PLANET) { orbit = candidate; break; }
            }
        }
        if (orbit < 0 || orbit >= 5) return PLANET_OPERATION_NO_FREE_ORBIT;
        if (MOX::star[star_index].planet_index[orbit] != game::NO_PLANET) return PLANET_OPERATION_INVALID_RELATION;
        auto planet = *original;
        planet.star_index = star_index;
        planet.orbit = static_cast<int8_t>(orbit);
        planet.colony_index = -1;
        auto id = game::Create_Planet(world_state, planet);
        if (!id) return PLANET_OPERATION_NO_CAPACITY;
        MOX::star[star_index].planet_index[orbit] = *id;
        if (result != nullptr) *result = *id;
        Request_Relationship_Refresh();
        return PLANET_OPERATION_OK;
    }

    e_planet_operation_result Delete_Planet(::game::s_world_state* world_state, ::game::id_planet planet_index) {
        if (!Valid_Planet(world_state, planet_index)) {
            return PLANET_OPERATION_INVALID_RELATION;
        }
        const s_planet_data planet = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index)));
        if (planet.colony_index != -1) {
            return PLANET_OPERATION_HAS_COLONY;
        }
        if (Planet_Has_External_Reference(world_state, legacy_world::Planet_Id(planet_index))) {
            return PLANET_OPERATION_IS_REFERENCED;
        }
        if (!Valid_Star(planet.star_index) || planet.orbit < 0 || planet.orbit >= 5 ||
            MOX::star[planet.star_index].planet_index[planet.orbit] != planet_index) {
            return PLANET_OPERATION_INVALID_RELATION;
        }

        MOX::star[planet.star_index].planet_index[planet.orbit] = ::game::NO_PLANET;
        game::Erase_Planet(world_state, planet_index);
        PLNTSUM::explored_planets.erase(planet_index);
        if (COLONY::g_scanned_planet == planet_index) {
            COLONY::g_scanned_planet = ::game::NO_PLANET;
        }
        if (COLONY::planet_display_handle == planet_index) {
            COLONY::planet_display_handle = ::game::NO_PLANET;
        }
        if (COLONY::planet_handle == planet_index) {
            COLONY::planet_handle = ::game::NO_PLANET;
        }
        Request_Relationship_Refresh();
        return PLANET_OPERATION_OK;
    }

    const char* Planet_Operation_Message(e_planet_operation_result result) {
        switch (result) {
            case PLANET_OPERATION_OK: return "Planet relationship updated successfully.";
            case PLANET_OPERATION_INVALID_RELATION: return "The star/planet relationship is invalid or the orbit is occupied.";
            case PLANET_OPERATION_NO_FREE_ORBIT: return "The target star has no free orbit.";
            case PLANET_OPERATION_NO_CAPACITY: return "Planet storage is full.";
            case PLANET_OPERATION_NOT_DENSE_TAIL: return "Only the last dense planet record can be deleted safely without remapping every planet ID.";
            case PLANET_OPERATION_HAS_COLONY: return "A planet with a colony or outpost cannot be deleted.";
            case PLANET_OPERATION_IS_REFERENCED: return "The planet is referenced by live game state and cannot be deleted safely.";
            case PLANET_OPERATION_NO_COLONY_CAPACITY: return "Colony storage is full.";
            default: return "Unknown planet operation result.";
        }
    }

    void Open_Star(int16_t star_index) {
        if (!Valid_Star(star_index)) {
            return;
        }
        if (g_star_editor.open && g_star_editor.index == star_index) {
            g_star_editor.focus_requested = true;
            return;
        }
        if (g_star_editor.open && g_star_editor.index != star_index &&
            Dirty(g_star_editor.draft, g_star_editor.baseline)) {
            g_star_editor.pending_index = star_index;
            g_star_editor.discard_requested = true;
            return;
        }
        Load_Star(star_index);
    }

    void Open_Planet(::game::id_planet planet_index) {
        if (!Valid_Planet(&::game::world, planet_index)) {
            return;
        }
        if (g_planet_editor.open && g_planet_editor.index == planet_index) {
            g_planet_editor.focus_requested = true;
            return;
        }
        if (g_planet_editor.open && g_planet_editor.index != planet_index &&
            Dirty(g_planet_editor.draft, g_planet_editor.baseline)) {
            g_planet_editor.pending_index = planet_index;
            g_planet_editor.discard_requested = true;
            return;
        }
        Load_Planet(&::game::world, planet_index);
    }
} // namespace debug_game_editors
#else
namespace debug_game_editors {
    e_planet_operation_result Add_Planet(::game::s_world_state* world_state, int16_t star_index, int16_t orbit, game::id_planet* new_planet_index) {
        (void)star_index;
        (void)orbit;
        (void)new_planet_index;
        return PLANET_OPERATION_INVALID_RELATION;
    }

    e_planet_operation_result Duplicate_Planet(::game::s_world_state* world_state, 
        ::game::id_planet source_planet_index,
        int16_t target_star_index,
        int16_t orbit,
        game::id_planet* new_planet_index) {
        (void)source_planet_index;
        (void)target_star_index;
        (void)orbit;
        (void)new_planet_index;
        return PLANET_OPERATION_INVALID_RELATION;
    }

    e_planet_operation_result Delete_Planet(::game::s_world_state* world_state, ::game::id_planet planet_index) {
        (void)planet_index;
        return PLANET_OPERATION_INVALID_RELATION;
    }

    const char* Planet_Operation_Message(e_planet_operation_result result) {
        (void)result;
        return "Debug overlay is disabled.";
    }

    void Open_Star(int16_t star_index) {
        (void)star_index;
    }

    void Open_Planet(::game::id_planet planet_index) {
        (void)planet_index;
    }
} // namespace debug_game_editors
#endif