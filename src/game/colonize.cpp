#include "pch.h"
#include "colony_reservations.h"

namespace COLONIZE {
    int16_t Star_N_Uncolonized_Planets(s_star_data* star) {
        int16_t uncolonized_count = 0;

        for (const auto& link : star->orbit_planet_ids) {
            const auto id = link;
            if (id != game::NO_PLANET && game::current_state.planets.at(id).colony_id == game::NO_COLONY) {
                ++uncolonized_count;
            }
        }

        return uncolonized_count;
    }

    int16_t Star_N_Outpostable_Planets(s_star_data* star) {
        return Star_N_Uncolonized_Planets(star);
    }

    int16_t Star_N_Colonizable_Planets_For_Player(s_star_data* star, game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int16_t colonizable_count = 0;

        for (const auto& link : star->orbit_planet_ids) {
            const auto id = link;
            if (id == game::NO_PLANET) continue;
            const auto* planet = &game::current_state.planets.at(id);

            if (planet->planet_type == PLANET_TYPE_PLANET) {
                game::id_colony colony_idx = planet->colony_id;
                if (colony_idx == game::NO_COLONY) {
                    colonizable_count++;
                } else {
                    s_colony* colony = &game::current_state.colonies.at(colony_idx);
                    if (colony->outpost_flag != 0 && legacy_world::Encode_Player_Slot(colony->owner) == player_idx) {
                        colonizable_count++;
                    }
                }
            }
        }

        return colonizable_count;
    }

