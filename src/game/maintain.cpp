#include "pch.h"

namespace maintain {
    int32_t (*ship_worths)[MAX_SHIPS];
    uint8_t research_building_table[4] = {
        BUILDING_AUTOLAB,
        BUILDING_SUPERCOMPUTER,
        BUILDING_GALACTIC_CYBERNET,
        BUILDING_RESEARCH_LAB};

    /**
     * Determines if a building is processed in the first maintenance phase.
     * Buildings with no maintenance, specific AI groups, or the Capitol are excluded.
     */
    bool __cdecl Phase1_Building(int16_t building_idx) {
        if (TECHDATA::buildings[building_idx].maintenance <= 0) {
            return false;
        }

        int16_t tech_app_idx = TECHDATA::buildings[building_idx].tech_app;
        uint8_t ai_group = TECHDATA::technology_applications[tech_app_idx].ai_group;

        if (ai_group == 3 || ai_group == 1) {
            return false;
        }

        return building_idx != BUILDING_CAPITOL;
    }

    void __cdecl Player_Maintenance(s_player* player) {
        if (player->do_not_research) {
            Kill_Research_Buildings(player);
        }

        if ((int32_t)player->surplus_bc + player->bc >= 0) {
            return;
        }

        int32_t worths[MAX_SHIPS];
        memset(worths, 0, sizeof(worths));
        ship_worths = &worths;

        uint8_t eligible_players[MAX_PLAYERS];
        memset(eligible_players, 0, sizeof(eligible_players));

        int32_t player_idx = (int32_t)(player - MOX::player);
        const int32_t player_count = std::clamp<int32_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        int32_t eligible_count = 0;

        for (int32_t i = 0; i < player_count; ++i) {
            if (i != player_idx && player->contact[i]) {
                eligible_players[i] = 1;
                eligible_count++;
            }
        }

        if (eligible_count == 0) {
            for (int32_t i = 0; i < player_count; ++i) {
                if (i != player_idx && !MOX::player[i].eliminated && MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    eligible_players[i] = 1;
                    eligible_count++;
                }
            }
        }

        if (eligible_count == 0) {
            for (int32_t i = 0; i < player_count; ++i) {
                if (i != player_idx && !MOX::player[i].eliminated) {
                    eligible_players[i] = 1;
                    eligible_count++;
                }
            }
        }

        if (eligible_count > 0 && MOX::NUM_SHIPS > 0) {
            for (int16_t j = 0; j < MOX::NUM_SHIPS; ++j) {
                s_ship_data& ship = MOX::ship[j];
                if (ship.owner == player_idx && ship.status < 5 && ship.d.ship_type == SHIP_TYPE_COMBAT) {
                    for (int32_t k = 0; k < player_count; ++k) {
                        if (eligible_players[k]) {
                            worths[j] += AIPOWER::Ship_Strength_Vs_Ships(&AIDATA::state.world, j, (int16_t)k);
                        }
                    }
                    worths[j] = worths[j] / ((ship.d.size + 1) * eligible_count);
                }
            }
        }

        if (((int32_t)player->command_points * 4) / 3 < (int32_t)player->command_points_used) {
            Kill_Ships(player, 1, 1, SHIP_TYPE_OUTPOST);
            Kill_Ships(player, 1, 0, SHIP_TYPE_OUTPOST);
            Kill_Ships(player, 1, 1, SHIP_TYPE_TRANSPORT);
            Kill_Ships(player, 1, 1, SHIP_TYPE_COMBAT);
            Kill_Ships(player, 0, 1, SHIP_TYPE_OUTPOST);
            Kill_Ships(player, 0, 0, SHIP_TYPE_OUTPOST);
            Kill_Ships(player, 0, 1, SHIP_TYPE_TRANSPORT);
            Kill_Ships(player, 0, 1, SHIP_TYPE_COMBAT);
        }

        Kill_Buildings(player, Phase1_Building);
        Kill_Ships(player, 1, 1, SHIP_TYPE_OUTPOST);
        Kill_Ships(player, 1, 0, SHIP_TYPE_OUTPOST);
        Kill_Ships(player, 0, 1, SHIP_TYPE_OUTPOST);
        Kill_Ships(player, 0, 0, SHIP_TYPE_OUTPOST);
        Kill_Ships(player, 1, 1, SHIP_TYPE_TRANSPORT);
        Kill_Ships(player, 1, 1, SHIP_TYPE_COMBAT);
        Kill_Ships(player, 0, 1, SHIP_TYPE_TRANSPORT);
        Kill_Ships(player, 0, 1, SHIP_TYPE_COMBAT);
        Kill_Buildings(player, Phase2_Building);
        Kill_Ships(player, 1, 0, SHIP_TYPE_TRANSPORT);
        Kill_Ships(player, 0, 0, SHIP_TYPE_TRANSPORT);
        Kill_Ships(player, 1, 1, SHIP_TYPE_COLONY);
        Kill_Ships(player, 0, 1, SHIP_TYPE_COLONY);
        Kill_Ships(player, 1, 0, SHIP_TYPE_COLONY);
        Kill_Ships(player, 0, 0, SHIP_TYPE_COLONY);
        Kill_Spy(player);
        Kill_Ships(player, 1, 0, SHIP_TYPE_COMBAT);
        Kill_Ships(player, 0, 0, SHIP_TYPE_COMBAT);
        Kill_Leaders(player);
        Kill_Buildings(player, All_But_Capitol);

        ship_worths = nullptr;
    }

