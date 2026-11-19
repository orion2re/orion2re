#include "pch.h"

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

        return MOX::ship[ship_idx].Legacy_Location(MOX::star);
    }

    int16_t __cdecl Ship_Owner(int16_t ship_idx) {
        int16_t error_code = 0;

        if (ship_idx < 0) {
            error_code = 1;
        } else if (ship_idx >= MAX_SHIPS) {
            error_code = 2;
        } else {
            int8_t owner = MOX::ship[ship_idx].owner;
            if (owner < 0) {
                error_code = 3;
            } else if (owner > 15) {
                error_code = 4;
            }
        }

        if (error_code == 0) {
            return (int16_t)MOX::ship[ship_idx].owner;
        }

        char message[200];
        int32_t current_owner = -1;
        if (ship_idx >= 0 && ship_idx < MAX_SHIPS) {
            current_owner = (int32_t)MOX::ship[ship_idx].owner;
        }

        snprintf(message, sizeof(message), "Memory Corruption! val == %d, ship_id == %d, owner == %d stardate = %d\n", (int)error_code, ship_idx, current_owner, MOX::stardate);

        exitm::Exit_With_Message(message);

        return 0;
    }

    int16_t __cdecl Ship_Stack_From_Saved_Array() {
        int16_t best_owner_match_idx = -1;
        int16_t stack_index = -1;
        bool found = false;

        if (MOX::ship_stack_count < 1) {
            return -1;
        }

        for (int16_t i = 0; i < MAX_SHIPS; ++i) {
            // Status >= 3 typically indicates the ship is inactive/destroyed
            if (MOX::ship[i].status < 3 && struct_::Test_Bit_Field(&MOX::fleet_box_ship_ids, i)) {
                int16_t target_owner = Ship_Owner(i);

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

    bool __cdecl Star_Has_Relocation(int16_t star_idx, int16_t player_idx) {
        return MOX::star[star_idx].relocate_ship_to[player_idx] != game::NO_STAR;
    }

    int16_t __cdecl Relocation(int16_t star_idx, int16_t player_idx) {
        return MOX::star.Position_Of(MOX::star[star_idx].relocate_ship_to[player_idx]);
    }

    void __cdecl Set_Colony_Auto_Select_For_Star(::game::s_world_state* world_state, int16_t star_idx, int16_t player_idx, game::id_planet* out_planet_idx) {
        int16_t colony_found = -1;
        bool found = false;
        *out_planet_idx = game::NO_PLANET;

        for (int16_t i = 0; i < 5; ++i) {
            if (found) {
                break;
            }

            ::game::id_planet planet_idx = MOX::star[star_idx].planet_index[i];
            if (planet_idx != ::game::NO_PLANET) {
                *out_planet_idx = planet_idx;
                int16_t colony_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;

                if (colony_idx > -1) {
                    colony_found = colony_idx;
                    // Check if colony belongs to the player and is not an outpost
                    if (MOX::colony[colony_idx].outpost_flag == 0 && MOX::colony[colony_idx].owner == player_idx) {
                        MOX::star[star_idx].last_orbit_selected[player_idx] = (int8_t)i;
                        found = true;
                    } else {
                        colony_found = -1;
                    }
                }
            }
        }

        if (colony_found == -1) {
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

    int16_t __cdecl Ship_Icon_Owner(int16_t icon_idx) {
        int16_t node_idx = MOX::ship_icon[icon_idx].node_idx;

        return Ship_Owner(MOX::ship_node[node_idx].ship_idx);
    }

    bool __cdecl Fleet_Movement_Icon_Selected(int16_t icon_idx) {
        int16_t node_idx = MOX::fleet_movement_icon[icon_idx].ship_node_idx;
        return (bool)MOX::ship_node[node_idx].selected;
    }

    void __cdecl Do_Get_Planet_Name(::game::s_world_state* world_state, ::game::id_planet planet_idx, char* out_str, uint8_t localized_flag) {
        char buffer[200];

        const auto star_idx = game::Planet(world_state, planet_idx).Star_Position(MOX::star);
        int8_t planet_num = HAROLD::Planet_Number(world_state, legacy_world::Planet_Id(planet_idx));

        const char* numeral_str;
        if (localized_flag == 0) {
            numeral_str = MOX::roman_literals[planet_num + 1];
        } else {
            numeral_str = MOX::roman_literals[planet_num + 1];
            if (strcmp(numeral_str, "I") == 0) {
                numeral_str = HAROLD::H_Message(0xa0);
            }
        }

        s_star_data* star = &MOX::star[star_idx];

        snprintf(buffer, sizeof(buffer), "%s %s", star->name, numeral_str);

        if (strlen(buffer) > 30) {
            buffer[30] = '\0';
        }

        size_t len = strlen(buffer) + 1;
        memcpy(out_str, buffer, len);
    }

    char* __cdecl Get_Planet_Name(::game::id_planet planet_idx) {
        Do_Get_Planet_Name(&::game::world, legacy_world::Planet_Id(planet_idx), MOX::temp_string, 1);
        return MOX::temp_string;
    }

    char* __cdecl Get_Planet_Name_No_Prime(::game::id_planet planet_idx) {
        Do_Get_Planet_Name(&::game::world, legacy_world::Planet_Id(planet_idx), MOX::temp_string, 0);
        return MOX::temp_string;
    }

    bool __cdecl Is_Combat_Ship(int16_t ship_index) {
        return MOX::ship[ship_index].d.ship_type == SHIP_TYPE_COMBAT;
    }

    int16_t __cdecl Last_Colony_Selected(::game::s_world_state* world_state, int16_t star_idx, int16_t player_idx) {
        int16_t colony_idx = -1;
        ::game::id_planet planet_idx = ::game::NO_PLANET;

        int8_t last_sel_idx = MOX::star[star_idx].last_orbit_selected[player_idx];

        if (last_sel_idx >= 0 && last_sel_idx < 5) {
            planet_idx = MOX::star[star_idx].planet_index[last_sel_idx];
        }

        if (planet_idx != ::game::NO_PLANET && (::game::Find_Planet(world_state, legacy_world::Planet_Id(planet_idx)) != nullptr)) {
            colony_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;
        }

        if (colony_idx == -1 || MOX::colony[colony_idx].owner != player_idx) {
            Set_Colony_Auto_Select_For_Star(world_state, star_idx, player_idx, &planet_idx);

            if (planet_idx != ::game::NO_PLANET && (::game::Find_Planet(world_state, legacy_world::Planet_Id(planet_idx)) != nullptr)) {
                return ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;
            }
            return -1;
        }

        return colony_idx;
    }

    bool __cdecl Player_Has_Colony_In_System(int16_t player_idx, int16_t system_idx) {
        return HAROLD::Player_Has_Colony(player_idx, system_idx);
    }

    void __cdecl Set_Relocation(int16_t star_idx, int16_t player_idx, int16_t target_star_idx) {
        MOX::star[star_idx].relocate_ship_to[player_idx] = MOX::star.Id_At(target_star_idx);
    }

    void __cdecl Set_All_Star_Relocations(int16_t target_star_idx, int16_t player_idx) {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (Star_Has_Relocation(i, player_idx)) {
                Set_Relocation(i, player_idx, target_star_idx);
            }
        }
    }

    void __cdecl Clear_All_Star_Relocations(int16_t player_idx) {
        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (Star_Has_Relocation(star_idx, player_idx)) {
                Clear_Star_Relocation(star_idx, player_idx);
            }
        }
    }

    char* __cdecl Design_Name(int16_t player_idx, int16_t design_idx) {
        strlcpy(MOX::temp_string, MOX::player[player_idx].ship_designs[design_idx].name, sizeof(MOX::temp_string));

        return MOX::temp_string;
    }

    int16_t __cdecl Design_Weapon(int16_t player_idx, int16_t design_idx, int16_t weapon_idx) {
        return MOX::player[player_idx].ship_designs[design_idx].ship_weapon[weapon_idx].type;
    }

    int8_t __cdecl Design_Weapon_Count(int16_t player_idx, int16_t design_idx, int16_t weapon_idx) {
        return MOX::player[player_idx].ship_designs[design_idx].ship_weapon[weapon_idx].count;
    }

    uint16_t __cdecl Design_Weapon_Specials(int16_t player_idx, int16_t design_idx, int16_t weapon_idx) {
        return MOX::player[player_idx].ship_designs[design_idx].ship_weapon[weapon_idx].specials;
    }

    int8_t __cdecl Design_Weapon_Firing_Arc(int16_t player_idx, int16_t design_idx, int16_t weapon_idx) {
        return MOX::player[player_idx].ship_designs[design_idx].ship_weapon[weapon_idx].firing_arc;
    }

    int8_t __cdecl Design_Weapon_Ammo(int16_t player_idx, int16_t design_idx, int16_t weapon_idx) {
        return MOX::player[player_idx].ship_designs[design_idx].ship_weapon[weapon_idx].ammo;
    }

    int8_t __cdecl Design_Picture_Num(int16_t player_idx, int16_t design_idx) {
        return MOX::player[player_idx].ship_designs[design_idx].picture_num;
    }

    int16_t __cdecl Fleet_Movement_Ship_Id(int16_t icon_idx) {
        return MOX::ship_node[MOX::fleet_movement_icon[icon_idx].ship_node_idx].ship_idx;
    }

    int16_t __cdecl Ship_Stack_Owner(int16_t stack_idx) {
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

    char* __cdecl Race_Name_String(int16_t race_index) {
        char local_buffer[20];

        Race_Name(race_index, local_buffer, sizeof(local_buffer));

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

    /**
     * @brief Finds the index of the ship stack containing the given ship ID.
     * 
     * Iterates through all active ship stacks. For each stack, traverses the linked 
     * list of ship nodes until a matching ship_idx is found.
     * 
     * @param ship_id The ID of the ship to search for.
     * @return The index of the stack containing the ship, or 0 if not found.
     */
    int16_t __cdecl Ship_Stack_From_Ship_Id(int16_t ship_id) {
        int16_t found_stack_index = 0;
        uint8_t match_found = 0;

        for (int16_t i = 0; i < (int16_t)MOX::ship_stack_count; ++i) {
            // Check if we found the match in a previous iteration
            if (match_found != 0) {
                break;
            }

            int16_t node_idx = MOX::ship_stack_start[i];
            uint8_t stack_traversal_done = 0;

            do {
                if (MOX::ship_node[node_idx].ship_idx == ship_id) {
                    found_stack_index = i;
                    match_found = 1;
                    stack_traversal_done = 1;
                } else {
                    node_idx = MOX::ship_node[node_idx].next_node;
                    // End of the linked list for this stack
                    if (node_idx == -1) {
                        stack_traversal_done = 1;
                    }
                }
            } while (stack_traversal_done == 0);
        }

        return found_stack_index;
    }

    int16_t __cdecl N_Colonies_At_Star(::game::s_world_state* world_state, int16_t star_index, int16_t player_index) {
        int16_t n_colonies = 0;

        for (int i = 0; i < 5; ++i) {
            ::game::id_planet planet_idx = MOX::star[star_index].planet_index[i];

            // Check if there is a planet in this orbit
            if (planet_idx != ::game::NO_PLANET) {
                int16_t colony_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;

                // Check if the planet has a colony
                if (colony_idx != -1) {
                    s_colony& colony = MOX::colony[colony_idx];

                    // Outpost flag must be 0 (full colony, not an outpost)
                    if (colony.outpost_flag == 0) {
                        // Filter by owner index, or accept any if player_index is -1
                        if (colony.owner == player_index || player_index == -1) {
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
                if (struct_::Test_Bit_Field(&MOX::fleet_box_ship_ids, MOX::ship_node[node_idx].ship_idx)) {
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
        return MOX::ship[ship_idx].x;
    }

    int16_t __cdecl Ship_Stack_Y(int16_t node_idx) {
        int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
        return MOX::ship[ship_idx].y;
    }

    bool __cdecl Player_Can_Order_Ship(int16_t player_idx, s_ship_data* ship) {
        if ((int16_t)ship->owner == player_idx) {
            return SHIPMOVE::Can_Order_Ship(ship);
        }

        return false;
    }

    int16_t __cdecl Nearest_Star_To_XY(int16_t* ship_coords) {
        int16_t best_star_idx = -1;
        uint8_t min_turns = 0xff;
        s_ship_move_info move_info;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            // Check if player has a colony or presence in the system
            if (HAROLD::Player_Has_Colony(MOX::PLAYER_NUM, i)) {
                // Check if the system is blockaded for the current player
                if (((MOX::star[i].blockaded >> (uint8_t)MOX::PLAYER_NUM) & 1) == 0) {
                    SHIPMOVE::Ships_Try_To_Move_To(ship_coords, i, &move_info);

                    // If ships can actually move to this star
                    if (move_info.moving != 0) {
                        // Update best candidate if this star is closer in terms of turns
                        if (move_info.turns_left < min_turns) {
                            min_turns = move_info.turns_left;
                            best_star_idx = i;
                        }
                    }
                }
            }
        }

        return best_star_idx;
    }

    int16_t __cdecl Get_Fleet_Box_Selected_Ship_Ids(
        int16_t* selected_ids_out,
        int16_t selected_ids_capacity,
        int16_t ship_type_filter) {
        // Initialize output buffer with 0xFFFF (-1)
        memset(selected_ids_out, 0xff, selected_ids_capacity * sizeof(selected_ids_out[0]));

        int16_t selected_count = 0;
        bool stop_flag = false;

        for (int16_t i = 0; i < MAX_SHIPS; ++i) {
            // Check stop flag at the top of the loop as per assembly
            if (stop_flag) {
                break;
            }

            int16_t ship_idx = MOX::ship_node[i].ship_idx;
            s_ship_data& ship = MOX::ship[ship_idx];

            bool is_match = false;
            // Verify ownership and optional ship type filter
            if ((int16_t)(int8_t)ship.owner == MOX::PLAYER_NUM) {
                if (ship_type_filter == 0x00FF || (int16_t)(int8_t)ship.d.ship_type == ship_type_filter) {
                    is_match = true;
                }
            }

            // If node is selected and criteria matches, record the index
            if (MOX::ship_node[i].selected != 0 && is_match) {
                selected_ids_out[selected_count] = ship_idx;
                selected_count++;

                if (selected_count > MAX_SHIPS) {
                    stop_flag = true;
                }
            }
        }

        return selected_count;
    }

    bool __cdecl Player_Has_Tech(int16_t tech_idx, int16_t player_idx) {
        return MOX::player[player_idx].tech_applications[tech_idx] == TECH_RESEARCH_STATUS_RESEARCHED;
    }

    int16_t __cdecl Ship_Stack_Star_Id(int16_t node_idx) {
        int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
        return MOX::ship[ship_idx].Legacy_Location(MOX::star);
    }

    int16_t __cdecl N_Star_Relocations(int16_t player_idx) {
        int16_t relocation_count = 0;

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (Star_Has_Relocation(star_idx, player_idx)) {
                relocation_count++;
            }
        }

        return relocation_count;
    }

    void __cdecl Clear_Star_Relocation(int16_t star_idx, int16_t player_idx) {
        MOX::star[star_idx].relocate_ship_to[player_idx] = game::NO_STAR;
    }

    int8_t __cdecl Orion_Owner(::game::s_world_state* world_state) {
        for (int i = 0; i < 5; ++i) {
            ::game::id_planet planet_idx = MOX::star[0].planet_index[i];

            if (planet_idx != ::game::NO_PLANET) {
                s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));

                if (planet->planet_type == PLANET_TYPE_PLANET) {
                    int16_t colony_idx = planet->colony_index;

                    if (colony_idx != -1) {
                        return MOX::colony[colony_idx].owner;
                    }
                }
            }
        }

        return -1;
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

    int16_t __cdecl Planet_Orbit(::game::s_world_state* world_state, ::game::id_planet planet_idx) {
        return (int16_t)::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).orbit;
    }

    bool __cdecl Enemy_Ship_Going_To_My_Colony() {
        bool enemy_approaching = false;
        int16_t i = 0;

        if (MOX::ship_stack_count > 0) {
            while (!enemy_approaching) {
                int16_t node_idx = MOX::ship_stack_start[i];
                int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;

                if (HACCESS::Ship_Owner(ship_idx) != MOX::PLAYER_NUM) {
                    int16_t location = MOX::ship[ship_idx].Legacy_Location(MOX::star);
                    int16_t abs_location = HAROLD::Absolute_Location(location);

                    if (HAROLD::Player_Has_Colony(MOX::PLAYER_NUM, abs_location)) {
                        enemy_approaching = true;
                    }
                }

                i++;
                if (i >= (int16_t)MOX::ship_stack_count) {
                    break;
                }
            }
        }

        return enemy_approaching;
    }

    void __cdecl Race_Name(int16_t race_id, char* race_name_out, uint32_t race_name_out_size) {
        if (race_name_out == nullptr || race_name_out_size == 0) {
            return;
        }

        strlcpy(race_name_out, "Human", race_name_out_size);

        if (race_id < MAX_PLAYERS) {
            strlcpy(race_name_out, MOX::player[race_id].race_name, race_name_out_size);
        } else if (race_id == MAX_PLAYERS) {
            strlcpy(race_name_out, MOX::race_names[13], race_name_out_size);
        } else {
            strlcpy(race_name_out, MOX::race_names[race_id + 5], race_name_out_size);
        }
    }

    void __cdecl Set_All_Colony_Auto_Select_Flags(::game::s_world_state* world_state) {
        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            bool found_valid = false;
            int8_t last_selected = MOX::star[star_idx].last_orbit_selected[MOX::PLAYER_NUM];

            if (last_selected != -1) {
                // Note: In original code, the selection is considered valid if the colony index
                // of the planet matches the player number.
                if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(last_selected))).colony_index == MOX::PLAYER_NUM) {
                    found_valid = true;
                }
            }

            if (!found_valid) {
                for (int16_t slot = 0; slot < 5; ++slot) {
                    ::game::id_planet planet_idx = MOX::star[star_idx].planet_index[slot];
                    if (planet_idx != ::game::NO_PLANET) {
                        int16_t col_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;
                        if (col_idx > -1) {
                            s_colony* colony = &MOX::colony[col_idx];
                            if (colony->outpost_flag == 0) {
                                if (colony->owner == (int8_t)MOX::PLAYER_NUM) {
                                    MOX::star[star_idx].last_orbit_selected[MOX::PLAYER_NUM] = (int8_t)slot;
                                } else {
                                    MOX::star[star_idx].last_orbit_selected[MOX::PLAYER_NUM] = -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    game::id_planet __cdecl System_Planet_Scanned_To_Planet_Id(int16_t star_idx, int16_t orbit) {
        return HAROLD::Planet_Index(star_idx, orbit);
    }

    int16_t __cdecl System_Ship_Scanned_To_Ship_Id(int16_t ship_node_idx) {
        return MOX::ship_node[ship_node_idx].ship_idx;
    }

    void __cdecl Clear_Fleet_Box_Selected_Ship_Status() {
        memset(MOX::fleet_icon_selection_status, 0, sizeof(MOX::fleet_icon_selection_status));
    }

    void __cdecl Set_Hotseat_Fleet_Box_To_Last_Stack(int16_t player_idx) {
        const int16_t ship_id = MAINSCR::hotseat_fleet_box[player_idx].stack_ship_id;

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        if (ship_id > -1 &&
            HACCESS::Ship_Owner(ship_id) == player_idx &&
            (int8_t)MOX::ship[ship_id].status < 3) {
            MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Id(ship_id);
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
        memset(MOX::fleet_box_ship_ids, 0, sizeof(MOX::fleet_box_ship_ids));

        if ((MOX::ship_stack_count > 0) && (MOX::fleet_box.count > 0)) {
            for (int16_t saved_ship_idx = 0; saved_ship_idx < MOX::fleet_box.count; ++saved_ship_idx) {
                const int32_t ship_node_idx = MOX::fleet_movement_icon[saved_ship_idx].ship_node_idx;
                struct_::Set_Bit_Field(
                    MOX::fleet_box_ship_ids,
                    MOX::ship_node[ship_node_idx].ship_idx);
            }
        }
    }

    void __cdecl Save_Hotseat_Fleet_Box_Ship(int16_t fleet_box_idx) {
        if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
            const int32_t ship_node_idx = MOX::fleet_movement_icon[0].ship_node_idx;
            MAINSCR::_hotseat_fleet_box_stack_ship_id(fleet_box_idx) =
                MOX::ship_node[ship_node_idx].ship_idx;
            return;
        }

        MAINSCR::_hotseat_fleet_box_stack_ship_id(fleet_box_idx) = -1;
    }
} // namespace HACCESS