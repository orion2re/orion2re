#include "pch.h"

#include "ai_compat.h"
#include "legacy_world.h"

namespace AIMOVE {
    void __cdecl Move_NPCs(const game::s_view* world) {
        std::vector<uint8_t> star_flag1(world->Star_Count(), 0);
        std::vector<uint8_t> star_flag2(world->Star_Count(), 0);

        std::optional<uint32_t> monster_count;

        for (int16_t ship_idx = 0; ship_idx < world->Ship_Count(); ++ship_idx) {
            s_ship_data* ship = &world->ships[ship_idx];

            if (ship->owner < MAX_PLAYERS) {
                continue;
            }

            if (ship->d.speed == 0) {
                continue;
            }

            if (ship->status == 1) {
                auto location = legacy_world::Read_Ship_Location(ship);
                if (location && legacy_world::Slot(location->star_id) < star_flag2.size()) star_flag2[legacy_world::Slot(location->star_id)] = 1;
                continue;
            }

            if (ship->status != 0) {
                continue;
            }

            const auto orbit = legacy_world::Orbit_Star(ship);
            if (!orbit || legacy_world::Slot(*orbit) >= (uint32_t)world->Star_Count()) continue;
            const uint32_t current_star = legacy_world::Slot(*orbit);

            bool try_move = false;

            if (ship->owner != 13) {
                try_move = true;
            } else {
                if (star_flag1[current_star] != 0) {
                    try_move = true;
                }

                star_flag1[current_star] = 1;
                star_flag2[current_star] = 1;

                ship->d.date_of_design++;

                if (ship->d.date_of_design == rules.eel_spawn_frequency) {
                    auto destination = AIMOVE::Npc_Dest(world, legacy_world::Ship_Id(ship_idx), star_flag2.data());
                    ship->d.date_of_design = 0;

                    // Assembly uses d.date_of_design at ship offset +0x61 as the 30-turn eel timer.
                    // Preserve the classic spawn gate when the population count is unknown.
                    if (destination) {
                        if (!monster_count) {
                            monster_count = 0;
                            for (int16_t i = 0; i < world->Ship_Count(); ++i) {
                                s_ship_data* s = &world->ships[i];
                                if (s->owner == 13 && s->d.speed > 0 && s->status < 5) {
                                    ++*monster_count;
                                }
                            }
                        }
                    }

                    if (!monster_count || *monster_count < rules.max_eels) {
                        s_ship_design eel_design = {};
                        INITSHIP::Load_Eel_Ship_Design(&eel_design, 1);

                        s_star_data* star = &world->stars[current_star];
                        int16_t new_ship_idx = SHIPMOVE::Create_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_EEL, &eel_design, star->x, star->y);

                        if (new_ship_idx != -1) {
                            s_MSG msg = {};
                            msg.msg_type = MSG_TYPE_EEL_SPAWNED;
                            msg.u.eel.star_idx = (int16_t)current_star;
                            MSG_::Add_Msg_All(msg);

                            // Assembly clears the same +0x61 timer field on the spawned eel.
                            world->ships[new_ship_idx].d.date_of_design = 0;
                        }
                        monster_count = monster_count ? *monster_count + 1 : 0;
                    }
                }
            }

            if (try_move) {
                auto destination = AIMOVE::Npc_Dest(world, legacy_world::Ship_Id(ship_idx), star_flag2.data());

                if (!destination) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_EEL_VANISHED;
                    msg.u.eel.star_idx = (int16_t)current_star;
                    MSG_::Add_Msg_All(msg);

                    SHIPS::Kill_Ship(ship_idx);
                } else {
                    const std::array<id_ship, 1> ships{legacy_world::Ship_Id(ship_idx)};
                    s_ship_move_info move_info = {};
                    ai_compat::Evaluate_Fleet_Move(ships, *destination, &move_info);
                    ai_compat::Order_Fleet_Move(ships, *destination, &move_info);
                }
            }
        }
    }

    int __cdecl Sort_npc_dests(const void* a_ptr, const void* b_ptr) {
        auto a = (const s_nonplayer_destination*)a_ptr;
        auto b = (const s_nonplayer_destination*)b_ptr;
        int result;

        result = ((int)a->has_lucky - (int)a->has_unlucky) - ((int)b->has_lucky - (int)b->has_unlucky);
        if (result != 0) {
            return result;
        }

        result = (int)(a->distance_parsecs >> 3) - (int)(b->distance_parsecs >> 3);
        if (result != 0) {
            return result;
        }

        result = (int)b->max_unlucky_empire_strength - (int)a->max_unlucky_empire_strength;
        if (result != 0) {
            return result;
        }

        result = (int)b->max_lucky_empire_strength - (int)a->max_lucky_empire_strength;
        if (result != 0) {
            return result;
        }

        return (int)a->distance_parsecs - (int)b->distance_parsecs;
    }

    std::optional<id_star> Npc_Dest(const game::s_view* world, id_ship ship_id, const uint8_t* star_flags) {
        const uint32_t ship_idx = legacy_world::Slot(ship_id);
        std::optional<id_star> destination;
        auto orbit = legacy_world::Orbit_Star(&world->ships[ship_idx]);
        if (!orbit) return std::nullopt;
        const uint32_t ship_location = legacy_world::Slot(*orbit);
        const auto legacy_location = legacy_world::Index(*orbit);
        if (!legacy_location) return std::nullopt;
        uint32_t player_str[MAX_PLAYERS] = {0};
        std::vector<s_nonplayer_destination> npc_dest(world->Star_Count());

        for (int i = 0; i < world->Player_Count(); i++) {
            if (world->players[i].eliminated == 0) {
                uint32_t str_val = special::isqrt(world->players[i].bc);
                str_val = special::isqrt(str_val);
                player_str[i] = str_val +
                    world->players[i].total_colonies * 4 +
                    (int16_t)(world->players[i].total_research / 4) +
                    world->players[i].total_ships;
            }
        }

        for (int16_t i = 0; i < world->Colony_Count(); i++) {
            int8_t owner = world->colonies[i].owner;
            if (owner != -1) {
                int16_t planet_idx = world->colonies[i].planet;
                int16_t star_idx = world->planets[planet_idx].star_index;

                if (ship_location != star_idx && star_flags[star_idx] == 0) {
                    if (world->players[owner].traits[TRAIT_LUCKY] == 0) {
                        npc_dest[star_idx].has_unlucky = 1;
                        if (npc_dest[star_idx].max_unlucky_empire_strength < player_str[owner]) {
                            npc_dest[star_idx].max_unlucky_empire_strength = player_str[owner];
                        }
                    } else {
                        npc_dest[star_idx].has_lucky = 1;
                        if (npc_dest[star_idx].max_lucky_empire_strength < player_str[owner]) {
                            npc_dest[star_idx].max_lucky_empire_strength = player_str[owner];
                        }
                    }
                    npc_dest[star_idx].star_id = legacy_world::Star_Id(star_idx);
                    npc_dest[star_idx].distance_parsecs = (uint8_t)GEO::Parsecs_Between_Stars(*legacy_location, star_idx);
                }
            }
        }

        qsort(npc_dest.data(), world->Star_Count(), sizeof(s_nonplayer_destination), Sort_npc_dests);

        int32_t valid_count = 0;
        for (int16_t i = 0; i < world->Star_Count(); i++) {
            if (valid_count >= 7) {
                break;
            }
            if (npc_dest[i].distance_parsecs != 0 && npc_dest[i].distance_parsecs < 20) {
                valid_count++;
                if (game_random::Random_From_Seed(world->random_seed, valid_count) == 1) {
                    destination = npc_dest[i].star_id;
                }
            }
        }

        return destination;
    }
} // namespace AIMOVE
