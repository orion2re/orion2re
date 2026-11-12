#include "pch.h"

namespace AIDATA {

    uint16_t g_base_planet_values[MAX_PLAYERS][MAX_PLANETS];
    uint16_t (*ship_strengths)[MAX_SHIPS][MAX_PLAYERS];
    uint16_t (*ship_strengths_in_nebula)[MAX_SHIPS][MAX_PLAYERS];
    uint16_t (*ship_bomb_strengths)[MAX_SHIPS][4];
    int16_t (*planet_stage_weights)[MAX_PLANETS][MAX_PLAYERS];
    uint32_t (*base_star_stage_weights)[MAX_STARS][MAX_PLAYERS];
    uint32_t (*star_stage_weights)[MAX_STARS][MAX_PLAYERS];
    uint8_t* n_turns_from_to;
    s_turns_from_to_intermediate_cache_entry* n_turns_from_to_intermediate;
    int8_t* ship_n_turns_to;
    s_ship_n_turns_to_intermediate_cache_entry* ship_n_turns_to_intermediate;
    size_t route_star_count;
    size_t route_player_count;
    size_t route_ship_count;
    int32_t (*possible_ship_strengths)[MAX_STARS][MAX_PLAYERS][MAX_PLAYERS][10];
    s_ai_colony_info (*ai_colony_info)[MAX_COLONIES];
    s_ai_player_info (*ai_player_info)[MAX_PLAYERS];
    s_ai_star_info (*ai_star_info)[MAX_STARS];
    uint8_t (*star_in_range_of_player)[MAX_STARS];
    uint8_t (*star_in_extended_range_of_player)[MAX_STARS];
    int32_t (*enroute_ship_strengths)[MAX_STARS][MAX_PLAYERS][MAX_PLAYERS][10];
    int8_t (*enroute_ships)[MAX_STARS][MAX_PLAYERS][10];
    int8_t (*enroute_human_ships)[MAX_STARS][10];
    int8_t (*enroute_nonstealthy_ships)[MAX_STARS][MAX_PLAYERS];
    int16_t (*all_ships)[MAX_SHIPS];
    int16_t (*i_all_combat_ships)[MAX_PLAYERS][MAX_STARS + 1];
    s_ai_tech_info* aitech;
    s_ai_refit_player (*airefit)[MAX_PLAYERS];
    int32_t (*ship_refit_desirabilities)[MAX_SHIPS];
    uint16_t (*base_planet_values)[MAX_PLAYERS][MAX_PLANETS] = &g_base_planet_values;
    uint16_t g_contextual_planet_values[MAX_PLAYERS * MAX_PLANETS];
    uint16_t (*contextual_planet_values)[MAX_PLAYERS * MAX_PLANETS] = &g_contextual_planet_values;
    uint16_t n_all_ships;

    int16_t __cdecl Colony_Dominant_Player(s_colony* colony) {
        uint8_t player_counts[MAX_PLAYERS] = {0};
        int total_pops = 0;

        if (colony->n_pops != 0) {
            for (int i = 0; i < (int)colony->n_pops; ++i) {
                uint32_t player_id = POP::Get_Race(colony->pop[i]);
                if (player_id < MAX_PLAYERS) {
                    total_pops++;
                    player_counts[player_id]++;
                }
            }
        }

        int max_player = (int)MOX::NUM_PLAYERS - 1;
        int i = 0;
        if (max_player > 0) {
            do {
                if (player_counts[i] > player_counts[max_player]) {
                    max_player = i;
                }
                i++;
            } while (i < max_player);
        }

        uint8_t max_count = player_counts[max_player];

        if (max_count * 2 > total_pops) {
            return (int16_t)max_player;
        }

        int16_t current_owner = (int16_t)colony->owner;
        uint8_t owner_count = player_counts[current_owner];

        if (owner_count * 3 <= total_pops && max_count != 0 && owner_count != max_count) {
            return (int16_t)max_player;
        }

        return current_owner;
    }

    int32_t __cdecl Refit_Desirability(s_ship_data* ship) {
        int32_t desirability = 0;
        int owner = (int)(int8_t)ship->owner;

        if ((int)(int8_t)ship->d.previous_owner != owner) {
            return 0;
        }

        s_ai_tech_info_player* ai_tech = &AIDATA::aitech->players[owner];

        if (ai_tech->best_shield > ship->d.shield_type) {
            desirability += (ai_tech->best_shield - ship->d.shield_type) * 10;
        }

        if (ai_tech->best_computer > ship->d.computer_type) {
            desirability += (ai_tech->best_computer - ship->d.computer_type) * 7;
        }

        if (ai_tech->best_armor > ship->d.armor_type) {
            desirability += (ai_tech->best_armor - ship->d.armor_type) * 5;
        }

        uint8_t best_beam_cost = 0;
        uint8_t best_missile_cost = 0;

        for (int i = 0; i < 8; ++i) {
            int weapon_type = (int)ship->d.ship_weapon[i].type;
            if (weapon_type != WEAPON_NO_WEAPONS) {
                uint16_t app_id = TECHDATA::weapons[weapon_type].tech_app_id;
                uint16_t field_id = TECHDATA::technology_applications[app_id].tech_field_id;
                uint8_t normalized_cost = (uint8_t)(TECHDATA::technology_fields[field_id].cost / 100);

                int8_t weapon_type_class = TECHDATA::weapons[weapon_type].type;
                if (weapon_type_class == WEAPON_TYPE_BEAM) {
                    if (normalized_cost > best_beam_cost) {
                        best_beam_cost = normalized_cost;
                    }
                } else if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_TORPEDO) {
                    if (normalized_cost > best_missile_cost) {
                        best_missile_cost = normalized_cost;
                    }
                }
            }
        }

