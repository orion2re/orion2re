#include "pch.h"
#include "legacy_ids.h"

namespace SETTLER {
    namespace {
        constexpr uint8_t MAX_SETTLER_ETA = 15;

        game::id_player Arriving_Resident_Owner(uint8_t group) {
            // The historical freight conversion used the low three bits of the group.
            if (group == POP::RACE_ANDROID) return game::id_player{0};
            if (group == POP::RACE_NATIVE) return game::id_player{1};
            return game::id_player{group};
        }
    }

    static void Count_Pop(
        const s_colony* colony,
        int32_t race_idx,
        int32_t* pop_limits,
        int32_t* pop_counts) {
        if (pop_limits[race_idx] == 0) {
            pop_limits[race_idx] =
                COLCALC::Colony_Race_Pop_Limit(colony, (int16_t)race_idx);
        }
        pop_counts[race_idx]++;
    }

    int8_t __cdecl Add_Settler_To_Colony(const s_settler_info& settler, s_colony* colony) {
        bool is_full = false;
        const int16_t race_idx = settler.race;
        auto profession = static_cast<POP::e_prof>(settler.profession);
        if (race_idx > POP::RACE_NATIVE || profession > POP::PROF_SCIENTIST) return 0;

        if (!Room_For_Another(colony, race_idx, 0)) {
            is_full = true;
        } else {
            if (profession == POP::PROF_FARMER) {
                int32_t farmer_count = 0;
                s_colony_pop* target_pop = nullptr;

                for (int32_t i = 0; i < (uint8_t)colony->n_pops; ++i) {
                    s_colony_pop pop_val = colony->pop[i];
                    if (POP::Is_Farmer(pop_val)) {
                        farmer_count++;
                        if (target_pop == nullptr && POP::Group_Player(pop_val) != game::NO_PLAYER) {
                            target_pop = &colony->pop[i];
                        }
                    }
                }

                if (farmer_count >= (uint8_t)colony->max_farms) {
                    if (race_idx < (int16_t)POP::RACE_ANDROID) {
                        profession = POP::PROF_SCIENTIST;
                    } else if (target_pop != nullptr) {
                        target_pop->profession = POP::PROF_SCIENTIST;
                    }
                }
            }
        }

        if (!is_full) {
            uint8_t n_pops = (uint8_t)colony->n_pops;
            colony->pop[n_pops] = {
                .group = settler.race,
                .profession = profession,
                .assigned = true,
                .original_owner = Arriving_Resident_Owner(settler.race),
            };
            colony->n_pops++;
        }

        return !is_full;
    }

    int32_t __cdecl N_Freighters_Player_Can_Scrap(game::id_player player_key) {
        if (player_key.id >= MAX_PLAYERS) return 0;
        const auto& player = game::current_state.players[player_key.id];
        return player.n_freighters - player.settlers_freighted * 5;
    }

    bool __cdecl Room_For_Another(const s_colony* colony, int16_t race_idx, int8_t include_freighters) {
        if (race_idx < 0 || race_idx >= COLONY_POP_RACE_COUNT) return false;
        int32_t pop_limits[COLONY_POP_RACE_COUNT] = {};
        int32_t pop_counts[COLONY_POP_RACE_COUNT] = {};

        for (int32_t i = 0; i < (uint8_t)colony->n_pops; ++i) {
            const auto group = colony->pop[i].group;
            if (group >= COLONY_POP_RACE_COUNT) return false;
            Count_Pop(colony, group, pop_limits, pop_counts);
        }

        const auto planet_id = colony->planet_id;
        if (include_freighters != 0 && planet_id != game::NO_PLANET) {
            s_player* player = &game::current_state.players.at(colony->owner.id);

            if (player->settlers_freighted > 0) {
                for (int32_t i = 0; i < (int32_t)player->settlers_freighted; ++i) {
                    const auto& settler = player->settlers[i];
                    if (settler.destination == planet_id && settler.race <= POP::RACE_NATIVE) {
                        Count_Pop(
                            colony,
                            settler.race,
                            pop_limits,
                            pop_counts);
                    }
                }
            }
        }

        if (pop_limits[race_idx] == 0) {
            pop_limits[race_idx] = COLCALC::Colony_Race_Pop_Limit(colony, race_idx);
        }

        int32_t total_pop_count = 0;
        for (int32_t counted_race = 0; counted_race < COLONY_POP_RACE_COUNT; ++counted_race) {
            total_pop_count += pop_counts[counted_race];
        }
        if (total_pop_count >= MAX_COLONY_POPULATION) {
            return false;
        }

        for (int32_t limiting_race = 0; limiting_race < COLONY_POP_RACE_COUNT; ++limiting_race) {
            if (pop_limits[limiting_race] < pop_limits[race_idx]) {
                continue;
            }

            int32_t constrained_pop_count = 0;
            for (int32_t counted_race = 0; counted_race < COLONY_POP_RACE_COUNT; ++counted_race) {
                if (pop_counts[counted_race] > 0 &&
                    pop_limits[counted_race] <= pop_limits[limiting_race]) {
                    constrained_pop_count += pop_counts[counted_race];
                }
            }

            if (pop_limits[limiting_race] <= constrained_pop_count) {
                return false;
            }
        }

        return true;
    }

