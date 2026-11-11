#include "pch.h"

namespace AIMOVE {
    void __cdecl Move_NPCs() {
        uint8_t star_flag1[MAX_STARS] = {0};
        uint8_t star_flag2[MAX_STARS] = {0};

        int32_t monster_count = -1;

        for (int16_t ship_idx = 0; ship_idx < MOX::NUM_SHIPS; ++ship_idx) {
            s_ship_data* ship = &MOX::ship[ship_idx];

            if (ship->owner < MAX_PLAYERS) {
                continue;
            }

            if (ship->d.speed == 0) {
                continue;
            }

            if (ship->status == 1) {
                star_flag2[ship->location - SHIP_LOCATION_MOVING_OFFSET] = 1;
                continue;
            }

            if (ship->status != 0) {
                continue;
            }

            bool try_move = false;

            if (ship->owner != 13) {
                try_move = true;
            } else {
                if (star_flag1[ship->location] != 0) {
                    try_move = true;
                }

                star_flag1[ship->location] = 1;
                star_flag2[ship->location] = 1;

                ship->d.date_of_design++;

                if (ship->d.date_of_design == eel_spawn_frequency) {
                    int16_t dest = AIMOVE::Npc_Dest(ship_idx, star_flag2);
                    ship->d.date_of_design = 0;

                    // Assembly uses d.date_of_design at ship offset +0x61 as the 30-turn eel timer.
                    // Original bug preserved: if dest == -1 and monster_count is still -1,
                    // the spawn gate below still passes because -1 < 4.
                    if (dest != -1) {
                        if (monster_count == -1) {
                            monster_count = 0;
                            for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                                s_ship_data* s = &MOX::ship[i];
                                if (s->owner == 13 && s->d.speed > 0 && s->status < 5) {
                                    monster_count++;
                                }
                            }
                        }
                    }

                    if (monster_count < max_eels) {
                        s_ship_design eel_design = {};
                        INITSHIP::Load_Eel_Ship_Design(&eel_design, 1);

                        s_star_data* star = &MOX::star[ship->location];
                        int16_t new_ship_idx = SHIPMOVE::Create_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_EEL, &eel_design, star->x, star->y);

                        if (new_ship_idx != -1) {
                            s_MSG msg = {};
                            msg.msg_type = MSG_TYPE_EEL_SPAWNED;
                            msg.u.eel.star_idx = ship->location;
                            MSG_::Add_Msg_All(msg);

                            // Assembly clears the same +0x61 timer field on the spawned eel.
                            MOX::ship[new_ship_idx].d.date_of_design = 0;
                        }
                        monster_count++;
                    }
                }
            }

            if (try_move) {
                int16_t dest = AIMOVE::Npc_Dest(ship_idx, star_flag2);

                if (dest == -1) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_EEL_VANISHED;
                    msg.u.eel.star_idx = ship->location;
                    MSG_::Add_Msg_All(msg);

                    SHIPS::Kill_Ship(ship_idx);
                } else {
                    int16_t ships_arr[2];
                    ships_arr[0] = ship_idx;
                    ships_arr[1] = -1;

                    s_ship_move_info move_info = {};
                    SHIPMOVE::Ships_Try_To_Move_To(ships_arr, dest, &move_info);
                    SHIPMOVE::Make_Ships_Move_To(ships_arr, dest, &move_info);
                }
            }
        }
    }

    int __cdecl Sort_npc_dests(const void* a_ptr, const void* b_ptr) {
        auto a = (const s_aimove_npc_dest*)a_ptr;
        auto b = (const s_aimove_npc_dest*)b_ptr;
        int result;

        result = ((int)a->has_lucky - (int)a->has_unlucky) - ((int)b->has_lucky - (int)b->has_unlucky);
        if (result != 0) {
            return result;
        }

        result = (int)(a->dist >> 3) - (int)(b->dist >> 3);
        if (result != 0) {
            return result;
        }

        result = (int)b->unlucky_strength - (int)a->unlucky_strength;
        if (result != 0) {
            return result;
        }

        result = (int)b->lucky_strength - (int)a->lucky_strength;
        if (result != 0) {
            return result;
        }

        return (int)a->dist - (int)b->dist;
    }

    int16_t __cdecl Npc_Dest(int16_t ship_idx, uint8_t* star_flags) {
        int16_t dest_star_idx = -1;
        int16_t ship_location = MOX::ship[ship_idx].location;
        uint32_t player_str[MAX_PLAYERS] = {0};
        s_aimove_npc_dest npc_dest[MAX_STARS];

        memset(npc_dest, 0, sizeof(npc_dest));

        for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
            if (MOX::player[i].eliminated == 0) {
                uint32_t str_val = special::isqrt(MOX::player[i].bc);
                str_val = special::isqrt(str_val);
                player_str[i] = str_val +
                    MOX::player[i].total_colonies * 4 +
                    (int16_t)(MOX::player[i].total_research / 4) +
                    MOX::player[i].total_ships;
            }
        }

        for (int16_t i = 0; i < MOX::NUM_COLONIES; i++) {
            int8_t owner = MOX::colony[i].owner;
            if (owner != -1) {
                int16_t planet_idx = MOX::colony[i].planet;
                int16_t star_idx = MOX::planet[planet_idx].star_index;

                if (ship_location != star_idx && star_flags[star_idx] == 0) {
                    if (MOX::player[owner].traits[TRAIT_LUCKY] == 0) {
                        npc_dest[star_idx].has_unlucky = 1;
                        if (npc_dest[star_idx].unlucky_strength < player_str[owner]) {
                            npc_dest[star_idx].unlucky_strength = player_str[owner];
                        }
                    } else {
                        npc_dest[star_idx].has_lucky = 1;
                        if (npc_dest[star_idx].lucky_strength < player_str[owner]) {
                            npc_dest[star_idx].lucky_strength = player_str[owner];
                        }
                    }
                    npc_dest[star_idx].star_idx = star_idx;
                    npc_dest[star_idx].dist = (uint8_t)GEO::Parsecs_Between_Stars(ship_location, star_idx);
                }
            }
        }

        qsort(npc_dest, MOX::NUM_STARS, sizeof(s_aimove_npc_dest), Sort_npc_dests);

        int32_t valid_count = 0;
        for (int16_t i = 0; i < MOX::NUM_STARS; i++) {
            if (valid_count >= 7) {
                break;
            }
            if (npc_dest[i].dist != 0 && npc_dest[i].dist < 20) {
                valid_count++;
                if (game_random::Random(valid_count) == 1) {
                    dest_star_idx = npc_dest[i].star_idx;
                }
            }
        }

        return dest_star_idx;
    }
} // namespace AIMOVE
