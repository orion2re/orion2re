#include "pch.h"

#include "legacy_ids.h"
#include "ship_ids.h"
#include "planet_pruning.h"
#include "planet_serialization.h"

using game::current_state;

#include "lua/lua_game.h"
#include "lua/lua_game_internal.h"
#include "lua/lua_game_setup.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_mod_runtime.h"

namespace MAPGEN {
    int16_t star_count_by_galaxy_size[GALAXY_SIZE_COUNT] = {20, 36, 54, 71, 99, 71};
    int16_t& maximum_star_count = star_count_by_galaxy_size[GALAXY_SIZE_MAXIMUM];
    s_map_layout layout[GALAXY_SIZE_COUNT] = {
        {0, 506, 400, 10, 10, 0, 0},
        {0, 759, 600, 15, 15, 1, 1},
        {0, 1012, 800, 20, 20, 2, 2},
        {0, 1518, 1200, 30, 30, 3, 3},
        {1, 1800, 1350, 30, 36, 3, 3},
        {0, 1518, 1200, 30, 30, 3, 3},
    };
    s_planet_generation_config planet_generation_config = {};
    s_home_system_config home_system_config = {3};
    s_special_generation_config special_generation_config = {1, 1, 1, 1, 1, 1};
    s_monster_generation_config monster_generation_config = {
        255, 0, 0, 0, {5, 5, 5, 8, 12}, 20};
    int16_t planet_limit = 250;
    uint8_t starting_reveal_radius;
    uint8_t star_visual_size_weight[3] = {3, 4, 3};
    s_nebula_count_config nebula_count[GALAXY_SIZE_COUNT] = {
        {0, 1, 0},
        {1, 2, 0},
        {1, 3, 0},
        {2, 4, 0},
        {2, 4, 1},
        {2, 4, 0},
    };
    uint8_t satellite_orbit_weight[5][GALAXY_AGE_COUNT] = {
        {25, 20, 10},
        {18, 20, 22},
        {17, 20, 30},
        {15, 20, 33},
        {25, 20, 5},
    };
    int8_t environment_group[6][5] = {
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE},
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS, PLANET_GROUP_LIFELESS, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
    };
    int8_t mineral_class[10][6] = {
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ULTRA_POOR, MINERAL_RESOURCE_ULTRA_POOR, MINERAL_RESOURCE_POOR},
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ULTRA_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_RICH},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_RICH},
        {MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_RICH},
        {MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ULTRA_RICH},
    };
    int8_t planet_gravity[MINERAL_RESOURCE_COUNT][PLANET_SIZE_COUNT] = {
        {PLANET_GRAVITY_LOW_G, PLANET_GRAVITY_LOW_G, PLANET_GRAVITY_LOW_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G},
        {PLANET_GRAVITY_LOW_G, PLANET_GRAVITY_LOW_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G},
        {PLANET_GRAVITY_LOW_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_HEAVY_G},
        {PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_HEAVY_G, PLANET_GRAVITY_HEAVY_G},
        {PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_NORMAL_G, PLANET_GRAVITY_HEAVY_G, PLANET_GRAVITY_HEAVY_G, PLANET_GRAVITY_HEAVY_G},
    };
    uint8_t n_marooned_heroes;
    uint8_t n_wormholes;
    uint8_t make_food_planet;
    uint8_t n_space_monsters;
    uint16_t companion_star;
    uint8_t n_maps_generated;
    uint16_t tests_failed[4];
    int8_t* evan_ptr;
    uint32_t num_maps_to_generate;
    s_animation_header* fill_msg_bitmap;
    char fill_msg[36];
    int16_t fill_msg_ctr;
    int8_t first_time_map_is_generated = 1;
    uint8_t special_count[SYSTEM_SPECIAL_COUNT];
    uint32_t n_stars;
    uint32_t n_planets;
    int16_t t_roll;
    uint32_t satellite_stat[6];
    uint32_t nebula_stat[MAX_NEBULAS + 1];
    int16_t star_count;
    const char s_raceopt_lbx[12] = "raceopt.lbx";
    const char s_starname_lbx[13] = "starname.lbx";
    uint8_t MAPGEN_TRACE = 1;

    void __cdecl Clear_Planets_In_Star_Structure() {
        for (auto& [star_id, star] : current_state.stars) {
            for (auto& planet_id : star.orbit_planet_ids) planet_id = game::NO_PLANET;
        }
    }

    /**
     * @brief Determines the base food production per farmer for a given planet.
     *
     * Planets in systems with non-habitable spectral classes produce no food
     * unless the system contains the Orion star special.
     */
    uint8_t __cdecl Generate_Food_Per_Farmer(const s_planet_data& planet, int16_t star_index) {
        return Generate_Food_Per_Farmer(planet, current_state.stars.at(game::Star_Key_At_Slot(current_state, star_index)));
    }

    uint8_t Generate_Food_Per_Farmer(const s_planet_data& planet, const s_star_data& star) {
        // Check if the system is either habitable or is the Orion system
        if (star.spectral_class >= STAR_CLASS_BLACK_HOLE && star.system_special != SYSTEM_SPECIAL_ORION_STAR) {
            return 0;
        }

        int8_t climate_idx = planet.climate;

        return MOX::food_per_farmer_table[climate_idx];
    }

    uint8_t __cdecl Generate_Max_Farms(const s_planet_data& planet, int16_t star_idx) {
        bool is_habitable = HAROLD::Habitable_Spectral_Class(star_idx);

        if (!is_habitable && game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special != SYSTEM_SPECIAL_ORION_STAR) {
            return 0;
        }

        int8_t size_idx = planet.size;
        return MOX::planet_max_farms[size_idx];
    }

    int8_t __cdecl Generate_Orbit(int16_t star_id) {
        uint8_t weights[5];
        bool available_orbit_has_weight = false;

        for (int i = 0; i < 5; ++i) {
            weights[i] = satellite_orbit_weight[i][MOX::settings.galaxy_age];
            if (game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_id)).orbit_planet_ids[i] == game::NO_PLANET && weights[i] != 0) {
                available_orbit_has_weight = true;
            }
        }

        if (available_orbit_has_weight) {
            int16_t choice;
            do {
                choice = RUSS::Get_Weighted_Choice_Char(weights, 5);
            } while (choice >= 0 && game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_id)).orbit_planet_ids[choice] != game::NO_PLANET);
            if (choice >= 0) return (int8_t)choice;
        }

        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            if (game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_id)).orbit_planet_ids[orbit] == game::NO_PLANET) return (int8_t)orbit;
        }
        return 0;
    }

    uint8_t __cdecl Generate_Size() {
        int32_t rand_val = game_random::Random(10);

        for (int16_t i = 0; i < PLANET_SIZE_COUNT; ++i) {
            int16_t threshold = (int16_t)(int8_t)MOX::planet_size_table[i];

            if (threshold >= (int16_t)rand_val) {
                return (uint8_t)i;
            }
        }

        return 0xFF;
    }

    int8_t __cdecl Generate_Mineral_Class(int16_t star_idx) {
        int32_t random_roll = game_random::Random(10);
        int8_t spectral_class = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).spectral_class;

        if (spectral_class >= STAR_CLASS_B && spectral_class < STAR_CLASS_BLACK_HOLE) {
            return mineral_class[random_roll - 1][spectral_class];
        }

        return -1;
    }

    /**
     * @brief Generates the type of satellite (planet/gas giant/etc.) for a given orbit.
     *
     * @param star_idx The index of the star in the stars array.
     * @param orbit The orbit index (0 being closest to the star).
     * @return The generated satellite type identifier.
     */
    uint8_t __cdecl Generate_Satellite_Type(int16_t star_idx, int8_t orbit) {
        if (orbit < 0 || orbit >= 5) return MAPGEN_SATELLITE_ASTEROID;
        if (orbit == 0) {
            bool valid_type_exists = false;
            for (int roll = 0; roll < 10; ++roll) {
                valid_type_exists = valid_type_exists ||
                    MOX::orbit_to_satellite_type[roll][orbit] !=
                        MAPGEN_SATELLITE_GAS_GIANT;
            }
            if (!valid_type_exists) return MAPGEN_SATELLITE_ASTEROID;
        }

        uint8_t satellite_type;
        int16_t roll;

        do {
            // Randomly roll for a satellite type based on weightings in the data table
            roll = (int16_t)(game_random::Random(10) - 1);
            MAPGEN::t_roll = roll;

            satellite_type = MOX::orbit_to_satellite_type[roll][orbit];

            if (satellite_type == MAPGEN_SATELLITE_COMPANION_STAR) {
                int chance = game_random::Random(100);

                // If the roll is specific and it's the inner-most orbit, try to spawn a binary companion
                if (roll == 1 && chance < 11 && orbit == 0) {
                    MAPGEN::companion_star = 1;
                    int8_t spectral = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).spectral_class;

                    if (spectral == STAR_CLASS_B) {
                        satellite_type = 5;
                    } else {
                        satellite_type = (uint8_t)(spectral + 4);
                    }
                } else {
                    satellite_type = MAPGEN_SATELLITE_ASTEROID;
                }
            }
        } while (satellite_type == MAPGEN_SATELLITE_GAS_GIANT && orbit == 0);

        if (satellite_type == MAPGEN_SATELLITE_INVALID) {
            satellite_type = MAPGEN_SATELLITE_ASTEROID;
        }

        return satellite_type;
    }

    /**
     * @brief Generates initial data for a satellite (moon) in a star system.
     */
    void __cdecl Generate_Satellite(s_planet_data& planet, int16_t star_idx) {
        planet.star_id = game::Star_Key_At_Row(current_state, star_idx);
        planet.size = Generate_Size();
        planet.mineral_class = Generate_Mineral_Class(star_idx);
        Get_Climate_Modifier(planet.mineral_class);
        planet.gravity_class = Generate_Gravity_Class(planet);
        planet.group = (int8_t)Get_Planet_Group(planet);
        planet.climate = Generate_Climate(planet.group);
        planet.climate_bg_type = (int8_t)(game_random::Random(3) - 1);
        Set_Secondary_Planet_Data(planet, star_idx);
    }

    void __cdecl Set_Secondary_Planet_Data(s_planet_data& planet, int16_t star_idx) {
        planet.max_farms = Generate_Max_Farms(planet, star_idx);
        planet.food_per_farmer = Generate_Food_Per_Farmer(planet, star_idx);
        planet.colony_id = game::NO_COLONY;
    }

    int8_t __cdecl Get_Climate_Modifier(int8_t climate_index) {
        int8_t modifier = 0;

        if (climate_index >= MINERAL_RESOURCE_ULTRA_POOR && climate_index < MINERAL_RESOURCE_COUNT) {
            modifier = MOX::climate_modifier_table[climate_index];

            if (modifier > 0) {
                RUSS::PFMT(0, 0, "------CLIMATE modifier is %d  Planet: %d  Mineral: %d", modifier, static_cast<int>(current_state.planets.size()), climate_index);
            }
        }

        return modifier;
    }

    int8_t __cdecl Generate_Gravity_Class(const s_planet_data& planet) {
        int16_t mineral_class = planet.mineral_class;

        if (mineral_class >= MINERAL_RESOURCE_ULTRA_POOR && mineral_class < MINERAL_RESOURCE_COUNT) {
            int16_t size = planet.size;
            if (size >= PLANET_SIZE_TINY && size < PLANET_SIZE_COUNT) {
                return planet_gravity[mineral_class][size];
            }
        }

        return PLANET_GRAVITY_LOW_G;
    }

    void __cdecl Update_Planet_Group_Cache() {
        return;
    }

    int16_t __cdecl Get_Planet_Group(const s_planet_data& planet) {
        int16_t orbit = (int16_t)planet.orbit;
        int16_t spectral_class = (int16_t)game::current_state.stars.at(planet.star_id).spectral_class;

        if (companion_star == 1) {
            orbit -= 2;
            if (orbit < 0) {
                return PLANET_GROUP_BOMBARDED;
            }
        }

        if (spectral_class < STAR_CLASS_BLACK_HOLE) {
            int8_t group = environment_group[spectral_class][orbit];

            Update_Planet_Group_Cache();

            return (int16_t)group;
        }

        return PLANET_GROUP_BOMBARDED;
    }

    /**
     * Generates a planetary climate based on star type and galaxy age.
     * If the galaxy is young or normal, it uses one weight table; if old, another.
     * If forced to be a food planet, it ensures the chosen climate supports agriculture.
     */
    int8_t __cdecl Generate_Climate(int16_t planet_group) {
        uint8_t weights[PLANET_CLIMATE_COUNT];
        memset(&weights, 0, sizeof(weights));

        // 1. Determine weights based on planet group and galaxy age
        for (int i = 0; i < PLANET_CLIMATE_COUNT; ++i) {
            if (MOX::g_galaxy_age <= GALAXY_AGE_AVERAGE) {
                weights[i] = MOX::normal_gal_climate_roll_table[i][planet_group];
            } else if (MOX::g_galaxy_age == GALAXY_AGE_OLD) {
                weights[i] = MOX::old_gal_climate_roll_table[i][planet_group];
            }
        }

        int16_t result = -1; // Symbolic: NO_CLIMATE_SELECTED
        int16_t match_count = 0;

        // 2. Specialized handling for food-producing planets
        if (make_food_planet != 0) {
            uint8_t match_index = 0;
            for (int i = 0; i < PLANET_CLIMATE_COUNT; ++i) {
                // Check if climate has a non-zero weight and supports food production
                if (weights[i] != 0 && MOX::food_per_farmer_table[i] > 0) {
                    match_count++;
                    match_index = (uint8_t)i;
                }
            }

            // If only one agriculture-capable climate is valid for this star, select it immediately
            if (match_count == 1) {
                result = match_index;
            }
        }

        // 3. Perform weighted selection if no unique result was forced
        if (result == -1) {
            if (make_food_planet == 0) {
                result = RUSS::Get_Weighted_Choice_Char(weights, PLANET_CLIMATE_COUNT);
            } else if (match_count > 0) {
                do {
                    result = RUSS::Get_Weighted_Choice_Char(weights, PLANET_CLIMATE_COUNT);
                } while (result >= 0 && MOX::food_per_farmer_table[result] == 0);
            }
        }
        return result >= 0 ? (int8_t)result : PLANET_CLIMATE_TERRAN;
    }

    void Enforce_Configured_Planet_Max() {
        if (std::ssize(current_state.stars) <= ORIGINAL_MAX_STARS) {
            if (planet_limit == 0) return;
            Enforce_Planet_Max(planet_limit);
        } else {
            Enforce_Planet_Max(MAX_PLANETS);
        }
    }

    std::vector<game::id_planet> Choose_Planets_To_Prune(
        std::vector<game::id_planet> candidates, int live_count, int limit) {
        std::vector<game::id_planet> removed;
        while (limit <= live_count && !candidates.empty()) {
            const size_t choice = game_random::Random(static_cast<uint32_t>(candidates.size())) - 1;
            removed.push_back(candidates[choice]);
            candidates[choice] = candidates.back();
            candidates.pop_back();
            --live_count;
        }
        return removed;
    }

    void Prune_Planet_Roster(game::s_game_state& state,
        std::span<const uint8_t> protected_stars, int limit) {
        std::vector<game::id_planet> candidates;
        candidates.reserve(state.planets.size());
        for (const auto id : state.planet_order) {
            const auto star = game::Star_Row(state, state.planets.at(id).star_id);
            if (star >= 0 && static_cast<size_t>(star) < protected_stars.size() && !protected_stars[star]) {
                candidates.push_back(id);
            }
        }
        const auto removed = Choose_Planets_To_Prune(std::move(candidates), static_cast<int>(state.planets.size()), limit);
        for (const auto id : removed) game::Erase_Planet(state, id);
    }

    void __cdecl Enforce_Planet_Max(int16_t max_planets) {
        if (max_planets > static_cast<int>(current_state.planets.size())) return;

        std::array<uint8_t, MAX_STARS> protected_stars{};
        protected_stars[0] = special_generation_config.generate_orion != 0;
        for (int16_t star = 0; star < std::ssize(current_state.stars); ++star) {
            if (HOMEGEN::Is_Added_Food_World(star)) protected_stars[star] = 1;
        }
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            const auto home = current_state.planets.find(game::current_state.players[i].home_planet_id);
            if (home != current_state.planets.end() && game::Star_Row(current_state, home->second.star_id) >= 0 && game::Star_Row(current_state, home->second.star_id) < MAX_STARS) {
                protected_stars[game::Star_Row(current_state, home->second.star_id)] = 1;
            }
        }

        if (capture::Check_Release_Version() == 0) {
            const int gas_giant_count = Gas_Giants();
            const int original_count = static_cast<int>(current_state.planets.size());
            RUSS::PFMT(0, 1, "Colony Planets: %d  Max: %d  Planet Count: %d  Removing %d planets",
                original_count - gas_giant_count, static_cast<int>(max_planets), original_count,
                original_count - gas_giant_count - max_planets);
            key::Wait_For_Key();
        }
        Prune_Planet_Roster(current_state, protected_stars, max_planets);

        Clear_Planets_In_Star_Structure();
        for (const auto& [id, planet] : current_state.planets) {
            game::current_state.stars.at(planet.star_id).orbit_planet_ids[planet.orbit] = id;
        }
    }

    static void Choose_Orbit_Special(const std::array<s_planet_data*, 5>& orbits,
        int16_t star_idx, int16_t* wormhole_out, int8_t* special_out, uint8_t commit_changes, int32_t excluded_mask) {
        uint16_t valid_planets[5] = {0};
        uint16_t large_planets[5] = {0};
        uint16_t habitable_planets[5] = {0};
        bool has_large = false;
        bool has_habitable = false;

        for (int i = 0; i < 5; ++i) {
            if (const auto* planet = orbits[i]) {
                if (planet->planet_type == PLANET_TYPE_PLANET && planet->planet_special == SYSTEM_SPECIAL_NO_SPECIAL) {
                    valid_planets[i] = 1;
                    int8_t climate = planet->climate;
                    // Habitable climates: Desert, Arid, Tundra, Ocean, Swamp, Terran, Gaia
                    if (climate == PLANET_CLIMATE_DESERT || climate == PLANET_CLIMATE_ARID ||
                        climate == PLANET_CLIMATE_TUNDRA || climate == PLANET_CLIMATE_OCEAN ||
                        climate == PLANET_CLIMATE_SWAMP || climate == PLANET_CLIMATE_TERRAN ||
                        climate == PLANET_CLIMATE_GAIA) {
                        habitable_planets[i] = 1;
                        has_habitable = true;
                        if (planet->size >= PLANET_SIZE_MEDIUM) { // Medium or larger
                            large_planets[i] = 1;
                            has_large = true;
                        }
                    }
                }
            }
        }

        int16_t tries = 0;
        uint16_t system_weights[SYSTEM_SPECIAL_COUNT];
        system_weights[SYSTEM_SPECIAL_NO_SPECIAL] = 0;

        while (true) {
            for (int s = SYSTEM_SPECIAL_STABLE_WORMHOLE; s < SYSTEM_SPECIAL_COUNT; ++s) {
                if ((excluded_mask >> s) & 1) {
                    system_weights[s] = 0;
                } else if (s == SYSTEM_SPECIAL_STABLE_WORMHOLE &&
                    !special_generation_config.generate_wormholes) {
                    system_weights[s] = 0;
                } else if (s == SYSTEM_SPECIAL_MAROONED_HERO) { // Marooned Hero
                    system_weights[s] =
                        (std::min<int16_t>(
                             Galaxy_Size_For_Classic_Rules_(MOX::settings.galaxy_size),
                             GALAXY_SIZE_HUGE) + 2 >=
                            n_marooned_heroes)
                        ? MOX::planet_special_weighted_chance[s]
                        : 0;
                } else if (s == SYSTEM_SPECIAL_NATIVES || s == SYSTEM_SPECIAL_SPLINTER_COLONY) { // Natives, Splinter Colony
                    system_weights[s] = (has_large) ? MOX::planet_special_weighted_chance[s] : 0;
                } else if (s == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) { // Ancient Artifacts
                    system_weights[s] = (has_habitable) ? MOX::planet_special_weighted_chance[s] : 0;
                } else if (s == SYSTEM_SPECIAL_SPACE_MONSTER) { // Space Monster
                    system_weights[s] = (MOX2::user_wants_n_space_monsters) ? MOX::planet_special_weighted_chance[s] : 0;
                } else {
                    system_weights[s] = MOX::planet_special_weighted_chance[s];
                }
            }

            int16_t choice = RUSS::Get_Weighted_Choice_Short(system_weights, SYSTEM_SPECIAL_COUNT);
            *special_out = (int8_t)choice;
            bool success = false;

            if (choice >= SYSTEM_SPECIAL_STABLE_WORMHOLE && choice <= SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                switch (choice) {
                    case SYSTEM_SPECIAL_STABLE_WORMHOLE:
                        if (commit_changes && n_wormholes < (Galaxy_Size_For_Classic_Rules_(MOX::settings.galaxy_size) * 4 + 4)) {
                            *wormhole_out = 1;
                            n_wormholes++;
                            success = true;
                        }
                        break;

                    case SYSTEM_SPECIAL_SPACE_DEBRIS:
                    case SYSTEM_SPECIAL_PIRATE_CACHE:
                    case SYSTEM_SPECIAL_SPACE_MONSTER:
                        if (commit_changes) {
                            game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special = (int8_t)choice;
                        }
                        success = true;
                        break;

                    case SYSTEM_SPECIAL_GOLD_DEPOSITS:
                    case SYSTEM_SPECIAL_GEM_DEPOSITS: {
                        int16_t p_idx = RUSS::Get_Weighted_Choice_Short(valid_planets, 5);
                        if (p_idx >= 0) {
                            if (commit_changes) {
                                orbits[p_idx]->planet_special = (int8_t)choice;
                                game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special = (int8_t)choice;
                            }
                            success = true;
                        }
                        break;
                    }

                    case SYSTEM_SPECIAL_NATIVES:
                    case SYSTEM_SPECIAL_SPLINTER_COLONY: {
                        int16_t p_idx = RUSS::Get_Weighted_Choice_Short(large_planets, 5);
                        if (p_idx >= 0) {
                            if (commit_changes) {
                                orbits[p_idx]->planet_special = (int8_t)choice;
                                game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special = (int8_t)choice;
                            }
                            success = true;
                        }
                        break;
                    }

                    case SYSTEM_SPECIAL_MAROONED_HERO:
                        n_marooned_heroes++; // Increment global attempt counter
                        if (commit_changes) {
                            game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special = (int8_t)choice;
                        }
                        success = true;
                        break;

                    case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS: {
                        int16_t p_idx = RUSS::Get_Weighted_Choice_Short(habitable_planets, 5);
                        if (p_idx >= 0) {
                            if (commit_changes) {
                                orbits[p_idx]->planet_special = (int8_t)choice;
                                game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special = (int8_t)choice;
                            }
                            success = true;
                        }
                        break;
                    }
                }
            }

            if (success) break;

            tries++;
            if (tries > 100) {
                *special_out = SYSTEM_SPECIAL_NO_SPECIAL;
                break;
            }
        }
    }

    void __cdecl Get_A_Planet_Or_Star_Special(int16_t star_idx, int16_t* wormhole_out, int8_t* special_out, uint8_t commit_changes, int32_t excluded_mask) {
        std::array<s_planet_data*, 5> orbits{};
        for (size_t orbit = 0; orbit < orbits.size(); ++orbit) {
            const auto id = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).orbit_planet_ids[orbit];
            const auto it = current_state.planets.find(id);
            if (it != current_state.planets.end()) orbits[orbit] = &it->second;
        }
        Choose_Orbit_Special(orbits, star_idx, wormhole_out, special_out, commit_changes, excluded_mask);
    }

    void Get_A_Planet_Or_Star_Special(game::s_game_state& state, int16_t star_idx, int16_t* wormhole_out,
        int8_t* special_out, uint8_t commit_changes, int32_t excluded_mask) {
        std::array<s_planet_data*, 5> orbits{};
        for (size_t orbit = 0; orbit < orbits.size(); ++orbit) {
            const auto id = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).orbit_planet_ids[orbit];
            const auto it = state.planets.find(id);
            if (it != state.planets.end()) orbits[orbit] = &it->second;
        }
        Choose_Orbit_Special(orbits, star_idx, wormhole_out, special_out, commit_changes, excluded_mask);
    }

    void Generate_Star_Special(game::s_game_state& state, int16_t star_idx, int16_t* wormhole_out, uint8_t* special_counts) {
        int8_t special_type = SYSTEM_SPECIAL_NO_SPECIAL;

        if (game_random::Random(100) > MOX::planet_special) {
            const int32_t excluded_mask = special_generation_config.generate_splinter_colonies ? 0 : 1 << SYSTEM_SPECIAL_SPLINTER_COLONY;
            Get_A_Planet_Or_Star_Special(state, star_idx, wormhole_out, &special_type, 1, excluded_mask);
        } else {
            special_type = SYSTEM_SPECIAL_NO_SPECIAL;
        }

        int16_t monster_chance =
            monster_generation_config.natural_chance[MOX::settings.game_difficulty];

        if ((int32_t)game_random::Random(100) - 1 < (int32_t)monster_chance &&
            n_space_monsters < monster_generation_config.natural_count_cap) {
            n_space_monsters++;
        }

        if (special_type < SYSTEM_SPECIAL_NO_SPECIAL ||
            special_type >= SYSTEM_SPECIAL_COUNT) {
            special_type = SYSTEM_SPECIAL_NO_SPECIAL;
        }
        special_counts[special_type]++;
    }

    int16_t __cdecl Gas_Giants() {
        int16_t gas_giant_count = 0;

        for (int16_t star_idx = 0; star_idx < std::ssize(current_state.star_order); ++star_idx) {
            const auto star_id = current_state.star_order[star_idx];
            for (int16_t planet_idx = 0; planet_idx < 5; ++planet_idx) {
                if (game::current_state.stars.at(star_id).orbit_planet_ids[planet_idx] != game::NO_PLANET) {
                    if (HAROLD::Planet_Type(star_idx, planet_idx) == PLANET_TYPE_GAS_GIANT) {
                        gas_giant_count++;
                    }
                }
            }
        }

        return gas_giant_count;
    }

    /**
     * Rolls the number of planetary orbits to populate for a star system.
     * @param star_idx The star whose spectral class is used for the roll.
     * @return The generated satellite count.
     */
    int16_t __cdecl Generate_Number_Of_Satellites(int16_t star_idx) {
        int32_t random_val = game_random::Random(10);
        int16_t result = 0;

        if (game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).spectral_class < STAR_CLASS_BLACK_HOLE) {
            int32_t row_index = random_val - 1;
            int8_t spectral_class = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).spectral_class;
            result = (int16_t)MOX::class_to_num_satellites[row_index][spectral_class];
        }

        return result;
    }

    static std::optional<s_planet_data> Generate_Planet_Record(int16_t star_idx, s_planet_data planet = {}) {
        int16_t orbit_res = Generate_Orbit(star_idx);
        int8_t orbit = (int8_t)orbit_res;
        int16_t type = Generate_Satellite_Type(star_idx, orbit);

        if (type == MAPGEN_SATELLITE_ASTEROID || type == MAPGEN_SATELLITE_GAS_GIANT || type == MAPGEN_SATELLITE_PLANET) {
            planet.orbit = orbit;
            planet.planet_type = (int8_t)type;

            Generate_Satellite(planet, star_idx);
            return planet;
        }
        return std::nullopt;
    }

    void Planet_Generation(game::s_game_state& state, int16_t star_idx) {
        if (const auto planet = Generate_Planet_Record(star_idx)) {
            const auto id = game::Create_Planet(state, *planet);
            if (!id) return;
            game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).orbit_planet_ids[planet->orbit] = *id;
            ++n_planets;
        }
    }

    void __cdecl Planet_Generation(int16_t star_idx) {
        Planet_Generation(current_state, star_idx);
    }

    int16_t __cdecl Home_Planet(game::id_planet planet_id) {
        for (int16_t i = 0; i < (int16_t)game::current_state.player_count; ++i) {
            if (game::current_state.players[i].home_planet_id == planet_id) {
                return i;
            }
        }
        return -1;
    }

    namespace {

        bool Valid_Generated_Map(const game::s_game_state& state) {
            if (state.planets.size() > MAX_PLANETS || std::ssize(current_state.stars) < 0 ||
                std::ssize(current_state.stars) > MAX_STARS || state.player_count < 0 ||
                state.player_count > MAX_PLAYERS) {
                return false;
            }
            for (int16_t star_idx = 0; star_idx < std::ssize(current_state.star_order);
                 ++star_idx) {
                const auto star_id_ = current_state.star_order[star_idx];
                const s_star_data& star = game::current_state.stars.at(star_id_);
                if (star.system_special < SYSTEM_SPECIAL_NO_SPECIAL ||
                    star.system_special >= SYSTEM_SPECIAL_COUNT) {
                    return false;
                }
                for (int16_t orbit = 0; orbit < 5; ++orbit) {
                    const auto id = star.orbit_planet_ids[orbit];
                    if (id == game::NO_PLANET) continue;
                    const auto it = state.planets.find(id);
                    if (it == state.planets.end() || game::Star_Row(current_state, it->second.star_id) != star_idx || it->second.orbit != orbit) {
                        return false;
                    }
                }
            }
            for (const auto& [id, planet] : state.planets) {
                if (game::Star_Row(current_state, planet.star_id) < 0 ||
                    game::Star_Row(current_state, planet.star_id) >= std::ssize(current_state.stars) || planet.orbit < 0 ||
                    planet.orbit >= 5 ||
                    game::current_state.stars.at(planet.star_id).orbit_planet_ids[planet.orbit] != id ||
                    planet.planet_type < PLANET_TYPE_ASTEROID ||
                    planet.planet_type > PLANET_TYPE_PLANET ||
                    planet.size < PLANET_SIZE_TINY ||
                    planet.size >= PLANET_SIZE_COUNT ||
                    planet.gravity_class < PLANET_GRAVITY_LOW_G ||
                    planet.gravity_class >= PLANET_GRAVITY_COUNT ||
                    planet.climate < PLANET_CLIMATE_TOXIC ||
                    planet.climate >= PLANET_CLIMATE_COUNT ||
                    planet.mineral_class < MINERAL_RESOURCE_ULTRA_POOR ||
                    planet.mineral_class >= MINERAL_RESOURCE_COUNT ||
                    planet.planet_special < SYSTEM_SPECIAL_NO_SPECIAL ||
                    planet.planet_special >= SYSTEM_SPECIAL_COUNT) {
                    return false;
                }
                if (planet.planet_special != SYSTEM_SPECIAL_NO_SPECIAL &&
                    game::current_state.stars.at(planet.star_id).system_special !=
                        planet.planet_special) {
                    return false;
                }
            }
            for (int16_t player_idx = 0; player_idx < state.player_count;
                 ++player_idx) {
                const auto home = state.players[player_idx].home_planet_id;
                if (!state.planets.contains(home)) {
                    return false;
                }
            }
            return true;
        }

        struct s_generated_map_scope {
            game::s_game_state& staged;
            game::s_game_state* previous = orion2re::lua::game::internal::galaxy_planet_candidate;

            explicit s_generated_map_scope(game::s_game_state& state) : staged(state) {
                orion2re::lua::game::internal::galaxy_planet_candidate = &staged;
                std::swap(current_state.stars, staged.stars);
                std::swap(current_state.star_order, staged.star_order);
                std::swap(current_state.star_order_sorted, staged.star_order_sorted);
            }

            ~s_generated_map_scope() {
                orion2re::lua::game::internal::galaxy_planet_candidate = previous;
                std::swap(current_state.stars, staged.stars);
                std::swap(current_state.star_order, staged.star_order);
                std::swap(current_state.star_order_sorted, staged.star_order_sorted);
            }
        };

        struct s_galaxy_candidate_scope {
            game::s_game_state& candidate_state;
            std::unique_ptr<game::s_game_state> prior_state;
            game::s_game_state* previous_planet_candidate = orion2re::lua::game::internal::galaxy_planet_candidate;
            s_nebula* nebulas = MOX::nebula;
            uint8_t nebula_count = MOX::NUM_NEBULAS;

            s_galaxy_candidate_scope(game::s_game_state& state,
                s_nebula* candidate_nebulas) : candidate_state(state),
                prior_state(std::make_unique<game::s_game_state>(std::move(current_state))) {
                current_state = std::move(candidate_state);
                orion2re::lua::game::internal::galaxy_planet_candidate = &current_state;
                MOX::nebula = candidate_nebulas;
            }

            ~s_galaxy_candidate_scope() {
                candidate_state = std::move(current_state);
                current_state = std::move(*prior_state);
                orion2re::lua::game::internal::galaxy_planet_candidate = previous_planet_candidate;
                MOX::nebula = nebulas;
                MOX::NUM_NEBULAS = nebula_count;
            }
        };

        bool Valid_Galaxy_Candidate(const game::s_game_state& state) {
            if (!Valid_Generated_Map(state) ||
                MOX::nebula == nullptr || state.colonies.size() > MAX_COLONIES || state.ship_slots.size() > MAX_SHIPS ||
                MOX::NUM_NEBULAS > MAX_NEBULAS) {
                return false;
            }
            for (const auto& [star_id, star] : state.stars) {
                const auto exit = state.stars.find(star.wormhole_star_id);
                if (star.x < 0 || star.x >= MOX::MAP_MAX_X || star.y < 0 ||
                    star.y >= MOX::MAP_MAX_Y ||
                    star.spectral_class < STAR_CLASS_B ||
                    star.spectral_class > STAR_CLASS_BLACK_HOLE ||
                    star.wormhole_star_id == star.id ||
                    (star.wormhole_star_id != game::NO_STAR &&
                        (exit == state.stars.end() || exit->second.wormhole_star_id != star.id))) {
                    return false;
                }
            }
            for (int16_t player_idx = 0; player_idx < state.player_count;
                 ++player_idx) {
                const auto home = state.players[player_idx].home_planet_id;
                const auto& planet = state.planets.at(home);
                if (planet.planet_type != PLANET_TYPE_PLANET ||
                    !game::current_state.colonies.contains(planet.colony_id) ||
                    game::current_state.colonies.at(planet.colony_id).planet_id != home ||
                    legacy_world::Encode_Player_Slot(game::current_state.colonies.at(planet.colony_id).owner) != player_idx ||
                    factions::Legacy_Code(game::current_state.stars.at(planet.star_id).owner) != player_idx) {
                    return false;
                }
            }
            for (const auto& [colony_id, colony] : state.colonies) {
                const auto it = state.planets.find(colony.planet_id);
                if (it == state.planets.end() || it->second.colony_id != colony_id ||
                    legacy_world::Encode_Player_Slot(colony.owner) < 0 || legacy_world::Encode_Player_Slot(colony.owner) >= state.player_count) {
                    return false;
                }
            }
            for (const auto& [id, planet] : state.planets) {
                const auto colony_id = planet.colony_id;
                if (colony_id != game::NO_COLONY &&
                    (!game::current_state.colonies.contains(colony_id) || game::current_state.colonies.at(colony_id).planet_id != id)) {
                    return false;
                }
            }
            for (int16_t ship_idx = 0; ship_idx < static_cast<int16_t>(state.ship_slots.size());
                 ++ship_idx) {
                if (state.ship_slots[ship_idx] == ::game::NO_SHIP) continue;
                const s_ship_data& ship = ship_slots::At(state, ship_idx);
                const auto orbit = game::Ship_Orbit_Row(state, ship.position);
                if (ship.position.kind != game::e_ship_location::at_star ||
                    orbit >= std::ssize(current_state.stars) ||
                    (ship.status == 0 && orbit < 0) ||
                    (ship.prior_jump_gate_destination != game::NO_STAR &&
                        !state.stars.contains(ship.prior_jump_gate_destination))) {
                    return false;
                }
                if (ship.officer_id != game::NO_LEADER && ship.officer_id.id >= MAX_LEADERS) {
                    return false;
                }
            }
            for (int16_t leader_idx = 0; leader_idx < MAX_LEADERS;
                 ++leader_idx) {
                const auto& leader = state.leaders.at(leader_idx);
                const auto star = state.stars.find(leader.star_location);
                if (leader.status == 5 &&
                    (star == state.stars.end() || star->second.system_special != SYSTEM_SPECIAL_MAROONED_HERO)) {
                    return false;
                }
                if (leader.status == 1 &&
                    leader.type == LEADER_TYPE_COLONY &&
                    (star == state.stars.end() || leader.employer.id >= MAX_PLAYERS ||
                        leader.employer.id >= state.player_count || star->second.officer_id[leader.employer.id] != leader.id)) {
                    return false;
                }
                if (leader.status == 1 && leader.type == LEADER_TYPE_SHIP) {
                    const auto* ship = game::Find_Leader_Ship(state, leader);
                    if (!ship || ship->officer_id != leader.id) return false;
                }
            }
            for (int16_t star_idx = 0; star_idx < std::ssize(current_state.star_order);
                 ++star_idx) {
                const auto star_id = current_state.star_order[star_idx];
                if (game::current_state.stars.at(star_id).system_special !=
                    SYSTEM_SPECIAL_MAROONED_HERO) {
                    continue;
                }
                int leader_count = 0;
                for (const auto& leader : state.leaders) {
                    leader_count += leader.status == 5 && leader.star_location == star_id;
                }
                if (leader_count != 1) {
                    return false;
                }
            }
            for (int16_t nebula_idx = 0;
                 nebula_idx < MOX::NUM_NEBULAS;
                 ++nebula_idx) {
                const s_nebula& nebula = MOX::nebula[nebula_idx];
                if (nebula.type < 0 || nebula.type >= 12 || nebula.x < 0 ||
                    nebula.x >= MOX::MAP_MAX_X || nebula.y < 0 ||
                    nebula.y >= MOX::MAP_MAX_Y) {
                    return false;
                }
            }
            GEO::Initialize_Black_Hole_Blocks(45);
            bool reached[MAX_STARS] = {};
            int16_t pending[MAX_STARS];
            int16_t pending_count = 0;
            int16_t non_black_holes = 0;
            for (int16_t star_idx = 0; star_idx < std::ssize(current_state.star_order);
                 ++star_idx) {
                const auto star_id = current_state.star_order[star_idx];
                if (game::current_state.stars.at(star_id).spectral_class ==
                    STAR_CLASS_BLACK_HOLE) {
                    continue;
                }
                ++non_black_holes;
                if (pending_count == 0) {
                    reached[star_idx] = true;
                    pending[pending_count++] = star_idx;
                }
            }
            int16_t reached_count = pending_count;
            while (pending_count > 0) {
                const int16_t source = pending[--pending_count];
                for (int16_t destination = 0;
                     destination < std::ssize(current_state.star_order);
                     ++destination) {
                    const auto destination_id = current_state.star_order[destination];
                    if (reached[destination] ||
                        game::current_state.stars.at(destination_id).spectral_class ==
                            STAR_CLASS_BLACK_HOLE ||
                        struct_::Test_Bit_Field(
                            game::current_state.stars.at(game::Star_Key_At_Row(current_state, source)).black_hole_blocks,
                            destination) ||
                        GEO::Parsecs_Between_Stars(source, destination) > 8) {
                        continue;
                    }
                    reached[destination] = true;
                    pending[pending_count++] = destination;
                    ++reached_count;
                }
            }
            return non_black_holes > 0 && reached_count == non_black_holes;
        }

        bool Compact_Galaxy_Candidate(game::s_game_state& state) {
            if (std::ssize(current_state.stars) < 0 || std::ssize(current_state.stars) > MAX_STARS) return false;
            int16_t star_remap[MAX_STARS];
            memset(star_remap, 0xff, sizeof(star_remap));

            int16_t next_star = 0;
            for (int16_t star_id = 0; star_id < std::ssize(current_state.star_order);
                 ++star_id) {
                const auto star_id_ = current_state.star_order[star_id];
                auto& star_record = current_state.stars.at(star_id_);
                if (star_record.spectral_class < STAR_CLASS_B ||
                    star_record.spectral_class >
                        STAR_CLASS_BLACK_HOLE) {
                    continue;
                }
                star_remap[star_id] = next_star;
                ++next_star;
            }

            std::erase_if(state.planets, [&](const auto& entry) {
                const auto star = game::Star_Row(current_state, entry.second.star_id);
                return star < 0 || star >= MAX_STARS || star_remap[star] < 0;
            });
            std::erase_if(state.planet_order, [&](auto id) { return !state.planets.contains(id); });
            for (auto& [id, planet] : state.planets) {
                if (planet.colony_id != game::NO_COLONY && !game::current_state.colonies.contains(planet.colony_id)) return false;
            }

            for (int16_t star_id = 0; star_id < std::ssize(current_state.stars); ++star_id) {
                if (star_remap[star_id] < 0) continue;
                const auto& star = state.stars.at(game::Star_Key_At_Row(state, star_id));
                const auto live_or_none = [&](game::id_star id) {
                    const auto position = game::Star_Row(state, id);
                    return id == game::NO_STAR || (position >= 0 && star_remap[position] >= 0);
                };
                if (!live_or_none(star.wormhole_star_id) || !live_or_none(star.next_wfi_in_list) ||
                    !std::ranges::all_of(star.relocate_ship_to, live_or_none)) return false;
            }
            for (int16_t ship_id = 0; ship_id < static_cast<int16_t>(state.ship_slots.size());
                 ++ship_id) {
                if (state.ship_slots[ship_id] == ::game::NO_SHIP) continue;
                s_ship_data& ship = ship_slots::At(state, ship_id);
                const auto orbit = game::Ship_Orbit_Row(state, ship.position);
                if (orbit >= 0) {
                    if (orbit >= MAX_STARS ||
                        star_remap[orbit] < 0) {
                        return false;
                    }
                    const auto& star = state.stars.at(ship.position.star_id);
                    ship.x = star.x;
                    ship.y = star.y;
                }
                const auto redirect = ship.prior_jump_gate_destination;
                if (redirect != game::NO_STAR) {
                    const auto position = game::Star_Row(state, redirect);
                    if (position < 0 || star_remap[position] < 0) return false;
                }
            }
            for (auto& leader : state.leaders) {
                if (leader.status == 5 ||
                    (leader.status == 1 &&
                        leader.type == LEADER_TYPE_COLONY)) {
                    const auto position = game::Leader_Assignment_Row(state, leader);
                    if (position < 0 || position >= MAX_STARS || star_remap[position] < 0) {
                        return false;
                    }
                    if (leader.star_location == game::NO_STAR) leader.Set_Star_Location(game::Star_Key_At_Row(state, position));
                }
            }
            for (int position = 0; position < std::ssize(state.star_order); ++position) {
                if (star_remap[position] < 0) state.stars.erase(game::Star_Key_At_Row(state, position));
            }
            std::erase_if(state.star_order, [&](auto id) { return !state.stars.contains(id); });
            state.star_order_sorted = std::is_sorted(state.star_order.begin(), state.star_order.end());
            int16_t next_nebula = 0;
            for (int16_t nebula_id = 0;
                 nebula_id < MOX::NUM_NEBULAS;
                 ++nebula_id) {
                if (MOX::nebula[nebula_id].type < 0) {
                    continue;
                }
                if (next_nebula != nebula_id) {
                    MOX::nebula[next_nebula] = MOX::nebula[nebula_id];
                }
                ++next_nebula;
            }
            MOX::NUM_NEBULAS = static_cast<uint8_t>(next_nebula);
            return true;
        }

        void Refresh_Configured_Planet(s_planet_data& planet) {
            planet.food_per_farmer = MAPGEN::Generate_Food_Per_Farmer(planet, game::Star_Row(current_state, planet.star_id));
            if ((planet.environmental_alterations & 0x2) != 0) {
                planet.food_per_farmer += COLCALC::food_farmer_bonus.soil_enrichment;
            }
            planet.max_farms = MAPGEN::Generate_Max_Farms(planet, game::Star_Row(current_state, planet.star_id));
        }

        bool Transform_Generated_Planets(game::s_game_state& state) {
            bool colony_state_changed = false;
            for (const auto id : state.planet_order) {
                auto* planet = &state.planets.at(id);
                s_star_data* star = &game::current_state.stars.at(planet->star_id);
                const int8_t original_climate = planet->climate;
                const int8_t original_mineral_class = planet->mineral_class;
                const int8_t original_gravity_class = planet->gravity_class;
                const int8_t original_size = planet->size;
                const int8_t original_special = planet->planet_special;

                if (MAPGEN::planet_generation_config.convert_toxic_to_radiated &&
                    planet->climate == PLANET_CLIMATE_TOXIC) {
                    planet->climate = PLANET_CLIMATE_RADIATED;
                }
                if (MAPGEN::planet_generation_config.improve_ultra_poor_minerals &&
                    planet->mineral_class == MINERAL_RESOURCE_ULTRA_POOR) {
                    planet->mineral_class = planet->size == PLANET_SIZE_HUGE ? MINERAL_RESOURCE_POOR : MINERAL_RESOURCE_ABUNDANT;
                }
                if (planet->colony_id == game::NO_COLONY &&
                    MAPGEN::planet_generation_config.normalize_low_gravity &&
                    planet->gravity_class == PLANET_GRAVITY_LOW_G) {
                    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
                }
                if (planet->colony_id == game::NO_COLONY &&
                    MAPGEN::planet_generation_config.normalize_heavy_gravity &&
                    planet->gravity_class == PLANET_GRAVITY_HEAVY_G) {
                    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
                }
                if (MAPGEN::planet_generation_config.upgrade_tiny_planets &&
                    planet->size == PLANET_SIZE_TINY) {
                    if (MAPGEN::planet_generation_config.upgrade_small_planets) {
                        planet->size = PLANET_SIZE_MEDIUM;
                    } else {
                        planet->size = (int8_t)game_random::Random(PLANET_SIZE_LARGE);
                        if (planet->size == PLANET_SIZE_LARGE &&
                            planet->mineral_class == MINERAL_RESOURCE_RICH) {
                            planet->size = PLANET_SIZE_MEDIUM;
                        }
                    }
                }
                if (MAPGEN::planet_generation_config.upgrade_small_planets &&
                    planet->size == PLANET_SIZE_SMALL) {
                    planet->size = PLANET_SIZE_MEDIUM;
                }
                if (MAPGEN::special_generation_config.replace_splinter_colonies_with_gems &&
                    planet->planet_special == SYSTEM_SPECIAL_SPLINTER_COLONY) {
                    planet->planet_special = SYSTEM_SPECIAL_GEM_DEPOSITS;
                    star->system_special = SYSTEM_SPECIAL_GEM_DEPOSITS;
                }
                if (MAPGEN::special_generation_config.replace_natives_with_gold &&
                    planet->planet_special == SYSTEM_SPECIAL_NATIVES) {
                    planet->planet_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;
                    star->system_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;
                }
                const bool planet_state_changed = planet->climate != original_climate ||
                    planet->mineral_class != original_mineral_class ||
                    planet->gravity_class != original_gravity_class ||
                    planet->size != original_size;
                if (planet_state_changed) {
                    Refresh_Configured_Planet(*planet);
                }
                colony_state_changed |= planet->colony_id != game::NO_COLONY &&
                    (planet_state_changed || planet->planet_special != original_special);
            }
            return colony_state_changed;
        }

        void Transform_Marooned_Heroes() {
            if (MAPGEN::special_generation_config.highlight_marooned_heroes) {
                for (auto& [key, star] : current_state.stars) {
                    if (star.system_special == SYSTEM_SPECIAL_MAROONED_HERO) {
                        star.spectral_class = STAR_CLASS_DWARF;
                    } else if (star.spectral_class == STAR_CLASS_DWARF) {
                        star.spectral_class = STAR_CLASS_M;
                    }
                }
            }

            if (MAPGEN::special_generation_config.remove_marooned_heroes) {
                for (auto& leader : current_state.leaders) {
                    const auto star = current_state.stars.find(leader.star_location);
                    if (leader.status == 5 && star != current_state.stars.end()) {
                        star->second.system_special = SYSTEM_SPECIAL_PIRATE_CACHE;
                        leader.Clear_Location();
                        leader.status = -1;
                    }
                }
            }
        }

        bool Transform_Monster_Guarded_Planets(game::s_game_state& state) {
            bool colony_state_changed = false;
            for (int16_t ship_idx = 0; ship_idx < static_cast<int16_t>(game::current_state.ship_slots.size()); ++ship_idx) {
                if (game::current_state.ship_slots[ship_idx] == ::game::NO_SHIP) continue;
                const s_ship_data* ship = &ship_slots::At(game::current_state, ship_idx);
                const auto orbit = game::Ship_Orbit_Row(current_state, ship->position);
                if (factions::Legacy_Code(ship->owner) < NONPLAYER_SHIP_TYPE_AMOEBA ||
                    factions::Legacy_Code(ship->owner) > NONPLAYER_SHIP_TYPE_HYDRA ||
                    orbit < 0 || orbit >= std::ssize(current_state.stars)) {
                    continue;
                }
                const auto* star = &game::current_state.stars.at(ship->position.star_id);
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const auto it = state.planets.find(star->orbit_planet_ids[orbit]);
                    if (it == state.planets.end()) continue;
                    auto* planet = &it->second;
                    const int8_t original_climate = planet->climate;
                    const int8_t original_mineral_class = planet->mineral_class;
                    const int8_t original_gravity_class = planet->gravity_class;
                    const int8_t original_size = planet->size;
                    if (MAPGEN::monster_generation_config.terraform_guarded_natives &&
                        planet->planet_special == SYSTEM_SPECIAL_NATIVES &&
                        planet->climate < PLANET_CLIMATE_OCEAN) {
                        planet->climate = PLANET_CLIMATE_SWAMP;
                    }
                    if (MAPGEN::monster_generation_config.recalculate_guarded_planet_gravity &&
                        planet->mineral_class >= MINERAL_RESOURCE_RICH) {
                        if ((planet->mineral_class == MINERAL_RESOURCE_ULTRA_RICH &&
                                planet->size >= PLANET_SIZE_MEDIUM) ||
                            (planet->mineral_class == MINERAL_RESOURCE_RICH &&
                                planet->size >= PLANET_SIZE_LARGE)) {
                            planet->gravity_class = PLANET_GRAVITY_HEAVY_G;
                        } else {
                            planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
                        }
                    }
                    if (MAPGEN::monster_generation_config.improve_guarded_planets) {
                        planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
                        if (planet->mineral_class >= MINERAL_RESOURCE_RICH &&
                            planet->climate < PLANET_CLIMATE_TERRAN) {
                            planet->climate = PLANET_CLIMATE_TERRAN;
                        }
                    }
                    const bool planet_state_changed = planet->climate != original_climate ||
                        planet->mineral_class != original_mineral_class ||
                        planet->gravity_class != original_gravity_class ||
                        planet->size != original_size;
                    if (planet_state_changed) {
                        Refresh_Configured_Planet(*planet);
                    }
                    colony_state_changed |= planet->colony_id != game::NO_COLONY && planet_state_changed;
                }
            }
            return colony_state_changed;
        }

        void Transform_Home_Systems(game::s_game_state& state) {
            const bool flatten = MAPGEN::home_system_config.normalize_unoccupied_planets ||
                MAPGEN::home_system_config.use_fixed_unoccupied_planets;
            for (int16_t player_idx = 0; player_idx < state.player_count; ++player_idx) {
                s_player* player = &state.players[player_idx];
                const auto* home = &state.planets.at(player->home_planet_id);
                s_star_data* star = &game::current_state.stars.at(home->star_id);
                int unoccupied_index = 0;
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const auto it = state.planets.find(star->orbit_planet_ids[orbit]);
                    if (it == state.planets.end()) continue;
                    auto* planet = &it->second;
                    if (planet->colony_id != game::NO_COLONY) continue;
                    const int8_t original_climate = planet->climate;
                    const int8_t original_mineral_class = planet->mineral_class;
                    const int8_t original_gravity_class = planet->gravity_class;
                    const int8_t original_size = planet->size;

                    const int8_t normal_gravity = home->gravity_class == PLANET_GRAVITY_LOW_G ? PLANET_GRAVITY_LOW_G : PLANET_GRAVITY_NORMAL_G;
                    if (flatten) {
                        planet->mineral_class = MINERAL_RESOURCE_ABUNDANT;
                        if (planet->climate < PLANET_CLIMATE_BARREN) {
                            planet->climate = PLANET_CLIMATE_BARREN;
                        }
                    }
                    if (unoccupied_index == 0) {
                        if (flatten) {
                            planet->gravity_class = home->gravity_class;
                            planet->size = PLANET_SIZE_LARGE;
                            if (MAPGEN::home_system_config.use_fixed_unoccupied_planets) {
                                planet->climate = PLANET_CLIMATE_SWAMP;
                            }
                        }
                        if (MAPGEN::home_system_config.minimum_starting_planet_production &&
                            planet->mineral_class < MINERAL_RESOURCE_ABUNDANT) {
                            planet->mineral_class = MINERAL_RESOURCE_ABUNDANT;
                        }
                        if (MAPGEN::home_system_config.ultra_rich_starting_planet) {
                            planet->mineral_class = MINERAL_RESOURCE_ULTRA_RICH;
                            planet->gravity_class = home->gravity_class;
                        } else if (MAPGEN::home_system_config.rich_starting_planet) {
                            planet->mineral_class = MINERAL_RESOURCE_RICH;
                            planet->gravity_class = home->gravity_class;
                        }
                        if (MAPGEN::home_system_config.huge_starting_planet) {
                            planet->size = PLANET_SIZE_HUGE;
                            planet->gravity_class = home->gravity_class;
                        }
                        if (MAPGEN::home_system_config.improve_first_unoccupied_planet ||
                            MAPGEN::home_system_config.make_first_unoccupied_planet_rich) {
                            planet->mineral_class = MINERAL_RESOURCE_RICH;
                        }
                        if (MAPGEN::home_system_config.improve_first_unoccupied_planet ||
                            MAPGEN::home_system_config.make_first_unoccupied_planet_huge) {
                            planet->size = PLANET_SIZE_HUGE;
                        }
                        if (MAPGEN::home_system_config.improve_first_unoccupied_planet) {
                            planet->climate = PLANET_CLIMATE_TERRAN;
                        }
                    } else if (flatten && unoccupied_index == 1) {
                        planet->gravity_class = normal_gravity;
                        planet->size = PLANET_SIZE_LARGE;
                        if (MAPGEN::home_system_config.use_fixed_unoccupied_planets) {
                            planet->climate = PLANET_CLIMATE_ARID;
                        }
                    } else if (flatten && unoccupied_index == 2) {
                        planet->gravity_class = normal_gravity;
                        planet->size = PLANET_SIZE_LARGE;
                        if (MAPGEN::home_system_config.use_fixed_unoccupied_planets) {
                            planet->climate = PLANET_CLIMATE_TUNDRA;
                            planet->mineral_class = MINERAL_RESOURCE_POOR;
                        }
                    } else if (unoccupied_index == 3) {
                        if (flatten) {
                            planet->gravity_class = normal_gravity;
                            planet->size = PLANET_SIZE_MEDIUM;
                            if (MAPGEN::home_system_config.use_fixed_unoccupied_planets) {
                                planet->climate = PLANET_CLIMATE_TERRAN;
                                planet->mineral_class = MINERAL_RESOURCE_POOR;
                            }
                        }
                        if (MAPGEN::home_system_config.improve_fourth_unoccupied_planet) {
                            planet->mineral_class = MINERAL_RESOURCE_POOR;
                            if (player->traits[TRAIT_CREATIVE] ||
                                player->traits[TRAIT_AQUATIC] ||
                                player->traits[TRAIT_ENVIRONMENT_IMMUNE]) {
                                planet->climate = PLANET_CLIMATE_TERRAN;
                                planet->size = PLANET_SIZE_MEDIUM;
                            } else {
                                planet->climate = PLANET_CLIMATE_GAIA;
                                planet->size = PLANET_SIZE_SMALL;
                            }
                        }
                    }
                    const bool planet_state_changed = planet->climate != original_climate ||
                        planet->mineral_class != original_mineral_class ||
                        planet->gravity_class != original_gravity_class ||
                        planet->size != original_size;
                    if (planet_state_changed) {
                        Refresh_Configured_Planet(*planet);
                    }
                    ++unoccupied_index;
                }
            }
        }

        bool Transform_Lua_Home_Systems(game::s_game_state& state, bool* succeeded) {
            *succeeded = false;
            if (!Valid_Generated_Map(state)) {
                exitm::Exit_With_Message(
                    "invalid map before Lua home-system setup");
            }
            auto draft = std::make_unique<game::s_game_state>(state);
            auto& staged = *draft;
            staged.stars = current_state.stars;
            staged.star_order = current_state.star_order;
            staged.star_order_sorted = current_state.star_order_sorted;
            bool colony_state_changed = false;
            {
                s_generated_map_scope scope(staged);
                for (int16_t player_idx = 0;
                     player_idx < staged.player_count;
                     ++player_idx) {
                    const auto home_planet = staged.players[player_idx].home_planet_id;
                    const orion2re::lua::game::s_home_system_generated_event
                        event = {
                            staged.players[player_idx].id,
                            static_cast<int16_t>(game::Star_Row(current_state, staged.planets.at(home_planet).star_id)),
                        };
                    orion2re::lua::game::Home_System_Generated(&event);
                }
                for (const auto id : staged.planet_order) {
                    const orion2re::lua::game::s_planet_generated_event event = {id};
                    orion2re::lua::game::Planet_Generated(&event);
                }
                if (!Valid_Generated_Map(staged)) {
                    exitm::Exit_With_Message(
                        "Lua home-system setup produced an invalid map");
                }
                for (const auto id : staged.planet_order) {
                    s_planet_data& planet = staged.planets.at(id);
                    const s_planet_data& original = state.planets.at(id);
                    const bool economy_changed =
                        planet.size != original.size ||
                        planet.climate != original.climate ||
                        planet.mineral_class != original.mineral_class ||
                        planet.gravity_class != original.gravity_class ||
                        planet.planet_special != original.planet_special ||
                        game::current_state.stars.at(planet.star_id).system_special !=
                            staged.stars.at(planet.star_id).system_special;
                    Refresh_Configured_Planet(planet);
                    colony_state_changed |= planet.colony_id != game::NO_COLONY &&
                        economy_changed;
                }
            }
            state = std::move(staged);
            *succeeded = true;
            return colony_state_changed;
        }

        void Reveal_Starting_Area(game::s_game_state& state) {
            const uint32_t radius = MAPGEN::starting_reveal_radius;
            if (radius == 0) return;

            const uint32_t radius_sq = radius * radius * 900;
            for (int16_t player_idx = 0; player_idx < state.player_count; ++player_idx) {
                const auto home_planet = state.players[player_idx].home_planet_id;
                const s_star_data* home = &state.stars.at(state.planets.at(home_planet).star_id);
                for (auto& [key, star] : state.stars) {
                    const int32_t dx = star.x - home->x;
                    const int32_t dy = star.y - home->y;
                    if ((uint32_t)(dx * dx + dy * dy) <= radius_sq) {
                        star.visited.set_slot(player_idx);
                    }
                }
            }
        }

    } // namespace

    bool __cdecl Finalize_Configured_New_Game(bool* succeeded) {
        bool local_succeeded = false;
        if (succeeded == nullptr) succeeded = &local_succeeded;
        *succeeded = false;
        auto draft = std::make_unique<game::s_game_state>(current_state);
        auto& staged = *draft;
        bool colony_state_changed = Transform_Generated_Planets(staged);
        Transform_Marooned_Heroes();
        colony_state_changed |= Transform_Monster_Guarded_Planets(staged);
        Transform_Home_Systems(staged);
        bool lua_succeeded = false;
        colony_state_changed |= Transform_Lua_Home_Systems(staged, &lua_succeeded);
        if (!lua_succeeded) return false;
        Reveal_Starting_Area(staged);
        current_state.stars = std::move(staged.stars);
        current_state.star_order = std::move(staged.star_order);
        current_state.star_order_sorted = staged.star_order_sorted;
        current_state.planets = std::move(staged.planets);
        current_state.planet_order = std::move(staged.planet_order);
        current_state.next_planet_id = staged.next_planet_id;
        *succeeded = true;
        return colony_state_changed;
    }

    bool __cdecl Apply_Lua_Galaxy_Generator(std::string* diagnostic) {
        std::string local_diagnostic;
        if (diagnostic == nullptr) diagnostic = &local_diagnostic;
        diagnostic->clear();
        if (!orion2re::lua::game::s_network_new_game_host_scope::Allows_Setup(
                MOX::game_type) ||
            !orion2re::lua::Has_Active_Galaxy_Generator()) {
            return true;
        }

        auto draft = std::make_unique<game::s_game_state>(current_state);
        auto& candidate_state = *draft;
        std::vector<s_nebula> nebulas(
            MOX::nebula, MOX::nebula + MAX_NEBULAS);
        uint64_t ship_generations[MAX_SHIPS];
        const std::uint64_t topology_generation =
            orion2re::lua::game::internal::Topology_Generation();
        if (!orion2re::lua::game::Capture_Ship_Generation_State(
                ship_generations, MAX_SHIPS)) {
            *diagnostic = "unable to capture galaxy ship identities";
            return false;
        }
        for (uint64_t& generation : ship_generations) {
            if (generation == UINT64_MAX) {
                *diagnostic = "galaxy ship identity generation exhausted";
                return false;
            }
            generation = generation == 0 ? 1 : generation + 1;
        }

        bool invoked = false;
        uint8_t candidate_nebula_count = MOX::NUM_NEBULAS;
        bool valid = false;
        orion2re::lua::game::internal::Advance_Topology_Generation();
        {
            s_galaxy_candidate_scope scope(candidate_state,
                nebulas.data());
            orion2re::lua::game::s_ship_generation_scope generation_scope(
                ship_generations);
            if (!orion2re::lua::Dispatch_Active_Galaxy_Generator(
                    &invoked, diagnostic)) {
                orion2re::lua::game::internal::Restore_Topology_Generation(
                    topology_generation);
                return false;
            }
            valid = invoked && Compact_Galaxy_Candidate(current_state) && Valid_Galaxy_Candidate(current_state);
            if (valid) {
                for (auto& [id, planet] : current_state.planets) Refresh_Configured_Planet(planet);
                GEO::Initialize_Star_In_Nebula_Info();
                candidate_nebula_count = MOX::NUM_NEBULAS;
            }
        }
        orion2re::lua::game::internal::Restore_Topology_Generation(
            topology_generation);
        if (!valid) {
            if (diagnostic->empty()) {
                *diagnostic =
                    "galaxy_generator produced invalid construction state";
            }
            return false;
        }
        for (uint64_t& generation : ship_generations) {
            if (generation == UINT64_MAX) {
                *diagnostic = "galaxy ship identity generation exhausted";
                return false;
            }
            ++generation;
        }

        current_state = std::move(candidate_state);
        memcpy(MOX::nebula,
            nebulas.data(),
            MAX_NEBULAS * sizeof(s_nebula));
        MOX::NUM_NEBULAS = candidate_nebula_count;
        orion2re::lua::game::Restore_Ship_Generation_State(
            ship_generations, MAX_SHIPS);
        orion2re::lua::game::Advance_Ship_World();
        orion2re::lua::game::internal::Advance_Topology_Generation();
        orion2re::lua::game::internal::Advance_Topology_Generation();
        return true;
    }
} // namespace MAPGEN