    void __cdecl Move_Settlers() {
        s_player* curr_player = game::current_state.players.data();
        if (game::current_state.players.data() + game::current_state.player_count <= curr_player) {
            return;
        }

        do {
            int32_t settler_idx = 0;
            struct s_arrivals {
                int8_t count = 0;
                int8_t over_limit = 0;
            };
            std::map<game::id_planet, s_arrivals> arrivals;

            if (curr_player->settlers_freighted > 0) {
                do {
                    auto& settler = curr_player->settlers[settler_idx];
                    // Preserve the legacy countdown: zero wraps to the maximum ETA.
                    settler.eta = (settler.eta + MAX_SETTLER_ETA) % (MAX_SETTLER_ETA + 1);
                    settler.source_colony = game::NO_COLONY;

                    if (settler.eta == 0) {
                        const auto dest = settler.destination;
                        const auto it = game::current_state.planets.find(dest);
                        if (it != game::current_state.planets.end()) {
                            const auto& planet = it->second;
                            const auto star = game::current_state.stars.find(planet.star_id);
                            if (star != game::current_state.stars.end()) {
                                auto& arrival = arrivals[dest];
                                ++arrival.count;
                                const bool blockaded = star->second.blockaded.test(curr_player->id);
                                if (!blockaded && EVENTS::Event_Check_Space_Anomaly_At_Star(game::Star_Row(game::current_state, planet.star_id)) == 0 &&
                                    game::current_state.colonies.contains(planet.colony_id)) {
                                    auto& colony = game::current_state.colonies.at(planet.colony_id);
                                    if (colony.outpost_flag == 0 && colony.owner == curr_player->id &&
                                        Add_Settler_To_Colony(settler, &colony) == 0) {
                                        ++arrival.over_limit;
                                    }
                                }
                            }
                        }

                        curr_player->settlers_freighted--;
                        int32_t remaining_settler_count = curr_player->settlers_freighted - settler_idx;
                        memmove(
                            &curr_player->settlers[settler_idx],
                            &curr_player->settlers[settler_idx + 1],
                            remaining_settler_count * sizeof(curr_player->settlers[0]));
                    } else {
                        settler_idx++;
                    }
                } while (settler_idx < curr_player->settlers_freighted);
            }

            for (const auto& [planet_id, arrival] : arrivals) {
                if (arrival.count == 0) continue;
                s_MSG msg = {};
                msg.msg_type = MSG_TYPE_COLONY;
                msg.u.colony.planet_id = planet_id;
                msg.u.colony.subtype = MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED;
                msg.u.colony.c.settlers.settlers = arrival.count;
                msg.u.colony.c.settlers.over_limit = arrival.over_limit;

                const auto star = game::current_state.planets.at(planet_id).star_id;
                msg.u.colony.c.settlers.blockaded =
                    game::current_state.stars.at(star).blockaded.test(curr_player->id);

                MSG_::Add_Msg(msg, curr_player->id);
            }

            curr_player++;
        } while (curr_player < game::current_state.players.data() + game::current_state.player_count);
    }

    static int32_t Settler_ETA(const s_player& player, const s_star_data& source, const s_star_data& destination) {
        int32_t distance = GEO::Parsecs_Between_Points(source.x, source.y, destination.x, destination.y);
        if (distance == 0) {
            return 0;
        }

        int32_t speed = (uint8_t)player.ship_speed;
        if (speed < 2) {
            speed = 2;
        }

        distance = (distance + speed - 1) / speed;
        if (distance > MAX_SETTLER_ETA) {
            distance = MAX_SETTLER_ETA;
        }

        return distance;
    }

