#include "pch.h"
#include <unordered_set>

namespace ADVCIV {
    int8_t* star_colony_owner;
    static std::map<game::id_planet, int8_t> selected_planets;
    static std::array<std::unordered_set<game::id_planet>, MAX_PLAYERS> candidate_ids;
    s_adv_civ_player* adv_civ_player;
    s_parsec* parsec;

    bool __cdecl Player_Has_Colony_At_Star(int16_t player_idx, int16_t star_idx) {
        return star_colony_owner[star_idx] == player_idx;
    }

    bool __cdecl Enemy_Has_Colony_At_Star(int16_t player_idx, int16_t star_idx) {
        int8_t owner = star_colony_owner[star_idx];

        if (owner > -1 && owner != player_idx) {
            return true;
        }

        return false;
    }

    void __cdecl Init_Advanced_Civilization_Fleet(int16_t player_index) {
        return;
    }

    void __cdecl Assign_Advanced_Civilization_Starting_Ships() {
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            Init_Advanced_Civilization_Fleet(i);
        }
    }

    int __cdecl qsort_planet_worth(const void* left_record, const void* right_record) {
        const auto* left_planet = (const s_adv_civ_player_planet*)left_record;
        const auto* right_planet = (const s_adv_civ_player_planet*)right_record;
        return right_planet->worth - left_planet->worth;
    }

    void __cdecl Get_Adv_Civ_Planet_Worthiness(s_planet_data& planet, int16_t player_idx, int32_t* worth, int8_t* has_colony, int16_t* proximity_bonus) {
        *proximity_bonus = 0;
        *worth = 0;
        *has_colony = 0;

        s_player* player = &game::current_state.players[player_idx];

        int32_t raw_worth = AIDATA::Uncolonized_Planet_Worth_To_Player(&game::current, &planet, player);
        *worth = (int32_t)(raw_worth & 0xFFFF);

        int16_t base_proximity = ADVCIV::Proximity_Bonus(player_idx, game::Star_Row(game::current_state, planet.star_id));
        *proximity_bonus = (int16_t)((int32_t)base_proximity * (*worth) / 100);

        if (Player_Has_Colony_At_Star(player_idx, game::Star_Row(game::current_state, planet.star_id))) {
            *worth = (*worth * 67) / 100;
            *has_colony = 1;
        }

        *worth += (int32_t)(*proximity_bonus);
    }

    void Get_Worth_For_All_Planets(game::s_game_state& state) {
        for (int16_t player_idx = 0; player_idx < game::current_state.player_count; ++player_idx) {
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
                    Get_Adv_Civ_Planet_Worthiness(state.planets.at(id), player_idx, &worth, &has_colony, &proximity_bonus);
                }

                planet.worth = worth;
            }

            qsort(player.planets, MAX_PLANETS, sizeof(s_adv_civ_player_planet), qsort_planet_worth);
        }
    }

    void Build_Adv_Civ_Star_List(game::s_game_state& state) {
        int16_t max_dist_so_far = 0;

        for (int16_t player_idx = 0; player_idx < game::current_state.player_count; ++player_idx) {
            adv_civ_player[player_idx].candidate_star_count = 0;

            const auto home_row = game::Star_Row(state, state.planets.at(game::current_state.players[player_idx].home_planet_id).star_id);
            for (int16_t star_idx = 1; star_idx < MAX_STARS; ++star_idx) {
                int16_t distance = (int16_t)parsec[home_row].dist[star_idx];
                if (max_dist_so_far < distance) {
                    max_dist_so_far = distance;
                }
            }

            adv_civ_player[player_idx].furthest_star_dist_from_home = max_dist_so_far;

            for (int16_t colony_row = 1; colony_row < std::ssize(state.star_order); ++colony_row) {
                const auto& colony_star = state.stars.at(state.star_order[colony_row]);
                if (colony_star.system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                    Player_Has_Colony_At_Star(player_idx, colony_row)) {
                    for (int16_t target_row = 1; target_row < std::ssize(state.star_order); ++target_row) {
                        const auto target_id = state.star_order[target_row];
                        int16_t distance = (int16_t)parsec[colony_row].dist[target_row];
                        if (distance < 10 && state.stars.at(target_id).system_special != SYSTEM_SPECIAL_SPACE_MONSTER) {
                            auto& star_proximity = adv_civ_player[player_idx].stars[target_row];
                            if (distance <= star_proximity.min_dist_to_colony) {
                                star_proximity.min_dist_to_colony = distance;
                            }
                            star_proximity.star_idx = target_row;
                        }
                    }
                }
            }
        }

        Build_Adv_Civ_Planet_List(state);
        Get_Worth_For_All_Planets(state);
    }

    void Build_Adv_Civ_Planet_List(const game::s_game_state& state) {
        if (game::current_state.player_count <= 0) {
            return;
        }

        for (int16_t player_idx = 0; player_idx < game::current_state.player_count; ++player_idx) {
            s_adv_civ_player* player = &ADVCIV::adv_civ_player[player_idx];

            for (int16_t star_list_idx = 0; star_list_idx < MAX_STARS; ++star_list_idx) {
                s_adv_civ_player_star* adv_star = &player->stars[star_list_idx];

                if (adv_star->min_dist_to_colony < 10 && adv_star->star_idx > 0 &&
                    adv_star->star_idx < std::ssize(state.star_order)) {
                    player->candidate_star_count++;
                    int16_t star_idx = adv_star->star_idx;
                    const auto& star = state.stars.at(state.star_order[star_idx]);

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
    int16_t Get_Next_Adv_Civ_Planet(const game::s_game_state& state, int16_t player_idx) {
        s_adv_civ_player& adv_player = adv_civ_player[player_idx];
        const auto home_planet_id = game::current_state.players[player_idx].home_planet_id;
        int16_t home_star_idx = game::Star_Row(state, state.planets.at(home_planet_id).star_id);

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
                    int16_t star_idx = game::Star_Row(state, state.planets.at(id).star_id);
                    int16_t distance = (int16_t)parsec[home_star_idx].dist[star_idx];

                    if (!Enemy_Has_Colony_At_Star(player_idx, star_idx)) {
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

    void Choose_Adv_Civ_Planets(game::s_game_state& state) {
        int16_t target_planet_count = Num_Adv_Civ_Planets();
        int16_t active_players_count = game::current_state.player_count;

        int16_t active_player_order[MAX_PLAYERS];
        int16_t picked_planet_idx[MAX_PLAYERS];
        int16_t round_active_players_count;

        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
            active_player_order[i] = -1;
            picked_planet_idx[i] = -1;
        }
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            active_player_order[i] = i;
        }

        while (active_players_count > 0) {
            for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
                picked_planet_idx[i] = -1;
            }

            round_active_players_count = active_players_count;

            Build_Adv_Civ_Star_List(state);
            HAROLD::Randomize_List(active_player_order, MAX_PLAYERS, 0);

            for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
                int16_t player_idx = active_player_order[i];
                if (player_idx == -1) continue;

                int16_t candidate_planet_idx = Get_Next_Adv_Civ_Planet(state, player_idx);
                if (candidate_planet_idx == -1) {
                    active_players_count--;
                    active_player_order[i] = -1;
                } else {
                    const auto id = adv_civ_player[player_idx].planets[candidate_planet_idx].planet_id;
                    int16_t star_idx = game::Star_Row(state, state.planets.at(id).star_id);
                    picked_planet_idx[player_idx] = candidate_planet_idx;
                    selected_planets[id] = (int8_t)player_idx;
                    star_colony_owner[star_idx] = (int8_t)player_idx;
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
                        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
                            if (active_player_order[i] == p) {
                                active_player_order[i] = -1;
                                break;
                            }
                        }
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

    int32_t Get_Players_Planet_Worthiness_Average(game::s_game_state& state, int16_t player_idx) {
        int32_t total_worth = 0;

        for (const auto& [planet_id, owner] : selected_planets) {
            if (owner == player_idx) {
                int32_t planet_worth;
                int8_t has_colony_at_star;
                int16_t proximity_bonus;

                ADVCIV::Get_Adv_Civ_Planet_Worthiness(
                    state.planets.at(planet_id),
                    player_idx,
                    &planet_worth,
                    &has_colony_at_star,
                    &proximity_bonus);

                total_worth += planet_worth;
            }
        }

        return (total_worth * 10) /
            (int32_t)ADVCIV::adv_civ_player[player_idx].settled_planet_count;
    }

    void __cdecl Advanced_Civilization_Colonies() {
        auto& staged = game::current_state;
        game_random::Set_Random_Seed(MOX::settings.start_of_game_seed);
        game_random::Get_Random_Seed();
        allocate::Push_Block(MOX::screen_seg);

        adv_civ_player = static_cast<s_adv_civ_player*>(allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, MAX_PLAYERS * sizeof(*adv_civ_player), alignof(s_adv_civ_player)));
        parsec = (s_parsec*)allocate::Allocate_Next_Block(MOX::screen_seg, MAX_STARS * sizeof(*parsec));
        star_colony_owner = (int8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, MAX_STARS);

        memset(adv_civ_player, 0, MAX_PLAYERS * sizeof(*adv_civ_player));
        memset(parsec, 0, MAX_STARS * sizeof(*parsec));
        selected_planets.clear();
        for (auto& ids : candidate_ids) ids.clear();
        memset(star_colony_owner, 0xFF, MAX_STARS);

        for (int16_t p_idx = 0; p_idx < game::current_state.player_count; ++p_idx) {
            const auto home_id = game::current_state.players[p_idx].home_planet_id;
            int16_t star_idx = game::Star_Row(game::current_state, staged.planets.at(home_id).star_id);

            auto& candidates = adv_civ_player[p_idx].planets;
            for (auto& entry : candidates) entry.planet_id = game::NO_PLANET;
            // Preserve the old sort's negative placeholder at the home world's rank.
            const auto home_rank = std::ranges::find(staged.planet_order, home_id);
            if (home_rank != staged.planet_order.end()) {
                candidates[home_rank - staged.planet_order.begin()].worth = -32766;
            }

            for (int16_t i = 0; i < MAX_STARS; ++i) {
                adv_civ_player[p_idx].stars[i].star_idx = -1;
                adv_civ_player[p_idx].stars[i].min_dist_to_colony = 9999;
            }

            adv_civ_player[p_idx].stars[star_idx].star_idx = star_idx;
            star_colony_owner[star_idx] = (int8_t)p_idx;

        }

        RUSS::Mox_Update();
        Build_Parsec_Star_Distances(parsec);
        RUSS::Mox_Update();
        Choose_Adv_Civ_Planets(staged);
        RUSS::Mox_Update();
        Twiddle_Selected_Adv_Civ_Planets(staged);
        RUSS::Mox_Update();
        Init_Non_Homeworld_Colonies();
        RUSS::Mox_Update();

        allocate::Pop_Block(MOX::screen_seg);
    }

    /**
     * Balances weaker advanced-civilization starts against the strongest one.
     * Improves selected planets until each player reaches at least 90% of the best average worth.
     */
    void Twiddle_Selected_Adv_Civ_Planets(game::s_game_state& state) {
        MAPGEN::MAPGEN_TRACE = 0;
        int16_t player_special_count[MAX_PLAYERS] = {0};
        int32_t max_worthiness = 0;
        int16_t best_player_idx = 0;

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            int32_t current_worth = Get_Players_Planet_Worthiness_Average(state, i);
            if (current_worth > max_worthiness) {
                max_worthiness = current_worth;
                best_player_idx = i;
            }
        }

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            for (const auto& [planet_id, owner] : selected_planets) {
                if (owner == i) {
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
                if (owner == i) {
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
                                MAPGEN::Get_A_Planet_Or_Star_Special(state, game::Star_Row(game::current_state, planet.star_id), &unused_special_slot, &generated_special, 0, 0);
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

                        if (Get_Players_Planet_Worthiness_Average(state, i) >= (max_worthiness * 90 / 100)) {
                            player_satisfied = true;
                        }
                    }

                    if (generated_special == SYSTEM_SPECIAL_NO_SPECIAL) {
                        for (int16_t k = 0; k < 6 && !player_satisfied; ++k) {
                            int8_t twiddle_result = Twiddle_Planet(planet, targets);
                            if (twiddle_result != 0) {
                                break;
                            }
                            MAPGEN::Set_Secondary_Planet_Data(planet, game::Star_Row(game::current_state, planet.star_id));
                            if (Get_Players_Planet_Worthiness_Average(state, i) >= (max_worthiness * 90 / 100)) {
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

    int16_t __cdecl Proximity_Bonus(int16_t player_idx, int16_t target_star_idx) {
        int16_t proximity_bonus = 0;
        int16_t i = 0;
        const int16_t num_stars = std::ssize(game::current_state.stars);

        if (num_stars > 0) {
            do {
                const int16_t colony_star_idx = ADVCIV::adv_civ_player[player_idx].stars[i].star_idx;

                if (ADVCIV::Player_Has_Colony_At_Star(player_idx, colony_star_idx)) {
                    const int16_t distance =
                        (int16_t)ADVCIV::parsec[colony_star_idx].dist[target_star_idx];

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
            ERICNET::Init_Non_Homeworld_Colony(planet_id, legacy_world::Decode_Player_Slot(owner));
        }
    }
} // namespace ADVCIV
