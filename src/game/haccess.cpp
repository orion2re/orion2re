#include "pch.h"
#include "ship_slots.h"
#include "fleet_movement.h"

namespace HACCESS {
    int16_t __cdecl Ship_Icon_Ship_Id(int16_t icon_idx) {
        int16_t node_idx = MOX::ship_icon[icon_idx].node_idx;

        if (node_idx >= 0 && node_idx < (int16_t)(sizeof(MOX::ship_node) / sizeof(MOX::ship_node[0]))) {
            return MOX::ship_node[node_idx].ship_idx;
        }

        return 0;
    }

    int16_t __cdecl Ship_Icon_Star_Id(int16_t ship_icon_idx) {
        int16_t node_idx = MOX::ship_icon[ship_icon_idx].node_idx;
        int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;

        return game::Ship_Orbit_Row(game::current_state, ship_slots::At(game::current_state, ship_idx).position);
    }

    game::owner_slot __cdecl Ship_Owner(int16_t ship_idx) {
        int16_t error_code = 0;

        if (ship_idx < 0) {
            error_code = 1;
        } else if (ship_idx >= MAX_SHIPS) {
            error_code = 2;
        } else {
            const auto owner = ship_slots::Owner(game::current_state, ship_idx).code;
            if (owner < 0) {
                error_code = 3;
            } else if (owner > 15) {
                error_code = 4;
            }
        }

        if (error_code == 0) {
            return ship_slots::Owner(game::current_state, ship_idx);
        }

        char message[200];
        int32_t current_owner = -1;
        if (ship_idx >= 0 && ship_idx < MAX_SHIPS) {
            current_owner = ship_slots::Owner(game::current_state, ship_idx).code;
        }

        snprintf(message, sizeof(message), "Memory Corruption! val == %d, ship_id == %d, owner == %d stardate = %d\n", (int)error_code, ship_idx, current_owner, MOX::stardate);

        exitm::Exit_With_Message(message);

        return game::NO_OWNER;
    }

    int16_t __cdecl Ship_Stack_From_Saved_Array() {
        int16_t best_owner_match_idx = -1;
        int16_t stack_index = -1;
        bool found = false;

        if (MOX::ship_stack_count < 1) {
            return -1;
        }

        for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
            if (game::current_state.ship_slots[i] == ::game::NO_SHIP) continue;
            // Status >= 3 typically indicates the ship is inactive/destroyed
            const auto key = game::Ship_Key_At_Row(game::current_state, i);
            if (ship_slots::At(game::current_state, i).status < 3 && MOX::fleet_box_ship_ids.contains(key)) {
                const auto target_owner = Ship_Owner(i);

                for (int16_t j = 0; j < (int16_t)MOX::ship_stack_count; ++j) {
                    if (found) break;

                    int16_t node_idx = MOX::ship_stack_start[j];
                    int16_t stack_ship_idx = MOX::ship_node[node_idx].ship_idx;

                    if (best_owner_match_idx == -1) {
                        if (Ship_Owner(stack_ship_idx) == target_owner) {
                            best_owner_match_idx = j;
                        }
                    }

                    if (stack_ship_idx == i) {
                        found = true;
                        stack_index = j;
                    }
                }
            }
            if (found) break;
        }

        if (!found && best_owner_match_idx != -1) {
            stack_index = best_owner_match_idx;
        }

