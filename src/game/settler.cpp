#include "pch.h"

namespace SETTLER {
    int32_t N_Settlers(const game::s_world_state* world, game::id_player player) {
        const auto found = world->settlers.find(player);
        return found == world->settlers.end() ? 0 : static_cast<int32_t>(found->second.size());
    }

    static void Count_Pop(
        s_colony* colony,
        int32_t race_idx,
        int32_t* pop_limits,
        int32_t* pop_counts) {
        if (pop_limits[race_idx] == 0) {
            pop_limits[race_idx] =
                COLCALC::Colony_Race_Pop_Limit(&::game::world, colony, (int16_t)race_idx);
        }
        pop_counts[race_idx]++;
    }

    int8_t Add_Settler_To_Colony(game::s_world_state* world_state, s_settler settler_data, s_colony* colony) {
        bool is_full = false;
        int16_t race_idx = (int16_t)settler_data.race;

        if (!Room_For_Another(world_state, colony, race_idx, 0)) {
            is_full = true;
        } else {
            if (settler_data.profession == POP::PROF_FARMER) {
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
                        settler_data.profession = POP::PROF_SCIENTIST;
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
            POP::Set_Prof(pop_slot, settler_data.profession);
            POP::Set_Assigned(pop_slot, true);
            POP::Set_Conquered(pop_slot, false);
            colony->n_pops++;
        }

        return !is_full;
    }

    int32_t N_Freighters_Player_Can_Scrap(game::s_world_state* world_state, int16_t player_idx) {
        return MOX::player[player_idx].n_freighters - N_Settlers(world_state, legacy_world::Player_Id(player_idx)) * 5;
    }

    bool Room_For_Another(game::s_world_state* world_state, s_colony* colony, int16_t race_idx, int8_t include_freighters) {
        int32_t pop_limits[10] = {};
        int32_t pop_counts[10] = {};

        for (int32_t i = 0; i < (uint8_t)colony->n_pops; ++i) {
            Count_Pop(colony, (int32_t)POP::Get_Race(colony->pop[i]), pop_limits, pop_counts);
        }

        if (include_freighters != 0) {
            const auto player_id = legacy_world::Player_Id(static_cast<uint32_t>(colony->owner));
            auto records = world_state->settlers.find(player_id);
            if (records != world_state->settlers.end()) {
                for (const auto& settler_data : records->second) {
                    if (settler_data.destination_planet == colony->planet) {
                        Count_Pop(colony, settler_data.race, pop_limits, pop_counts);
                    }
                }
            }
        }

        if (pop_limits[race_idx] == 0) {
            pop_limits[race_idx] = COLCALC::Colony_Race_Pop_Limit(&::game::world, colony, race_idx);
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

    void Move_Settlers(game::s_world_state* world_state) {
        for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            auto& records = world_state->settlers[legacy_world::Player_Id(player_idx)];
            struct s_arrivals { uint16_t total = 0; uint16_t over_limit = 0; };
            std::map<game::id_planet, s_arrivals> arrivals;
            for (auto entry = records.begin(); entry != records.end();) {
                entry->eta = entry->eta == 0 ? 15 : entry->eta - 1;
                entry->source_colony.reset();
                if (entry->eta != 0) { ++entry; continue; }
                const auto id = entry->destination_planet;
                const auto* planet = game::Find_Planet(world_state, legacy_world::Planet_Id(id));
                if (planet != nullptr) {
                    auto& arrival = arrivals[id];
                    ++arrival.total;
                    const auto& star = MOX::star[planet->star_index];
                    if ((star.blockaded & (1u << player_idx)) == 0 &&
                        EVENTS::Event_Check_Space_Anomaly_At_Star(planet->star_index) == 0 && planet->colony_index >= 0) {
                        auto* colony = &MOX::colony[planet->colony_index];
                        if (colony->owner == player_idx && colony->outpost_flag == 0 &&
                            !Add_Settler_To_Colony(world_state, *entry, colony)) ++arrival.over_limit;
                    }
                }
                entry = records.erase(entry);
            }
            MOX::player[player_idx].settlers_freighted = static_cast<int16_t>(records.size());
            for (const auto& [id, arrival] : arrivals) {
                s_MSG msg = {};
                msg.msg_type = MSG_TYPE_COLONY;
                msg.u.colony.planet_idx = id;
                msg.u.colony.subtype = MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED;
                msg.u.colony.c.settlers.settlers = static_cast<uint8_t>(arrival.total);
                msg.u.colony.c.settlers.over_limit = static_cast<uint8_t>(arrival.over_limit);
                const auto& planet = game::Planet(world_state, legacy_world::Planet_Id(id));
                msg.u.colony.c.settlers.blockaded = (MOX::star[planet.star_index].blockaded >> player_idx) & 1;
                MSG_::Add_Msg(msg, player_idx);
            }
        }
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

    void __cdecl Pop_Tries_To_Settle(::game::s_world_state* world_state, 
        s_colony* source_colony,
        int16_t selected_pop_idx,
        s_colony* dest_colony,
        s_settler_settle_result* result) {
        s_player* player = &MOX::player[source_colony->owner];
        const int16_t source_star_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(source_colony->planet))).star_index;
        const int16_t dest_star_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(dest_colony->planet))).star_index;

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

