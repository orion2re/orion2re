#include "pch.h"

namespace COLONIZE {
    int16_t Star_N_Uncolonized_Planets(::game::s_world_state* world_state, s_star_data* star) {
        int16_t uncolonized_count = 0;

        for (int16_t i = 0; i < 5; ++i) {
            ::game::id_planet planet_idx = star->planet_index[i];

            if (planet_idx != ::game::NO_PLANET) {
                if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index == -1) {
                    uncolonized_count++;
                }
            }
        }

        return uncolonized_count;
    }

    int16_t Star_N_Outpostable_Planets(::game::s_world_state* world_state, s_star_data* star) {
        int16_t outpostable_count = 0;

        for (int32_t i = 0; i < 5; ++i) {
            ::game::id_planet planet_idx = star->planet_index[i];
            if (planet_idx != ::game::NO_PLANET) {
                if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index == -1) {
                    outpostable_count++;
                }
            }
        }

        return outpostable_count;
    }

    int16_t Star_N_Colonizable_Planets_For_Player(::game::s_world_state* world_state, s_star_data* star, int16_t player_idx) {
        int16_t colonizable_count = 0;

        for (int32_t i = 0; i < 5; ++i) {
            ::game::id_planet planet_idx = star->planet_index[i];
            if (planet_idx == ::game::NO_PLANET) {
                continue;
            }

            s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));

            if (planet->planet_type == PLANET_TYPE_PLANET) {
                int16_t colony_idx = planet->colony_index;
                if (colony_idx == -1) {
                    colonizable_count++;
                } else {
                    s_colony* colony = &MOX::colony[colony_idx];
                    if (colony->outpost_flag != 0 && colony->owner == player_idx) {
                        colonizable_count++;
                    }
                }
            }
        }

        return colonizable_count;
    }

    void Find_Colonize_Players(::game::s_world_state* world_state) {
        uint8_t colony_bits[MAX_STARS];
        uint8_t outpost_bits[MAX_STARS];

        memset(colony_bits, 0, sizeof(colony_bits));
        memset(outpost_bits, 0, sizeof(outpost_bits));

        // Reset priority for all stars
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            MOX::star[i].colonize_player = MAX_PLAYERS;
        }

        // Phase 1: Check ships (Colony and Outpost ships)
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->status == 0) { // Active
                if (ship->d.ship_type == SHIP_TYPE_COLONY || ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                    int16_t star_idx = ship->Legacy_Location(MOX::star);
                    if (star_idx >= 0 && star_idx < MAX_STARS) {
                        s_star_data* star = &MOX::star[star_idx];
                        if (star->colonize_player == MAX_PLAYERS) {
                            star->colonize_player = ship->owner;
                        }

                        if (star->colonize_player == ship->owner) {
                            if (ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                                outpost_bits[star_idx] |= (1 << (ship->owner & 0x1F));
                            } else {
                                colony_bits[star_idx] |= (1 << (ship->owner & 0x1F));
                            }
                        } else {
                            star->colonize_player = -1; // Conflict
                        }
                    }
                }
            }
        }

        // Phase 2: Check existing colonies with colony bases
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner != -1 && colony->buildings[BUILDING_COLONY_BASE] != 0) {
                const auto star_idx = game::Planet(world_state, colony->planet).Star_Position(MOX::star);
                s_star_data* star = &MOX::star[star_idx];
                if (star->colonize_player == MAX_PLAYERS) {
                    star->colonize_player = colony->owner;
                }

                if (star->colonize_player == colony->owner) {
                    colony_bits[star_idx] |= (1 << (colony->owner & 0x1F));
                } else {
                    star->colonize_player = -1;
                }
            }
        }

        // Phase 3: Validate priority against actual colonizable/outpostable planets
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];
            int8_t priority_player = star->colonize_player;
            if (priority_player == MAX_PLAYERS) {
                star->colonize_player = -1;
            } else {
                bool keep = false;
                if ((colony_bits[i] >> (priority_player & 0x1F)) & 1) {
                    if (Star_N_Colonizable_Planets_For_Player(world_state, star, priority_player) > 0) {
                        keep = true;
                    }
                }
                if (!keep && ((outpost_bits[i] >> (priority_player & 0x1F)) & 1)) {
                    if (Star_N_Outpostable_Planets(world_state, star) > 0) {
                        keep = true;
                    }
                }

                if (!keep) {
                    star->colonize_player = -1;
                }
            }
        }

        // Phase 4: Block priority if an enemy/rival warship is present
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->status == 0 && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                int16_t star_idx = ship->Legacy_Location(MOX::star);
                if (star_idx >= 0 && star_idx < MAX_STARS) {
                    s_star_data* star = &MOX::star[star_idx];
                    int8_t colonizer = star->colonize_player;
                    if (colonizer >= 0 && colonizer < MAX_PLAYERS && ship->owner != colonizer) {
                        if (ship->owner >= MAX_PLAYERS) {
                            star->colonize_player = -1;
                        } else {
                            // Check treaty status: if we have peace/alliance (4, 5, 6), priority is contested
                            int8_t treaty = MOX::player[colonizer].treaty[ship->owner];
                            if (treaty >= 4 && treaty <= 6) {
                                star->colonize_player = -1;
                            }
                        }
                    }
                }
            }
        }

        // Phase 5: Cleanup redundant colony bases
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner != -1 && colony->buildings[BUILDING_COLONY_BASE] != 0) {
                const auto star_idx = game::Planet(world_state, colony->planet).Star_Position(MOX::star);
                s_star_data* star = &MOX::star[star_idx];

                bool remove_base = false;
                if (star->colonize_player != colony->owner) {
                    remove_base = true;
                } else if (Star_N_Colonizable_Planets_For_Player(world_state, star, colony->owner) == 0) {
                    remove_base = true;
                }

                if (remove_base) {
                    int16_t owner = colony->owner;
                    int32_t refund32 = TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                    int16_t refund16 = (int16_t)refund32;
                    colony->buildings[BUILDING_COLONY_BASE] = 0;
                    MOX::player[owner].bc += refund32;

                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_COLONY;
                    msg.u.colony.planet_idx = colony->planet;
                    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED;
                    msg.u.colony.c.scrap.building_id = BUILDING_COLONY_BASE;
                    msg.u.colony.c.scrap.bc = refund16;
                    MSG_::Add_Msg(msg, owner);
                }
            }
        }
    }

    size_t Plan_Colony_Reservations(std::span<const int16_t> demand,
        size_t first_player, std::span<int16_t> reserved_for_player) {
        if (first_player >= demand.size() || demand.size() > MAX_PLAYERS) return 0;

        int16_t allocated_counts[MAX_PLAYERS]{};
        size_t player = first_player;
        size_t skipped = 0;
        size_t reserved = 0;
        while (reserved < reserved_for_player.size() && skipped < demand.size()) {
            if (allocated_counts[player] < demand[player]) {
                reserved_for_player[reserved++] = static_cast<int16_t>(player);
                ++allocated_counts[player];
                skipped = 0;
            } else {
                ++skipped;
            }
            if (++player == demand.size()) player = 0;
        }
        return reserved;
    }

    void Allocate_New_Colony_Slots() {
        int16_t uncolonized_counts[MAX_PLAYERS] = {0};
        int16_t free_slots_indices[MAX_COLONIES];

        int16_t free_slots_count = 0;

        Find_Colonize_Players(&::game::world);

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];
            if (star->colonize_player != -1) {
                int16_t count = Star_N_Uncolonized_Planets(&::game::world, star);
                uncolonized_counts[(int8_t)star->colonize_player] += count;
            }
        }

        for (int16_t i = 0; i < MAX_COLONIES; ++i) {
            MOX::colony[i].allocated_to = (char)0xFF;
            if (i >= MOX::NUM_COLONIES || MOX::colony[i].planet == ::game::NO_PLANET) {
                free_slots_indices[free_slots_count++] = i;
            }
        }

        if (MOX::NUM_PLAYERS <= 0) {
            return;
        }

        int16_t reserved_for_player[MAX_COLONIES];
        const size_t reserved = Plan_Colony_Reservations(
            {uncolonized_counts, static_cast<size_t>(MOX::NUM_PLAYERS)},
            MOX::stardate % MOX::NUM_PLAYERS,
            {reserved_for_player, static_cast<size_t>(free_slots_count)});
        for (size_t i = 0; i < reserved; ++i) {
            MOX::colony[free_slots_indices[i]].allocated_to = static_cast<int8_t>(reserved_for_player[i]);
        }
    }

    void __cdecl AI_Colonize(::game::s_world_state* world_state, int16_t player_idx) {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];
            bool done_with_star = false;

            while (!done_with_star) {
                if (star->colonize_player == player_idx) {
                    s_colony* colony_base_ptr = REPORT::Player_Colony_Base_At_Star(world_state, player_idx, i);
                    s_ship_data* colony_ship_ptr = REPORT::Player_Ship_Type_At_Star(player_idx, SHIP_TYPE_COLONY, i, 1);

                    if (colony_base_ptr == nullptr && colony_ship_ptr == nullptr) {
                        done_with_star = true;
                    } else {
                        uint32_t best_worth = 0;
                        game::id_planet best_planet_idx = game::NO_PLANET;

                        for (int p = 0; p < 5; ++p) {
                            ::game::id_planet planet_idx = star->planet_index[p];
                            if (planet_idx != ::game::NO_PLANET && ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index == -1) {
                                uint32_t worth = AIDATA::Uncolonized_Planet_Worth_To_Player(&game::current, &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))), &MOX::player[player_idx]);
                                if (best_worth < (worth & 0xFFFF)) {
                                    best_planet_idx = planet_idx;
                                    best_worth = worth & 0xFFFF;
                                }
                            }
                        }

                        if (best_planet_idx == game::NO_PLANET) {
                            done_with_star = true;
                            if (colony_base_ptr != nullptr) {
                                // Sell colony base buildings if no worthwhile planets found
                                do {
                                    MOX::player[player_idx].bc += TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                                    colony_base_ptr->buildings[BUILDING_COLONY_BASE] = 0;
                                    colony_base_ptr = REPORT::Player_Colony_Base_At_Star(world_state, player_idx, i);
                                } while (colony_base_ptr != nullptr);
                            }
                        } else {
                            COLONIZE::Make_New_Colony(legacy_world::Planet_Id(best_planet_idx), player_idx);

                            if (colony_base_ptr == nullptr) {
                                // Consumed a colony ship
                                for (int16_t s = 0; s < MOX::NUM_SHIPS; ++s) {
                                    s_ship_data* ship = &MOX::ship[s];
                                    if (ship->mission == 4 && ship->owner == (char)player_idx && ship->Legacy_Location(MOX::star) == i) {
                                        ship->mission = 0;
                                    }
                                }
                                SHIPS::Kill_Ship(&game::world, (int16_t)(colony_ship_ptr - MOX::ship));
                                MAINSCR::Update_Ship_Icons_In_Between_Turns();
                            } else {
                                // Consumed an existing colony base building
                                ERICNET::Remove_Building(world_state, MOX::colony.Position_Of(colony_base_ptr), BUILDING_COLONY_BASE);
                            }
                            // Continue checking system (logic loops to see if more ships/bases exist)
                        }
                    }
                } else {
                    done_with_star = true;
                }
            }
        }
    }

    void __cdecl All_AI_Colonize() {
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                AI_Colonize(&::game::world, i);
            }
        }
    }

    void __cdecl Make_New_Colony_Or_Outpost(::game::s_world_state* world_state, ::game::id_planet planet_idx, int16_t player_idx, int16_t is_outpost) {
        bool is_reused = false;
        s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));
        s_colony* colony = nullptr;

        if (is_outpost == 0 && planet->colony_index != -1) {
            is_reused = true;
            colony = &MOX::colony[planet->colony_index];
        } else {
            for (int16_t colony_idx = 0; colony_idx < MAX_COLONIES; ++colony_idx) {
                if (MOX::colony[colony_idx].allocated_to == (int8_t)player_idx) {
                    colony = &MOX::colony[colony_idx];
                    break;
                }
            }
        }

        if (colony == nullptr) {
            return;
        }

        const int16_t colony_idx = MOX::colony.Position_Of(colony);
        const bool network = MOX::game_type == 2 || MOX::game_type == 3;
        if (!MOX::colony.Activate(colony_idx, network ? player_idx : -1)) return;
        if (colony_idx >= MOX::NUM_COLONIES) MOX::NUM_COLONIES = colony_idx + 1;
        INITGAME::Init_Colony(world_state, colony_idx, legacy_world::Planet_Id(planet_idx), (uint8_t)player_idx);
        colony = &MOX::colony[colony_idx];

        colony->allocated_to = -1;

        if (is_outpost == 0) {
            colony->n_pops = 1;

            if (first_pop_to_production != 0 ||
                planet->food_per_farmer < 1 ||
                MOX::player[player_idx].traits[TRAIT_EATS_MINERALS] != 0 ||
                MOX::player[player_idx].traits[TRAIT_CYBERNETIC] != 0) {
                POP::Set_Prof(&colony->pop[0], POP::PROF_WORKER);
            } else {
                POP::Set_Prof(&colony->pop[0], POP::PROF_FARMER);
            }

            POP::Set_Race(&colony->pop[0], player_idx);
            POP::Set_Original_Owner(&colony->pop[0], player_idx);
            POP::Set_Assigned(&colony->pop[0], true);

            if (planet->planet_special == SYSTEM_SPECIAL_NATIVES) {
                for (int32_t i = 1; i < 4; ++i) {
                    POP::Set_Prof(&colony->pop[i], POP::PROF_FARMER);
                    POP::Set_Race(&colony->pop[i], POP::RACE_NATIVE);
                    POP::Set_Original_Owner(&colony->pop[i], player_idx);
                    POP::Set_Assigned(&colony->pop[i], true);
                }
                colony->n_pops = 4;
                planet->planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
                MOX::star[planet->Star_Position(MOX::star)].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
            }

            if (is_reused) {
                colony->buildings[BUILDING_MARINE_BARRACKS] = 1;
            }

            COLCALC::Colony_Calculation(colony_idx);
            colony->just_produced = COLONY_PRODUCTION_JUST_COLONIZED;
            if (MOX::player[player_idx].home_planet_id == ::game::NO_PLANET) {
                MOX::player[player_idx].home_planet_id = planet_idx;
            }
        } else {
            colony->outpost_flag = 1;
            colony->n_pops = 0;
            colony->climate = planet->planet_type == PLANET_TYPE_PLANET
                ? planet->climate
                : PLANET_CLIMATE_BARREN;
        }

        colony->producing[0] = COLONY_PRODUCTION_NONE;
        COLCALC::Compute_Star_Colony_Stuff(world_state, &MOX::star[planet->Star_Position(MOX::star)]);
    }

    void __cdecl Make_New_Colony(::game::id_planet planet_idx, int16_t player_idx) {
        COLONIZE::Make_New_Colony_Or_Outpost(&::game::world, legacy_world::Planet_Id(planet_idx), player_idx, 0);
    }

    void __cdecl Make_New_Outpost(::game::id_planet planet_idx, int16_t player_idx) {
        COLONIZE::Make_New_Colony_Or_Outpost(&::game::world, legacy_world::Planet_Id(planet_idx), player_idx, 1);
    }
} // namespace COLONIZE