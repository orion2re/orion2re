#include "pch.h"
#include <unordered_set>

namespace ADVCIV {
    static std::map<game::id_planet, game::id_player> selected_planets;
    static std::array<std::unordered_set<game::id_planet>, MAX_PLAYERS> candidate_ids;
    s_adv_civ_player* adv_civ_player;
    s_parsec* parsec;

    game::id_player Planned_Colony_Owner(const planned_colony_owners& owners, game::id_star star) {
        const auto it = owners.find(star);
        return it == owners.end() ? game::NO_PLAYER : it->second;
    }

    static int16_t Proximity_Bonus(const game::s_game_state& state, const planned_colony_owners& owners,
        game::id_player player, game::id_star target_star);
    static void Twiddle_Selected_Adv_Civ_Planets(game::s_game_state& state, const planned_colony_owners& owners);

    void __cdecl Init_Advanced_Civilization_Fleet(game::id_player) {
        return;
    }

    void __cdecl Assign_Advanced_Civilization_Starting_Ships() {
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            Init_Advanced_Civilization_Fleet(game::current_state.players[i].id);
        }
    }

    int __cdecl qsort_planet_worth(const void* left_record, const void* right_record) {
        const auto* left_planet = (const s_adv_civ_player_planet*)left_record;
        const auto* right_planet = (const s_adv_civ_player_planet*)right_record;
        return right_planet->worth - left_planet->worth;
    }

    void Get_Adv_Civ_Planet_Worthiness(game::s_game_state& state, const planned_colony_owners& owners,
        s_planet_data& planet, s_player& player, int32_t* worth, int8_t* has_colony, int16_t* proximity_bonus) {
        *proximity_bonus = 0;
        *worth = 0;
        *has_colony = 0;

        int32_t raw_worth = AIDATA::Uncolonized_Planet_Worth_To_Player(&state, &planet, &player);
        *worth = (int32_t)(raw_worth & 0xFFFF);

        int16_t base_proximity = Proximity_Bonus(state, owners, player.id, planet.star_id);
        *proximity_bonus = (int16_t)((int32_t)base_proximity * (*worth) / 100);

        if (Planned_Colony_Owner(owners, planet.star_id) == player.id) {
            *worth = (*worth * 67) / 100;
            *has_colony = 1;
        }

        *worth += (int32_t)(*proximity_bonus);
    }

    static void Get_Worth_For_All_Planets(game::s_game_state& state, const planned_colony_owners& owners) {
        for (int16_t player_idx = 0; player_idx < state.player_count; ++player_idx) {
            s_adv_civ_player& player = adv_civ_player[player_idx];

            for (int16_t i = 0; i < player.candidate_planet_count; ++i) {
                s_adv_civ_player_planet& planet = player.planets[i];

                int32_t worth = 0;
                int8_t has_colony = 0;
                int16_t proximity_bonus = 0;

                const auto id = planet.planet_id;
                if (id == game::NO_PLANET) {
                    worth = -1;
                } else if (!selected_planets.contains(id)) {
                    Get_Adv_Civ_Planet_Worthiness(state, owners, state.planets.at(id), state.players[player_idx], &worth, &has_colony, &proximity_bonus);
                }

                planet.worth = worth;
            }

            qsort(player.planets, MAX_PLANETS, sizeof(s_adv_civ_player_planet), qsort_planet_worth);
        }
    }

    static void Build_Adv_Civ_Star_List(game::s_game_state& state, const planned_colony_owners& owners) {
        int16_t max_dist_so_far = 0;

        for (int16_t player_idx = 0; player_idx < state.player_count; ++player_idx) {
            const auto& player = state.players[player_idx];
            adv_civ_player[player_idx].candidate_star_count = 0;

            const auto home_slot = game::Star_Slot(state, state.planets.at(player.home_planet_id).star_id);
            for (int16_t star_slot = 1; star_slot < MAX_STARS; ++star_slot) {
                int16_t distance = (int16_t)parsec[home_slot].dist[star_slot];
                if (max_dist_so_far < distance) {
                    max_dist_so_far = distance;
                }
            }

            adv_civ_player[player_idx].furthest_star_dist_from_home = max_dist_so_far;

            for (int16_t colony_slot = 1; colony_slot < std::ssize(state.star_order); ++colony_slot) {
                const auto colony_star_id = game::Star_Key_At_Slot(state, colony_slot);
                const auto& colony_star = state.stars.at(colony_star_id);
                if (colony_star.system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                    Planned_Colony_Owner(owners, colony_star_id) == player.id) {
                    for (int16_t target_slot = 1; target_slot < std::ssize(state.star_order); ++target_slot) {
                        const auto target_id = game::Star_Key_At_Slot(state, target_slot);
                        int16_t distance = (int16_t)parsec[colony_slot].dist[target_slot];
                        if (distance < 10 && state.stars.at(target_id).system_special != SYSTEM_SPECIAL_SPACE_MONSTER) {
                            auto& star_proximity = adv_civ_player[player_idx].stars[target_slot];
                            if (distance <= star_proximity.min_dist_to_colony) {
                                star_proximity.min_dist_to_colony = distance;
                            }
                            star_proximity.star_slot = target_slot;
                        }
                    }
                }
            }
        }

        Build_Adv_Civ_Planet_List(state);
        Get_Worth_For_All_Planets(state, owners);
    }

    void Build_Adv_Civ_Planet_List(const game::s_game_state& state) {
        if (state.player_count <= 0) {
            return;
        }

        for (int16_t player_idx = 0; player_idx < state.player_count; ++player_idx) {
            s_adv_civ_player* player = &ADVCIV::adv_civ_player[player_idx];

            for (int16_t star_list_idx = 0; star_list_idx < MAX_STARS; ++star_list_idx) {
                s_adv_civ_player_star* adv_star = &player->stars[star_list_idx];

                if (adv_star->min_dist_to_colony < 10 && adv_star->star_slot > 0 &&
                    adv_star->star_slot < std::ssize(state.star_order)) {
                    player->candidate_star_count++;
                    const auto& star = state.stars.at(game::Star_Key_At_Slot(state, adv_star->star_slot));

                    for (const auto& link : star.orbit_planet_ids) {
                        const auto planet_id = link;
                        const auto it = state.planets.find(planet_id);
                        if (it == state.planets.end()) continue;
                        const auto& planet = it->second;
                        if (planet.planet_type != PLANET_TYPE_PLANET || planet.colony_id != game::NO_COLONY) continue;
                        if (!candidate_ids[player_idx].insert(planet_id).second) continue;

                        auto& candidate = player->planets[player->candidate_planet_count++];
                        candidate = {};
                        candidate.planet_id = planet_id;
                    }
                }
            }
        }
    }

    /**
     * Returns the next available candidate planet for an advanced-civilization player.
     * Skips planets that exceed the expansion distance limit or share a star with an enemy colony.
     */
    static int16_t Get_Next_Adv_Civ_Planet(const game::s_game_state& state, const planned_colony_owners& owners, const s_player& player) {
        const auto& adv_player = adv_civ_player[player.id.id];
        const auto home_slot = game::Star_Slot(state, state.planets.at(player.home_planet_id).star_id);

        int16_t expansion_cutoff = Num_Adv_Civ_Planets();

        if (adv_player.settled_planet_count < expansion_cutoff) {
            int16_t total_planet_count = adv_player.candidate_planet_count;

            int32_t dist_calc = ((int32_t)adv_player.furthest_star_dist_from_home * 4) / 10;
            int32_t galaxy_limit = (int32_t)Galaxy_Size_For_Classic_Rules_(
                                       MOX::settings.galaxy_size) +
                9;
            int16_t max_dist = (int16_t)((dist_calc > galaxy_limit) ? dist_calc : galaxy_limit);

            for (int16_t i = 0; i < total_planet_count; ++i) {
                const auto id = adv_player.planets[i].planet_id;

                if (id != game::NO_PLANET && !selected_planets.contains(id)) {
                    const auto star = state.planets.at(id).star_id;
                    const auto owner = Planned_Colony_Owner(owners, star);

                    if (owner == game::NO_PLAYER || owner == player.id) {
                        const auto slot = game::Star_Slot(state, star);
                        if (slot < 0) continue;
                        int16_t distance = (int16_t)parsec[home_slot].dist[slot];
                        if (distance <= max_dist) {
                            return i;
                        }
                    }
                }
            }
        }

        return -1;
    }

    int16_t Num_Adv_Civ_Planets() {
        int32_t stars_half = (int32_t)std::ssize(game::current_state.stars) / 2;
        int32_t scaled_stars = (int16_t)stars_half * 10;
        int32_t players = (int32_t)game::current_state.player_count;

        int32_t result_val = (scaled_stars / players) + 9;
        return (int16_t)(result_val / 10) - 1;
    }

    void __cdecl Build_Parsec_Star_Distances(s_parsec* parsec_distances) {
        for (int16_t i = 0; i < MAX_STARS; ++i) {
            memset(parsec_distances[i].dist, 0xFF, sizeof(parsec_distances[i].dist));

            if (i < std::ssize(game::current_state.stars)) {
                for (int16_t j = 0; j < std::ssize(game::current_state.stars); ++j) {
                    if (i == j) {
                        parsec_distances[i].dist[j] = 0;
                    } else {
                        parsec_distances[i].dist[j] = GEO::Parsecs_Between_Stars(i, j);
                    }
                }
            }
        }
    }

    static void Choose_Adv_Civ_Planets(game::s_game_state& state, planned_colony_owners& owners) {
        int16_t target_planet_count = Num_Adv_Civ_Planets();
        int16_t active_players_count = game::current_state.player_count;

        std::array<game::id_player, MAX_PLAYERS> active_player_order;
        active_player_order.fill(game::NO_PLAYER);
        int16_t picked_planet_idx[MAX_PLAYERS];
        int16_t round_active_players_count;

        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
            picked_planet_idx[i] = -1;
        }
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            active_player_order[i] = state.players[i].id;
        }

        while (active_players_count > 0) {
            for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
                picked_planet_idx[i] = -1;
            }

            round_active_players_count = active_players_count;

            Build_Adv_Civ_Star_List(state, owners);
            // Match Randomize_List's swap count and RNG order, including empty entries.
            for (int i = 0; i < 3 * MAX_PLAYERS / 2; ++i) {
                const auto first = game_random::Random(MAX_PLAYERS) - 1;
                const auto second = game_random::Random(MAX_PLAYERS) - 1;
                std::swap(active_player_order[first], active_player_order[second]);
            }

            for (auto& player : active_player_order) {
                if (player == game::NO_PLAYER) continue;

                int16_t candidate_planet_idx = Get_Next_Adv_Civ_Planet(state, owners, state.players[player.id]);
                if (candidate_planet_idx == -1) {
                    active_players_count--;
                    player = game::NO_PLAYER;
                } else {
                    const auto id = adv_civ_player[player.id].planets[candidate_planet_idx].planet_id;
                    picked_planet_idx[player.id] = candidate_planet_idx;
                    selected_planets[id] = player;
                    owners[state.planets.at(id).star_id] = player;
                }
            }

            int16_t round_pick_count = 0;
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                if (picked_planet_idx[i] != -1) {
                    round_pick_count++;
                }
            }

            if (round_pick_count == round_active_players_count) {
                for (int16_t p = 0; p < game::current_state.player_count; ++p) {
                    int16_t candidate_planet_idx = picked_planet_idx[p];
                    if (candidate_planet_idx == -1) continue;

                    s_adv_civ_player& plr = adv_civ_player[p];
                    s_adv_civ_player_planet& cand_planet = plr.planets[candidate_planet_idx];

                    plr.settled_planet_count++;
                    cand_planet.worth = -32766;

                    if (plr.candidate_planet_count > 1) {
                        plr.planets[candidate_planet_idx] = plr.planets[plr.candidate_planet_count - 1];
                    }
                    plr.candidate_planet_count--;

                    if (plr.settled_planet_count >= target_planet_count) {
                        const auto retiring = std::ranges::find(active_player_order, state.players[p].id);
                        if (retiring != active_player_order.end()) *retiring = game::NO_PLAYER;
                        active_players_count--;
                    }
                }
            } else {
                for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                    int16_t candidate_planet_idx = picked_planet_idx[i];
                    if (candidate_planet_idx >= 0 && candidate_planet_idx < MAX_PLANETS) {
                        selected_planets.erase(adv_civ_player[i].planets[candidate_planet_idx].planet_id);
                    }
                }
                active_players_count = 0;
            }
        }
    }

    static int32_t Get_Players_Planet_Worthiness_Average(game::s_game_state& state, const planned_colony_owners& owners, s_player& player) {
        int32_t total_worth = 0;

        for (const auto& [planet_id, owner] : selected_planets) {
            if (owner == player.id) {
                int32_t planet_worth;
                int8_t has_colony_at_star;
                int16_t proximity_bonus;

                ADVCIV::Get_Adv_Civ_Planet_Worthiness(
                    state, owners,
                    state.planets.at(planet_id),
                    player,
                    &planet_worth,
                    &has_colony_at_star,
                    &proximity_bonus);

                total_worth += planet_worth;
            }
        }

        return (total_worth * 10) /
            (int32_t)ADVCIV::adv_civ_player[player.id.id].settled_planet_count;
    }

    void __cdecl Advanced_Civilization_Colonies() {
        auto& staged = game::current_state;
        game_random::Set_Random_Seed(MOX::settings.start_of_game_seed);
        game_random::Get_Random_Seed();
        allocate::Push_Block(MOX::screen_seg);

        adv_civ_player = static_cast<s_adv_civ_player*>(allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, MAX_PLAYERS * sizeof(*adv_civ_player), alignof(s_adv_civ_player)));
        parsec = (s_parsec*)allocate::Allocate_Next_Block(MOX::screen_seg, MAX_STARS * sizeof(*parsec));
        planned_colony_owners owners;

        memset(adv_civ_player, 0, MAX_PLAYERS * sizeof(*adv_civ_player));
        memset(parsec, 0, MAX_STARS * sizeof(*parsec));
        selected_planets.clear();
        for (auto& ids : candidate_ids) ids.clear();

        for (int16_t p_idx = 0; p_idx < game::current_state.player_count; ++p_idx) {
            const auto& player = staged.players[p_idx];
            const auto home_id = player.home_planet_id;
            const auto home_star = staged.planets.at(home_id).star_id;
            const auto home_slot = game::Star_Slot(staged, home_star);

            auto& candidates = adv_civ_player[p_idx].planets;
            for (auto& entry : candidates) entry.planet_id = game::NO_PLANET;
            // Preserve the old sort's negative placeholder at the home world's rank.
            const auto home_rank = std::ranges::find(staged.planet_order, home_id);
            if (home_rank != staged.planet_order.end()) {
                candidates[home_rank - staged.planet_order.begin()].worth = -32766;
            }

            for (int16_t i = 0; i < MAX_STARS; ++i) {
                adv_civ_player[p_idx].stars[i].star_slot = -1;
                adv_civ_player[p_idx].stars[i].min_dist_to_colony = 9999;
            }

            adv_civ_player[p_idx].stars[home_slot].star_slot = home_slot;
            owners[home_star] = player.id;

        }

        RUSS::Mox_Update();
        Build_Parsec_Star_Distances(parsec);
        RUSS::Mox_Update();
        Choose_Adv_Civ_Planets(staged, owners);
        RUSS::Mox_Update();
        Twiddle_Selected_Adv_Civ_Planets(staged, owners);
        RUSS::Mox_Update();
        Init_Non_Homeworld_Colonies();
        RUSS::Mox_Update();

        allocate::Pop_Block(MOX::screen_seg);
    }

    /**
     * Balances weaker advanced-civilization starts against the strongest one.
     * Improves selected planets until each player reaches at least 90% of the best average worth.
     */
    static void Twiddle_Selected_Adv_Civ_Planets(game::s_game_state& state, const planned_colony_owners& owners) {
        MAPGEN::MAPGEN_TRACE = 0;
        int16_t player_special_count[MAX_PLAYERS] = {0};
        int32_t max_worthiness = 0;
        int16_t best_player_idx = 0;

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            int32_t current_worth = Get_Players_Planet_Worthiness_Average(state, owners, state.players[i]);
            if (current_worth > max_worthiness) {
                max_worthiness = current_worth;
                best_player_idx = i;
            }
        }

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            for (const auto& [planet_id, owner] : selected_planets) {
                if (owner == state.players[i].id) {
                    int8_t spec = state.planets.at(planet_id).planet_special;
                    if (spec == SYSTEM_SPECIAL_GEM_DEPOSITS || spec == SYSTEM_SPECIAL_GOLD_DEPOSITS || spec == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                        player_special_count[i]++;
                    }
                }
            }
        }

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (i == best_player_idx) continue;

            bool player_satisfied = false;
            for (const auto& [planet_id, owner] : selected_planets) {
                if (player_satisfied) break;
                if (owner == state.players[i].id) {
                    auto& planet = state.planets.at(planet_id);
                    int8_t generated_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    uint8_t targets[3];
                    targets[0] = (uint8_t)(PLANET_SIZE_HUGE - planet.size);
                    int32_t climate_diff = PLANET_CLIMATE_TERRAN - planet.climate;
                    targets[1] = (climate_diff < 0) ? 0 : (uint8_t)climate_diff;
                    targets[2] = (uint8_t)(MINERAL_RESOURCE_ULTRA_RICH - planet.mineral_class);

                    if (player_special_count[i] < player_special_count[best_player_idx] &&
                        planet.planet_special == SYSTEM_SPECIAL_NO_SPECIAL &&
                        game::current_state.stars.at(planet.star_id).system_special == SYSTEM_SPECIAL_NO_SPECIAL) {
                        if (!(game::current_state.players[i].home_planet_id == planet_id && game::current_state.players[i].traits[TRAIT_ANCIENT_HOME_WORLD] != 0)) {
                            int16_t attempts = 0;
                            do {
                                int16_t unused_special_slot;
                                MAPGEN::Get_A_Planet_Or_Star_Special(state, game::Star_Slot(state, planet.star_id), &unused_special_slot, &generated_special, 0, 0);
                                if (generated_special == SYSTEM_SPECIAL_GEM_DEPOSITS || generated_special == SYSTEM_SPECIAL_GOLD_DEPOSITS || generated_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                                    attempts = 100;
                                } else {
                                    generated_special = SYSTEM_SPECIAL_NO_SPECIAL;
                                }
                                attempts++;
                            } while (attempts < 100);
                        }

                        if (generated_special > SYSTEM_SPECIAL_NO_SPECIAL) {
                            planet.planet_special = generated_special;
                            player_special_count[i]++;
                            game::current_state.stars.at(planet.star_id).system_special = generated_special;
                        }

                        if (Get_Players_Planet_Worthiness_Average(state, owners, state.players[i]) >= (max_worthiness * 90 / 100)) {
                            player_satisfied = true;
                        }
                    }

                    if (generated_special == SYSTEM_SPECIAL_NO_SPECIAL) {
                        for (int16_t k = 0; k < 6 && !player_satisfied; ++k) {
                            int8_t twiddle_result = Twiddle_Planet(planet, targets);
                            if (twiddle_result != 0) {
                                break;
                            }
                            MAPGEN::Set_Secondary_Planet_Data(planet, game::Star_Slot(state, planet.star_id));
                            if (Get_Players_Planet_Worthiness_Average(state, owners, state.players[i]) >= (max_worthiness * 90 / 100)) {
                                player_satisfied = true;
                            }
                        }
                    }
                }
            }
        }
    }

    /**
     * Applies one weighted upgrade step to the selected planet.
     * Returns 1 when no further weighted upgrade choice is available.
     */
    int8_t __cdecl Twiddle_Planet(s_planet_data& planet, uint8_t* upgrade_weights) {
        int16_t choice = RUSS::Get_Weighted_Choice_Char(upgrade_weights, 3);

        if (choice == -1) {
            return 1;
        }

        if (choice == 0) {
            int32_t new_size = (int32_t)planet.size + 1;
            if (new_size > PLANET_SIZE_HUGE) {
                new_size = PLANET_SIZE_HUGE;
            }
            planet.size = (int8_t)new_size;
            upgrade_weights[0]--;
        } else if (choice == 1) {
            int32_t new_climate = (int32_t)planet.climate + 1;
            if (new_climate > PLANET_CLIMATE_GAIA) {
                new_climate = PLANET_CLIMATE_GAIA;
            }
            planet.climate = (int8_t)new_climate;
            upgrade_weights[1]--;
        } else if (choice == 2) {
            int32_t new_minerals = (int32_t)planet.mineral_class + 1;
            if (new_minerals > MINERAL_RESOURCE_ULTRA_RICH) {
                new_minerals = MINERAL_RESOURCE_ULTRA_RICH;
            }
            planet.mineral_class = (int8_t)new_minerals;
            upgrade_weights[2]--;
        }

        return 0;
    }

    static int16_t Proximity_Bonus(const game::s_game_state& state, const planned_colony_owners& owners,
        game::id_player player, game::id_star target_star) {
        int16_t proximity_bonus = 0;
        int16_t i = 0;
        const auto target_slot = game::Star_Slot(state, target_star);
        if (target_slot < 0) return 0;
        const auto num_stars = std::ssize(state.star_order);

        if (num_stars > 0) {
            do {
                const auto colony_slot = adv_civ_player[player.id].stars[i].star_slot;
                const auto colony_star = game::Star_Key_At_Slot(state, colony_slot);

                if (colony_star != game::NO_STAR && Planned_Colony_Owner(owners, colony_star) == player) {
                    const int16_t distance =
                        (int16_t)parsec[colony_slot].dist[target_slot];

                    if (distance <= 9) {
                        switch (distance) {
                            case 1:
                            case 2:
                                proximity_bonus = (int16_t)(proximity_bonus + 20);
                                break;

                            case 3:
                            case 4:
                                proximity_bonus = (int16_t)(proximity_bonus + 10);
                                break;

                            case 5:
                                proximity_bonus = (int16_t)(proximity_bonus + 5);
                                break;

                            default:
                                break;
                        }
                    }
                }

                ++i;
            } while (i < num_stars);
        }

        return proximity_bonus;
    }

    void __cdecl Init_Non_Homeworld_Colonies() {
        for (const auto& [planet_id, owner] : selected_planets) {
            ERICNET::Init_Non_Homeworld_Colony(planet_id, owner);
        }
    }
} // namespace ADVCIV