        if (EVENTS::Event_Check_Space_Anomaly(world_state, (int16_t)(source_colony - MOX::colony)) != 0) {
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

        if (Room_For_Another(world_state, dest_colony, (int16_t)POP::Get_Race(source_colony->pop[selected_pop_idx]), 1) == 0) {
            result->no_room_at_destination = 1;
        }

        if ((((uint32_t)(uint8_t)MOX::star[dest_star_idx].blockaded >>
                 ((uint32_t)(uint8_t)dest_colony->owner & 0x1fU)) &
                1U) != 0) {
            result->dest_blockaded = 1;
        }

        if (EVENTS::Event_Check_Space_Anomaly(world_state, (int16_t)(dest_colony - MOX::colony)) != 0) {
            result->dest_space_anomaly = 1;
        }

        result->eta_turns = (uint8_t)Settler_ETA(player, source_star_idx, dest_star_idx);
        result->can_settle = 1;
    }

    void __cdecl Settle_Pop(::game::s_world_state* world_state, s_colony* source_colony, int16_t pop_index, s_colony* dest_colony) {
        if (dest_colony != source_colony) {
            s_player* owner_player = &MOX::player[(int32_t)source_colony->owner];
            const ::game::id_planet source_planet_index = source_colony->planet;
            const ::game::id_planet dest_planet_index = dest_colony->planet;
            const int16_t source_star_index = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(source_planet_index))).star_index;
            const int16_t dest_star_index = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(dest_planet_index))).star_index;
            POP::value_type* pop_slot = &source_colony->pop[(int32_t)pop_index];
            const POP::value_type pop_value = POP::Load(pop_slot);
            s_settler encoded_settler{};

            encoded_settler.race = static_cast<uint8_t>(POP::Get_Race(pop_value));
            encoded_settler.profession = POP::Get_Prof(pop_value);

            if (dest_star_index == source_star_index) {
                Add_Settler_To_Colony(world_state, encoded_settler, dest_colony);
            } else {
                encoded_settler.eta = static_cast<uint16_t>(Settler_ETA(owner_player, source_star_index, dest_star_index));
                encoded_settler.source_colony = legacy_world::Colony_Id(static_cast<uint32_t>(source_colony - MOX::colony));
                encoded_settler.destination_planet = dest_colony->planet;
                auto& records = world_state->settlers[legacy_world::Player_Id(static_cast<uint32_t>(source_colony->owner))];
                records.push_back(encoded_settler);
                owner_player->settlers_freighted = static_cast<int16_t>(records.size());
            }

            const uint8_t last_pop_index =
                (uint8_t)((uint8_t)source_colony->n_pops - 1);
            const POP::value_type last_pop_value = source_colony->pop[last_pop_index];

            source_colony->n_pops = last_pop_index;
            POP::Store(pop_slot, last_pop_value);
        }
    }
} // namespace SETTLER