    void __cdecl Pop_Tries_To_Settle(
        const game::s_game_state& state,
        game::id_colony source_id,
        int16_t selected_pop_idx,
        game::id_colony destination_id,
        s_settler_settle_result* result) {
        const auto* source_colony = &state.colonies.at(source_id);
        const auto* dest_colony = &state.colonies.at(destination_id);
        const auto* player = &state.players[source_colony->owner.id];
        const auto& source_star = state.stars.at(state.planets.at(source_colony->planet_id).star_id);
        const auto& dest_star = state.stars.at(state.planets.at(dest_colony->planet_id).star_id);

        memset(result, 0, sizeof(*result));

        if (dest_colony->owner != source_colony->owner) {
            result->dest_owner_mismatch = 1;
            return;
        }

        if (source_star.blockaded.test(source_colony->owner)) {
            result->source_blockaded = 1;
            return;
        }

        if (EVENTS::Event_Check_Space_Anomaly(source_id) != 0) {
            result->source_space_anomaly = 1;
            return;
        }

        if (EVENTS::Event_Check_Plague(source_id) != 0) {
            result->source_plague = 1;
            return;
        }

        if (dest_colony->outpost_flag != 0) {
            result->dest_is_outpost = 1;
            return;
        }

        if (!allow_last_population_transfer && source_colony->n_pops == 1) {
            result->only_one_pop_left = 1;
            return;
        }

        if (source_colony->pop[selected_pop_idx].conquered) {
            result->selected_pop_conquered = 1;
            return;
        }

        if (source_id == destination_id) {
            result->can_settle = 1;
            return;
        }

        if (dest_star.id != source_star.id) {
            if (player->n_freighters < player->settlers_freighted * 5 + 5) {
                result->insufficient_freighters = 1;
                return;
            }

            if ((uint16_t)player->settlers_freighted >= max_pop_freighted) {
                result->max_settlers_in_transit = 1;
                return;
            }
        }

        if (Room_For_Another(dest_colony, source_colony->pop[selected_pop_idx].group, 1) == 0) {
            result->no_room_at_destination = 1;
        }

        if (dest_star.blockaded.test(dest_colony->owner)) {
            result->dest_blockaded = 1;
        }

        if (EVENTS::Event_Check_Space_Anomaly(destination_id) != 0) {
            result->dest_space_anomaly = 1;
        }

        result->eta_turns = (uint8_t)Settler_ETA(*player, source_star, dest_star);
        result->can_settle = 1;
    }

    void __cdecl Settle_Pop(game::id_colony source_id,
        int16_t pop_index, game::id_colony destination_id) {
        if (source_id != destination_id) {
            auto& state = game::current_state;
            auto* source_colony = &state.colonies.at(source_id);
            auto* dest_colony = &state.colonies.at(destination_id);
            s_player* owner_player = &game::current_state.players.at(source_colony->owner.id);
            const auto dest_id = dest_colony->planet_id;
            const auto& planets = state.planets;
            const auto source_star = planets.at(source_colony->planet_id).star_id;
            const auto dest_star = planets.at(dest_id).star_id;
            s_colony_pop* pop_slot = &source_colony->pop[(int32_t)pop_index];
            const s_colony_pop pop_value = *pop_slot;
            s_settler_info settler{
                .race = pop_value.group,
                .profession = static_cast<uint8_t>(pop_value.profession),
            };

            if (dest_star == source_star) {
                Add_Settler_To_Colony(settler, dest_colony);
            } else {
                settler.eta = static_cast<uint8_t>(Settler_ETA(*owner_player, state.stars.at(source_star), state.stars.at(dest_star)));
                settler.source_colony = source_id;
                settler.destination = dest_id;

                const int32_t freighted_index = (int32_t)owner_player->settlers_freighted;
                owner_player->settlers[freighted_index] = settler;
                owner_player->settlers_freighted =
                    (int16_t)(owner_player->settlers_freighted + 1);
            }

            const uint8_t last_pop_index =
                (uint8_t)((uint8_t)source_colony->n_pops - 1);
            const s_colony_pop last_pop_value = source_colony->pop[last_pop_index];

            source_colony->n_pops = last_pop_index;
            *pop_slot = last_pop_value;
        }
    }
} // namespace SETTLER