        if (best_beam_cost != 0 && ai_tech->best_weapon1 > best_beam_cost) {
            desirability += (ai_tech->best_weapon1 - best_beam_cost) * 8;
        }

        if (best_missile_cost != 0 && ai_tech->best_weapon2 > best_missile_cost) {
            desirability += (ai_tech->best_weapon2 - best_missile_cost) * 6;
        }

        int size = (int)(int8_t)ship->d.size;
        if (desirability != 0 && size < SHIP_SIZE_DOOM_STAR) {
            s_ship_design* standard_design = &MOX::player[owner].ship_designs[size];
            // NOTE: This raw compare assumes s_ship_design has a stable packed byte layout.
            if (memcmp(&ship->d, standard_design, 99 /* sizeof(s_ship_design) */) == 0) {
                return 0;
            }
        }

        return desirability;
    }

    void Compute_AI_Data() {
        RUSS::Assert_Settings();
        ai_state::Begin_Turn();

        memset(ai_player_info, 0, sizeof(*ai_player_info));
        RUSS::Mox_Update();
        memset(ship_strengths, 0, sizeof(*ship_strengths));
        memset(ship_strengths_in_nebula, 0, sizeof(*ship_strengths_in_nebula));
        memset(ship_bomb_strengths, 0, sizeof(*ship_bomb_strengths));
        memset(enroute_ship_strengths, 0, sizeof(*enroute_ship_strengths));
        memset(enroute_ships, 0, sizeof(*enroute_ships));
        memset(enroute_human_ships, 0, sizeof(*enroute_human_ships));
        memset(enroute_nonstealthy_ships, 0, sizeof(*enroute_nonstealthy_ships));

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data& ship = MOX::ship[i];
            if (ship.status < 3) {
                int owner = ship.owner;
                bool player_owned = owner >= 0 && owner < MAX_PLAYERS;
                int16_t star_idx = -1;
                int turn_idx = -1;
                if (ship.status == 0) {
                    star_idx = ship.location;
                    turn_idx = 0;
                } else if (ship.status == 1) {
                    star_idx = ship.location - SHIP_LOCATION_MOVING_OFFSET;
                    turn_idx = (int)ship.turns_left;
                } else if (ship.status == 2) {
                    star_idx = HAROLD::XY_To_Star_Id(ship.x, ship.y);
                    turn_idx = 0;
                }

                if (star_idx < MOX::NUM_STARS) {
                    bool in_nebula = MOX::star[star_idx].in_nebula != 0;
                    if (player_owned && turn_idx < 10 && MOX::player[owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        (*enroute_human_ships)[star_idx][turn_idx] = 1;
                    }
                    for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
                        if (owner != p && MOX::player[p].eliminated == 0) {
                            if (ship.d.ship_type == SHIP_TYPE_COMBAT) {
                                uint16_t str = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships(i, p);
                                (*ship_strengths)[i][p] = str;
                                if (player_owned) {
                                    (*ai_player_info)[owner].total_ship_strength[p] += str;
                                }
                                (*ship_strengths_in_nebula)[i][p] = (uint16_t)AIPOWER::Ship_Strength_Vs_Ships_In_Nebula(i, p);
                                AIPOWER::Ship_Strength_Vs_Planetary_Shield(i, (*ship_bomb_strengths)[i]);
                                RUSS::Mox_Update();
                                if (player_owned && turn_idx < 10) {
                                    uint16_t cur_str = in_nebula ? (*ship_strengths_in_nebula)[i][p] : (*ship_strengths)[i][p];
                                    (*enroute_ship_strengths)[star_idx][owner][p][turn_idx] += cur_str;
                                }
                            }
                            if (player_owned) {
                                if (turn_idx < 10) {
                                    (*enroute_ships)[star_idx][owner][turn_idx] = 1;
                                }
                                if (MOX::player[owner].traits[TRAIT_STEALTHY_SHIPS] == 0 && (*enroute_nonstealthy_ships)[star_idx][owner] == 0) {
                                    if (!SHIPSTAK::Ship_Has_Stealth_Device(i)) {
                                        (*enroute_nonstealthy_ships)[star_idx][owner] = 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            memset(MOX::player[p].total_ship_strength, 0, sizeof(MOX::player[p].total_ship_strength));
        }

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            int owner = MOX::ship[i].owner;
            if (owner >= 0 && owner < MAX_PLAYERS) {
                for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
                    MOX::player[owner].total_ship_strength[p] += (*ship_strengths)[i][p];
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
            for (int p_o = 0; p_o < MOX::NUM_PLAYERS; ++p_o) {
                for (int t = 0; t < 9; ++t) {
                    for (int p_v = 0; p_v < MOX::NUM_PLAYERS; ++p_v) {
                        (*enroute_ship_strengths)[s][p_o][p_v][t + 1] += (*enroute_ship_strengths)[s][p_o][p_v][t];
                        if ((*enroute_ships)[s][p_o][t]) {
                            (*enroute_ships)[s][p_o][t + 1] = 1;
                        }
                    }
                    if ((*enroute_human_ships)[s][t]) {
                        (*enroute_human_ships)[s][t + 1] = 1;
                    }
                }
            }
        }

        RUSS::Mox_Update();
        int32_t range_sq[MAX_PLAYERS], ext_range_sq[MAX_PLAYERS];
        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            int32_t r = MOX::player[p].ship_range;
            range_sq[p] = (r * 30) * (r * 30);
            int32_t ext_r = r * 45;
            ext_range_sq[p] = ext_r * ext_r;
        }

        memset(star_in_range_of_player, 0, MAX_STARS);
        memset(star_in_extended_range_of_player, 0, MAX_STARS);

        for (int16_t c = 0; c < MOX::NUM_COLONIES; ++c) {
            s_colony& col = MOX::colony[c];
            if (col.planet != -1) {
                int16_t star_idx = MOX::planet[col.planet].star_index;
                int16_t sx = MOX::star[star_idx].x;
                int16_t sy = MOX::star[star_idx].y;
                for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                    int32_t dx = sx - MOX::star[s].x;
                    int32_t dy = sy - MOX::star[s].y;
                    int32_t d2 = dx * dx + dy * dy;
                    if (d2 <= ext_range_sq[col.owner]) {
                        if (!struct_::Test_Bit_Field(MOX::star[star_idx].black_hole_blocks, s)) {
                            (*star_in_extended_range_of_player)[s] |= (1 << col.owner);
                            if (d2 <= range_sq[(int)col.owner] && !struct_::Test_Bit_Field(MOX::star[star_idx].black_hole_blocks, s)) {
                                (*star_in_range_of_player)[s] |= (1 << col.owner);
                            }
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        memset(n_turns_from_to, -1, Route_Entry_Count());
        memset(n_turns_from_to_intermediate, -1, Route_Entry_Count() * sizeof(*n_turns_from_to_intermediate));
        for (int16_t s1 = 0; s1 < MOX::NUM_STARS; ++s1) {
            for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
                if (((*star_in_extended_range_of_player)[s1] >> p) & 1) {
                    for (int16_t s2 = 0; s2 < MOX::NUM_STARS; ++s2) {
                        if (((*star_in_extended_range_of_player)[s2] >> p) & 1) {
                            N_Turns_From_To(s1, s2, p) = (uint8_t)SHIPMOVE::Player_N_Turns_From_Star_To_Star((int16_t)p, s1, s2);
                            RUSS::Mox_Update();
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        memset(ship_n_turns_to, -1, Ship_Route_Entry_Count());
        memset(ship_n_turns_to_intermediate, -1, Ship_Route_Entry_Count() * sizeof(*ship_n_turns_to_intermediate));
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data& ship = MOX::ship[i];
            if (ship.status < 5 && ship.status == 1) {
                int16_t dest_star = SHIPMOVE::Ship_Destination_Star(&ship);
                Ship_N_Turns_To(i, dest_star) = ship.turns_left;
                if (SHIPMOVE::Can_Order_Ship(&ship)) {
                    uint8_t mask = (1 << ship.owner);
                    int16_t move_info[2] = {i, -1};
                    s_ship_move_info out_info;
                    for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                        if (((*star_in_extended_range_of_player)[s] & mask) && s != dest_star) {
                            SHIPMOVE::Ships_Try_To_Move_To(move_info, s, &out_info);
                            Ship_N_Turns_To(i, s) = out_info.turns_left;
                            RUSS::Mox_Update();
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        memset(ai_colony_info, 0, sizeof(*ai_colony_info));
        for (int16_t c = 0; c < MOX::NUM_COLONIES; ++c) {
            s_colony& col = MOX::colony[c];
            if (col.owner != -1) {
                s_ai_colony_info& ai_col = (*ai_colony_info)[c];
                RUSS::Mox_Update();
                for (int p_idx = 0; p_idx < col.n_pops; ++p_idx) {
                    if (POP::Is_Conquered(col.pop[p_idx])) {
                        ai_col.n_flagged_pops++;
                    }
                }
                ai_col.race = (int8_t)Colony_Dominant_Player(&col);
                ai_col.pop_limit = (int8_t)COLCALC::Colony_Race_Pop_Limit(&col, (int16_t)ai_col.race);
                if (MOX::player[ai_col.race].traits[TRAIT_EATS_MINERALS] == 0 || MOX::player[col.owner].traits[TRAIT_EATS_MINERALS] == 0) {
                    ai_col.food_buildings_applicable = 1;
                }
                if (MOX::player[(int)ai_col.race].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    ai_col.pollution_controls_applicable = 1;
                }
            }
        }

        for (int16_t c = 0; c < MOX::NUM_COLONIES; ++c) {
            s_colony& col = MOX::colony[c];
            if (col.owner != -1) {
                int owner = col.owner;
                int16_t star_idx = MOX::planet[col.planet].star_index;
                RUSS::Mox_Update();
                for (int p = MOX::NUM_PLAYERS - 1; p >= 0; --p) {
                    if (p != owner && (*enroute_ships)[star_idx][p][9]) {
                        (*ai_colony_info)[c].threatened = 1;
                        break;
                    }
                }
            }
        }

        RUSS::Mox_Update();
        memset(ai_star_info, 0, sizeof(*ai_star_info));
        for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
            s_ai_star_info& ai_star = (*ai_star_info)[s];
            for (int pl_i = 0; pl_i < 5; ++pl_i) {
                if (MOX::star[s].planet_index[pl_i] != -1) {
                    ai_star.n_planets_present++;
                }
            }
            for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
                if (MOX::player[p].objectives != PLAYER_OBJECTIVE_HUMAN && MOX::player[p].eliminated == 0 && (((*star_in_extended_range_of_player)[s] >> p) & 1)) {
                    for (int other_p = MOX::NUM_PLAYERS - 1; other_p >= 0; --other_p) {
                        if (other_p != p && (*enroute_ships)[s][other_p][9]) {
                            ai_star.per_player[p].enemy_enroute_or_threat = 1;
                            break;
                        }
                    }
                }
            }
        }

        Compute_Star_Player_Range_Info();

        RUSS::Mox_Update();
        for (int p1 = 0; p1 < MOX::NUM_PLAYERS; ++p1) {
            RUSS::Mox_Update();
            for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                if ((MOX::star[s].has_colony >> p1) & 1) {
                    (*ai_player_info)[p1].n_stars_with_colony++;
                    if ((MOX::star[s].blockaded >> p1) & 1) {
                        (*ai_player_info)[p1].n_blockaded_stars++;
                    }
                }
            }
            for (int p2 = 0; p2 < MOX::NUM_PLAYERS; ++p2) {
                if (p1 == p2) continue;
                if (MOX::player[p1].contact[p2]) {
                    (*ai_player_info)[p1].n_contacts++;
                    (*ai_player_info)[p1].n_reachable_players++;
                    (*ai_player_info)[p1].reachable_players[p2] = 1;
                } else {
                    bool reachable = false;
                    uint8_t mask = (1 << p1) | (1 << p2);
                    for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                        if ((*ai_star_info)[s].n_planets_present != 0 && (((*star_in_extended_range_of_player)[s] & mask) == mask)) {
                            reachable = true;
                            break;
                        }
                    }
                    if (reachable) {
                        (*ai_player_info)[p1].n_reachable_players++;
                        (*ai_player_info)[p1].reachable_players[p2] = 1;
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int16_t c = 0; c < MOX::NUM_COLONIES; ++c) {
            s_colony& col = MOX::colony[c];
            if (col.owner != -1) {
                (*ai_player_info)[(int)col.owner].colony_count++;
                if ((MOX::star[MOX::planet[col.planet].star_index].blockaded >> col.owner) & 1) {
                    (*ai_player_info)[(int)col.owner].colony_blockaded_count++;
                }
            }
        }

        RUSS::Mox_Update();
        int16_t max_colonies = 0;
        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            if ((*ai_player_info)[p].colony_count > max_colonies) {
                max_colonies = (*ai_player_info)[p].colony_count;
            }
        }
        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            (*ai_player_info)[p].colony_deficit = max_colonies - (*ai_player_info)[p].colony_count;
        }

        RUSS::Mox_Update();
        Compute_Base_Planet_Values();
        RUSS::Mox_Update();
        Compute_Contextual_Planet_Values();
        RUSS::Mox_Update();
        ::aitech::Compute_AI_Tech_Info(aitech);
        RUSS::Mox_Update();

        n_all_ships = 0;
        memset(airefit, 0, sizeof(*airefit));
        memset(ship_refit_desirabilities, 0, sizeof(*ship_refit_desirabilities));

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->d.ship_type == SHIP_TYPE_COMBAT && ship->owner >= 0 && ship->owner < MOX::NUM_PLAYERS && ship->status < 3 && ship->mission != 4 /* SHIP_MISSION_SCRAP */) {
                (*all_ships)[n_all_ships++] = i;
                if (ship->status == 0) {
                    RUSS::Mox_Update();
                    int32_t d = Refit_Desirability(ship);
                    (*ship_refit_desirabilities)[i] = d;
                    s_ai_refit_player& refit_p = (*airefit)[ship->owner];
                    refit_p.entry[refit_p.n_entries].desirability = d;
                    if (d != 0) {
                        refit_p.entry[refit_p.n_entries].ship_idx = i;
                        refit_p.n_entries++;
                    }
                }
            }
        }

        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            RUSS::Mox_Update();
            qsort((*airefit)[p].entry, (*airefit)[p].n_entries, 6, airefit_cmp);
        }

        RUSS::Mox_Update();
        memset(i_all_combat_ships, -1, sizeof(*i_all_combat_ships));

        qsort(all_ships, n_all_ships, 2, sort_ships);

        for (int i = 0; i < n_all_ships; ++i) {
            int16_t ship_idx = (*all_ships)[i];
            int16_t loc = MOX::ship[ship_idx].location;
            int owner = MOX::ship[ship_idx].owner;
            if (loc < MAX_STARS) {
                if ((*i_all_combat_ships)[owner][loc] == -1) {
                    (*i_all_combat_ships)[owner][loc] = (int16_t)i;
                }
            } else {
                if ((*i_all_combat_ships)[owner][MOX::NUM_STARS] == -1) {
                    (*i_all_combat_ships)[owner][MOX::NUM_STARS] = (int16_t)i;
                }
            }
        }

        RUSS::Mox_Update();

        memset(possible_ship_strengths, 0, sizeof(*possible_ship_strengths));

        for (int i_as = 0; i_as < n_all_ships; ++i_as) {
            int16_t i = (*all_ships)[i_as];
            int8_t stealth = (int8_t)struct_::Test_Bit_Field(MOX::ship[i].d.special_device_flags, 11);
            int16_t start_star = MOX::ship[i].location;
            int owner = MOX::ship[i].owner;
            int initial_delay = 0;

            RUSS::Mox_Update();

            if (MOX::ship[i].status == 10) { // SHIP_STATUS_ANTARAN
                initial_delay = 0;
            } else if (start_star < MOX::NUM_STARS) {
                initial_delay = 0;
            } else if (start_star < SHIP_LOCATION_WORMHOLE_OFFSET) {
                start_star -= SHIP_LOCATION_MOVING_OFFSET;
                initial_delay = (int)MOX::ship[i].turns_left;
            } else {
                start_star = HAROLD::XY_To_Star_Id(MOX::ship[i].x, MOX::ship[i].y);
            }

            bool in_nebula = MOX::star[start_star].in_nebula != 0;

            for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
                if (p != owner) {
                    for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                        if ((((*star_in_extended_range_of_player)[s] >> p) & 1) || s == start_star) {
                            int turns = Player_N_Turns_From_Star_To_Star_With_Intermediate(owner, start_star, s, nullptr, stealth);
                            if (turns != -1) {
                                int eta = initial_delay + turns;
                                if (eta < 10) {
                                    uint16_t str = in_nebula ? (*ship_strengths_in_nebula)[i][p] : (*ship_strengths)[i][p];
                                    for (int t = eta; t < 10; ++t) {
                                        (*possible_ship_strengths)[s][owner][p][t] += str;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        RUSS::Mox_Update();
        memset(planet_stage_weights, 0, sizeof(*planet_stage_weights));
        for (int16_t pl = 0; pl < MOX::planet_count; ++pl) {
            if (MOX::planet[pl].colony_index != -1) {
                int owner = MOX::colony[MOX::planet[pl].colony_index].owner;
                (*planet_stage_weights)[pl][owner] = (*base_planet_values)[owner][pl];
            }
        }

        RUSS::Mox_Update();
        memset(star_stage_weights, 0, sizeof(*star_stage_weights));
        memset(base_star_stage_weights, 0, sizeof(*base_star_stage_weights));
        int16_t best_planet_at_star[MAX_STARS][MAX_PLAYERS];
        int16_t best_planet_val_at_star[MAX_STARS][MAX_PLAYERS];
        memset(best_planet_at_star, -1, sizeof(best_planet_at_star));
        memset(best_planet_val_at_star, 0, sizeof(best_planet_val_at_star));

        uint8_t player_treaty_matrix[MAX_PLAYERS][MAX_PLAYERS];
        memset(player_treaty_matrix, 0, sizeof(player_treaty_matrix));

        for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
            int flag = 0;
            for (int j = 0; j < MOX::NUM_PLAYERS; j++) {
                if (MOX::player[j].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    player_treaty_matrix[i][j] = 1;
                }
                if (MOX::player[i].treaty[j] > 3 && MOX::player[i].treaty[j] < 7) {
                    flag = 1;
                }
            }
            if (!flag) {
                memset(player_treaty_matrix[i], 1, sizeof(player_treaty_matrix[i]));
            }

            player_treaty_matrix[i][i] = 0;
        }

        for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
            if (MOX::star[s].has_colony) {
                RUSS::Mox_Update();
                for (int pl_i = 0; pl_i < 5; ++pl_i) {
                    int16_t pl_idx = MOX::star[s].planet_index[pl_i];
                    if (pl_idx != -1 && MOX::planet[pl_idx].colony_index != -1) {
                        int owner = MOX::colony[MOX::planet[pl_idx].colony_index].owner;
                        int16_t val = (*planet_stage_weights)[pl_idx][owner];
                        if (val > best_planet_val_at_star[s][owner]) {
                            best_planet_at_star[s][owner] = pl_idx;
                            best_planet_val_at_star[s][owner] = val;
                        }
                    }
                }
                for (int p_o = 0; p_o < MOX::NUM_PLAYERS; ++p_o) {
                    if (best_planet_at_star[s][p_o] != -1) {
                        int32_t threat_sum = 0;
                        int32_t value_sum = 0;

                        for (int p_v = 0; p_v < MOX::NUM_PLAYERS; ++p_v) {
                            if (player_treaty_matrix[p_o][p_v]) {
                                int16_t max_eta = 8;
                                if ((*ai_player_info)[p_v].n_stars_with_colony * 5 < (*ai_player_info)[p_o].n_stars_with_colony) {
                                    max_eta = ((*ai_player_info)[p_v].n_stars_with_colony <= 1) ? 1 : 4;
                                }
                                RUSS::Mox_Update();
                                for (int16_t other_s = 0; other_s < MOX::NUM_STARS; ++other_s) {
                                    int32_t str = (*possible_ship_strengths)[other_s][p_v][p_o][0];
                                    if (str != 0) {
                                        int turns = Player_N_Turns_From_Star_To_Star_With_Intermediate((int16_t)p_v, other_s, s, nullptr, true);
                                        if (turns != -1 && turns < max_eta) {
                                            threat_sum += str / (turns + 1);
                                        }
                                    }
                                }
                            }
                        }

                        for (int16_t pl = 0; pl < MOX::planet_count; ++pl) {
                            if (pl != best_planet_at_star[s][p_o] && Player_N_Turns_From_Star_To_Star_With_Intermediate((int16_t)p_o, s, MOX::planet[pl].star_index, nullptr, false) < 3) {
                                RUSS::Mox_Update();
                                value_sum += (*planet_stage_weights)[pl][p_o];
                            }
                        }
                        int32_t base_weight = best_planet_val_at_star[s][p_o] + (value_sum >> 4);
                        (*base_star_stage_weights)[s][p_o] = base_weight;
                        (*star_stage_weights)[s][p_o] = ((base_weight + 7) >> 3) * ((threat_sum + 7) >> 3);
                    }
                }
            }
        }

        RUSS::Mox_Update();
        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            AISTAGE::Compute_Defensive_Stage_Points((int16_t)p);
        }

        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                int8_t target = MOX::star[s].surrender_to[p];
                if (target >= MAX_PLAYERS) {
                    int target_p = target - MAX_PLAYERS;
                    MOX::star[s].surrender_to[p] = -1;
                    if (target_p >= 0 && target_p < MOX::NUM_PLAYERS &&
                        MOX::player[p].eliminated == 0 && MOX::player[target_p].eliminated == 0) {
                        if (COLCALC::May_Surrender_Star(s, (int16_t)p, (int16_t)target_p)) {
                            COLCALC::Surrender_Star(s, (int16_t)p, (int16_t)target_p);
                        }
                    }
                }
            }
        }

        RUSS::Assert_Settings();
        for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
            MOX::star[s].just_visited = 0;
        }
    }

    void __cdecl Compute_Star_Player_Range_Info() {
        if (MOX::NUM_STARS <= 0) {
            return;
        }

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            s_ai_star_info& ai_info = (*ai_star_info)[star_idx];

            for (int16_t colony_idx = 0; colony_idx < MOX::NUM_COLONIES; ++colony_idx) {
                s_colony& col = MOX::colony[colony_idx];
                int owner = (int)col.owner;

                if (owner != -1) {
                    int16_t col_star_idx = MOX::planet[col.planet].star_index;
                    int ship_range = (int)MOX::player[owner].ship_range;

                    if (Star_In_Range_Of_Given_Star(star_idx, col_star_idx, ship_range)) {
                        ai_info.per_player[owner].nearest_eta++;
                    }
                }
            }

            int8_t in_normal_range[MAX_PLAYERS];
            int8_t in_extended_range[MAX_PLAYERS];
            Find_Players_In_Range(star_idx, in_normal_range, in_extended_range);

            for (int player_idx = 0; player_idx < (int)MOX::NUM_PLAYERS; ++player_idx) {
                if ((MOX::star[star_idx].has_colony >> player_idx) & 1) {
                    s_player& plr = MOX::player[player_idx];
                    s_ai_star_player_block& block = ai_info.per_player[player_idx];

                    for (int other_idx = 0; other_idx < (int)MOX::NUM_PLAYERS; ++other_idx) {
                        if (player_idx == other_idx) {
                            continue;
                        }
                        if (MOX::player[other_idx].eliminated) {
                            continue;
                        }

                        if (in_normal_range[other_idx]) {
                            int treaty = (int)plr.treaty[other_idx];
                            switch (treaty) {
                                case 0:
                                    block.neutral_count++;
                                    break;
                                case 1:
                                case 2:
                                case 3:
                                    block.enemy_count++;
                                    break;
                                case 4:
                                case 5:
                                case 6:
                                    block.alliance_count++;
                                    break;
                                default:
                                    break;
                            }
                        } else if (in_extended_range[other_idx]) {
                            block.out_of_range_count++;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Find_Players_In_Range(int16_t star_idx, int8_t* in_normal_range_out, int8_t* in_extended_range_out) {
        auto& target_star = MOX::star[star_idx];
        int32_t target_x = target_star.x;
        int32_t target_y = target_star.y;

        memset(in_normal_range_out, 0, MAX_PLAYERS);
        memset(in_extended_range_out, 0, MAX_PLAYERS);

        int32_t normal_range_sq[MAX_PLAYERS];
        int32_t extended_range_sq[MAX_PLAYERS];

        for (int player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            int32_t range = MOX::player[player_idx].ship_range;
            int32_t range_sq = range * range;

            normal_range_sq[player_idx] = range_sq * 900;
            extended_range_sq[player_idx] = range_sq * 2025;
        }

        for (int16_t colony_idx = 0; colony_idx < MOX::NUM_COLONIES; ++colony_idx) {
            auto& colony = MOX::colony[colony_idx];
            int8_t owner = colony.owner;

            if (owner != -1 && in_normal_range_out[owner] == 0) {
                auto& planet = MOX::planet[colony.planet];
                auto& colony_star = MOX::star[planet.star_index];

                int32_t dx = (int32_t)colony_star.x - target_x;
                int32_t dy = (int32_t)colony_star.y - target_y;
                int32_t dist_sq = dx * dx + dy * dy;

                if (dist_sq <= extended_range_sq[owner]) {
                    in_extended_range_out[owner] = 1;
                    if (dist_sq <= normal_range_sq[owner]) {
                        in_normal_range_out[owner] = 1;
                    }
                }
            }
        }
    }

    int __cdecl airefit_cmp(const void* lhs, const void* rhs) {
        const s_ai_refit_entry* lhs_entry = (const s_ai_refit_entry*)lhs;
        const s_ai_refit_entry* rhs_entry = (const s_ai_refit_entry*)rhs;

        int32_t lhs_desirability = lhs_entry->desirability;
        int32_t rhs_desirability = rhs_entry->desirability;

        return lhs_desirability - rhs_desirability;
    }

    int __cdecl sort_ships(const void* lhs, const void* rhs) {
        int16_t idx1 = *(const int16_t*)lhs;
        int16_t idx2 = *(const int16_t*)rhs;

        s_ship_data& ship1 = MOX::ship[idx1];
        s_ship_data& ship2 = MOX::ship[idx2];

        if (ship1.owner < ship2.owner) {
            return -1;
        }
        if (ship1.owner > ship2.owner) {
            return 1;
        }

        int16_t loc1 = ship1.location;
        int16_t loc2 = ship2.location;

        if (loc2 > loc1) {
            if (loc1 < MAX_STARS) {
                return -1;
            }
        } else if (loc2 < loc1) {
            if (loc2 < MAX_STARS) {
                return 1;
            }
        }

        if (loc1 < MAX_STARS && loc2 < MAX_STARS) {
            return 0;
        }

        if (ship1.x < ship2.x) {
            return -1;
        }
        if (ship1.x > ship2.x) {
            return 1;
        }

        if (ship1.y < ship2.y) {
            return -1;
        }
        if (ship1.y > ship2.y) {
            return 1;
        }

        return 0;
    }

    void __cdecl Compute_Base_Planet_Values() {
        memset(*base_planet_values, 0, sizeof(*base_planet_values));

        if (MOX::NUM_PLAYERS <= 0) {
            return;
        }

        for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            for (int16_t planet_idx = 0; planet_idx < MOX::planet_count; ++planet_idx) {
                s_planet_data* planet = &MOX::planet[planet_idx];

                uint8_t system_visibility_bits = (*star_in_extended_range_of_player)[planet->star_index];

                if ((system_visibility_bits >> (player_idx & 0x1F)) & 1) {
                    int16_t col_idx = planet->colony_index;

                    RUSS::Mox_Update();

                    uint16_t value;
                    if (col_idx == -1) {
                        value = AIDATA::Uncolonized_Planet_Worth_To_Player(planet, &MOX::player[player_idx]);
                    } else {
                        value = Colony_Worth_To_Player(&MOX::colony[col_idx], &MOX::player[player_idx]);
                    }

                    (*base_planet_values)[player_idx][planet_idx] = value;
                }
            }
        }
    }

    void __cdecl Deallocate_AI_Data() {
        RUSS::Assert_Settings();
        ai_state::Complete_Turn();
    }

    /**
     * Computes the "contextual" value of planets for each player, adjusting the base
     * values based on ownership of other planets in the same star system.
     */
    void __cdecl Compute_Contextual_Planet_Values() {
        // Clear all contextual values for all players (MAX_PLAYERS players * MAX_PLANETS planets)
        memset(*AIDATA::contextual_planet_values, 0, sizeof(*AIDATA::contextual_planet_values));

        if (MOX::NUM_PLAYERS <= 0) {
            return;
        }

        for (int p = 0; p < (int)MOX::NUM_PLAYERS; ++p) {
            for (int16_t i = 0; i < MOX::planet_count; ++i) {
                uint16_t base_val = (*AIDATA::base_planet_values)[p][i];
                if (base_val <= 0) {
                    continue;
                }

                s_planet_data* planet = &MOX::planet[i];
                s_star_data* star = &MOX::star[planet->star_index];

                // Refresh UI/input during processing
                RUSS::Mox_Update();

                uint16_t& contextual_val = (*AIDATA::contextual_planet_values)[p * MAX_PLANETS + i];
                contextual_val = base_val;

                int uncolonized_system_base_sum = 0;
                int owned_system_base_sum = 0;
                int owned_colonies_in_system = 0;
                int enemy_colonies_in_system = 0;

                // Examine all potential planets in the system (up to 5)
                for (int k = 0; k < 5; ++k) {
                    int16_t sys_planet_idx = star->planet_index[k];

                    if (sys_planet_idx == -1 || sys_planet_idx == i) {
                        continue;
                    }

                    int16_t colony_idx = MOX::planet[sys_planet_idx].colony_index;

                    if (colony_idx == -1) {
                        // Uncolonized planet in system
                        uncolonized_system_base_sum += (*AIDATA::base_planet_values)[p][sys_planet_idx];
                    } else {
                        if (MOX::colony[colony_idx].owner == p) {
                            // Colony owned by the current player
                            owned_colonies_in_system++;
                            owned_system_base_sum += (*AIDATA::base_planet_values)[p][sys_planet_idx];
                        } else {
                            // Colony owned by an enemy
                            enemy_colonies_in_system++;
                        }
                    }
                }

                if (planet->colony_index == -1) {
                    // Current planet is not yet colonized
                    if (owned_colonies_in_system > 0) {
                        // Bonus if we already have a presence in the system
                        contextual_val = (int16_t)((contextual_val * (planet->size + 1)) / 10);
                    } else {
                        // Small bonus for general system potential
                        contextual_val += (int16_t)(uncolonized_system_base_sum / 8);
                    }

                    // Penalize value if enemies are present
                    if (enemy_colonies_in_system > 0) {
                        contextual_val = (int16_t)(contextual_val / (enemy_colonies_in_system + 2));
                    }
                } else {
                    // Current planet is already colonized; add value of neighboring potential
                    contextual_val += (int16_t)(uncolonized_system_base_sum / 8);
                    contextual_val += (int16_t)(owned_system_base_sum / 4);
                }
            }
        }
    }

    /**
     * Calculates the "proximity worth" value of a colony for a specific player.
     * This logic accounts for difficulty settings, alien management centers,
     * and strategic range to existing colonies.
     */
    int16_t __cdecl Player_N_Turns_From_Star_To_Star_With_Intermediate(
        int16_t player_idx, int16_t source_star_idx, int16_t dest_star_idx, int16_t* intermediate_star_idx_out, bool use_extended_range) {
        s_turns_from_to_intermediate_cache_entry& info =
            N_Turns_From_To_Intermediate(source_star_idx, dest_star_idx, player_idx);

        if (info.computed == -1) {
            s_player* plr = &MOX::player[player_idx];

            // Check if player can reach the star directly
            if (plr->ship_speed != 0 &&
                plr->tech_applications[TECH_APP_JUMP_GATE] != TECH_RESEARCH_STATUS_RESEARCHED &&
                plr->tech_applications[TECH_APP_STAR_GATE] != TECH_RESEARCH_STATUS_RESEARCHED &&
                MOX::star[source_star_idx].wormhole_star_id == -1 &&
                MOX::star[dest_star_idx].wormhole_star_id == -1 &&
                N_Turns_From_To(source_star_idx, dest_star_idx, player_idx) != 0xFF) {
                int dist = GEO::Parsecs_Between_Stars(source_star_idx, dest_star_idx);
                int turns = (dist + (uint8_t)plr->ship_speed - 1) / (uint8_t)plr->ship_speed;

                if (N_Turns_From_To(source_star_idx, dest_star_idx, player_idx) <= turns) {
                    if (((*star_in_range_of_player)[dest_star_idx] >> player_idx) & 1) {
                        info.turns = (uint8_t)turns;
                        info.intermediate_star = dest_star_idx;
                    } else {
                        info.intermediate_star = -1;
                    }
                    info.extended_turns = (uint8_t)turns;
                    info.extended_intermediate_star = dest_star_idx;
                    info.computed = 0;
                }
            }

            if (info.computed == -1) {
                for (int16_t i = MOX::NUM_STARS - 1; i >= 0; --i) {
                    // Check if Star i is in range and not blocked
                    if (((*star_in_extended_range_of_player)[i] >> player_idx) & 1) {
                        s_star_data* s = &MOX::star[i];

                        if (i == dest_star_idx || (s->spectral_class != STAR_CLASS_BLACK_HOLE && s->system_special != SYSTEM_SPECIAL_SPACE_MONSTER && (((s->has_colony >> player_idx) & 1) || s->has_colony == 0))) {
                            uint8_t dist_a_i = N_Turns_From_To(source_star_idx, i, player_idx);
                            uint8_t dist_i_b = N_Turns_From_To(i, dest_star_idx, player_idx);
                            int total_turns = dist_a_i + dist_i_b;

                            if ((((*star_in_range_of_player)[i] >> player_idx) & 1) && total_turns < info.turns) {
                                info.turns = (uint8_t)total_turns;
                                info.intermediate_star = (i == source_star_idx) ? dest_star_idx : i;
                            }

                            if (total_turns < info.extended_turns) {
                                info.extended_turns = (uint8_t)total_turns;
                                info.extended_intermediate_star = (i == source_star_idx) ? dest_star_idx : i;
                            }
                        }
                    }
                    if (info.turns <= 1) break;
                }
            }
            info.computed = 1;
        }

        if (intermediate_star_idx_out) {
            *intermediate_star_idx_out = use_extended_range ? info.extended_intermediate_star : info.intermediate_star;
        }

        if (use_extended_range) {
            return (int16_t)info.extended_turns;
        }
        return (int16_t)info.turns;
    }

    int16_t __cdecl Ship_N_Turns_To_Star_With_Intermediate(int16_t ship_idx, int16_t dest_star_idx, int16_t* intermediate_star_idx_out) {
        s_ship_n_turns_to_intermediate_cache_entry& cached_info =
            Ship_N_Turns_To_Intermediate(ship_idx, dest_star_idx);

        bool has_stargate = struct_::Test_Bit_Field(MOX::ship[ship_idx].d.special_device_flags, 11);

        if (cached_info.computed == -1) {
            int16_t loc = MOX::ship[ship_idx].location;
            if (loc >= MAX_STARS && loc < SHIP_LOCATION_WORMHOLE_OFFSET) {
                int owner = MOX::ship[ship_idx].owner;
                int16_t ship_dest_star = loc - SHIP_LOCATION_MOVING_OFFSET;

                if (cached_info.turns > 1) {
                    for (int16_t i = MOX::NUM_STARS - 1; i >= 0; i--) {
                        bool in_range = ((*star_in_range_of_player)[i] >> owner) & 1;
                        bool in_ext_range = ((*star_in_extended_range_of_player)[i] >> owner) & 1;

                        if (in_range || (in_ext_range && has_stargate)) {
                            if (i == dest_star_idx || i == ship_dest_star ||
                                (MOX::star[i].spectral_class != STAR_CLASS_BLACK_HOLE &&
                                    MOX::star[i].system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                                    (((MOX::star[i].has_colony >> owner) & 1) != 0 || MOX::star[i].has_colony == 0))) {
                                bool blocked = GEO::Black_Hole_Blocks_Points(
                                    MOX::ship[ship_idx].x, MOX::ship[ship_idx].y, MOX::star[i].x, MOX::star[i].y);

                                if (!blocked) {
                                    uint32_t total_turns = N_Turns_From_To(i, dest_star_idx, owner) +
                                        (uint8_t)Ship_N_Turns_To(ship_idx, i);
                                    if (total_turns < cached_info.turns) {
                                        cached_info.turns = (uint8_t)total_turns;
                                        cached_info.intermediate_star = i;
                                    }
                                }
                            }
                        }
                        if (cached_info.turns <= 1) {
                            break;
                        }
                    }
                }

            } else {
                if (loc >= SHIP_LOCATION_WORMHOLE_OFFSET) {
                    loc = HAROLD::XY_To_Star_Id(MOX::ship[ship_idx].x, MOX::ship[ship_idx].y);
                }

                cached_info.turns = (uint8_t)AIDATA::Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    MOX::ship[ship_idx].owner,
                    loc,
                    dest_star_idx,
                    intermediate_star_idx_out,
                    has_stargate);

                // BUG: intermediate_star_idx_out is dereferenced here without a null check.
                cached_info.intermediate_star = *intermediate_star_idx_out;
            }
            cached_info.computed = 1;
        }

        if (intermediate_star_idx_out != nullptr) {
            *intermediate_star_idx_out = cached_info.intermediate_star;
        }

        return cached_info.turns;
    }


} // namespace AIDATA