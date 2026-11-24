#include "pch.h"
#include "fleet_movement.h"

#include "legacy_ids.h"
#include "ship_location_queries.h"
#include "ship_ids.h"

namespace AIMOVE {
    void __cdecl Move_NPCs(game::s_game_state* world) {
        std::vector<uint8_t> star_flag1(std::ssize(world->stars), 0);
        std::vector<uint8_t> star_flag2(std::ssize(world->stars), 0);

        std::optional<uint32_t> monster_count;

        for (int16_t ship_idx = 0; ship_idx < static_cast<int16_t>(world->ship_slots.size()); ++ship_idx) {
            if (world->ship_slots[ship_idx] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(*world, ship_idx);

            if (factions::Legacy_Code(ship->owner) < MAX_PLAYERS) {
                continue;
            }

            if (ship->d.speed == 0) {
                continue;
            }

            if (ship->status == 1) {
                auto location = ships::Location(ship->position);
                const auto position = location ? game::Star_Row(*world, location->star_id) : -1;
                if (position >= 0) star_flag2[position] = 1;
                continue;
            }

            if (ship->status != 0) {
                continue;
            }

            const auto orbit = ships::Orbit_Star(ship->position);
            if (!orbit) continue;
            const auto current_star = game::Star_Row(*world, *orbit);
            if (current_star < 0) continue;

            bool try_move = false;

            if (factions::Legacy_Code(ship->owner) != 13) {
                try_move = true;
            } else {
                if (star_flag1[current_star] != 0) {
                    try_move = true;
                }

                star_flag1[current_star] = 1;
                star_flag2[current_star] = 1;

                ship->d.date_of_design++;

                if (ship->d.date_of_design == rules.eel_spawn_frequency) {
                    auto destination = AIMOVE::Npc_Dest(world, game::Ship_Key_At_Row(*world, ship_idx), star_flag2.data());
                    ship->d.date_of_design = 0;

                    // Assembly uses d.date_of_design at ship offset +0x61 as the 30-turn eel timer.
                    // Preserve the classic spawn gate when the population count is unknown.
                    if (destination) {
                        if (!monster_count) {
                            monster_count = 0;
                            for (int16_t i = 0; i < static_cast<int16_t>(world->ship_slots.size()); ++i) {
                                if (world->ship_slots[i] == ::game::NO_SHIP) continue;
                                s_ship_data* s = &ship_slots::At(*world, i);
                                if (factions::Legacy_Code(s->owner) == 13 && s->d.speed > 0 && s->status < 5) {
                                    ++*monster_count;
                                }
                            }
                        }
                    }

                    if (!monster_count || *monster_count < rules.max_eels) {
                        s_ship_design eel_design = {};
                        INITSHIP::Load_Eel_Ship_Design(&eel_design, 1);

                        const auto& star = world->stars.at(*orbit);
                        const auto new_key = SHIPMOVE::Create_Nonplayer_Ship(game::owner_slot{NONPLAYER_SHIP_TYPE_EEL}, &eel_design, star.x, star.y);

                        if (new_key != game::NO_SHIP) {
                            s_MSG msg = {};
                            msg.msg_type = MSG_TYPE_EEL_SPAWNED;
                            msg.u.eel.destination = {game::e_star_destination::star, *orbit};
                            MSG_::Add_Msg_All(msg);

                            // Assembly clears the same +0x61 timer field on the spawned eel.
                            if (auto* spawned = [&] { const auto entry = world->ships.find(new_key); return entry == world->ships.end() ? nullptr : &entry->second; }()) spawned->d.date_of_design = 0;
                        }
                        monster_count = monster_count ? *monster_count + 1 : 0;
                    }
                }
            }

            if (try_move) {
                auto destination = AIMOVE::Npc_Dest(world, game::Ship_Key_At_Row(*world, ship_idx), star_flag2.data());

                if (!destination) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_EEL_VANISHED;
                    msg.u.eel.destination = {game::e_star_destination::star, *orbit};
                    MSG_::Add_Msg_All(msg);

                    SHIPS::Kill_Ship(*ship);
                } else {
                    const std::array<id_ship, 1> ships{game::Ship_Key_At_Row(*world, ship_idx)};
                    s_ship_move_info move_info = {};
                    SHIPMOVE::Evaluate_Fleet_Move(*world, ships, {game::e_star_destination::star, *destination}, &move_info);
                    SHIPMOVE::Order_Fleet_Move(*world, ships, {game::e_star_destination::star, *destination}, &move_info);
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

    std::optional<id_star> Npc_Dest(game::s_game_state* world, id_ship ship_id, const uint8_t* star_flags) {
        const auto ship_entry = world->ships.find(ship_id);
        const auto* ship = ship_entry == world->ships.end() ? nullptr : &ship_entry->second;
        if (!ship) return std::nullopt;
        std::optional<id_star> destination;
        const auto orbit = ships::Orbit_Star(ship->position);
        if (!orbit) return std::nullopt;
        const auto ship_location = world->stars.find(*orbit);
        if (ship_location == world->stars.end()) return std::nullopt;
        uint32_t player_str[MAX_PLAYERS] = {0};
        std::vector<s_nonplayer_destination> npc_dest(std::ssize(world->stars));

        for (int i = 0; i < world->player_count; i++) {
            if (world->players[i].eliminated == 0) {
                uint32_t str_val = special::isqrt(world->players[i].bc);
                str_val = special::isqrt(str_val);
                player_str[i] = str_val +
                    world->players[i].total_colonies * 4 +
                    (int16_t)(world->players[i].total_research / 4) +
                    world->players[i].total_ships;
            }
        }

        for (auto& [roster_id, roster_colony] : world->colonies) {
            int8_t owner = legacy_world::Encode_Player_Slot(roster_colony.owner);
            if (owner != -1) {
                const auto planet_idx = roster_colony.planet_id;
                const auto key = world->planets.at(planet_idx).star_id;
                const auto star_idx = game::Star_Row(*world, key);

                if (star_idx >= 0 && key != *orbit && star_flags[star_idx] == 0) {
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
                    npc_dest[star_idx].star_id = key;
                    const auto& from = ship_location->second;
                    const auto& to = world->stars.at(key);
                    npc_dest[star_idx].distance_parsecs = (uint8_t)GEO::Parsecs_Between_Points(from.x, from.y, to.x, to.y);
                }
            }
        }

        qsort(npc_dest.data(), std::ssize(world->stars), sizeof(s_nonplayer_destination), Sort_npc_dests);

        int32_t valid_count = 0;
        for (int16_t i = 0; i < std::ssize(world->stars); i++) {
            if (valid_count >= 7) {
                break;
            }
            if (npc_dest[i].distance_parsecs != 0 && npc_dest[i].distance_parsecs < 20) {
                valid_count++;
                if (game_random::Random(valid_count) == 1) {
                    destination = npc_dest[i].star_id;
                }
            }
        }

        return destination;
    }
} // namespace AIMOVE
