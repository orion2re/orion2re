#include "pch.h"

namespace SHIPSTAK {
    void __cdecl Delete_Ship_Node(int16_t stack_idx, int16_t prev_node_idx, int16_t node_idx) {
        if (prev_node_idx == -1) {
            MOX::ship_stack_start[stack_idx] = MOX::ship_node[node_idx].next_node;
        } else {
            MOX::ship_node[prev_node_idx].next_node = MOX::ship_node[node_idx].next_node;
        }
    }

    int16_t __cdecl Ship_Has_Special(int16_t ship_idx, int16_t special_idx) {
        int16_t has_special = 0;

        if (struct_::Test_Bit_Field(MOX::ship[ship_idx].d.special_device_flags, special_idx)) {
            if (!struct_::Test_Bit_Field(MOX::ship[ship_idx].special_device_damage_flags, special_idx)) {
                has_special = 1;
            }
        }

        return has_special;
    }

    int16_t __cdecl Ship_Stack_Star_Id(int16_t node_idx) {
        return MOX::ship[MOX::ship_node[node_idx].ship_idx].location;
    }

    bool __cdecl Ship_Has_Stealth_Device(int16_t ship_idx) {
        if (Ship_Has_Special(ship_idx, SPECIAL_STEALTH_FIELD) != 0) {
            return true;
        }

        if (Ship_Has_Special(ship_idx, SPECIAL_CLOAKING_DEVICE) != 0) {
            return true;
        }

        if (Ship_Has_Special(ship_idx, SPECIAL_PHASING_CLOAK) != 0) {
            return true;
        }

        return false;
    }

    void __cdecl Find_Ship_Stacks() {
        uint8_t* ship_skip_flags = (uint8_t*)allocate::Allocate_First_Block(MOX::global_data_seg, MAX_SHIPS);

        if (MOX::NUM_SHIPS > 0) {
            memset(ship_skip_flags, 0, MOX::NUM_SHIPS);
        }

        MOX::ship_stack_count = 0;
        MOX::next_free_node = 0;

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            if (MOX::ship[i].status >= 3) {
                ship_skip_flags[i] = 1;
            }
        }

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            if (ship_skip_flags[i] == 1) {
                continue;
            }

            bool found_stack = false;
            for (int16_t j = 0; j < MOX::ship_stack_count; ++j) {
                int16_t first_node_idx = MOX::ship_stack_start[j];
                int16_t first_ship_idx = MOX::ship_node[first_node_idx].ship_idx;

                if (MOX::ship[first_ship_idx].location == MOX::ship[i].location &&
                    MOX::ship[first_ship_idx].x == MOX::ship[i].x &&
                    MOX::ship[first_ship_idx].y == MOX::ship[i].y &&
                    MOX::ship[first_ship_idx].owner == MOX::ship[i].owner) {
                    int16_t node_idx = first_node_idx;
                    int16_t last_node = node_idx;

                    while (node_idx != -1) {
                        last_node = node_idx;
                        node_idx = MOX::ship_node[node_idx].next_node;
                    }

                    int16_t next_free = MOX::next_free_node;
                    MOX::ship_node[last_node].next_node = next_free;
                    MOX::ship_node[next_free].ship_idx = i;
                    MOX::ship_node[next_free].next_node = -1;
                    MOX::ship_node[next_free].selected = 0;
                    MOX::next_free_node++;

                    found_stack = true;
                    break;
                }
            }