        return stack_index;
    }

    int16_t __cdecl Node_Ptr_From_Ship_Stack(int16_t index) {
        if (MOX::ship_stack_count == 0) {
            return 0;
        }

        if (MOX::ship_stack_count < index) {
            MOVEBOX::Vanish_Fleet_Movement_Box();
            return 0;
        }

        if (MOX::ship_stack_count == index) {
            return Ship_Stack_From_Saved_Array();
        }

        return MOX::ship_stack_start[index];
    }

    bool __cdecl Star_Has_Relocation(int16_t star_idx, game::id_player player) {
        const auto& state = game::current_state;
        const auto& star = state.stars.at(game::Star_Key_At_Row(state, star_idx));
        return player.id < MAX_PLAYERS && state.stars.contains(star.relocate_ship_to[player.id]);
    }

    int16_t __cdecl Relocation(int16_t star_idx, game::id_player player) {
        if (player.id >= MAX_PLAYERS) return -1;
        return game::Star_Row(game::current_state, game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).relocate_ship_to[player.id]);
    }

    void __cdecl Set_Colony_Auto_Select_For_Star(int16_t star_idx, game::id_player player, game::id_planet* out_planet_idx) {
        game::id_colony colony_found = game::NO_COLONY;
        bool found = false;
        *out_planet_idx = game::NO_PLANET;
        if (player.id >= MAX_PLAYERS) return;

        for (int16_t i = 0; i < 5; ++i) {
            if (found) {
                break;
            }

            const auto planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[i];
            if (planet_idx != game::NO_PLANET) {
                *out_planet_idx = planet_idx;
                game::id_colony colony_idx = game::current_state.planets.at(planet_idx).colony_id;

                if (colony_idx != game::NO_COLONY) {
                    colony_found = colony_idx;
                    // Check if colony belongs to the player and is not an outpost
                    if (game::current_state.colonies.at(colony_idx).outpost_flag == 0 && game::current_state.colonies.at(colony_idx).owner == player) {
                        game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).last_planet_selected[player.id] = (int8_t)i;
                        found = true;
                    } else {
                        colony_found = game::NO_COLONY;
                    }
                }
            }
        }

        if (colony_found == game::NO_COLONY) {
            *out_planet_idx = game::NO_PLANET;
        }
    }

    // BUG: The original implementation contains a potential bug where it skips every
    // second node in the linked list chain during traversal.
    int16_t __cdecl Ship_Icon_Id_From_Node_Ptr(int16_t ship_node_idx) {
        int16_t found_icon_idx = 0;
        bool found = false;
        int16_t i = 0;

        if (MOX::ship_icon_count > 0) {
            do {
                if (found) {
                    break;
                }

                int16_t current_node_idx = MOX::ship_icon[i].node_idx;

                if (current_node_idx == ship_node_idx) {
                    found = true;
                    found_icon_idx = i;
                } else {
                    // Traverse the node chain.
                    while (!found) {
                        if (current_node_idx == -1) {
                            break;
                        }

                        // Advance to next node and compare
                        current_node_idx = MOX::ship_node[current_node_idx].next_node;
                        if (current_node_idx == ship_node_idx) {
                            found = true;
                            found_icon_idx = i;
                        } else if (current_node_idx != -1) {
                            // Advance to the next node again without comparing in this iteration.
                            current_node_idx = MOX::ship_node[current_node_idx].next_node;
                        }
                    }
                }
                i++;
            } while (i < MOX::ship_icon_count);
        }

        return found_icon_idx;
    }

    game::owner_slot __cdecl Ship_Icon_Owner(int16_t icon_idx) {
        int16_t node_idx = MOX::ship_icon[icon_idx].node_idx;

        return Ship_Owner(MOX::ship_node[node_idx].ship_idx);
    }

    bool __cdecl Fleet_Movement_Icon_Selected(int16_t icon_idx) {
        int16_t node_idx = MOX::fleet_movement_icon[icon_idx].ship_node_idx;
        return (bool)MOX::ship_node[node_idx].selected;
    }

    static void Do_Get_Planet_Name(const game::s_game_state& state, game::id_planet id,
        char* out_str, uint8_t localized_flag) {
        char buffer[200];

        const auto planet = state.planets.find(id);
        if (planet == state.planets.end()) {
            out_str[0] = '\0';
            return;
        }
        const auto star = state.stars.find(planet->second.star_id);
        if (star == state.stars.end()) {
            out_str[0] = '\0';
            return;
        }
        const auto planet_num = HAROLD::Planet_Number(state, id);

        const char* numeral_str;
        if (localized_flag == 0) {
            numeral_str = MOX::roman_literals[planet_num + 1];
        } else {
            numeral_str = MOX::roman_literals[planet_num + 1];
            if (strcmp(numeral_str, "I") == 0) {
                numeral_str = HAROLD::H_Message(0xa0);
            }
        }

        snprintf(buffer, sizeof(buffer), "%s %s", star->second.name, numeral_str);

        if (strlen(buffer) > 30) {
            buffer[30] = '\0';
        }

        size_t len = strlen(buffer) + 1;
        memcpy(out_str, buffer, len);
    }

    char* Get_Planet_Name(const game::s_game_state& state, game::id_planet id) {
        Do_Get_Planet_Name(state, id, MOX::temp_string, 1);
        return MOX::temp_string;
    }

    char* Get_Planet_Name_No_Prime(const game::s_game_state& state, game::id_planet id) {
        Do_Get_Planet_Name(state, id, MOX::temp_string, 0);
        return MOX::temp_string;
    }

    game::id_colony __cdecl Last_Colony_Selected(int16_t star_idx, game::id_player player) {
        if (player.id >= MAX_PLAYERS) return game::NO_COLONY;
        auto colony_id = game::NO_COLONY;
        auto planet_idx = game::NO_PLANET;

        int8_t last_sel_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).last_planet_selected[player.id];

        if (last_sel_idx >= 0 && last_sel_idx < 5) {
            planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[last_sel_idx];
        }

        if (game::current_state.planets.contains(planet_idx)) {
            colony_id = game::current_state.planets.at(planet_idx).colony_id;
        }

        if (!game::current_state.colonies.contains(colony_id) || game::current_state.colonies.at(colony_id).owner != player) {
            Set_Colony_Auto_Select_For_Star(star_idx, player, &planet_idx);

            if (game::current_state.planets.contains(planet_idx)) {
                return game::current_state.planets.at(planet_idx).colony_id;
            }
            return game::NO_COLONY;
        }

        return colony_id;
    }

    bool __cdecl Player_Has_Colony_In_System(game::id_player player, int16_t system_idx) {
        return HAROLD::Player_Has_Colony(player, system_idx);
    }

    void __cdecl Set_Relocation(int16_t star_idx, game::id_player player, int16_t target_star_idx) {
        if (player.id >= MAX_PLAYERS) return;
        game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).relocate_ship_to[player.id] = game::Star_Key_At_Row(game::current_state, target_star_idx);
    }

    void __cdecl Set_All_Star_Relocations(int16_t target_star_idx, game::id_player player) {
        if (player.id >= MAX_PLAYERS) return;
        auto& state = game::current_state;
        const auto target = game::Star_Key_At_Row(state, target_star_idx);
        for (auto& [id, star] : state.stars) {
            auto& destination = star.relocate_ship_to[player.id];
            if (state.stars.contains(destination)) destination = target;
        }
    }

    void __cdecl Clear_All_Star_Relocations(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return;
        auto& state = game::current_state;
        for (auto& [id, star] : state.stars) {
            auto& destination = star.relocate_ship_to[player.id];
            if (state.stars.contains(destination)) destination = game::NO_STAR;
        }
    }

    char* __cdecl Design_Name(game::id_player player, int16_t design_idx) {
        strlcpy(MOX::temp_string, game::current_state.players.at(player.id).ship_designs[design_idx].name, sizeof(MOX::temp_string));

        return MOX::temp_string;
    }

    int16_t __cdecl Design_Weapon(game::id_player player, int16_t design_idx, int16_t weapon_idx) {
        return game::current_state.players.at(player.id).ship_designs[design_idx].ship_weapon[weapon_idx].type;
    }

    int8_t __cdecl Design_Weapon_Count(game::id_player player, int16_t design_idx, int16_t weapon_idx) {
        return game::current_state.players.at(player.id).ship_designs[design_idx].ship_weapon[weapon_idx].count;
    }

    uint16_t __cdecl Design_Weapon_Specials(game::id_player player, int16_t design_idx, int16_t weapon_idx) {
        return game::current_state.players.at(player.id).ship_designs[design_idx].ship_weapon[weapon_idx].specials;
    }

    int8_t __cdecl Design_Weapon_Firing_Arc(game::id_player player, int16_t design_idx, int16_t weapon_idx) {
        return game::current_state.players.at(player.id).ship_designs[design_idx].ship_weapon[weapon_idx].firing_arc;
    }

    int8_t __cdecl Design_Weapon_Ammo(game::id_player player, int16_t design_idx, int16_t weapon_idx) {
        return game::current_state.players.at(player.id).ship_designs[design_idx].ship_weapon[weapon_idx].ammo;
    }

    int8_t __cdecl Design_Picture_Num(game::id_player player, int16_t design_idx) {
        return game::current_state.players.at(player.id).ship_designs[design_idx].picture_num;
    }

    game::id_ship Fleet_Movement_Ship_Key(int16_t icon_idx) {
        if (icon_idx < 0 || icon_idx >= MOX::fleet_box.count || icon_idx >= std::ssize(MOX::fleet_movement_icon)) return game::NO_SHIP;
        const auto node = MOX::fleet_movement_icon[icon_idx].ship_node_idx;
        if (node < 0 || node >= std::ssize(MOX::ship_node)) return game::NO_SHIP;
        return game::Ship_Key_At_Slot(game::current_state, MOX::ship_node[node].ship_idx);
    }

    game::owner_slot __cdecl Ship_Stack_Owner(int16_t stack_idx) {
        return HACCESS::Ship_Owner(MOX::ship_node[stack_idx].ship_idx);
    }

    /**
     * Finds the index in the current ship stack matching the node_idx of a given ship icon.
     *
     * @param icon_idx Index into the ship_icon array.
     * @return The stack index if found, -1 if not found, or 0 if the stack is empty.
     */
    int16_t __cdecl Ship_Stack_From_Ship_Icon(int16_t icon_idx) {
        int16_t target_node_idx = MOX::ship_icon[icon_idx].node_idx;
        int16_t count = (int16_t)MOX::ship_stack_count;
        int16_t i = 0;

        // If the stack is empty, the function immediately returns 0.
        if (count == 0) {
            return 0;
        }

        bool found = false;
        while (i < count) {
            if (MOX::ship_stack_start[i] == target_node_idx) {
                found = true;
                break;
            }
            i++;
        }

        if (!found) {
            return -1;
        }

        return i;
    }

    char* __cdecl Race_Name_String(game::owner_slot owner) {
        char local_buffer[20];

        Race_Name(owner, local_buffer, sizeof(local_buffer));

        strlcpy(MOX::temp_string, local_buffer, sizeof(MOX::temp_string));

        return MOX::temp_string;
    }

    int16_t __cdecl Weapons_Index_From_Tech_App(int16_t tech_app_id) {
        int16_t result_index = 0;
        bool found = false;

        for (int16_t i = 0; i < (int16_t)(sizeof(TECHDATA::weapons) / sizeof(TECHDATA::weapons[0])); ++i) {
            // Check if we have already found the weapon to early exit
            if (found) {
                break;
            }

            if (TECHDATA::weapons[i].tech_app_id == tech_app_id) {
                result_index = i;
                found = true;
            }
        }

        return result_index;
    }

    int16_t Ship_Stack_From_Ship_Key(game::id_ship ship) {
        const auto row = game::Ship_Row(game::current_state, ship);
        if (row < 0) return -1;
        for (int16_t stack = 0; stack < MOX::ship_stack_count && stack < std::ssize(MOX::ship_stack_start); ++stack) {
            int16_t node = MOX::ship_stack_start[stack];
            for (int visited = 0; node >= 0 && node < std::ssize(MOX::ship_node) && visited < MAX_SHIPS; ++visited) {
                if (MOX::ship_node[node].ship_idx == row) return stack;
                node = MOX::ship_node[node].next_node;
            }
        }
        return -1;
    }

    int16_t __cdecl N_Colonies_At_Star(int16_t star_index, game::id_player owner_filter) {
        int16_t n_colonies = 0;

        for (int i = 0; i < 5; ++i) {
            const auto planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_index)).orbit_planet_ids[i];

            // Check if there is a planet in this orbit
            if (planet_idx != game::NO_PLANET) {
                game::id_colony colony_idx = game::current_state.planets.at(planet_idx).colony_id;

                // Check if the planet has a colony
                if (colony_idx != game::NO_COLONY) {
                    s_colony& colony = game::current_state.colonies.at(colony_idx);

                    // Outpost flag must be 0 (full colony, not an outpost)
                    if (colony.outpost_flag == 0) {
                        if (owner_filter == game::NO_PLAYER || colony.owner == owner_filter) {
                            n_colonies++;
                        }
                    }
                }
            }
        }

        return n_colonies;
    }

    int8_t __cdecl Get_Stack_Start_From_Saved_Ship_Array() {
        MOX::fleet_box_ship_stack = -1;
        int16_t stack_idx = 0;
        bool found = false;
        bool finished = false;

        while (!finished) {
            int16_t node_idx = MOX::ship_stack_start[stack_idx];

            do {
                // Check if ship from this stack is in the saved ship collection
                const auto key = game::Ship_Key_At_Row(game::current_state, MOX::ship_node[node_idx].ship_idx);
                if (MOX::fleet_box_ship_ids.contains(key)) {
                    MOX::fleet_box_ship_stack = stack_idx;
                    found = true;
                }
                node_idx = MOX::ship_node[node_idx].next_node;
            } while (node_idx != -1 && !found);

            if (found) {
                break;
            }

            stack_idx++;
            if (stack_idx >= (int16_t)MOX::ship_stack_count) {
                finished = true;
            }
        }

        if (MOX::fleet_box_ship_stack > -1 && MOX::fleet_box_ship_stack < (int16_t)MOX::ship_stack_count) {
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Ship_Stack_X(int16_t stack_idx) {
        int16_t ship_idx = MOX::ship_node[stack_idx].ship_idx;
        return ship_slots::At(game::current_state, ship_idx).x;
    }

    int16_t __cdecl Ship_Stack_Y(int16_t node_idx) {
        int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
        return ship_slots::At(game::current_state, ship_idx).y;
    }

    bool Player_Can_Order_Ship(game::id_player player, const s_ship_data* ship) {
        return ship && player.id < MAX_PLAYERS && factions::Player(ship->owner) == player &&
            SHIPMOVE::Can_Order_Ship(game::current_state, *ship);
    }

    game::id_star Nearest_Friendly_Star(game::id_ship ship, game::id_player player) {
        auto best_star = game::NO_STAR;
        uint8_t min_turns = 0xff;
        if (player.id >= MAX_PLAYERS) return best_star;
        const game::id_ship keys[] = {ship};
        for (const auto star_id : game::current_state.star_order) {
            const auto& star = game::current_state.stars.at(star_id);
            if (!game::Has_Player_Colony(star, player.id) || (star.blockaded.test(player))) continue;
            s_ship_move_info move_info{};
            SHIPMOVE::Evaluate_Fleet_Move(game::current_state, keys, {game::e_star_destination::star, star_id}, &move_info);
            if (move_info.moving && move_info.turns_left < min_turns) {
                min_turns = move_info.turns_left;
                best_star = star_id;
            }
        }

        return best_star;
    }

    static const s_ship_data* Find_Selected_Owned_Ship(const s_ship_node& node, int16_t ship_type_filter) {
        if (!node.selected) return nullptr;
        const auto* ship = game::Find_Legacy_Ship_At_Row(game::current_state, node.ship_idx);
        return ship && factions::Legacy_Code(ship->owner) == MOX::Legacy_Local_Player_Slot() && ship->status < 3 &&
            (ship_type_filter == 0xFF || ship->d.ship_type == ship_type_filter) ? ship : nullptr;
    }

    std::vector<game::id_ship> Selected_Fleet_Ship_Keys(int16_t ship_type_filter) {
        std::vector<game::id_ship> keys;
        for (const auto& node : MOX::ship_node) {
            if (const auto* ship = Find_Selected_Owned_Ship(node, ship_type_filter); ship && ship->id != game::NO_SHIP) {
                keys.push_back(ship->id);
            }
        }
        return keys;
    }

    std::vector<game::id_ship> Selected_Stack_Ship_Keys(int16_t stack) {
        if (stack < 0 || stack >= MOX::ship_stack_count || stack >= std::ssize(MOX::ship_stack_start)) return {};
        std::vector<game::id_ship> keys;
        auto node_idx = MOX::ship_stack_start[stack];
        for (int visited = 0; node_idx != -1; ++visited) {
            if (node_idx < 0 || node_idx >= std::ssize(MOX::ship_node) || visited >= std::ssize(MOX::ship_node)) return {};
            const auto& node = MOX::ship_node[node_idx];
            if (const auto* ship = Find_Selected_Owned_Ship(node, 0xFF); ship && ship->id != game::NO_SHIP) keys.push_back(ship->id);
            node_idx = node.next_node;
        }
        return keys;
    }

    bool __cdecl Player_Has_Tech(int16_t tech_idx, game::id_player player) {
        return player.id < MAX_PLAYERS && game::current_state.players[player.id].tech_applications[tech_idx] == TECH_RESEARCH_STATUS_RESEARCHED;
    }

    int16_t __cdecl Ship_Stack_Star_Id(int16_t node_idx) {
        int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
        return game::Ship_Orbit_Row(game::current_state, ship_slots::At(game::current_state, ship_idx).position);
    }

    int16_t __cdecl N_Star_Relocations(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return 0;
        int16_t relocation_count = 0;

        const auto& stars = game::current_state.stars;
        for (const auto& [id, star] : stars) {
            if (stars.contains(star.relocate_ship_to[player.id])) ++relocation_count;
        }

        return relocation_count;
    }

    void __cdecl Clear_Star_Relocation(int16_t star_idx, game::id_player player) {
        if (player.id >= MAX_PLAYERS) return;
        game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).relocate_ship_to[player.id] = game::NO_STAR;
    }

    game::id_player __cdecl Orion_Owner() {
        const auto& state = game::current_state;
        const auto orion = std::ranges::find_if(state.stars, [](const auto& entry) {
            return entry.second.system_special == SYSTEM_SPECIAL_ORION_STAR;
        });
        if (orion == state.stars.end()) return game::NO_PLAYER;
        for (const auto link : orion->second.orbit_planet_ids) {
            const auto id = link;
            if (id == game::NO_PLANET) continue;
            const auto& planet = game::current_state.planets.at(id);
            if (planet.planet_type == PLANET_TYPE_PLANET && planet.colony_id != game::NO_COLONY) {
                return state.colonies.at(planet.colony_id).owner;
            }
        }

        return game::NO_PLAYER;
    }

    int8_t __cdecl Special_Is_Planet_Special(int16_t special_type) {
        if (special_type == SYSTEM_SPECIAL_GOLD_DEPOSITS ||
            special_type == SYSTEM_SPECIAL_GEM_DEPOSITS ||
            special_type == SYSTEM_SPECIAL_NATIVES ||
            special_type == SYSTEM_SPECIAL_SPLINTER_COLONY ||
            special_type == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
            return 1;
        }

        return 0;
    }

    bool __cdecl Enemy_Ship_Going_To_My_Colony() {
        const auto& stars = game::current_state.stars;
        for (int16_t i = 0; i < MOX::ship_stack_count; ++i) {
            const auto node_idx = MOX::ship_stack_start[i];
            const auto ship_idx = MOX::ship_node[node_idx].ship_idx;
            if (HACCESS::Ship_Owner(ship_idx) == factions::From_Player(MOX::local_player)) continue;

            const auto& position = ship_slots::At(game::current_state, ship_idx).position;
            if (position.target != game::e_ship_target::star) continue;
            const auto star = stars.find(position.star_id.get());
            if (star != stars.end() && star->second.has_colony.test(MOX::local_player)) return true;
        }
        return false;
    }

    void __cdecl Race_Name(game::owner_slot owner, char* race_name_out, uint32_t race_name_out_size) {
        if (race_name_out == nullptr || race_name_out_size == 0) {
            return;
        }

        strlcpy(race_name_out, "Human", race_name_out_size);

        if (const auto player = factions::Player(owner); player != game::NO_PLAYER) {
            strlcpy(race_name_out, game::current_state.players[player.id].race_name, race_name_out_size);
        } else if (owner.code >= MAX_PLAYERS && owner.code < 15) {
            strlcpy(race_name_out, MOX::race_names[owner.code + 5], race_name_out_size);
        }
    }

    void __cdecl Set_All_Colony_Auto_Select_Flags() {
        for (auto& [star_id, star_record] : game::current_state.stars) {
            bool found_valid = false;
            int8_t last_selected = star_record.last_planet_selected[MOX::Legacy_Local_Player_Slot()];

            if (last_selected >= 0 && last_selected < 5) {
                const auto id = star_record.orbit_planet_ids[last_selected];
                const auto planet = game::current_state.planets.find(id);
                if (planet != game::current_state.planets.end()) {
                    const auto colony_id = planet->second.colony_id;
                    found_valid = game::current_state.colonies.contains(colony_id) &&
                        legacy_world::Encode_Player_Slot(game::current_state.colonies.at(colony_id).owner) == MOX::Legacy_Local_Player_Slot();
                }
            }

            if (!found_valid) {
                for (int16_t slot = 0; slot < 5; ++slot) {
                    const auto planet_idx = star_record.orbit_planet_ids[slot];
                    if (planet_idx != game::NO_PLANET) {
                        const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                        if (game::current_state.colonies.contains(colony_id)) {
                            s_colony* colony = &game::current_state.colonies.at(colony_id);
                            if (colony->outpost_flag == 0) {
                                if (legacy_world::Encode_Player_Slot(colony->owner) == (int8_t)MOX::Legacy_Local_Player_Slot()) {
                                    star_record.last_planet_selected[MOX::Legacy_Local_Player_Slot()] = (int8_t)slot;
                                } else {
                                    star_record.last_planet_selected[MOX::Legacy_Local_Player_Slot()] = -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    game::id_planet __cdecl System_Planet_Scanned_To_Planet_Id(int16_t star_idx, int16_t planet_idx) {
        return game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[planet_idx];
    }

    int16_t __cdecl System_Ship_Scanned_To_Ship_Id(int16_t ship_node_idx) {
        return MOX::ship_node[ship_node_idx].ship_idx;
    }

    void __cdecl Clear_Fleet_Box_Selected_Ship_Status() {
        MOX::fleet_selected_ships.clear();
    }

    void __cdecl Set_Hotseat_Fleet_Box_To_Last_Stack(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return;
        const auto key = MAINSCR::hotseat_fleet_box[player.id].stack_ship_id.get();

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        if (const auto* ship = [&] { const auto entry = game::current_state.ships.find(key); return entry == game::current_state.ships.end() ? nullptr : &entry->second; }();
            ship && ship->owner == factions::From_Player(player) && ship->status < 3) {
            MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Key(key);
        } else {
            MOX::fleet_box_ship_stack = -1;
        }

        if (MOX::fleet_box_ship_stack == -1) {
            MOVEBOX::Vanish_Fleet_Movement_Box();
            MOX::saved_boxes[2].is_open = 0;
            MOX::saved_boxes[2].field_id = (int16_t)MOX::box_off_flag[2];
        }
    }

    void __cdecl Build_Saved_Ship_Array() {
        MOX::fleet_box_ship_ids.clear();

        if ((MOX::ship_stack_count > 0) && (MOX::fleet_box.count > 0)) {
            for (int16_t saved_ship_idx = 0; saved_ship_idx < MOX::fleet_box.count; ++saved_ship_idx) {
                const int32_t ship_node_idx = MOX::fleet_movement_icon[saved_ship_idx].ship_node_idx;
                const auto key = game::Ship_Key_At_Row(game::current_state, MOX::ship_node[ship_node_idx].ship_idx);
                if (key != game::NO_SHIP) MOX::fleet_box_ship_ids.insert(key);
            }
        }
    }

    void __cdecl Save_Hotseat_Fleet_Box_Ship(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return;
        if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
            const int32_t ship_node_idx = MOX::fleet_movement_icon[0].ship_node_idx;
            MAINSCR::hotseat_fleet_box[player.id].stack_ship_id =
                game::Ship_Key_At_Row(game::current_state, MOX::ship_node[ship_node_idx].ship_idx);
            return;
        }

        MAINSCR::hotseat_fleet_box[player.id].stack_ship_id = game::NO_SHIP;
    }
} // namespace HACCESS