    bool __cdecl Phase2_Building(int16_t building_idx) {
        return TECHDATA::buildings[building_idx].maintenance > 0;
    }

    bool __cdecl All_But_Capitol(int16_t building_idx) {
        return building_idx != BUILDING_CAPITOL;
    }

    /**
     * @brief Removes research-specific buildings from all colonies of a specific player and provides BC compensation.
     *
     * This function iterates through all colonies belonging to the provided player. It checks for specific
     * research buildings (IDs 6, 30, 19, 35) and removes them if present. The player is compensated with
     * half of the building's cost, and a difficulty-based bonus is applied unless they have reached objective 100.
     * If the colony was currently producing the removed building, its production is reset.
     */
    void __cdecl Kill_Research_Buildings(s_player* player) {
        int32_t player_idx = (int32_t)(player - MOX::player);
        bool any_removed = false;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];

            if (colony->owner == player_idx) {
                for (int32_t j = 0; j < (int32_t)(sizeof(research_building_table) / sizeof(research_building_table[0])); ++j) {
                    uint8_t building_id = maintain::research_building_table[j];

                    if (colony->buildings[building_id] != 0) {
                        int32_t cost = TECHDATA::buildings[building_id].cost;

                        ERICNET::Remove_Building(i, (int16_t)building_id);
                        any_removed = true;

                        int32_t compensation = cost / 2;
                        player->bc += compensation;

                        if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                            player->bc += MOX::settings.game_difficulty;
                        }
                    }

                    if (colony->producing[0] == building_id) {
                        colony->producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
                    }
                }
            }
        }

        if (any_removed) {
            COLCALC::Do_Player_Colony_Calculations((int16_t)player_idx);
        }
    }

    /**
     * @brief Checks if the ship is currently located at a star system where its owner has a colony.
     *
     * @param ship Pointer to the ship data.
     * @return true if the ship is at a colony belonging to its owner, false otherwise.
     */
    void __cdecl Make_Scrap_Ships_Dead() {
        int32_t num_ships = (int32_t)MOX::NUM_SHIPS;
        s_ship_data* ships = MOX::ship;

        SHIPMOVE::Recalculate_All_Moving_Fleets();

        for (int16_t i = 0; i < num_ships; ++i) {
            if (ships[i].status == 7 || ships[i].status == 8) {
                SHIPS::Kill_Ship(i);
            }
        }
    }

    /**
     * Iteratively scraps the cheapest ships matching criteria until player maintenance deficit is resolved.
     */
    void __cdecl Kill_Ships(s_player* player, bool at_colony_only, bool restricted_mission, int16_t ship_type) {
        int16_t player_idx = (int16_t)(player - MOX::player);
        bool no_more_ships_found = false;

        while ((player->bc + player->surplus_bc < 0) && !no_more_ships_found) {
            s_ship_data* cheapest_ship = nullptr;
            int16_t min_worth = 0x7FFF;

            for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                s_ship_data* ship = &MOX::ship[i];

                if (ship->owner == player_idx &&
                    (int16_t)ship->d.ship_type == ship_type &&
                    ship->status < 3 /* Active */) {
                    if (at_colony_only && !Ship_Is_At_Colony(ship)) {
                        continue;
                    }

                    if (restricted_mission) {
                        if (!(ship->mission == 0 || ship->mission == 3 || ship->mission == 7)) {
                            continue;
                        }
                    }

                    int16_t worth = (*maintain::ship_worths)[i];
                    if (worth < min_worth) {
                        min_worth = worth;
                        cheapest_ship = ship;
                    }
                }
            }

            if (cheapest_ship == nullptr) {
                no_more_ships_found = true;
            } else {
                int16_t scrap_cost = maintain::Ship_Scrap_Value(cheapest_ship);
                player->bc += scrap_cost;

                if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                    player->bc += MOX::settings.game_difficulty;
                }

                cheapest_ship->status = 7; /* Scrapped */

                COLCALC::Do_Player_Colony_Post_Production(player_idx);

                int16_t ship_index = (int16_t)(cheapest_ship - MOX::ship);

                s_MSG msg;
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_TYPE_SCRAP_SHIP;
                msg.u.scrap_ship.where = cheapest_ship->location;
                msg.u.scrap_ship.ship_idx = ship_index;
                msg.u.scrap_ship.bc = scrap_cost;

                MSG_::Add_Msg(msg, player_idx);

                if (cheapest_ship->officer_index != -1) {
                    int16_t officer_idx = cheapest_ship->officer_index;
                    const AIDATA::s_world_view world{
                        .players = MOX::player,
                        .ships = MOX::ship,
                        .leaders = MOX::leaders,
                        .ship_count = &MOX::NUM_SHIPS,
                    };
                    aileader::Reassign_Fleet_Officer(&world, player_idx, officer_idx);

                    if (cheapest_ship->officer_index == officer_idx) {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                    }
                    cheapest_ship->officer_index = -1;
                }

                MAINSCR::Update_Ship_Icons_In_Between_Turns();
            }
        }
    }

    /**
     * Reduces the number of spies for a player if they have a negative BC balance (deficit).
     * The spies are removed until the deficit is cleared or all spies are gone.
     */
    void __cdecl Kill_Spy(s_player* player) {
        int deficit = (int)player->bc + (int)player->surplus_bc;

        if (deficit < 0) {
            deficit = -deficit;
            int16_t player_idx = (int16_t)(player - MOX::player);

            for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                if (deficit == 0) {
                    break;
                }

                if (MOX::player[i].eliminated == 0) {
                    int16_t num_spies = BILL::Get_Their_Spy_Number(player_idx, (int16_t)i);

                    if (num_spies != 0) {
                        if (deficit < (int)num_spies) {
                            int16_t new_spy_count = num_spies - (int16_t)deficit;
                            BILL::Set_Their_Spy_Number(player_idx, (int16_t)i, new_spy_count);
                            deficit = 0;
                        } else {
                            deficit -= (int)num_spies;
                            BILL::Set_Their_Spy_Number(player_idx, (int16_t)i, 0);
                        }
                    }
                }
            }

            COLCALC::Do_Player_Colony_Post_Production(player_idx);
        }
    }

    void __cdecl Kill_Leaders(s_player* player) {
        int32_t player_idx = (int32_t)(player - MOX::player);
        bool searched_without_removal = false;
        int32_t balance = (int32_t)player->surplus_bc + player->bc;

        do {
            if (balance >= 0 || searched_without_removal) {
                return;
            }

            searched_without_removal = true;
            for (int16_t leader_idx = 0; leader_idx < MAX_LEADERS; ++leader_idx) {
                if (MOX::leaders[leader_idx].player_index == player_idx) {
                    if (OFFICER::Officer_Maintenance((int16_t)player_idx, leader_idx) > 0) {
                        OFFICER::Deassign_Officer(leader_idx);
                        searched_without_removal = false;
                        COLCALC::Do_Player_Colony_Calculations((int16_t)player_idx);

                        s_MSG msg;
                        memset(&msg, 0, sizeof(msg));
                        msg.msg_type = MSG_TYPE_SCRAP_LEADER;
                        msg.u.scrap_leader.leader_idx = (int8_t)leader_idx;

                        MSG_::Add_Msg(msg, (int16_t)player_idx);
                        break;
                    }
                }
            }

            balance = (int32_t)player->surplus_bc + player->bc;
        } while (true);
    }

    int32_t __cdecl Building_Worth(s_colony* colony, int16_t building_idx) {
        int32_t worth = TECHDATA::buildings[building_idx].cost;

        switch (building_idx) {
            case BUILDING_ARMOR_BARRACKS:
            case BUILDING_MARINE_BARRACKS: {
                const int32_t owner_player_idx = colony->owner;
                const int32_t owner_trait_group = MOX::player[owner_player_idx].traits[0] / 2;

                if (owner_trait_group >= GOVERNMENT_FEUDAL / 2 && owner_trait_group <= GOVERNMENT_DICTATORSHIP / 2) {
                    if (building_idx == BUILDING_MARINE_BARRACKS) {
                        if (colony->buildings[BUILDING_ARMOR_BARRACKS] == 0) {
                            return worth * 8;
                        }
                    } else {
                        if (colony->buildings[BUILDING_MARINE_BARRACKS] == 0) {
                            return worth * 8;
                        }
                    }
                }
                break;
            }

            case BUILDING_AUTOMATED_FACTORY:
                return worth * 8;

            case BUILDING_DEEP_CORE_MINE:
                return worth * 7;

            case BUILDING_BIOSPHERES:
                return worth * 8;

            case BUILDING_HOLO_SIMULATOR:
            case BUILDING_PLEASURE_DOME: {
                const int32_t owner_player_idx = colony->owner;
                const int32_t owner_trait_group = MOX::player[owner_player_idx].traits[0] / 2;

                if (owner_trait_group != GOVERNMENT_UNIFICATION / 2) {
                    return worth * 7;
                }
                break;
            }

            case BUILDING_HYDROPONIC_FARM:
                return worth * 6;

            case BUILDING_GRAVITY_GENERATOR:
                return worth * 4;

            default:
                break;
        }

        return worth;
    }

    void __cdecl Kill_Buildings(s_player* player, bool(__cdecl* can_scrap)(int16_t)) {
        int16_t player_idx = (int16_t)(player - MOX::player);
        bool no_more_buildings = false;

        while ((player->bc + player->surplus_bc < 0) && !no_more_buildings) {
            int32_t min_worth = 0x7FFF;
            int32_t candidates_count = 0;
            s_colony* best_colony = nullptr;
            int32_t best_building = 0;

            for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
                s_colony* col = &MOX::colony[i];
                if (col->owner == (int8_t)player_idx) {
                    for (int32_t building_idx = 0; building_idx < (int32_t)(sizeof(col->buildings) / sizeof(col->buildings[0])); ++building_idx) {
                        if (col->buildings[building_idx] != 0 && can_scrap((int16_t)building_idx)) {
                            int32_t worth = Building_Worth(col, (int16_t)building_idx);
                            if (worth < min_worth) {
                                min_worth = worth;
                                candidates_count = 1;
                                best_colony = col;
                                best_building = building_idx;
                            } else if (worth == min_worth) {
                                candidates_count++;
                                if (game_random::Random((uint32_t)candidates_count) == 1) {
                                    best_colony = col;
                                    best_building = building_idx;
                                }
                            }
                        }
                    }
                }
            }

            if (best_colony == nullptr) {
                no_more_buildings = true;
            } else {
                int16_t scrap_value = (int16_t)(TECHDATA::buildings[best_building].cost / 2);
                int16_t colony_idx = (int16_t)(best_colony - MOX::colony);

                ERICNET::Remove_Building(colony_idx, (int16_t)best_building);

                player->bc += scrap_value;
                if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                    player->bc += MOX::settings.game_difficulty;
                }

                COLCALC::Colony_Calculation(colony_idx);

                s_MSG msg;
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_TYPE_COLONY;
                msg.u.colony.planet_idx = best_colony->planet;
                msg.u.colony.subtype = MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED;
                msg.u.colony.c.scrap.building_id = (int16_t)best_building;
                msg.u.colony.c.scrap.bc = scrap_value;

                MSG_::Add_Msg(msg, player_idx);
            }
        }
    }
} // namespace maintain