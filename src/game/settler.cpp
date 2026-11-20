#include "pch.h"
#include "legacy_ids.h"

namespace SETTLER {
    static void Count_Pop(
        s_colony* colony,
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
                POP::value_type* target_pop = nullptr;

                for (int32_t i = 0; i < (uint8_t)colony->n_pops; ++i) {
                    POP::value_type pop_val = colony->pop[i];
                    if (POP::Is_Farmer(pop_val)) {
                        farmer_count++;
                        if (target_pop == nullptr && POP::Get_Race(pop_val) < POP::RACE_ANDROID) {
                            target_pop = &colony->pop[i];
                        }
                    }
                }

                if (farmer_count >= (uint8_t)colony->max_farms) {
                    if (race_idx < (int16_t)POP::RACE_ANDROID) {
                        profession = POP::PROF_SCIENTIST;
                    } else if (target_pop != nullptr) {
                        POP::Set_Prof(target_pop, POP::PROF_SCIENTIST);
                    }
                }
            }
        }

        if (!is_full) {
            uint8_t n_pops = (uint8_t)colony->n_pops;
            POP::value_type* pop_slot = &colony->pop[n_pops];
            POP::Set_Race(pop_slot, (uint32_t)race_idx);
            POP::Set_Original_Owner(pop_slot, (uint32_t)race_idx);
            POP::Set_Prof(pop_slot, profession);
            POP::Set_Assigned(pop_slot, true);
            POP::Set_Conquered(pop_slot, false);
            colony->n_pops++;
        }

        return !is_full;
    }

    int32_t __cdecl N_Freighters_Player_Can_Scrap(int16_t player_idx) {
        return MOX::player[player_idx].n_freighters - (MOX::player[player_idx].settlers_freighted * 5);
    }

    bool __cdecl Room_For_Another(s_colony* colony, int16_t race_idx, int8_t include_freighters) {
        int32_t pop_limits[10] = {};
        int32_t pop_counts[10] = {};

        for (int32_t i = 0; i < (uint8_t)colony->n_pops; ++i) {
            Count_Pop(colony, (int32_t)POP::Get_Race(colony->pop[i]), pop_limits, pop_counts);
        }

        const auto planet_id = colony->planet_id.get();
        if (include_freighters != 0 && planet_id != game::NO_PLANET) {
            s_player* player = &MOX::player[(int)colony->owner];

            if (player->settlers_freighted > 0) {
                for (int32_t i = 0; i < (int32_t)player->settlers_freighted; ++i) {
                    const auto& settler = player->settlers[i];
                    if (settler.destination.get() == planet_id && settler.race <= POP::RACE_NATIVE) {
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
        for (int32_t counted_race = 0; counted_race < 10; ++counted_race) {
            total_pop_count += pop_counts[counted_race];
        }
        if (total_pop_count >= 42) {
            return false;
        }

        for (int32_t limiting_race = 0; limiting_race < 10; ++limiting_race) {
            if (pop_limits[limiting_race] < pop_limits[race_idx]) {
                continue;
            }

            int32_t constrained_pop_count = 0;
            for (int32_t counted_race = 0; counted_race < 10; ++counted_race) {
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
        s_player* curr_player = MOX::player;
        if (MOX::player + MOX::NUM_PLAYERS <= curr_player) {
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
                    settler.eta = (settler.eta - 1u) & 0x0fu;
                    settler.source_colony = SOURCE_COLONY_IN_TRANSIT;

                    if (settler.eta == 0) {
                        const auto dest = settler.destination.get();
                        const auto it = game::current_state.planets.find(dest);
                        if (it != game::current_state.planets.end()) {
                            const auto& planet = it->second;
                            const int16_t star_index = planet.star_index;
                            if (star_index >= 0 && star_index < MOX::NUM_STARS) {
                                auto& arrival = arrivals[dest];
                                ++arrival.count;
                                const int32_t player_idx = (int32_t)(curr_player - MOX::player);
                                const bool blockaded = ((MOX::star[star_index].blockaded >> (player_idx & 0x1f)) & 1) != 0;
                                if (!blockaded && EVENTS::Event_Check_Space_Anomaly_At_Star(star_index) == 0 &&
                                    planet.colony_index >= 0 && planet.colony_index < MOX::NUM_COLONIES) {
                                    auto& colony = MOX::colony[planet.colony_index];
                                    if (colony.outpost_flag == 0 && colony.owner == player_idx &&
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
                const auto planet_idx = SERIAL::Encode_Legacy_Planet_Reference(planet_id);
                if (planet_idx < 0) continue; // Colony messages still use a signed 16-bit field.
                s_MSG msg = {};
                msg.msg_type = MSG_TYPE_COLONY;
                msg.u.colony.planet_idx = planet_idx;
                msg.u.colony.subtype = MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED;
                msg.u.colony.c.settlers.settlers = arrival.count;
                msg.u.colony.c.settlers.over_limit = arrival.over_limit;

                int32_t player_idx = (int32_t)(curr_player - MOX::player);
                const auto star_index = game::current_state.planets.at(planet_id).star_index;
                msg.u.colony.c.settlers.blockaded =
                    (uint8_t)(MOX::star[star_index].blockaded >> ((uint8_t)player_idx & 0x1f)) & 1;

                MSG_::Add_Msg(msg, player_idx);
            }

            curr_player++;
        } while (curr_player < MOX::player + MOX::NUM_PLAYERS);
    }

    int32_t __cdecl Settler_ETA(s_player* player, int16_t source_star, int16_t dest_star) {
        int32_t distance = GEO::Parsecs_Between_Stars(source_star, dest_star);
        if (distance == 0) {
            return 0;
        }

        int32_t speed = (uint8_t)player->ship_speed;
        if (speed < 2) {
            speed = 2;
        }

        distance = (distance + speed - 1) / speed;
        if (distance > 15) {
            distance = 15;
        }

        return distance;
    }

    void __cdecl Pop_Tries_To_Settle(
        s_colony* source_colony,
        int16_t selected_pop_idx,
        s_colony* dest_colony,
        s_settler_settle_result* result) {
        s_player* player = &MOX::player[source_colony->owner];
        const int16_t source_star_idx = game::current_state.planets.at(source_colony->planet_id.get()).star_index;
        const int16_t dest_star_idx = game::current_state.planets.at(dest_colony->planet_id.get()).star_index;

        memset(result, 0, sizeof(*result));

        if (dest_colony->owner != source_colony->owner) {
            result->dest_owner_mismatch = 1;
            return;
        }

        if ((((uint32_t)(uint8_t)MOX::star[source_star_idx].blockaded >>
                 ((uint32_t)(uint8_t)source_colony->owner & 0x1fU)) &
                1U) != 0) {
            result->source_blockaded = 1;
            return;
        }

        if (EVENTS::Event_Check_Space_Anomaly((int16_t)(source_colony - MOX::colony)) != 0) {
            result->source_space_anomaly = 1;
            return;
        }

        if (EVENTS::Event_Check_Plague((int16_t)(source_colony - MOX::colony)) != 0) {
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

        if (POP::Is_Conquered(source_colony->pop[selected_pop_idx])) {
            result->selected_pop_flag_0x400 = 1;
            return;
        }

        if (dest_colony == source_colony) {
            result->can_settle = 1;
            return;
        }

        if (dest_star_idx != source_star_idx) {
            if (player->n_freighters < player->settlers_freighted * 5 + 5) {
                result->insufficient_freighters = 1;
                return;
            }

            if ((uint16_t)player->settlers_freighted >= max_pop_freighted) {
                result->max_settlers_in_transit = 1;
                return;
            }
        }

        if (Room_For_Another(dest_colony, (int16_t)POP::Get_Race(source_colony->pop[selected_pop_idx]), 1) == 0) {
            result->no_room_at_destination = 1;
        }

        if ((((uint32_t)(uint8_t)MOX::star[dest_star_idx].blockaded >>
                 ((uint32_t)(uint8_t)dest_colony->owner & 0x1fU)) &
                1U) != 0) {
            result->dest_blockaded = 1;
        }

        if (EVENTS::Event_Check_Space_Anomaly((int16_t)(dest_colony - MOX::colony)) != 0) {
            result->dest_space_anomaly = 1;
        }

        result->eta_turns = (uint8_t)Settler_ETA(player, source_star_idx, dest_star_idx);
        result->can_settle = 1;
    }

    void __cdecl Settle_Pop(s_colony* source_colony, int16_t pop_index, s_colony* dest_colony) {
        if (dest_colony != source_colony) {
            s_player* owner_player = &MOX::player[(int32_t)source_colony->owner];
            const auto source_id = source_colony->planet_id.get();
            const auto dest_id = dest_colony->planet_id.get();
            const auto& planets = game::current_state.planets;
            const auto source_star_index = planets.at(source_id).star_index;
            const auto dest_star_index = planets.at(dest_id).star_index;
            POP::value_type* pop_slot = &source_colony->pop[(int32_t)pop_index];
            const POP::value_type pop_value = POP::Load(pop_slot);
            s_settler_info settler{
                .race = static_cast<uint8_t>(POP::Get_Race(pop_value)),
                .profession = static_cast<uint8_t>(POP::Get_Prof(pop_value)),
            };

            if (dest_star_index == source_star_index) {
                Add_Settler_To_Colony(settler, dest_colony);
            } else {
                settler.eta = static_cast<uint8_t>(Settler_ETA(owner_player, source_star_index, dest_star_index));
                settler.source_colony = static_cast<int16_t>(source_colony - MOX::colony);
                settler.destination = dest_id;

                const int32_t freighted_index = (int32_t)owner_player->settlers_freighted;
                owner_player->settlers[freighted_index] = settler;
                owner_player->settlers_freighted =
                    (int16_t)(owner_player->settlers_freighted + 1);
            }

            const uint8_t last_pop_index =
                (uint8_t)((uint8_t)source_colony->n_pops - 1);
            const POP::value_type last_pop_value = source_colony->pop[last_pop_index];

            source_colony->n_pops = last_pop_index;
            POP::Store(pop_slot, last_pop_value);
        }
    }
} // namespace SETTLER