            if (!found_stack) {
                int16_t next_free = MOX::next_free_node;
                MOX::ship_stack_start[MOX::ship_stack_count] = next_free;
                MOX::ship_node[next_free].ship_idx = i;
                MOX::ship_node[next_free].next_node = -1;
                MOX::ship_node[next_free].selected = 0;
                MOX::next_free_node++;
                MOX::ship_stack_count++;
            }
        }

        if (MOX::player[MOX::PLAYER_NUM].traits[TRAIT_OMNISCIENCE] == 0) {
            Remove_Non_Detected_Ships();
        }

        for (int16_t j = 0; j < MOX::ship_stack_count; ++j) {
            Sort_Ships_In_Stack(MOX::ship_stack_start[j]);
        }
    }

    void __cdecl Remove_Non_Detected_Ships() {
        const int16_t player_num = MOX::PLAYER_NUM;
        int32_t scan_range = scan_bonus.no_scanner;

        if ((int8_t)MOX::player[player_num].tech_applications[TECH_APP_SPACE_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
            scan_range = scan_bonus.space_scanner;
        }
        if ((int8_t)MOX::player[player_num].tech_applications[TECH_APP_TACHYON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
            scan_range = scan_bonus.tachyon_scanner;
        }
        if ((int8_t)MOX::player[player_num].tech_applications[TECH_APP_NEUTRON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
            scan_range = scan_bonus.neutron_scanner;
        }
        if ((int8_t)MOX::player[player_num].tech_applications[TECH_APP_SENSORS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            scan_range = scan_bonus.sensors;
        }

        for (int16_t stack_idx = 0; stack_idx < MOX::ship_stack_count; ++stack_idx) {
            int16_t* stack_head_ptr = &MOX::ship_stack_start[stack_idx];
            int16_t stack_head_node = *stack_head_ptr;

            const s_ship_data& stack_head_ship = MOX::ship[MOX::ship_node[stack_head_node].ship_idx];
            if ((int16_t)(int8_t)stack_head_ship.owner != player_num) {
                int32_t best_detection = -300;

                for (int16_t colony_idx = 0; colony_idx < MOX::NUM_COLONIES; ++colony_idx) {
                    const s_colony& colony = MOX::colony[colony_idx];
                    if ((int16_t)(int8_t)colony.owner == player_num) {
                        const int16_t colony_star_idx = MOX::planet[colony.planet].star_index;
                        const s_star_data& colony_star = MOX::star[colony_star_idx];

                        int32_t range_steps = (special::Range(
                                                   stack_head_ship.x,
                                                   stack_head_ship.y,
                                                   colony_star.x,
                                                   colony_star.y) +
                                                  29) /
                            30;

                        if (range_steps == 0) {
                            best_detection = scan_bonus.planet;
                        } else {
                            int32_t detection = scan_range;
                            if (colony.buildings[BUILDING_STAR_BASE] != 0) {
                                detection += scan_bonus.star_base;
                            }
                            if (colony.buildings[BUILDING_BATTLESTATION] != 0) {
                                detection += scan_bonus.battlestation;
                            }
                            if (colony.buildings[BUILDING_STAR_FORTRESS] != 0) {
                                detection += scan_bonus.star_fortress;
                            }
                            detection -= range_steps;
                            if (best_detection < detection) {
                                best_detection = detection;
                            }
                        }
                    }
                }

                for (int16_t other_ship_idx = 0; other_ship_idx < MOX::NUM_SHIPS; ++other_ship_idx) {
                    const s_ship_data& other_ship = MOX::ship[other_ship_idx];
                    if ((int16_t)(int8_t)other_ship.owner == player_num && (int8_t)other_ship.status < 3) {
                        int32_t range_steps = (special::Range(
                                                   stack_head_ship.x,
                                                   stack_head_ship.y,
                                                   other_ship.x,
                                                   other_ship.y) +
                                                  29) /
                            30;

                        if (range_steps == 0) {
                            best_detection = scan_bonus.fleet;
                        } else {
                            int32_t detection = scan_range - range_steps;
                            if (SHIPSTAK::Ship_Has_Special(other_ship_idx, SPECIAL_BATTLE_SCANNER) != 0) {
                                detection += scan_bonus.battle_scanner;
                            }
                            if (SHIPSTAK::Ship_Has_Special(other_ship_idx, SPECIAL_SCOUT_LAB) != 0) {
                                detection += scan_bonus.scout_lab;
                            }
                            if (best_detection < detection) {
                                best_detection = detection;
                            }
                        }
                    }
                }

                int16_t prev_node = -1;
                for (int16_t node_idx = *stack_head_ptr; node_idx != -1;) {
                    int16_t current_node = node_idx;
                    int16_t next_node = MOX::ship_node[current_node].next_node;
                    int16_t node_ship_idx = MOX::ship_node[current_node].ship_idx;
                    s_ship_data& ship = MOX::ship[node_ship_idx];
                    int16_t owner = (int16_t)(int8_t)ship.owner;

                    if (owner <= MAX_PLAYERS || ship.d.speed != 0) {
                        const bool stealthy_race =
                            owner < MAX_PLAYERS &&
                            MOX::player[owner].traits[TRAIT_STEALTHY_SHIPS] == 1;

                        int32_t device_bonus = 0;
                        if (SHIPSTAK::Ship_Has_Special(node_ship_idx, SPECIAL_STEALTH_FIELD) != 0) {
                            device_bonus += stealth_bonus.stealth_field;
                        }
                        if (SHIPSTAK::Ship_Has_Special(node_ship_idx, SPECIAL_CLOAKING_DEVICE) != 0) {
                            device_bonus += stealth_bonus.cloaking_device;
                        }
                        if (SHIPSTAK::Ship_Has_Special(node_ship_idx, SPECIAL_PHASING_CLOAK) != 0) {
                            device_bonus += stealth_bonus.phasing_cloak;
                        }

                        int32_t detection = 0;
                        if (owner == MAX_PLAYERS) {
                            detection = -5;
                        } else if ((int8_t)ship.status == 1 &&
                            SHIPSTAK::Traveling_Size_Penalty_Applies(
                                stealthy_race, device_bonus != 0)) {
                            detection = -ship.d.size;
                        }

                        if (stealthy_race) {
                            detection += stealth_bonus.stealthy_ships;
                        }

                        detection += device_bonus;

                        if (best_detection < detection) {
                            SHIPSTAK::Delete_Ship_Node(stack_idx, prev_node, current_node);
                        } else {
                            prev_node = current_node;
                        }
                    } else {
                        if (!HAROLD::Player_Has_Visited(player_num, ship.location)) {
                            SHIPSTAK::Delete_Ship_Node(stack_idx, prev_node, current_node);
                        }
                    }

                    node_idx = next_node;
                }

                if (*stack_head_ptr == -1) {
                    SHIPSTAK::Delete_Ship_Stack(stack_idx);
                    --stack_idx;
                }
            }
        }
    }

    void __cdecl Sort_Ships_In_Stack(int16_t start_node) {
        int16_t ship_count;
        int16_t ship_entries[MAX_SHIPS * 2];
        int16_t current_node = start_node;

        Build_Ship_Type_Array(start_node, ship_entries, &ship_count);

        if (ship_count > 1) {
            qsort(ship_entries, (size_t)ship_count, sizeof(ship_entries[0]) * 2, qsort_ships_by_type);

            if (ship_count > 0) {
                int16_t entry_idx = 0;
                while (entry_idx < ship_count) {
                    MOX::ship_node[current_node].ship_idx = ship_entries[entry_idx * 2];
                    current_node = MOX::ship_node[current_node].next_node;
                    ++entry_idx;
                }
            }
        }
    }

    void __cdecl Build_Ship_Type_Array(int16_t node_idx, int16_t* stack_array, int16_t* count_ptr) {
        *count_ptr = 0;
        int16_t current_node_idx = node_idx;

        do {
            const auto& node = MOX::ship_node[current_node_idx];
            int16_t ship_idx = node.ship_idx;

            if (ship_idx >= 0) {
                stack_array[*count_ptr * 2] = ship_idx;
                const auto& ship = MOX::ship[ship_idx];

                switch (ship.d.ship_type) {
                    case SHIP_TYPE_COMBAT:
                        stack_array[*count_ptr * 2 + 1] = (int16_t)ship.d.size * 50 + (int16_t)ship.d.picture_num;
                        break;
                    case SHIP_TYPE_COLONY:
                        stack_array[*count_ptr * 2 + 1] = -1;
                        break;
                    case SHIP_TYPE_TRANSPORT:
                        stack_array[*count_ptr * 2 + 1] = -2;
                        break;
                    case SHIP_TYPE_OUTPOST:
                        stack_array[*count_ptr * 2 + 1] = -3;
                        break;
                    default:
                        // case 3 and values > 4 skip setting the second array element
                        break;
                }
                (*count_ptr)++;
            }

            current_node_idx = node.next_node;
        } while (current_node_idx > 0 && current_node_idx < MAX_SHIPS);
    }

    int __cdecl qsort_ships_by_type(const void* left_entry, const void* right_entry) {
        const int16_t* left_words = (const int16_t*)left_entry;
        const int16_t* right_words = (const int16_t*)right_entry;

        return (int)right_words[1] - (int)left_words[1];
    }

    void __cdecl Delete_Ship_Stack(int16_t stack_idx) {
        strings::Delete_Structure(
            (int32_t)stack_idx,
            MOX::ship_stack_start,
            (int32_t)sizeof(MOX::ship_stack_start[0]),
            (int32_t)MOX::ship_stack_count);

        MOX::ship_stack_count--;
    }
} // namespace SHIPSTAK