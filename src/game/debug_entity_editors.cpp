#include "pch.h"
#include "ship_slots.h"
#include "build_queue.h"
#include "legacy_ids.h"
#include "ship_ids.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

#include "lua/lua_game_ship_identity.h"

namespace debug_game_editors {
    namespace {
        constexpr const char* k_ship_sizes[] = {
            "Small", "Medium", "Large", "Huge", "Titan", "Doom Star"};
        constexpr const char* k_ship_types[] = {
            "Combat", "Colony", "Transport", "Unused", "Outpost"};
        constexpr const char* k_ship_statuses[] = {
            "Active", "In transit", "Moving", "Status 3", "Refitting", "Destroyed", "Building", "Scrap pending", "Scrap pending 2", "Retreating", "Antaran"};
        constexpr const char* k_population_jobs[] = {"Farmer", "Worker", "Scientist"};

        template <typename T, typename Key = int16_t>
        struct s_editor_state {
            bool open;
            bool focus_requested;
            bool discard_requested;
            Key index;
            Key pending_index;
            T baseline;
            T draft;
        };

        s_editor_state<s_ship_data, game::id_ship> g_ship_editor = {};
        s_editor_state<s_colony, game::id_colony> g_colony_editor = {};
        int8_t g_new_population_race = 0;
        int8_t g_new_population_job = POP::PROF_FARMER;
        int16_t g_autobuild_preset = 0;
        const char* g_colony_operation_status = nullptr;
        bool g_convert_outpost_requested = false;
        game::id_colony g_pending_outpost_colony = game::NO_COLONY;

        void Load_Colony_(game::id_colony index);

        bool Valid_Ship_(game::id_ship ship) {
            return ship != game::NO_SHIP && game::current_state.ships.contains(ship);
        }

        bool Ship_Inactive_(const s_ship_data& ship) {
            return ship.status == 5 || ship.status == 7 || ship.status == 8;
        }

        bool Valid_Colony_(game::id_colony index) {
            return game::current_state.colonies.contains(index);
        }

        void Request_Redraw_() {
            SYS::Clear_System_Display_Loaded_Flag();
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            MOX::draw_main_screen_to_back = 1;
            video::Set_Refresh_Stencil();
        }

        void Refresh_Ship_State_() {
            const int16_t player_count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
            for (int16_t player = 0; player < player_count; ++player) {
                game::current_state.players[player].total_ships = 0;
            }
            const int16_t ship_count = std::clamp<int16_t>(static_cast<int16_t>(game::current_state.ship_slots.size()), 0, MAX_SHIPS);
            for (int16_t ship_index = 0; ship_index < ship_count; ++ship_index) {
                if (game::current_state.ship_slots[ship_index] == game::NO_SHIP) continue;
                const s_ship_data& ship = ship_slots::At(game::current_state, ship_index);
                if (factions::Legacy_Code(ship.owner) >= 0 && factions::Legacy_Code(ship.owner) < player_count && ship.status < 5 &&
                    ship.d.ship_type == SHIP_TYPE_COMBAT &&
                    ship.d.size >= 0 && ship.d.size < SHIP_SIZE_COUNT) {
                    game::current_state.players.at(factions::Player(ship.owner).id).total_ships += (1 << ship.d.size);
                }
            }
            SHIPS::Build_Ship_Icons();
            Request_Redraw_();
        }

        void Refresh_Colony_State_() {
            COLCALC::Compute_All_Star_Colony_Stuff();
            COLCALC::All_Colony_Calculations();
            MAINSCR::Update_Star_Owners();
            Request_Redraw_();
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

        bool Input_S32_(const char* label, int32_t* value) {
            const int32_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S32, value, &step, nullptr, "%d");
        }