    void Find_Colonize_Players() {
        std::array<game::player_bitmap, MAX_STARS> colony_bits{};
        std::array<game::player_bitmap, MAX_STARS> outpost_bits{};

        // Reset priority for all stars
        for (auto& [key, star] : game::current_state.stars) {
            star.colonize_player = factions::From_Legacy(MAX_PLAYERS);
        }

        // Phase 1: Check ships (Colony and Outpost ships)
        for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
            s_ship_data* ship = &game::current_state.ship_records[i];
            if (ship->status == 0) { // Active
                if (ship->d.ship_type == SHIP_TYPE_COLONY || ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                    int16_t star_idx = game::Ship_Orbit_Row(game::current_state, ship->position);
                    if (star_idx >= 0 && star_idx < MAX_STARS) {
                        s_star_data* star = &game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx));
                        if (factions::Legacy_Code(star->colonize_player) == MAX_PLAYERS) {
                            star->colonize_player = factions::From_Legacy(factions::Legacy_Code(ship->owner));
                        }

                        if (factions::Legacy_Code(star->colonize_player) == factions::Legacy_Code(ship->owner)) {
                            if (ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                                outpost_bits[star_idx].set(factions::Player(ship->owner));
                            } else {
                                colony_bits[star_idx].set(factions::Player(ship->owner));
                            }
                        } else {
                            star->colonize_player = factions::From_Legacy(-1); // Conflict
                        }
                    }
                }
            }
        }

        // Phase 2: Check existing colonies with colony bases
        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* colony = &roster_colony;
            if (legacy_world::Encode_Player_Slot(colony->owner) != -1 && colony->buildings[BUILDING_COLONY_BASE] != 0) {
                int16_t star_idx = game::Star_Row(game::current_state, game::current_state.planets.at(colony->planet_id).star_id);
                s_star_data* star = &game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx));
                if (factions::Legacy_Code(star->colonize_player) == MAX_PLAYERS) {
                    star->colonize_player = factions::From_Legacy(legacy_world::Encode_Player_Slot(colony->owner));
                }

                if (factions::Legacy_Code(star->colonize_player) == legacy_world::Encode_Player_Slot(colony->owner)) {
                    colony_bits[star_idx].set(colony->owner);
                } else {
                    star->colonize_player = factions::From_Legacy(-1);
                }
            }
        }

        // Phase 3: Validate priority against actual colonizable/outpostable planets
        for (int16_t i = 0; i < std::ssize(game::current_state.star_order); ++i) {
            const auto star_id = game::current_state.star_order[i];
            s_star_data* star = &game::current_state.stars.at(star_id);
            int8_t priority_player = factions::Legacy_Code(star->colonize_player);
            if (priority_player == MAX_PLAYERS) {
                star->colonize_player = factions::From_Legacy(-1);
            } else {
                bool keep = false;
                if (colony_bits[i].test_slot(priority_player)) {
                    if (Star_N_Colonizable_Planets_For_Player(star, legacy_world::Decode_Player_Slot(priority_player)) > 0) {
                        keep = true;
                    }
                }
                if (!keep && outpost_bits[i].test_slot(priority_player)) {
                    if (Star_N_Outpostable_Planets(star) > 0) {
                        keep = true;
                    }
                }

                if (!keep) {
                    star->colonize_player = factions::From_Legacy(-1);
                }
            }
        }

        // Phase 4: Block priority if an enemy/rival warship is present
        for (int16_t i = 0; i < game::current_state.ship_count; ++i) {
            s_ship_data* ship = &game::current_state.ship_records[i];
            if (ship->status == 0 && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                int16_t star_idx = game::Ship_Orbit_Row(game::current_state, ship->position);
                if (star_idx >= 0 && star_idx < MAX_STARS) {
                    s_star_data* star = &game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx));
                    int8_t colonizer = factions::Legacy_Code(star->colonize_player);
                    if (colonizer >= 0 && colonizer < MAX_PLAYERS && factions::Legacy_Code(ship->owner) != colonizer) {
                        if (factions::Legacy_Code(ship->owner) >= MAX_PLAYERS) {
                            star->colonize_player = factions::From_Legacy(-1);
                        } else {
                            // Check treaty status: if we have peace/alliance (4, 5, 6), priority is contested
                            int8_t treaty = game::current_state.players[colonizer].treaty[factions::Legacy_Code(ship->owner)];
                            if (treaty >= 4 && treaty <= 6) {
                                star->colonize_player = factions::From_Legacy(-1);
                            }
                        }
                    }
                }
            }
        }

        // Phase 5: Cleanup redundant colony bases
        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* colony = &roster_colony;
            if (legacy_world::Encode_Player_Slot(colony->owner) != -1 && colony->buildings[BUILDING_COLONY_BASE] != 0) {
                s_star_data* star = &game::current_state.stars.at(game::current_state.planets.at(colony->planet_id).star_id);

                bool remove_base = false;
                if (factions::Legacy_Code(star->colonize_player) != legacy_world::Encode_Player_Slot(colony->owner)) {
                    remove_base = true;
                } else if (Star_N_Colonizable_Planets_For_Player(star, colony->owner) == 0) {
                    remove_base = true;
                }

                if (remove_base) {
                    int16_t owner = legacy_world::Encode_Player_Slot(colony->owner);
                    int32_t refund32 = TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                    int16_t refund16 = (int16_t)refund32;
                    colony->buildings[BUILDING_COLONY_BASE] = 0;
                    game::current_state.players[owner].bc += refund32;

                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_COLONY;
                    msg.u.colony.planet_id = colony->planet_id;
                    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED;
                    msg.u.colony.c.scrap.building_id = BUILDING_COLONY_BASE;
                    msg.u.colony.c.scrap.bc = refund16;
                    MSG_::Add_Msg(msg, owner);
                }
            }
        }
    }

    void Allocate_New_Colony_Slots() {
        auto& state = game::current_state;
        Find_Colonize_Players();
        for (auto& [id, colony] : state.colonies) {
            colony.allocated_to = legacy_world::Decode_Player_Slot(-1);
        }
        game::Reserve_Colony_Ids(state, game::current_state.player_count, MOX::stardate);
    }

    void __cdecl AI_Colonize(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        for (int16_t i = 0; i < std::ssize(game::current_state.star_order); ++i) {
            const auto star_id = game::current_state.star_order[i];
            s_star_data* star = &game::current_state.stars.at(star_id);
            bool done_with_star = false;

            while (!done_with_star) {
                if (factions::Legacy_Code(star->colonize_player) == player_idx) {
                    auto colony_base_id = REPORT::Player_Colony_Base_At_Star(legacy_world::Decode_Player_Slot(player_idx), i);
                    s_ship_data* colony_ship_ptr = REPORT::Player_Ship_Type_At_Star(legacy_world::Decode_Player_Slot(player_idx), SHIP_TYPE_COLONY, i, 1);

                    if (colony_base_id == game::NO_COLONY && colony_ship_ptr == nullptr) {
                        done_with_star = true;
                    } else {
                        uint32_t best_worth = 0;
                        auto best_planet_id = game::NO_PLANET;

                        for (const auto& link : star->orbit_planet_ids) {
                            const auto id = link;
                            if (id == game::NO_PLANET) continue;
                            auto& planet = game::current_state.planets.at(id);
                            if (planet.colony_id == game::NO_COLONY) {
                                uint32_t worth = AIDATA::Uncolonized_Planet_Worth_To_Player(&game::current_state, &planet, &game::current_state.players[player_idx]);
                                if (best_worth < (worth & 0xFFFF)) {
                                    best_planet_id = id;
                                    best_worth = worth & 0xFFFF;
                                }
                            }
                        }

                        if (best_planet_id == game::NO_PLANET) {
                            done_with_star = true;
                            if (colony_base_id != game::NO_COLONY) {
                                // Sell colony base buildings if no worthwhile planets found
                                do {
                                    game::current_state.players[player_idx].bc += TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                                    game::current_state.colonies.at(colony_base_id).buildings[BUILDING_COLONY_BASE] = 0;
                                    colony_base_id = REPORT::Player_Colony_Base_At_Star(legacy_world::Decode_Player_Slot(player_idx), i);
                                } while (colony_base_id != game::NO_COLONY);
                            }
                        } else {
                            if (!Make_New_Colony(best_planet_id, player_key)) break;

                            if (colony_base_id == game::NO_COLONY) {
                                // Consumed a colony ship
                                for (int16_t s = 0; s < game::current_state.ship_count; ++s) {
                                    s_ship_data* ship = &game::current_state.ship_records[s];
                                    if (ship->mission == 4 && factions::Legacy_Code(ship->owner) == (char)player_idx && game::Ship_Orbit_Row(game::current_state, ship->position) == i) {
                                        ship->mission = 0;
                                    }
                                }
                                SHIPS::Kill_Ship(*colony_ship_ptr);
                                MAINSCR::Update_Ship_Icons_In_Between_Turns();
                            } else {
                                // Consumed an existing colony base building
                                ERICNET::Remove_Building(game::current_state, colony_base_id, BUILDING_COLONY_BASE);
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
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (game::current_state.players[i].objectives != PLAYER_OBJECTIVE_HUMAN) {
                AI_Colonize(legacy_world::Decode_Player_Slot(i));
            }
        }
    }

    static bool Make_New_Colony_Or_Outpost(game::id_planet planet_id, int16_t player_idx, bool is_outpost, bool require_reservation = true) {
        auto& state = game::current_state;
        const auto target = state.planets.find(planet_id);
        if (target == state.planets.end() || player_idx < 0 || player_idx >= MAX_PLAYERS) return false;
        auto* planet = &target->second;
        const bool is_reused = planet->colony_id != game::NO_COLONY;
        if (is_reused) {
            const auto existing = state.colonies.find(planet->colony_id);
            if (is_outpost || existing == state.colonies.end() || legacy_world::Encode_Player_Slot(existing->second.owner) != player_idx ||
                !existing->second.outpost_flag || existing->second.planet_id != planet_id) return false;
        }
        auto& reservations = state.colony_reservations[player_idx];
        if (!is_reused && require_reservation && reservations.empty()) return false;
        const auto reserved = is_reused ? planet->colony_id : require_reservation ? reservations.front() : game::NO_COLONY;
        const auto colony_id = INITGAME::Init_Colony(state, reserved, planet_id, static_cast<uint8_t>(player_idx));
        if (colony_id == game::NO_COLONY) return false;
        if (!is_reused && require_reservation) reservations.erase(reservations.begin());
        auto* colony = &state.colonies.at(colony_id);

        colony->allocated_to = legacy_world::Decode_Player_Slot(-1);

        if (is_outpost == 0) {
            colony->n_pops = 1;

            if (first_pop_to_production != 0 ||
                planet->food_per_farmer < 1 ||
                game::current_state.players[player_idx].traits[TRAIT_EATS_MINERALS] != 0 ||
                game::current_state.players[player_idx].traits[TRAIT_CYBERNETIC] != 0) {
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
                game::current_state.stars.at(planet->star_id).system_special = SYSTEM_SPECIAL_NO_SPECIAL;
            }

            if (is_reused) {
                colony->buildings[BUILDING_MARINE_BARRACKS] = 1;
            }

            COLCALC::Colony_Calculation(colony_id);
            colony->just_produced = COLONY_PRODUCTION_JUST_COLONIZED;
            if (game::current_state.players[player_idx].home_planet_id == game::NO_PLANET) {
                game::current_state.players[player_idx].home_planet_id = planet_id;
            }
        } else {
            colony->outpost_flag = 1;
            colony->n_pops = 0;
            colony->climate = planet->planet_type == PLANET_TYPE_PLANET
                ? planet->climate
                : PLANET_CLIMATE_BARREN;
        }

        colony->producing[0] = COLONY_PRODUCTION_NONE;
        COLCALC::Compute_Star_Colony_Stuff(&game::current_state.stars.at(planet->star_id));
        return true;
    }

    bool __cdecl Make_New_Colony(game::id_planet planet_id, game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        return Make_New_Colony_Or_Outpost(planet_id, player_idx, false);
    }

    bool __cdecl Make_New_Outpost(game::id_planet planet_id, game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        return Make_New_Colony_Or_Outpost(planet_id, player_idx, true);
    }

    bool Found_From_Ship(game::id_ship key, game::id_star source, game::id_planet site, game::id_player player_key, bool outpost) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto& state = game::current_state;
        auto* ship = game::Find_Ship(state, key);
        const auto planet = state.planets.find(site);
        if (!ship || factions::Legacy_Code(ship->owner) != player_idx || ship->status != 0 ||
            ship->d.ship_type != (outpost ? SHIP_TYPE_OUTPOST : SHIP_TYPE_COLONY) ||
            ship->position.Get() != game::s_ship_location{game::e_ship_location::at_star, source} ||
            !state.stars.contains(source) || planet == state.planets.end() || planet->second.star_id != source) return false;
        if (!Make_New_Colony_Or_Outpost(site, player_idx, outpost)) return false;
        // The main-screen flow marks the consumed hull for later cleanup.
        ship->status = 5;
        return true;
    }

    void Make_Debug_Colony(game::id_planet planet_id, game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        Make_New_Colony_Or_Outpost(planet_id, player_idx, false, false);
    }
} // namespace COLONIZE