        bool Ship_Owner_(int8_t* owner) {
            char preview[32];
            const int16_t player_count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
            if ((*owner >= 0 && *owner < player_count) ||
                (*owner >= MAX_PLAYERS && *owner <= NONPLAYER_SHIP_TYPE_HYDRA)) {
                HACCESS::Race_Name(factions::From_Legacy(*owner), preview, sizeof(preview));
            } else {
                strlcpy(preview, "Invalid", sizeof(preview));
            }
            bool changed = false;
            if (ImGui::BeginCombo("Owner", preview)) {
                for (int16_t index = 0; index <= NONPLAYER_SHIP_TYPE_HYDRA; ++index) {
                    if (index >= player_count && index < MAX_PLAYERS) {
                        continue;
                    }
                    char name[32];
                    HACCESS::Race_Name(factions::From_Legacy(index), name, sizeof(name));
                    ImGui::PushID((int32_t)index);
                    if (ImGui::Selectable(name, *owner == index)) {
                        *owner = (int8_t)index;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        const char* Population_Race_Name_(int8_t race, char* buffer, size_t buffer_size) {
            if (race == POP::RACE_ANDROID) {
                return "Android";
            }
            if (race == POP::RACE_NATIVE) {
                return "Native";
            }
            if (race >= 0 && race < MAX_PLAYERS) {
                snprintf(buffer, buffer_size, "%d: %s", (int)race, game::current_state.players[race].race_name);
                return buffer;
            }
            snprintf(buffer, buffer_size, "Race %d", (int)race);
            return buffer;
        }

        bool Population_Race_(const char* label, int8_t* race) {
            char preview_buffer[32];
            const char* preview = Population_Race_Name_(*race, preview_buffer, sizeof(preview_buffer));
            bool changed = false;
            if (ImGui::BeginCombo(label, preview)) {
                for (int8_t candidate = 0; candidate <= POP::RACE_NATIVE; ++candidate) {
                    char name_buffer[32];
                    const char* name = Population_Race_Name_(
                        candidate, name_buffer, sizeof(name_buffer));
                    ImGui::PushID((int32_t)candidate);
                    if (ImGui::Selectable(name, *race == candidate)) {
                        *race = candidate;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        template <typename T, typename Key>
        bool Dirty_(const s_editor_state<T, Key>& editor) {
            return editor.draft != editor.baseline;
        }

        template <typename T>
        bool Merge_Field_(const T& baseline, const T& draft, const T& live, T* merged) {
            if (memcmp(&draft, &baseline, sizeof(T)) == 0) {
                return true;
            }
            if (memcmp(&live, &baseline, sizeof(T)) != 0 && memcmp(&live, &draft, sizeof(T)) != 0) {
                return false;
            }
            memcpy(merged, &draft, sizeof(T));
            return true;
        }

        template <size_t N>
        bool Merge_Ship_Weapons_(const s_ship_weapons (&baseline)[N], const s_ship_weapons (&draft)[N],
                                 const s_ship_weapons (&live)[N], s_ship_weapons (*merged)[N]) {
            if (std::ranges::equal(draft, baseline)) return true;
            if (!std::ranges::equal(live, baseline) && !std::ranges::equal(live, draft)) return false;
            std::ranges::copy(draft, *merged);
            return true;
        }

        bool Merge_Ship_(const s_ship_data& baseline, const s_ship_data& draft, const s_ship_data& live, s_ship_data* merged) {
            *merged = live;
            bool valid = true;
#define MERGE_SHIP_FIELD(field) valid = Merge_Field_(baseline.field, draft.field, live.field, &merged->field) && valid
            MERGE_SHIP_FIELD(d.name);
            MERGE_SHIP_FIELD(d.size);
            MERGE_SHIP_FIELD(d.ship_type);
            MERGE_SHIP_FIELD(d.shield_type);
            MERGE_SHIP_FIELD(d.ftl_type);
            MERGE_SHIP_FIELD(d.speed);
            MERGE_SHIP_FIELD(d.computer_type);
            MERGE_SHIP_FIELD(d.armor_type);
            MERGE_SHIP_FIELD(d.special_device_flags);
            valid = Merge_Ship_Weapons_(baseline.d.ship_weapon, draft.d.ship_weapon,
                                        live.d.ship_weapon, &merged->d.ship_weapon) && valid;
            MERGE_SHIP_FIELD(d.picture_num);
            MERGE_SHIP_FIELD(d.previous_owner);
            MERGE_SHIP_FIELD(d.cost);
            MERGE_SHIP_FIELD(d.combat_speed);
            MERGE_SHIP_FIELD(d.date_of_design);
            MERGE_SHIP_FIELD(owner);
            MERGE_SHIP_FIELD(status);
            MERGE_SHIP_FIELD(position);
            MERGE_SHIP_FIELD(x);
            MERGE_SHIP_FIELD(y);
            MERGE_SHIP_FIELD(group_has_navigator);
            MERGE_SHIP_FIELD(travelling_speed);
            MERGE_SHIP_FIELD(turns_left);
            MERGE_SHIP_FIELD(shield_damage_percent);
            MERGE_SHIP_FIELD(drive_damage_percent);
            MERGE_SHIP_FIELD(computer_damage);
            MERGE_SHIP_FIELD(crew_quality);
            MERGE_SHIP_FIELD(crew_experience);
            MERGE_SHIP_FIELD(officer_id);
            MERGE_SHIP_FIELD(special_device_damage_flags);
            MERGE_SHIP_FIELD(armor_damage);
            MERGE_SHIP_FIELD(structural_damage);
            MERGE_SHIP_FIELD(mission);
            MERGE_SHIP_FIELD(just_built);
            MERGE_SHIP_FIELD(prior_jump_gate_destination);
            MERGE_SHIP_FIELD(prior_jump_gate_stardate);
#undef MERGE_SHIP_FIELD
            return valid;
        }

        bool Merge_Colony_(const s_colony& baseline, const s_colony& draft, const s_colony& live, s_colony* merged) {
            *merged = live;
            bool valid = true;
#define MERGE_COLONY_FIELD(field) valid = Merge_Field_(baseline.field, draft.field, live.field, &merged->field) && valid
            MERGE_COLONY_FIELD(owner);
            MERGE_COLONY_FIELD(allocated_to);
            MERGE_COLONY_FIELD(planet_id);
            MERGE_COLONY_FIELD(outpost_flag);
            MERGE_COLONY_FIELD(morale);
            MERGE_COLONY_FIELD(pollution);
            MERGE_COLONY_FIELD(n_pops);
            MERGE_COLONY_FIELD(specialty);
            MERGE_COLONY_FIELD(pop);
            MERGE_COLONY_FIELD(pop_roundoff);
            MERGE_COLONY_FIELD(pop_growth);
            MERGE_COLONY_FIELD(n_turns_existed);
            MERGE_COLONY_FIELD(food2_per_farmer);
            MERGE_COLONY_FIELD(industry_per_worker);
            MERGE_COLONY_FIELD(research_per_scientist);
            MERGE_COLONY_FIELD(max_farms);
            MERGE_COLONY_FIELD(max_population);
            MERGE_COLONY_FIELD(climate);
            MERGE_COLONY_FIELD(ground_strength);
            MERGE_COLONY_FIELD(space_strength);
            MERGE_COLONY_FIELD(production);
            MERGE_COLONY_FIELD(maintenance);
            MERGE_COLONY_FIELD(imports);
            MERGE_COLONY_FIELD(n_industry_recyclers);
            MERGE_COLONY_FIELD(food2_needed_for_our_empire);
            MERGE_COLONY_FIELD(food2_needed_for_assimilated);
            MERGE_COLONY_FIELD(food2_needed_for_conquered);
            MERGE_COLONY_FIELD(food2_needed_for_natives);
            MERGE_COLONY_FIELD(industry2_needed_for_our_empire);
            MERGE_COLONY_FIELD(industry2_needed_for_androids);
            MERGE_COLONY_FIELD(industry2_needed_for_assimilated);
            MERGE_COLONY_FIELD(industry2_needed_for_conquered);
            MERGE_COLONY_FIELD(food2_needed_for_empire);
            MERGE_COLONY_FIELD(industry2_needed_for_empire);
            MERGE_COLONY_FIELD(n_food_replicated);
            MERGE_COLONY_FIELD(producing);
            MERGE_COLONY_FIELD(just_produced);
            MERGE_COLONY_FIELD(production_spent);
            MERGE_COLONY_FIELD(n_industry_taxed);
            MERGE_COLONY_FIELD(auto_building);
            MERGE_COLONY_FIELD(production_surplus);
            MERGE_COLONY_FIELD(bought_outright);
            MERGE_COLONY_FIELD(occupation_points);
            MERGE_COLONY_FIELD(occupation_policy);
            MERGE_COLONY_FIELD(military);
            MERGE_COLONY_FIELD(tank_roundoff);
            MERGE_COLONY_FIELD(infantry_roundoff);
            MERGE_COLONY_FIELD(last_turn_building_destroyed);
#undef MERGE_COLONY_FIELD
            return valid;
        }

        bool One_Shot_Building_(int16_t building) {
            return building == BUILDING_SOIL_ENRICHMENT ||
                building == BUILDING_GAIA_TRANSFORMATION ||
                building == BUILDING_TERRAFORMING ||
                building == BUILDING_ARTIFICIAL_PLANET;
        }

        bool Valid_Weapon_Arc_(int8_t arc) {
            return arc == 0 || arc == 1 || arc == 2 || arc == 4 || arc == 8 || arc == 0x0f || arc == 0x10;
        }

        bool Validate_Ship_(const s_ship_data& ship) {
            const int16_t player_count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
            const int16_t star_count = std::clamp<int16_t>(std::ssize(game::current_state.stars), 0, MAX_STARS);
            const bool owner_valid = (factions::Legacy_Code(ship.owner) >= 0 && factions::Legacy_Code(ship.owner) < player_count) ||
                (factions::Legacy_Code(ship.owner) >= MAX_PLAYERS && factions::Legacy_Code(ship.owner) <= NONPLAYER_SHIP_TYPE_HYDRA);
            bool location_valid = true;
            const auto destination = game::Ship_Target_Row(game::current_state, ship.position, star_count);
            if (ship.status == 0) {
                location_valid = ship.position.kind == game::e_ship_location::at_star && destination >= 0;
            } else if (ship.status == 1) {
                location_valid = ship.position.kind == game::e_ship_location::in_flight && destination >= 0 &&
                    ship.travelling_speed > 0;
            } else if (ship.status == 2) {
                location_valid = ship.position.kind == game::e_ship_location::in_wormhole && destination >= 0;
            }
            if (ship.position.target == game::e_ship_target::star)
                location_valid &= game::current_state.stars.contains(ship.position.star_id.get());
            bool weapons_valid = true;
            for (const s_ship_weapons& weapon : ship.d.ship_weapon) {
                weapons_valid = weapons_valid && weapon.type >= 0 && weapon.type < WEAPON_COUNT &&
                    weapon.count >= 0 && weapon.current_count >= 0 &&
                    weapon.current_count <= weapon.count && Valid_Weapon_Arc_(weapon.firing_arc);
            }
            const auto jump_gate = ship.prior_jump_gate_destination;
            const bool jump_gate_valid = jump_gate == game::NO_STAR || game::current_state.stars.contains(jump_gate);
            return ship.d.name[sizeof(ship.d.name) - 1] == '\0' &&
                ship.d.size >= 0 && ship.d.size < SHIP_SIZE_COUNT &&
                ship.d.ship_type >= 0 && ship.d.ship_type < SHIP_TYPE_COUNT &&
                ship.d.ship_type != SHIP_TYPE_UNUSED &&
                ship.d.shield_type >= 0 && ship.d.shield_type < SHIELD_COUNT &&
                ship.d.ftl_type >= 0 && ship.d.ftl_type < DRIVE_COUNT &&
                ship.d.computer_type >= 0 && ship.d.computer_type < COMPUTER_COUNT &&
                ship.d.armor_type >= 0 && ship.d.armor_type < ARMOR_COUNT &&
                ship.d.picture_num >= 0 && ship.d.picture_num <= 47 &&
                factions::Valid_Design_Owner(ship.d.previous_owner) && weapons_valid && owner_valid &&
                ship.status >= 0 && ship.status < (int8_t)(sizeof(k_ship_statuses) / sizeof(k_ship_statuses[0])) &&
                (ship.officer_id == game::NO_LEADER || ship.officer_id.id < MAX_LEADERS) &&
                location_valid && jump_gate_valid;
        }

        bool Validate_Colony_(const s_colony& colony) {
            const int16_t player_count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
            return legacy_world::Encode_Player_Slot(colony.owner) >= 0 && legacy_world::Encode_Player_Slot(colony.owner) < player_count &&
                colony.planet_id != game::NO_PLANET && game::current_state.planets.contains(colony.planet_id) &&
                (legacy_world::Encode_Player_Slot(colony.allocated_to) == -1 ||
                    (legacy_world::Encode_Player_Slot(colony.allocated_to) >= 0 && legacy_world::Encode_Player_Slot(colony.allocated_to) < player_count)) &&
                colony.outpost_flag <= 1 && colony.n_pops <= MAX_COLONY_POPULATION &&
                colony.pollution >= 0 &&
                colony.auto_building < BUILD_QUEUE::AUTOBUILD_CUSTOM_BASE + BUILD_QUEUE::PRESET_COUNT &&
                colony.occupation_policy >= 0 && colony.occupation_policy <= 4 &&
                [&colony]() {
                    for (int32_t index = 0; index < colony.n_pops; ++index) {
                        if (colony.pop[index].group > POP::RACE_NATIVE ||
                            colony.pop[index].profession > POP::PROF_SCIENTIST) {
                            return false;
                        }
                    }
                    return true;
                }();
        }

        void Draw_Ship_Form_(s_ship_data& ship) {
            ImGui::InputText("Name", ship.d.name, sizeof(ship.d.name));
            auto owner = static_cast<int8_t>(ship.owner.code);
            if (Ship_Owner_(&owner)) ship.owner = game::owner_slot{owner};
            debug_game_widgets::Enum("Size", &ship.d.size, k_ship_sizes, SHIP_SIZE_COUNT);
            debug_game_widgets::Enum("Type", &ship.d.ship_type, k_ship_types, SHIP_TYPE_COUNT);
            debug_game_widgets::Enum("Status", &ship.status, k_ship_statuses, (int16_t)(sizeof(k_ship_statuses) / sizeof(k_ship_statuses[0])));
            static const char* const movement[] = {"Orbit", "In flight", "Departing"};
            auto position = ship.position.Get();
            int kind = static_cast<int>(position.kind);
            int destination = static_cast<int>(position.target);
            static const char* const destinations[] = {"Star", "Antares", "None"};
            ImGui::Combo("Movement", &kind, movement, 3);
            ImGui::Combo("Destination type", &destination, destinations, 3);
            const auto target = static_cast<game::e_ship_target>(destination);
            if (target == game::e_ship_target::star) {
                auto key = position.star_id;
                debug_game_widgets::Star_Reference(game::current_state, "Destination", &key);
                ship.position.Set(key == game::NO_STAR ? game::NO_SHIP_LOCATION
                    : game::s_ship_location{static_cast<game::e_ship_location>(kind), key});
            } else if (target == game::e_ship_target::antares) {
                ship.position.Set({static_cast<game::e_ship_location>(kind), game::NO_STAR, target});
            } else {
                ship.position.Set(game::NO_SHIP_LOCATION);
            }
            Input_S16_("X", &ship.x);
            Input_S16_("Y", &ship.y);
            Input_U8_("Turns left", &ship.turns_left);
            Input_U8_("Travelling speed", &ship.travelling_speed);
            debug_game_widgets::Officer_Reference("Officer", &ship.officer_id);

            if (ImGui::CollapsingHeader("Design")) {
                Input_S8_("Shield", &ship.d.shield_type);
                Input_S8_("FTL drive", &ship.d.ftl_type);
                Input_S8_("Computer", &ship.d.computer_type);
                Input_S8_("Armor", &ship.d.armor_type);
                Input_S8_("Picture", &ship.d.picture_num);
                Input_S32_("Previous owner", &ship.d.previous_owner.code);
                auto date = ship.d.date_of_design;
                if (Input_U16_("Design date", &date)) ship.d.date_of_design = date;
                ImGui::Text("Strategic speed: %d (derived)", (int)ship.d.speed);
                ImGui::Text("Cost: %d (derived)", (int)ship.d.cost);
                ImGui::Text("Combat speed: %u (derived)", (unsigned)ship.d.combat_speed);
                if (ImGui::TreeNode("Special devices")) {
                    debug_game_widgets::Ship_Specials(
                        "ship_specials", ship.d.special_device_flags, sizeof(ship.d.special_device_flags));
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Weapon slots")) {
                    ImGui::TextUnformatted("Arcs: 0 none, 1 forward, 2 forward extended, 4 back extended, 8 rear, 15 all sectors, 16 360 degrees.");
                    for (int32_t slot = 0; slot < 8; ++slot) {
                        ImGui::PushID(slot);
                        s_ship_weapons& weapon = ship.d.ship_weapon[slot];
                        char label[24];
                        snprintf(label, sizeof(label), "Slot %d", slot + 1);
                        if (ImGui::TreeNode(label)) {
                            Input_S16_("Weapon type", &weapon.type);
                            Input_S8_("Installed count", &weapon.count);
                            Input_S8_("Current count", &weapon.current_count);
                            Input_S8_("Firing arc", &weapon.firing_arc);
                            Input_U16_("Special flags", &weapon.specials);
                            Input_S8_("Ammo", &weapon.ammo);
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Damage and crew")) {
                Input_S8_("Shield damage percent", &ship.shield_damage_percent);
                Input_S8_("Drive damage percent", &ship.drive_damage_percent);
                Input_S8_("Computer damage", &ship.computer_damage);
                Input_S16_("Armor damage", &ship.armor_damage);
                Input_S16_("Structural damage", &ship.structural_damage);
                Input_S8_("Crew quality", &ship.crew_quality);
                Input_U16_("Crew experience", &ship.crew_experience);
                if (ImGui::TreeNode("Damaged special devices")) {
                    debug_game_widgets::Ship_Specials(
                        "damaged_ship_specials", ship.special_device_damage_flags, sizeof(ship.special_device_damage_flags));
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Advanced")) {
                Input_S8_("Group has navigator", &ship.group_has_navigator);
                Input_S8_("Mission", &ship.mission);
                Input_S8_("Just built", &ship.just_built);
                auto redirect = ship.prior_jump_gate_destination;
                if (debug_game_widgets::Star_Reference(game::current_state, "Prior jump-gate destination", &redirect)) {
                    ship.prior_jump_gate_destination = redirect;
                }
                Input_S32_("Prior jump-gate stardate", &ship.prior_jump_gate_stardate);
            }
        }

        void Draw_Colony_Form_(s_colony& colony) {
            char race_name[32];
            const int16_t player_count = std::clamp<int16_t>(game::current_state.player_count, 0, MAX_PLAYERS);
            if (legacy_world::Encode_Player_Slot(colony.owner) >= 0 && legacy_world::Encode_Player_Slot(colony.owner) < player_count) {
                HACCESS::Race_Name(factions::From_Player(colony.owner), race_name, sizeof(race_name));
            } else {
                strlcpy(race_name, "Invalid owner", sizeof(race_name));
            }
            ImGui::Text("Owner: %s", race_name);
            ImGui::Text("Population units: %u", (unsigned)colony.n_pops);
            ImGui::TextUnformatted("Owner and planet are read-only relationship fields.");
            ImGui::BeginDisabled();
            auto planet_field = colony.planet_id;
            if (debug_game_widgets::Planet_Reference("Planet", &planet_field, false)) {
                colony.planet_id = planet_field;
            }
            ImGui::EndDisabled();
            if (ImGui::Button("Open planet")) {
                Open_Planet(colony.planet_id);
            }
            debug_game_widgets::Player_Reference("Allocated to", &colony.allocated_to);
            ImGui::Text("Legacy officer slot: %d (not an assignment)", (int)colony.legacy_officer_slot);
            ImGui::Text("Effective officer: %d (calculated)", leader_slots::Encode(COLCALC::Colony_Officer(&colony)));
            ImGui::Text("Outpost: %s", colony.outpost_flag != 0 ? "yes" : "no");
            if (colony.outpost_flag != 0) {
                const bool network_game = MOX::game_type == 2 || MOX::game_type == 3;
                ImGui::BeginDisabled(Dirty_(g_colony_editor) || network_game);
                if (ImGui::Button("Convert outpost to colony")) {
                    g_pending_outpost_colony = g_colony_editor.index;
                    g_convert_outpost_requested = true;
                }
                ImGui::EndDisabled();
                if (network_game) {
                    ImGui::TextUnformatted("Conversion is disabled in network games.");
                } else if (Dirty_(g_colony_editor)) {
                    ImGui::TextUnformatted("Save or discard draft changes before conversion.");
                }
            }
            Input_S8_("Turns existed", &colony.n_turns_existed);

            if (ImGui::CollapsingHeader("Capacity and output")) {
                ImGui::Text("Morale: %d", (int)colony.morale);
                ImGui::Text("Pollution: %d", (int)colony.pollution);
                ImGui::Text("Climate: %s", colony.climate < PLANET_CLIMATE_COUNT ? MOX::planet_climate_string[colony.climate] : "Invalid");
                ImGui::Text("Food per farmer: %d", (int)colony.food2_per_farmer);
                ImGui::Text("Industry per worker: %d", (int)colony.industry_per_worker);
                ImGui::Text("Research per scientist: %d", (int)colony.research_per_scientist);
                ImGui::Text("Max farms: %u", (unsigned)colony.max_farms);
                ImGui::Text("Max population: %d", (int)colony.max_population);
                ImGui::Text("Ground strength: %d", (int)colony.ground_strength);
                ImGui::Text("Space strength: %d", (int)colony.space_strength);
                ImGui::Text("Specialty: %d", (int)colony.specialty);
                if (ImGui::TreeNode("Economy arrays")) {
                    for (int32_t index = 0; index < ECON_COUNT; ++index) {
                        ImGui::Text(
                            "%d: production %d, maintenance %u, imports %d",
                            index,
                            (int)colony.production[index],
                            (unsigned)colony.maintenance[index],
                            (int)colony.imports[index]);
                    }
                    ImGui::Text("Industry recyclers: %d", (int)colony.n_industry_recyclers);
                    ImGui::Text("Food replicated: %d", (int)colony.n_food_replicated);
                    ImGui::Text("Industry taxed: %d", (int)colony.n_industry_taxed);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Calculated consumption")) {
                    ImGui::Text(
                        "Food needs: empire %u, assimilated %u, conquered %u, natives %u",
                        (unsigned)colony.food2_needed_for_our_empire,
                        (unsigned)colony.food2_needed_for_assimilated,
                        (unsigned)colony.food2_needed_for_conquered,
                        (unsigned)colony.food2_needed_for_natives);
                    ImGui::Text(
                        "Industry needs: empire %u, androids %u, assimilated %u, conquered %u",
                        (unsigned)colony.industry2_needed_for_our_empire,
                        (unsigned)colony.industry2_needed_for_androids,
                        (unsigned)colony.industry2_needed_for_assimilated,
                        (unsigned)colony.industry2_needed_for_conquered);
                    for (int32_t player = 0; player < MAX_PLAYERS; ++player) {
                        ImGui::Text(
                            "Player %d: food %d, industry %d",
                            player,
                            (int)colony.food2_needed_for_empire[player],
                            (int)colony.industry2_needed_for_empire[player]);
                    }
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Population records")) {
                ImGui::TextUnformatted("Add, remove, or edit population units. Calculated growth is read-only.");
                ImGui::BeginDisabled(colony.outpost_flag != 0);
                Population_Race_("New population race", &g_new_population_race);
                debug_game_widgets::Enum(
                    "New population job",
                    &g_new_population_job,
                    k_population_jobs,
                    3);
                if (ImGui::Button("Add population")) {
                    const s_settler_info settler{
                        .race = static_cast<uint8_t>(g_new_population_race),
                        .profession = static_cast<uint8_t>(g_new_population_job),
                    };
                    g_colony_operation_status = SETTLER::Add_Settler_To_Colony(
                                                    settler, &colony) != 0
                        ? "Population added to draft."
                        : "The colony has no room for that population.";
                }
                for (int32_t index = 0; index < colony.n_pops; ++index) {
                    ImGui::PushID(index);
                    s_colony_pop* population = &colony.pop[index];
                    char race_buffer[32];
                    char label[64];
                    snprintf(
                        label,
                        sizeof(label),
                        "%d: %s, %s",
                        index,
                        Population_Race_Name_(
                            (int8_t)population->group,
                            race_buffer,
                            sizeof(race_buffer)),
                        k_population_jobs[(int)population->profession < 3
                                ? (int)population->profession
                                : 0]);
                    bool remove = false;
                    if (ImGui::TreeNode(label)) {
                        int8_t race = (int8_t)population->group;
                        if (Population_Race_("Race", &race)) {
                            population->group = static_cast<uint8_t>(race);
                        }
                        int8_t job = (int8_t)population->profession;
                        if (debug_game_widgets::Enum(
                                "Job", &job, k_population_jobs, 3)) {
                            population->profession = static_cast<POP::e_prof>(job);
                        }
                        int8_t original_owner = static_cast<int8_t>(population->original_owner.id);
                        if (debug_game_widgets::Player_Reference(
                                "Original owner", &original_owner, false)) {
                            population->original_owner = game::id_player{static_cast<uint32_t>(original_owner)};
                        }
                        ImGui::Checkbox("Assigned", &population->assigned);
                        ImGui::Checkbox("Conquered", &population->conquered);
                        remove = ImGui::Button("Remove population");
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (remove) {
                        const int32_t remaining = colony.n_pops - index - 1;
                        if (remaining > 0) {
                            memmove(
                                &colony.pop[index],
                                &colony.pop[index + 1],
                                remaining * sizeof(colony.pop[0]));
                        }
                        colony.n_pops--;
                        colony.pop[colony.n_pops] = {};
                        g_colony_operation_status = "Population removed from draft.";
                        break;
                    }
                }
                ImGui::EndDisabled();
                if (colony.outpost_flag != 0) {
                    ImGui::TextUnformatted("Convert the outpost before editing population.");
                }
                if (ImGui::TreeNode("Growth by race")) {
                    for (int32_t index = 0; index < COLONY_POP_RACE_COUNT; ++index) {
                        ImGui::PushID(index);
                        Input_S16_("Roundoff", &colony.pop_roundoff[index]);
                        ImGui::SameLine();
                        ImGui::Text("Growth: %d", (int)colony.pop_growth[index]);
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Production queue and bookkeeping")) {
                ImGui::BeginDisabled(colony.outpost_flag != 0);
                for (int32_t index = 0; index < 7; ++index) {
                    ImGui::PushID(index);
                    const auto product = colony.producing[index];
                    if (const auto key = game::Queued_Ship_Key(product); key != game::NO_SHIP) {
                        ImGui::Text("Queue slot %d: ship %u", index + 1, key.id);
                    } else {
                        ImGui::Text("Queue slot %d: %d", index + 1, production::Code_Or(product));
                    }
                    if (colony.producing[index] != COLONY_PRODUCTION_NONE) {
                        ImGui::SameLine();
                        ImGui::BeginDisabled(Dirty_(g_colony_editor));
                        if (ImGui::SmallButton("Remove")) {
                            COLBLDG::Delete_From_Colony_Queue(game::current_state, g_colony_editor.index, (int16_t)index);
                            Refresh_Colony_State_();
                            Load_Colony_(g_colony_editor.index);
                            g_colony_operation_status = "Queue item removed.";
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::PopID();
                }
                ImGui::BeginDisabled(Dirty_(g_colony_editor));
                if (ImGui::Button("Clear queue")) {
                    COLBLDG::Clear_Colony_Queue(game::current_state, g_colony_editor.index);
                    Refresh_Colony_State_();
                    Load_Colony_(g_colony_editor.index);
                    g_colony_operation_status = "Production queue cleared.";
                }
                ImGui::SameLine();
                const s_colony& live_colony = game::current_state.colonies.at(g_colony_editor.index);
                const bool can_buy = live_colony.bought_outright == 0 &&
                    live_colony.producing[0] != COLONY_PRODUCTION_NONE &&
                    !BUILD_QUEUE::Colony_Production_Is_Continuous_Project(
                        live_colony.producing[0]) &&
                    COLCALC::Colony_Cost_To_Buy_Producing_Product(
                        &game::current_state.colonies.at(g_colony_editor.index)) > 0 &&
                    !COLCALC::Colony_Is_Blockaded(
                        &game::current_state.colonies.at(g_colony_editor.index)) &&
                    COLCALC::Colony_Can_Afford_To_Buy_Product(
                        g_colony_editor.index);
                ImGui::BeginDisabled(!can_buy);
                if (ImGui::Button("Buy current product")) {
                    COLCALC::Colony_Buy_Outright(g_colony_editor.index);
                    g_colony_operation_status = "Current product bought outright.";
                    Refresh_Colony_State_();
                    Load_Colony_(g_colony_editor.index);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(live_colony.bought_outright == 0);
                if (ImGui::Button("Unbuy current product")) {
                    BUILD_QUEUE::Unbuy_Product(
                        &game::current_state.colonies.at(g_colony_editor.index));
                    g_colony_operation_status = "Current product purchase refunded.";
                    Refresh_Colony_State_();
                    Load_Colony_(g_colony_editor.index);
                }
                ImGui::EndDisabled();
                Input_S16_("Autobuild preset", &g_autobuild_preset);
                ImGui::SameLine();
                ImGui::BeginDisabled(
                    g_autobuild_preset < 0 ||
                    g_autobuild_preset >= BUILD_QUEUE::PRESET_COUNT);
                if (ImGui::Button("Apply preset")) {
                    g_colony_operation_status = BUILD_QUEUE::Apply_Preset_To_Colony(
                                                    g_colony_editor.index,
                                                    g_autobuild_preset)
                        ? "Autobuild preset applied."
                        : "Autobuild preset could not be applied.";
                    Refresh_Colony_State_();
                    Load_Colony_(g_colony_editor.index);
                }
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                if (Dirty_(g_colony_editor)) {
                    ImGui::TextUnformatted("Save or discard draft changes before queue operations.");
                }
                ImGui::InputScalar("Just produced", ImGuiDataType_S64, &colony.just_produced);
                Input_S16_("Production spent", &colony.production_spent);
                Input_S16_("Production surplus", &colony.production_surplus);
                Input_S16_("Bought outright", &colony.bought_outright);
                ImGui::EndDisabled();
            }

            if (ImGui::CollapsingHeader("Management")) {
                Input_U8_("Auto building preset", &colony.auto_building);
                Input_S8_("Occupation points", &colony.occupation_points);
                Input_S8_("Occupation policy", &colony.occupation_policy);
                Input_S16_("Tanks", &colony.military[0]);
                Input_S16_("Infantry", &colony.military[1]);
                Input_S8_("Tank roundoff", &colony.tank_roundoff);
                Input_S8_("Infantry roundoff", &colony.infantry_roundoff);
            }

            if (ImGui::CollapsingHeader("Buildings and demolition state")) {
                ImGui::TextUnformatted("Operations apply immediately and preserve building side effects.");
                ImGui::BeginDisabled(colony.outpost_flag != 0);
                ImGui::Text("Sentinel slot 0: %u", (unsigned)colony.buildings[0]);
                for (int32_t index = 1; index < BUILDING_COUNT; ++index) {
                    ImGui::PushID(index);
                    bool present = colony.buildings[index] != 0;
                    const bool one_shot = One_Shot_Building_((int16_t)index);
                    const bool addition_available = present ||
                        COLCALC::Colony_Can_Build_Product(
                            &game::current_state.colonies.at(g_colony_editor.index),
                            (int16_t)index) == 1;
                    const char* name = TECHDATA::buildings[index].name;
                    char label[96];
                    snprintf(
                        label,
                        sizeof(label),
                        "%d: %s",
                        index,
                        name != nullptr ? name : "Unnamed building");
                    ImGui::BeginDisabled(
                        Dirty_(g_colony_editor) || !addition_available);
                    bool apply = false;
                    if (one_shot) {
                        apply = ImGui::Button(label);
                    } else if (ImGui::Checkbox(label, &present)) {
                        if (present) {
                            ERICNET::Add_Building(
                                g_colony_editor.index, (int16_t)index);
                        } else {
                            ERICNET::Remove_Building(game::current_state, g_colony_editor.index, (int16_t)index);
                        }
                        apply = true;
                    }
                    if (apply) {
                        if (one_shot) {
                            ERICNET::Add_Building(
                                g_colony_editor.index, (int16_t)index);
                        }
                        Refresh_Colony_State_();
                        Load_Colony_(g_colony_editor.index);
                        g_colony_operation_status = one_shot
                            ? "Building effect applied."
                            : "Building state updated.";
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::EndDisabled();
                if (colony.outpost_flag != 0) {
                    ImGui::TextUnformatted("Convert the outpost before changing buildings.");
                }
                Input_U16_(
                    "Last-turn building destroyed", &colony.last_turn_building_destroyed);
            }
            if (g_colony_operation_status != nullptr) {
                ImGui::TextWrapped("%s", g_colony_operation_status);
            }
        }

        template <typename T, typename Key = int16_t>
        void Draw_Discard_Popup_(const char* popup_id, s_editor_state<T, Key>* editor, void (*load_pending)(Key)) {
            const auto no_selection = [] {
                if constexpr (std::is_same_v<Key, game::id_colony>) return game::NO_COLONY;
                else if constexpr (std::is_same_v<Key, game::id_ship>) return game::NO_SHIP;
                else return Key{-1};
            }();
            if (editor->discard_requested) {
                ImGui::OpenPopup(popup_id);
                editor->discard_requested = false;
            }
            if (ImGui::BeginPopupModal(popup_id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Discard unsaved changes?");
                if (ImGui::Button("Discard")) {
                    const auto next = editor->pending_index;
                    ImGui::CloseCurrentPopup();
                    if (next != no_selection) {
                        load_pending(next);
                    } else {
                        editor->open = false;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep editing")) {
                    editor->pending_index = no_selection;
                    editor->open = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void Load_Ship_(game::id_ship index) {
            if (!Valid_Ship_(index)) {
                g_ship_editor.open = false;
                return;
            }
            g_ship_editor.index = index;
            g_ship_editor.pending_index = game::NO_SHIP;
            g_ship_editor.baseline = game::current_state.ships.at(index);
            g_ship_editor.draft = g_ship_editor.baseline;
            g_ship_editor.open = true;
            g_ship_editor.focus_requested = true;
        }

        void Load_Colony_(game::id_colony index) {
            g_colony_editor.index = index;
            g_colony_editor.pending_index = game::NO_COLONY;
            g_colony_editor.baseline = game::current_state.colonies.at(index);
            g_colony_editor.draft = g_colony_editor.baseline;
            g_colony_editor.open = true;
            g_colony_editor.focus_requested = true;
        }

        void Draw_Ship_Editor_() {
            if (!g_ship_editor.open || !Valid_Ship_(g_ship_editor.index)) {
                g_ship_editor.open = false;
                return;
            }
            if (g_ship_editor.focus_requested) {
                ImGui::SetNextWindowFocus();
                g_ship_editor.focus_requested = false;
            }
            ImGui::SetNextWindowSize(ImVec2(350.0f, 470.0f), ImGuiCond_FirstUseEver);
            char title[48];
            snprintf(title, sizeof(title), "Ship Editor [%u]", g_ship_editor.index.id);
            bool open = true;
            if (ImGui::Begin(title, &open)) {
                Draw_Ship_Form_(g_ship_editor.draft);
                ImGui::Separator();
                const bool dirty = Dirty_(g_ship_editor);
                const bool valid = Validate_Ship_(g_ship_editor.draft);
                s_ship_data merged;
                auto& state = game::current_state;
                const auto& live = state.ships.at(g_ship_editor.index);
                const bool mergeable = Merge_Ship_(g_ship_editor.baseline, g_ship_editor.draft, live, &merged);
                ImGui::BeginDisabled(!dirty || !valid || !mergeable);
                if (ImGui::Button("Save")) {
                    merged.d.speed = static_cast<uint8_t>(
                        INITSHIP::Get_FTL_Speed(merged.owner, merged.d.ftl_type));
                    const auto player = factions::Player(merged.owner);
                    if (merged.d.ship_type == SHIP_TYPE_COMBAT && player != game::NO_PLAYER) {
                        merged.d.cost = DESIGN::Calculate_Design(
                            player, &merged.d, &merged.d.combat_speed);
                    } else if (merged.d.ship_type != SHIP_TYPE_COMBAT && player != game::NO_PLAYER) {
                        merged.d.cost = (int16_t)COLCALC::Ship_Type_Cost_For_Player(
                            merged.d.ship_type, player);
                        merged.d.combat_speed =
                            TECHDATA::drives[merged.d.ftl_type].combat_speed_min[merged.d.size];
                    }
                    const bool reactivated = Ship_Inactive_(live) && !Ship_Inactive_(merged);
                    const auto slot = game::Ship_Slot(state, g_ship_editor.index);
                    if (reactivated) {
                        merged.id = ship_slots::Allocate_Key(game::current_state,
            MOX::game_type == 2 || MOX::game_type == 3 ? std::optional{MOX::local_player} : std::nullopt);
                    }
                    if (slot >= 0 && (!reactivated ||
                        (merged.id != game::NO_SHIP && orion2re::lua::game::Advance_Ship_Generation(slot)))) {
                        ship_slots::Put(state, slot, merged);
                        g_ship_editor.index = merged.id;
                        g_ship_editor.baseline = merged;
                        g_ship_editor.draft = merged;
                        Refresh_Ship_State_();
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    if (dirty) {
                        g_ship_editor.pending_index = game::NO_SHIP;
                        g_ship_editor.discard_requested = true;
                    } else {
                        open = false;
                    }
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(!mergeable ? "Conflict with live game state" : (!valid ? "Invalid values" : (dirty ? "Unsaved changes" : "No changes")));
                if (!mergeable && ImGui::Button("Reload live data")) {
                    Load_Ship_(g_ship_editor.index);
                }
            }
            ImGui::End();
            if (!open && Dirty_(g_ship_editor)) {
                g_ship_editor.pending_index = game::NO_SHIP;
                g_ship_editor.discard_requested = true;
                open = true;
            }
            g_ship_editor.open = open;
            Draw_Discard_Popup_("Discard ship changes?", &g_ship_editor, Load_Ship_);
        }

        void Draw_Colony_Editor_() {
            if (!g_colony_editor.open || !Valid_Colony_(g_colony_editor.index)) {
                g_colony_editor.open = false;
                return;
            }
            if (g_colony_editor.focus_requested) {
                ImGui::SetNextWindowFocus();
                g_colony_editor.focus_requested = false;
            }
            ImGui::SetNextWindowSize(ImVec2(350.0f, 450.0f), ImGuiCond_FirstUseEver);
            char title[48];
            snprintf(title, sizeof(title), "Colony Editor [%u]", g_colony_editor.index.id);
            bool open = true;
            if (ImGui::Begin(title, &open)) {
                Draw_Colony_Form_(g_colony_editor.draft);
                ImGui::Separator();
                const bool dirty = Dirty_(g_colony_editor);
                const bool valid = Validate_Colony_(g_colony_editor.draft);
                s_colony merged;
                const bool mergeable = Merge_Colony_(g_colony_editor.baseline, g_colony_editor.draft, game::current_state.colonies.at(g_colony_editor.index), &merged);
                ImGui::BeginDisabled(!dirty || !valid || !mergeable);
                if (ImGui::Button("Save")) {
                    game::current_state.colonies.at(g_colony_editor.index) = merged;
                    Refresh_Colony_State_();
                    g_colony_editor.baseline = game::current_state.colonies.at(g_colony_editor.index);
                    g_colony_editor.draft = g_colony_editor.baseline;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    if (dirty) {
                        g_colony_editor.pending_index = game::NO_COLONY;
                        g_colony_editor.discard_requested = true;
                    } else {
                        open = false;
                    }
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(!mergeable ? "Conflict with live game state" : (!valid ? "Invalid values" : (dirty ? "Unsaved changes" : "No changes")));
                if (!mergeable && ImGui::Button("Reload live data")) {
                    Load_Colony_(g_colony_editor.index);
                }
            }
            ImGui::End();
            if (!open && Dirty_(g_colony_editor)) {
                g_colony_editor.pending_index = game::NO_COLONY;
                g_colony_editor.discard_requested = true;
                open = true;
            }
            g_colony_editor.open = open;
            Draw_Discard_Popup_("Discard colony changes?", &g_colony_editor, Load_Colony_);
            if (g_convert_outpost_requested) {
                ImGui::OpenPopup("Convert outpost to colony?");
                g_convert_outpost_requested = false;
            }
            if (ImGui::BeginPopupModal(
                    "Convert outpost to colony?",
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("This reinitializes the outpost as a new colony.");
                const bool can_convert = Valid_Colony_(g_pending_outpost_colony) &&
                    game::current_state.colonies.at(g_pending_outpost_colony).outpost_flag != 0 &&
                    game::current_state.colonies.at(g_pending_outpost_colony).planet_id != game::NO_PLANET &&
                    game::current_state.planets.contains(game::current_state.colonies.at(g_pending_outpost_colony).planet_id) &&
                    game::current_state.planets.at(game::current_state.colonies.at(g_pending_outpost_colony).planet_id).colony_id ==
                        g_pending_outpost_colony &&
                    MOX::game_type != 2 && MOX::game_type != 3;
                ImGui::BeginDisabled(!can_convert);
                if (ImGui::Button("Convert")) {
                    const game::id_colony colony_index = g_pending_outpost_colony;
                    const auto planet_index = game::current_state.colonies.at(colony_index).planet_id;
                    const int8_t owner = legacy_world::Encode_Player_Slot(game::current_state.colonies.at(colony_index).owner);
                    COLONIZE::Make_New_Colony(game::current_state.colonies.at(colony_index).planet_id, legacy_world::Decode_Player_Slot(owner));
                    Refresh_Colony_State_();
                    Load_Colony_(colony_index);
                    g_colony_operation_status = "Outpost converted to a colony.";
                    g_pending_outpost_colony = game::NO_COLONY;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    g_pending_outpost_colony = game::NO_COLONY;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            Draw_Ship_Editor_();
            Draw_Colony_Editor_();
        }

        debug_overlay::s_screen_panel_registration g_registration(
            debug_overlay::k_all_screens, Draw_);
    } // namespace

    void Open_Ship(game::id_ship ship_index) {
        if (!Valid_Ship_(ship_index)) {
            return;
        }
        if (g_ship_editor.open && g_ship_editor.index == ship_index) {
            g_ship_editor.focus_requested = true;
            return;
        }
        if (g_ship_editor.open && Dirty_(g_ship_editor)) {
            g_ship_editor.pending_index = ship_index;
            g_ship_editor.discard_requested = true;
            return;
        }
        Load_Ship_(ship_index);
    }

    void Open_Colony(game::id_colony colony_index) {
        if (!Valid_Colony_(colony_index)) {
            return;
        }
        if (g_colony_editor.open && g_colony_editor.index == colony_index) {
            g_colony_editor.focus_requested = true;
            return;
        }
        if (g_colony_editor.open && Dirty_(g_colony_editor)) {
            g_colony_editor.pending_index = colony_index;
            g_colony_editor.discard_requested = true;
            return;
        }
        Load_Colony_(colony_index);
    }
} // namespace debug_game_editors
#else
namespace debug_game_editors {
    void Open_Ship(game::id_ship ship_index) {
        (void)ship_index;
    }

    void Open_Colony(game::id_colony colony_index) {
        (void)colony_index;
    }
} // namespace debug_game_editors
#endif
