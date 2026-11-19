#include "pch.h"

#include "lua/lua_game.h"
#include "lua/lua_game_internal.h"
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
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            for (int j = 0; j < 5; ++j) {
                MOX::star[i].planet_index[j] = ::game::NO_PLANET;
            }
        }
    }

    /**
     * @brief Determines the base food production per farmer for a given planet.
     *
     * Planets in systems with non-habitable spectral classes produce no food
     * unless the system contains the Orion star special.
     */
    uint8_t __cdecl Generate_Food_Per_Farmer(::game::s_world_state* world_state, int16_t star_index, ::game::id_planet planet_index) {
        bool habitable = HAROLD::Habitable_Spectral_Class(star_index);

        // Check if the system is either habitable or is the Orion system
        if (!habitable && MOX::star[star_index].system_special != SYSTEM_SPECIAL_ORION_STAR) {
            return 0;
        }

        int8_t climate_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index))).climate;

        return MOX::food_per_farmer_table[climate_idx];
    }

    uint8_t __cdecl Generate_Max_Farms(::game::s_world_state* world_state, int16_t star_idx, ::game::id_planet planet_idx) {
        bool is_habitable = HAROLD::Habitable_Spectral_Class(star_idx);

        if (!is_habitable && MOX::star[star_idx].system_special != SYSTEM_SPECIAL_ORION_STAR) {
            return 0;
        }

        int8_t size_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).size;
        return MOX::planet_max_farms[size_idx];
    }

    int8_t __cdecl Generate_Orbit(int16_t star_id) {
        uint8_t weights[5];
        bool available_orbit_has_weight = false;

        for (int i = 0; i < 5; ++i) {
            weights[i] = satellite_orbit_weight[i][MOX::settings.galaxy_age];
            if (MOX::star[star_id].planet_index[i] == ::game::NO_PLANET && weights[i] != 0) {
                available_orbit_has_weight = true;
            }
        }

        if (available_orbit_has_weight) {
            int16_t choice;
            do {
                choice = RUSS::Get_Weighted_Choice_Char(weights, 5);
            } while (choice >= 0 && MOX::star[star_id].planet_index[choice] != ::game::NO_PLANET);
            if (choice >= 0) return (int8_t)choice;
        }

        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            if (MOX::star[star_id].planet_index[orbit] == ::game::NO_PLANET) return (int8_t)orbit;
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

    int8_t Generate_Mineral_Class(int16_t star_idx, game::id_planet unused_planet_idx) {
        (void)unused_planet_idx;
        int32_t random_roll = game_random::Random(10);
        int8_t spectral_class = MOX::star[star_idx].spectral_class;

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
                    int8_t spectral = MOX::star[star_idx].spectral_class;

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
    void Generate_Satellite(game::s_world_state* world_state, game::id_planet planet_idx, int16_t star_idx, int16_t orbit, int16_t type) {
        auto& planet = game::Planet(world_state, legacy_world::Planet_Id(planet_idx));
        planet.parent_star = MOX::star.Id_At(star_idx);
        planet.orbit = static_cast<int8_t>(orbit);
        planet.planet_type = static_cast<int8_t>(type);
        planet.size = Generate_Size();
        planet.mineral_class = Generate_Mineral_Class(star_idx, legacy_world::Planet_Id(planet_idx));
        Get_Climate_Modifier(world_state, planet.mineral_class);
        planet.gravity_class = Generate_Gravity_Class(world_state, legacy_world::Planet_Id(planet_idx));
        planet.group = static_cast<int8_t>(Get_Planet_Group(world_state, legacy_world::Planet_Id(planet_idx)));
        planet.climate = Generate_Climate(planet.group);
        planet.climate_bg_type = static_cast<int8_t>(game_random::Random(3) - 1);
        Set_Secondary_Planet_Data(world_state, star_idx, legacy_world::Planet_Id(planet_idx));
    }

    void __cdecl Set_Secondary_Planet_Data(::game::s_world_state* world_state, int16_t star_idx, ::game::id_planet planet_idx) {
        s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));

        planet->max_farms = Generate_Max_Farms(world_state, star_idx, legacy_world::Planet_Id(planet_idx));
        planet->food_per_farmer = Generate_Food_Per_Farmer(world_state, star_idx, legacy_world::Planet_Id(planet_idx));
        planet->colony_index = -1;
    }

    int8_t __cdecl Get_Climate_Modifier(::game::s_world_state* world_state, int8_t climate_index) {
        int8_t modifier = 0;

        if (climate_index >= MINERAL_RESOURCE_ULTRA_POOR && climate_index < MINERAL_RESOURCE_COUNT) {
            modifier = MOX::climate_modifier_table[climate_index];

            if (modifier > 0) {
                RUSS::PFMT(0, 0, "------CLIMATE modifier is %d  Planet: %d  Mineral: %d", modifier, ::game::Planet_Count(world_state), climate_index);
            }
        }

        return modifier;
    }

    int8_t __cdecl Generate_Gravity_Class(::game::s_world_state* world_state, ::game::id_planet planet_idx) {
        s_planet_data& planet = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));

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

    int16_t __cdecl Get_Planet_Group(::game::s_world_state* world_state, ::game::id_planet planet_index) {
        s_planet_data& planet = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index)));
        int16_t orbit = (int16_t)planet.orbit;
        const auto star_index = planet.Star_Position(MOX::star);
        int16_t spectral_class = (int16_t)MOX::star[star_index].spectral_class;

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

    void Enforce_Configured_Planet_Max(game::s_world_state* world_state) {
        if (MOX::NUM_STARS <= ORIGINAL_MAX_STARS) {
            if (planet_limit == 0) return;
            Enforce_Planet_Max(world_state, planet_limit);
        } else {
            Enforce_Planet_Max(world_state, MAX_PLANETS);
        }
    }

    void Enforce_Planet_Max(game::s_world_state* world_state, int16_t max_planets) {
        if (max_planets > game::Planet_Count(world_state)) return;
        std::array<bool, MAX_STARS> home_systems{};
        for (int16_t player = 0; player < MOX::NUM_PLAYERS; ++player) {
            const auto* home = game::Find_Planet(world_state, legacy_world::Planet_Id(MOX::player[player].home_planet_id));
            const auto position = home != nullptr ? home->Star_Position(MOX::star) : -1;
            if (position >= 0) home_systems[position] = true;
        }
        std::vector<game::id_planet> removable;
        for (const auto& [id, planet] : world_state->planets) {
            const auto position = planet.Star_Position(MOX::star);
            if (position >= 0 && (!special_generation_config.generate_orion || position != 0) &&
                !home_systems[position] && !HOMEGEN::Is_Added_Food_World(position)) {
                removable.push_back(id);
            }
        }
        // Preserve the native threshold and random-selection order, not array compaction.
        while (max_planets <= game::Planet_Count(world_state) && !removable.empty()) {
            const size_t choice = static_cast<size_t>(game_random::Random(static_cast<int32_t>(removable.size())) - 1);
            const auto id = removable[choice];
            removable[choice] = removable.back();
            removable.pop_back();
            const auto planet = game::Planet(world_state, legacy_world::Planet_Id(id));
            MOX::star[planet.Star_Position(MOX::star)].planet_index[planet.orbit] = game::NO_PLANET;
            game::Erase_Planet(world_state, legacy_world::Planet_Id(id));
        }
    }

    void __cdecl Get_A_Planet_Or_Star_Special(::game::s_world_state* world_state, int16_t star_idx, int16_t* wormhole_out, int8_t* special_out, uint8_t commit_changes, int32_t excluded_mask) {
        uint16_t valid_planets[5] = {0};
        uint16_t large_planets[5] = {0};
        uint16_t habitable_planets[5] = {0};
        bool has_large = false;
        bool has_habitable = false;

        for (int i = 0; i < 5; ++i) {
            ::game::id_planet p_idx = MOX::star[star_idx].planet_index[i];
            if (p_idx != ::game::NO_PLANET) {
                s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx)));
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
                            MOX::star[star_idx].system_special = (int8_t)choice;
                        }
                        success = true;
                        break;

                    case SYSTEM_SPECIAL_GOLD_DEPOSITS:
                    case SYSTEM_SPECIAL_GEM_DEPOSITS: {
                        const int16_t p_idx = RUSS::Get_Weighted_Choice_Short(valid_planets, 5);
                        if (p_idx >= 0) {
                            if (commit_changes) {
                                const auto actual_p_idx = MOX::star[star_idx].planet_index[p_idx];
                                ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(actual_p_idx))).planet_special = (int8_t)choice;
                                MOX::star[star_idx].system_special = (int8_t)choice;
                            }
                            success = true;
                        }
                        break;
                    }

                    case SYSTEM_SPECIAL_NATIVES:
                    case SYSTEM_SPECIAL_SPLINTER_COLONY: {
                        const int16_t p_idx = RUSS::Get_Weighted_Choice_Short(large_planets, 5);
                        if (p_idx >= 0) {
                            if (commit_changes) {
                                const auto actual_p_idx = MOX::star[star_idx].planet_index[p_idx];
                                ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(actual_p_idx))).planet_special = (int8_t)choice;
                                MOX::star[star_idx].system_special = (int8_t)choice;
                            }
                            success = true;
                        }
                        break;
                    }

                    case SYSTEM_SPECIAL_MAROONED_HERO:
                        n_marooned_heroes++; // Increment global attempt counter
                        if (commit_changes) {
                            MOX::star[star_idx].system_special = (int8_t)choice;
                        }
                        success = true;
                        break;

                    case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS: {
                        const int16_t p_idx = RUSS::Get_Weighted_Choice_Short(habitable_planets, 5);
                        if (p_idx >= 0) {
                            if (commit_changes) {
                                const auto actual_p_idx = MOX::star[star_idx].planet_index[p_idx];
                                ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(actual_p_idx))).planet_special = (int8_t)choice;
                                MOX::star[star_idx].system_special = (int8_t)choice;
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

    void __cdecl Generate_Star_Special(int16_t star_idx, int16_t* wormhole_out, uint8_t* special_counts) {
        int8_t special_type = SYSTEM_SPECIAL_NO_SPECIAL;

        if (game_random::Random(100) > MOX::planet_special) {
            const int32_t excluded_mask = special_generation_config.generate_splinter_colonies ? 0 : 1 << SYSTEM_SPECIAL_SPLINTER_COLONY;
            Get_A_Planet_Or_Star_Special(&::game::world, 
                star_idx, wormhole_out, &special_type, 1, excluded_mask);
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

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            for (int16_t planet_idx_position = 0; planet_idx_position < 5; ++planet_idx_position) {
                if (MOX::star[star_idx].planet_index[planet_idx_position] != ::game::NO_PLANET) {
                    if (HAROLD::Planet_Type(&::game::world, star_idx, planet_idx_position) == PLANET_TYPE_GAS_GIANT) {
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

        if (MOX::star[star_idx].spectral_class < STAR_CLASS_BLACK_HOLE) {
            int32_t row_index = random_val - 1;
            int8_t spectral_class = MOX::star[star_idx].spectral_class;
            result = (int16_t)MOX::class_to_num_satellites[row_index][spectral_class];
        }

        return result;
    }

    void Planet_Generation(game::s_world_state* world_state, int16_t star_idx) {
        const int16_t orbit = Generate_Orbit(star_idx);
        const int16_t type = Generate_Satellite_Type(star_idx, orbit);
        if (type != MAPGEN_SATELLITE_ASTEROID && type != MAPGEN_SATELLITE_GAS_GIANT && type != MAPGEN_SATELLITE_PLANET) return;
        auto id = game::Create_Planet(world_state, s_planet_data{});
        if (!id) return;
        MOX::star[star_idx].planet_index[orbit] = *id;
        Generate_Satellite(world_state, legacy_world::Planet_Id(*id), star_idx, orbit, type);
        ++n_planets;
    }

    int16_t __cdecl Home_Planet(::game::id_planet planet_id) {
        for (int16_t i = 0; i < (int16_t)MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].home_planet_id == planet_id) {
                return i;
            }
        }
        return -1;
    }

    namespace {

        bool Valid_Generated_Map(::game::s_world_state* world_state) {
            if (MOX::star == nullptr ||
                MOX::player == nullptr || ::game::Planet_Count(world_state) < 0 ||
                ::game::Planet_Count(world_state) > MAX_PLANETS || MOX::NUM_STARS < 0 ||
                MOX::NUM_STARS > MAX_STARS || MOX::NUM_PLAYERS < 0 ||
                MOX::NUM_PLAYERS > MAX_PLAYERS) {
                return false;
            }
            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS;
                 ++star_idx) {
                const s_star_data& star = MOX::star[star_idx];
                if (star.system_special < SYSTEM_SPECIAL_NO_SPECIAL ||
                    star.system_special >= SYSTEM_SPECIAL_COUNT) {
                    return false;
                }
                for (int16_t orbit = 0; orbit < 5; ++orbit) {
                    const ::game::id_planet planet_idx = star.planet_index[orbit];
                    if (planet_idx == ::game::NO_PLANET) continue;
                    if (planet_idx == ::game::NO_PLANET || (::game::Find_Planet(world_state, legacy_world::Planet_Id(planet_idx)) == nullptr) ||
                        game::Planet(world_state, planet_idx).parent_star != MOX::star.Id_At(star_idx) ||
                        ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).orbit != orbit) {
                        return false;
                    }
                }
            }
            for (const auto& planet_entry : world_state->planets) {
                const auto planet_idx = planet_entry.first;
                const s_planet_data& planet = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));
                const auto position = planet.Star_Position(MOX::star);
                if (position < 0 ||
                    position >= MOX::NUM_STARS || planet.orbit < 0 ||
                    planet.orbit >= 5 ||
                    MOX::star[position]
                            .planet_index[planet.orbit] != planet_idx ||
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
                    MOX::star[position].system_special !=
                        planet.planet_special) {
                    return false;
                }
            }
            for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS;
                 ++player_idx) {
                const auto home_planet_idx =
                    MOX::player[player_idx].home_planet_id;
                if (game::Find_Planet(world_state, home_planet_idx) == nullptr) {
                    return false;
                }
            }
            return true;
        }

        struct s_staged_stars {
            game::s_star_slots live = MOX::star;
            game::s_world_state candidate;
            std::vector<s_star_data> fixture_records;
            game::s_star_slots view{&candidate, &MOX::NUM_STARS};

            explicit s_staged_stars(int16_t fixture_extent) {
                if (live.state != nullptr) {
                    candidate.stars = live.state->stars;
                    candidate.star_order = live.state->star_order;
                    candidate.vacant_star_positions = live.state->vacant_star_positions;
                    candidate.next_star_id = live.state->next_star_id;
                    candidate.antares = live.state->antares;
                } else {
                    fixture_records.assign(live.fixture, live.fixture + fixture_extent);
                    view = fixture_records.data();
                }
            }

            void Commit() {
                if (live.state != nullptr) {
                    live.state->stars = std::move(candidate.stars);
                    live.state->star_order = candidate.star_order;
                    live.state->vacant_star_positions = std::move(candidate.vacant_star_positions);
                    live.state->next_star_id = candidate.next_star_id;
                    live.state->antares = candidate.antares;
                } else {
                    std::copy(fixture_records.begin(), fixture_records.end(), live.fixture);
                }
            }
        };

        struct s_generated_map_scope {
            game::s_world_state* world;
            std::map<game::id_planet, s_planet_data>* planets;
            game::s_star_slots stars = MOX::star;
            s_generated_map_scope(game::s_world_state* state,
                std::map<game::id_planet, s_planet_data>* staged_planets, game::s_star_slots staged_stars)
                : world(state), planets(staged_planets) {
                world->planets.swap(*planets);
                MOX::star = staged_stars;
            }
            ~s_generated_map_scope() {
                world->planets.swap(*planets);
                MOX::star = stars;
            }
        };

        struct s_galaxy_candidate_scope {
            std::map<game::id_planet, s_planet_data>* candidate_planets;
            game::s_star_slots stars = MOX::star;
            s_player* players = MOX::player;
            game::s_colony_slots colonies = MOX::colony;
            s_ship_data* ships = MOX::ship;
            game::s_world_state* world_state;
            std::map<game::id_leader, s_leader_data> leaders;
            s_leader_data* candidate_leaders;
            s_nebula* nebulas = MOX::nebula;
            int16_t star_count = MOX::NUM_STARS;
            int16_t colony_count = MOX::NUM_COLONIES;
            int16_t ship_count = MOX::NUM_SHIPS;
            uint8_t nebula_count = MOX::NUM_NEBULAS;

            s_galaxy_candidate_scope(game::s_world_state* state, std::map<game::id_planet, s_planet_data>* staged_planets,
                game::s_star_slots candidate_stars,
                s_player* candidate_players,
                game::s_colony_slots candidate_colonies,
                s_ship_data* candidate_ships,
                s_leader_data* candidate_leaders,
                s_nebula* candidate_nebulas) : world_state(state), candidate_leaders(candidate_leaders) {
                candidate_planets = staged_planets;
                world_state->planets.swap(*candidate_planets);
                MOX::star = candidate_stars;
                MOX::player = candidate_players;
                MOX::colony = candidate_colonies;
                MOX::ship = candidate_ships;
                leaders.swap(world_state->leaders);
                game::Import_Leaders(world_state, {candidate_leaders, MAX_LEADERS});
                MOX::nebula = candidate_nebulas;
            }

            ~s_galaxy_candidate_scope() {
                world_state->planets.swap(*candidate_planets);
                MOX::star = stars;
                MOX::player = players;
                MOX::colony = colonies;
                MOX::ship = ships;
                const auto records = game::Export_Leaders(world_state);
                std::copy(records.begin(), records.end(), candidate_leaders);
                leaders.swap(world_state->leaders);
                MOX::nebula = nebulas;
                MOX::NUM_STARS = star_count;
                MOX::NUM_COLONIES = colony_count;
                MOX::NUM_SHIPS = ship_count;
                MOX::NUM_NEBULAS = nebula_count;
            }
        };

        bool Valid_Galaxy_Candidate(game::s_world_state* world_state) {
            if (!Valid_Generated_Map(world_state) || MOX::colony == nullptr ||
                MOX::ship == nullptr || world_state->leaders.size() != MAX_LEADERS ||
                MOX::nebula == nullptr || MOX::NUM_COLONIES < 0 ||
                MOX::NUM_COLONIES > MAX_COLONIES || MOX::NUM_SHIPS < 0 ||
                MOX::NUM_SHIPS > MAX_SHIPS ||
                MOX::NUM_NEBULAS > MAX_NEBULAS) {
                return false;
            }
            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS;
                 ++star_idx) {
                const s_star_data& star = MOX::star[star_idx];
                const auto wormhole = MOX::star.Position_Of(star.wormhole_star_id);
                if (star.x < 0 || star.x >= MOX::MAP_MAX_X || star.y < 0 ||
                    star.y >= MOX::MAP_MAX_Y ||
                    star.spectral_class < STAR_CLASS_B ||
                    star.spectral_class > STAR_CLASS_BLACK_HOLE ||
                    (star.wormhole_star_id != game::NO_STAR &&
                        (wormhole < 0 || wormhole >= MOX::NUM_STARS || wormhole == star_idx ||
                            MOX::star[wormhole].wormhole_star_id != MOX::star.Id_At(star_idx)))) {
                    return false;
                }
            }
            for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS;
                 ++player_idx) {
                const auto home_planet =
                    MOX::player[player_idx].home_planet_id;
                if (game::Find_Planet(world_state, home_planet) == nullptr ||
                    ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(home_planet))).planet_type !=
                        PLANET_TYPE_PLANET ||
                    ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(home_planet))).colony_index < 0 ||
                    ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(home_planet))).colony_index >=
                        MOX::NUM_COLONIES ||
                    MOX::colony[::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(home_planet))).colony_index]
                            .planet != home_planet ||
                    MOX::colony[::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(home_planet))).colony_index]
                            .owner != player_idx ||
                    MOX::star[game::Planet(world_state, home_planet).Star_Position(MOX::star)].owner !=
                        player_idx) {
                    return false;
                }
            }
            for (int16_t colony_idx = 0;
                 colony_idx < MOX::NUM_COLONIES;
                 ++colony_idx) {
                const s_colony& colony = MOX::colony[colony_idx];
                if (colony.owner < 0 && colony.planet == game::NO_PLANET) continue;
                if (colony.planet == ::game::NO_PLANET || (::game::Find_Planet(world_state, legacy_world::Planet_Id(colony.planet)) == nullptr) ||
                    colony.owner < 0 || colony.owner >= MOX::NUM_PLAYERS ||
                    ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony.planet))).colony_index != colony_idx) {
                    return false;
                }
            }
            for (int16_t ship_idx = 0; ship_idx < MOX::NUM_SHIPS;
                 ++ship_idx) {
                const s_ship_data& ship = MOX::ship[ship_idx];
                const auto position = ship.Legacy_Location(MOX::star);
                const auto prior_position = MOX::star.Position_Of(ship.prior_jump_gate_destination);
                const auto destination_kind = ship.location.destination.Kind();
                if (destination_kind == game::e_ship_destination::antares || position >= MOX::NUM_STARS ||
                    (destination_kind == game::e_ship_destination::star && position < 0) ||
                    (ship.status == 0 && position < 0) ||
                    (ship.prior_jump_gate_destination != game::NO_STAR &&
                        (prior_position < 0 || prior_position >= MOX::NUM_STARS))) {
                    return false;
                }
                if (ship.officer_index < -1 ||
                    ship.officer_index >= MAX_LEADERS) {
                    return false;
                }
            }
            for (const auto& [id, leader] : world_state->leaders) {
                const int32_t leader_idx = static_cast<int32_t>(id.id);
                if (leader.status == 5 &&
                    (leader.location < 0 ||
                        leader.location >= MOX::NUM_STARS ||
                        MOX::star[leader.location].system_special !=
                            SYSTEM_SPECIAL_MAROONED_HERO)) {
                    return false;
                }
                if (leader.status == 1 &&
                    leader.type == LEADER_TYPE_COLONY &&
                    (leader.location < 0 ||
                        leader.location >= MOX::NUM_STARS ||
                        leader.player_index < 0 ||
                        leader.player_index >= MOX::NUM_PLAYERS ||
                        MOX::star[leader.location]
                                .officer_index[leader.player_index] !=
                            leader_idx)) {
                    return false;
                }
                if (leader.status == 1 && leader.type == LEADER_TYPE_SHIP &&
                    (leader.location < 0 ||
                        leader.location >= MOX::NUM_SHIPS ||
                        MOX::ship[leader.location].officer_index !=
                            leader_idx)) {
                    return false;
                }
            }
            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS;
                 ++star_idx) {
                if (MOX::star[star_idx].system_special !=
                    SYSTEM_SPECIAL_MAROONED_HERO) {
                    continue;
                }
                int leader_count = 0;
                for (const auto& [id, leader] : world_state->leaders) {
                    leader_count += leader.status == 5 && leader.location == star_idx;
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
            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS;
                 ++star_idx) {
                if (MOX::star[star_idx].spectral_class ==
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
                     destination < MOX::NUM_STARS;
                     ++destination) {
                    if (reached[destination] ||
                        MOX::star[destination].spectral_class ==
                            STAR_CLASS_BLACK_HOLE ||
                        struct_::Test_Bit_Field(
                            MOX::star[source].black_hole_blocks,
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

        bool Compact_Galaxy_Candidate(game::s_world_state* world_state) {
            int16_t star_remap[MAX_STARS];
            memset(star_remap, 0xff, sizeof(star_remap));

            std::array<game::id_star, MAX_STARS> compacted;
            compacted.fill(game::NO_STAR);
            auto* star_state = MOX::star.state;
            int16_t next_star = 0;
            for (int16_t star_id = 0; star_id < MOX::NUM_STARS;
                 ++star_id) {
                if (MOX::star[star_id].spectral_class < STAR_CLASS_B ||
                    MOX::star[star_id].spectral_class >
                        STAR_CLASS_BLACK_HOLE) {
                    MOX::star.Retire(star_id);
                    continue;
                }
                star_remap[star_id] = next_star;
                if (star_state != nullptr) {
                    const auto id = MOX::star.Id_At(star_id);
                    if (id == game::NO_STAR) return false;
                    compacted[next_star] = id;
                } else if (next_star != star_id) {
                    MOX::star[next_star] = MOX::star[star_id];
                }
                ++next_star;
            }
            if (star_state != nullptr) {
                star_state->star_order = compacted;
                star_state->vacant_star_positions.clear();
            }

            for (auto entry = world_state->planets.begin(); entry != world_state->planets.end();) {
                auto& planet = entry->second;
                const auto parent = planet.parent_star;
                if (star_state != nullptr) {
                    if (game::Find_Star(star_state, parent) == nullptr) {
                        entry = world_state->planets.erase(entry);
                    } else {
                        ++entry;
                    }
                } else if (parent.id >= MAX_STARS || star_remap[parent.id] < 0) {
                    entry = world_state->planets.erase(entry);
                } else {
                    planet.parent_star = MOX::star.Id_At(star_remap[parent.id]);
                    ++entry;
                }
            }

            for (int16_t star_id = 0; star_id < next_star; ++star_id) {
                s_star_data& star = MOX::star[star_id];
                for (const auto id : star.planet_index) {
                    if (id != game::NO_PLANET && game::Find_Planet(world_state, id) == nullptr) return false;
                }
                const auto check_reference = [&](game::id_star* id) {
                    if (*id == game::NO_STAR) return true;
                    if (star_state != nullptr) return game::Find_Star(star_state, *id) != nullptr;
                    // Legacy array fixtures encode these references by position.
                    if (id->id >= MAX_STARS || star_remap[id->id] < 0) return false;
                    *id = MOX::star.Id_At(star_remap[id->id]);
                    return true;
                };
                if (!check_reference(&star.wormhole_star_id)) return false;
                if (star.next_wfi_in_list >= 0) {
                    if (star.next_wfi_in_list >= MAX_STARS ||
                        star_remap[star.next_wfi_in_list] < 0) {
                        return false;
                    }
                    star.next_wfi_in_list =
                        star_remap[star.next_wfi_in_list];
                }
                for (auto& relocation : star.relocate_ship_to) {
                    if (!check_reference(&relocation)) return false;
                }
            }
            for (int16_t player_id = 0; player_id < MOX::NUM_PLAYERS; ++player_id) {
                if (game::Find_Planet(world_state, MOX::player[player_id].home_planet_id) == nullptr) return false;
            }
            for (int16_t colony_id = 0; colony_id < MOX::NUM_COLONIES; ++colony_id) {
                if (MOX::colony[colony_id].owner < 0 && MOX::colony[colony_id].planet == game::NO_PLANET) continue;
                if (game::Find_Planet(world_state, MOX::colony[colony_id].planet) == nullptr) return false;
            }
            for (const auto& [id, planet] : world_state->planets) {
                if (planet.colony_index >= MOX::NUM_COLONIES) return false;
            }
            for (int16_t ship_id = 0; ship_id < MOX::NUM_SHIPS;
                 ++ship_id) {
                s_ship_data& ship = MOX::ship[ship_id];
                if (ship.location.destination.Kind() == game::e_ship_destination::star) {
                    auto star_id = ship.location.destination.Star_Id_Or_None();
                    if (star_state == nullptr) {
                        const auto old = star_id.id;
                        if (old >= MAX_STARS || star_remap[old] < 0) return false;
                        star_id = MOX::star.Id_At(star_remap[old]);
                        ship.location.destination = game::s_ship_destination::Star(star_id);
                    }
                    const auto position = MOX::star.Position_Of(star_id);
                    if (position < 0 || position >= next_star || ship.location.kind != game::e_ship_location::at_star) return false;
                    ship.x = MOX::star[position].x;
                    ship.y = MOX::star[position].y;
                }
                if (ship.prior_jump_gate_destination != game::NO_STAR) {
                    if (star_state == nullptr) {
                        const auto old = ship.prior_jump_gate_destination.id;
                        if (old >= MAX_STARS || star_remap[old] < 0) return false;
                        ship.prior_jump_gate_destination = MOX::star.Id_At(star_remap[old]);
                    } else if (game::Find_Star(star_state, ship.prior_jump_gate_destination) == nullptr) {
                        return false;
                    }
                }
            }
            for (auto& [id, leader] : world_state->leaders) {
                if (leader.status == 5 ||
                    (leader.status == 1 &&
                        leader.type == LEADER_TYPE_COLONY)) {
                    if (leader.location < 0 || leader.location >= MAX_STARS ||
                        star_remap[leader.location] < 0) {
                        return false;
                    }
                    leader.location = star_remap[leader.location];
                }
            }
            MOX::NUM_STARS = next_star;
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

        void Refresh_Configured_Planet(game::s_world_state* world_state, game::id_planet planet_idx) {
            auto* planet = &game::Planet(world_state, planet_idx);
            planet->food_per_farmer = MAPGEN::Generate_Food_Per_Farmer(world_state, 
                planet->Star_Position(MOX::star), planet_idx);
            if ((planet->environmental_alterations & 0x2) != 0) {
                planet->food_per_farmer += COLCALC::food_farmer_bonus.soil_enrichment;
            }
            planet->max_farms = MAPGEN::Generate_Max_Farms(world_state, planet->Star_Position(MOX::star), planet_idx);
        }

        bool Transform_Generated_Planets(::game::s_world_state* world_state) {
            bool colony_state_changed = false;
            for (const auto& planet_entry : (world_state)->planets) {
            const ::game::id_planet planet_idx = planet_entry.first;
                s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));
                s_star_data* star = &MOX::star[planet->Star_Position(MOX::star)];
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
                if (planet->colony_index == -1 &&
                    MAPGEN::planet_generation_config.normalize_low_gravity &&
                    planet->gravity_class == PLANET_GRAVITY_LOW_G) {
                    planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
                }
                if (planet->colony_index == -1 &&
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
                    Refresh_Configured_Planet(world_state, planet_idx);
                }
                colony_state_changed |= planet->colony_index >= 0 &&
                    (planet_state_changed || planet->planet_special != original_special);
            }
            return colony_state_changed;
        }

        void Transform_Marooned_Heroes(game::s_world_state* world_state) {
            if (MAPGEN::special_generation_config.highlight_marooned_heroes) {
                for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
                    s_star_data* star = &MOX::star[star_idx];
                    if (star->system_special == SYSTEM_SPECIAL_MAROONED_HERO) {
                        star->spectral_class = STAR_CLASS_DWARF;
                    } else if (star->spectral_class == STAR_CLASS_DWARF) {
                        star->spectral_class = STAR_CLASS_M;
                    }
                }
            }

            if (MAPGEN::special_generation_config.remove_marooned_heroes) {
                for (const auto& leader_entry : world_state->leaders) {
                    const int32_t leader_idx = static_cast<int32_t>(leader_entry.first.id);
                    s_leader_data* leader = &game::Leader(world_state, leader_idx);
                    if (leader->status == 5 && leader->location >= 0 &&
                        leader->location < MOX::NUM_STARS) {
                        MOX::star[leader->location].system_special = SYSTEM_SPECIAL_PIRATE_CACHE;
                        leader->location = -1;
                        leader->status = -1;
                    }
                }
            }
        }

        bool Transform_Monster_Guarded_Planets(::game::s_world_state* world_state) {
            bool colony_state_changed = false;
            for (int16_t ship_idx = 0; ship_idx < MOX::NUM_SHIPS; ++ship_idx) {
                const s_ship_data* ship = &MOX::ship[ship_idx];
                if (ship->owner < NONPLAYER_SHIP_TYPE_AMOEBA ||
                    ship->owner > NONPLAYER_SHIP_TYPE_HYDRA ||
                    ship->Legacy_Location(MOX::star) < 0 || ship->Legacy_Location(MOX::star) >= MOX::NUM_STARS) {
                    continue;
                }
                s_star_data* star = &MOX::star[ship->Legacy_Location(MOX::star)];
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const ::game::id_planet planet_idx = star->planet_index[orbit];
                    if (planet_idx == ::game::NO_PLANET || (::game::Find_Planet(world_state, legacy_world::Planet_Id(planet_idx)) == nullptr)) continue;
                    s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));
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
                        Refresh_Configured_Planet(world_state, planet_idx);
                    }
                    colony_state_changed |= planet->colony_index >= 0 && planet_state_changed;
                }
            }
            return colony_state_changed;
        }

        void Transform_Home_Systems(::game::s_world_state* world_state) {
            const bool flatten = MAPGEN::home_system_config.normalize_unoccupied_planets ||
                MAPGEN::home_system_config.use_fixed_unoccupied_planets;
            for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                s_player* player = &MOX::player[player_idx];
                s_planet_data* home = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(player->home_planet_id)));
                s_star_data* star = &MOX::star[home->Star_Position(MOX::star)];
                int unoccupied_index = 0;
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const ::game::id_planet planet_idx = star->planet_index[orbit];
                    if (planet_idx == ::game::NO_PLANET) continue;
                    s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx)));
                    if (planet->colony_index != -1) continue;
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
                        Refresh_Configured_Planet(world_state, planet_idx);
                    }
                    ++unoccupied_index;
                }
            }
        }

        bool Transform_Lua_Home_Systems(game::s_world_state* world_state, bool* succeeded) {
            *succeeded = false;
            if (!Valid_Generated_Map(world_state)) exitm::Exit_With_Message("invalid map before Lua home-system setup");
            auto staged_planets = world_state->planets;
            s_staged_stars staged_stars(MOX::NUM_STARS);
            bool colony_state_changed = false;
            {
                s_generated_map_scope scope(world_state, &staged_planets, staged_stars.view);
                for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                    const auto home = MOX::player[player_idx].home_planet_id;
                    const orion2re::lua::game::s_home_system_generated_event event = {player_idx, game::Planet(world_state, home).Star_Position(MOX::star)};
                    orion2re::lua::game::Home_System_Generated(world_state, &event);
                }
                for (const auto& [id, planet] : world_state->planets) {
                    const orion2re::lua::game::s_planet_generated_event event = {id};
                    orion2re::lua::game::Planet_Generated(world_state, &event);
                }
                if (!Valid_Generated_Map(world_state)) exitm::Exit_With_Message("Lua home-system setup produced an invalid map");
                for (const auto& [id, planet] : world_state->planets) {
                    const auto& original = scope.planets->at(id);
                    const bool economy_changed = planet.size != original.size || planet.climate != original.climate ||
                        planet.mineral_class != original.mineral_class || planet.gravity_class != original.gravity_class ||
                        planet.planet_special != original.planet_special ||
                        MOX::star[planet.Star_Position(MOX::star)].system_special != scope.stars[planet.Star_Position(scope.stars)].system_special;
                    Refresh_Configured_Planet(world_state, id);
                    colony_state_changed |= planet.colony_index >= 0 && economy_changed;
                }
            }
            world_state->planets = std::move(staged_planets);
            staged_stars.Commit();
            *succeeded = true;
            return colony_state_changed;
        }

        void Reveal_Starting_Area(::game::s_world_state* world_state) {
            const uint32_t radius = MAPGEN::starting_reveal_radius;
            if (radius == 0) return;

            const uint32_t radius_sq = radius * radius * 900;
            for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                const auto home_planet_idx = MOX::player[player_idx].home_planet_id;
                const s_star_data* home = &MOX::star[game::Planet(world_state, home_planet_idx).Star_Position(MOX::star)];
                for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
                    s_star_data* star = &MOX::star[star_idx];
                    const int32_t dx = star->x - home->x;
                    const int32_t dy = star->y - home->y;
                    if ((uint32_t)(dx * dx + dy * dy) <= radius_sq) {
                        star->visited |= (uint8_t)(1u << player_idx);
                    }
                }
            }
        }

    } // namespace

    bool __cdecl Finalize_Configured_New_Game(game::s_world_state* world_state, bool* succeeded) {
        bool local_succeeded = false;
        if (succeeded == nullptr) succeeded = &local_succeeded;
        *succeeded = false;
        bool colony_state_changed = Transform_Generated_Planets(world_state);
        Transform_Marooned_Heroes(world_state);
        colony_state_changed |= Transform_Monster_Guarded_Planets(world_state);
        Transform_Home_Systems(world_state);
        bool lua_succeeded = false;
        colony_state_changed |= Transform_Lua_Home_Systems(world_state, &lua_succeeded);
        if (!lua_succeeded) return false;
        Reveal_Starting_Area(world_state);
        *succeeded = true;
        return colony_state_changed;
    }

    bool __cdecl Apply_Lua_Galaxy_Generator(game::s_world_state* world_state, std::string* diagnostic) {
        std::string local_diagnostic;
        if (diagnostic == nullptr) diagnostic = &local_diagnostic;
        diagnostic->clear();
        if (MOX::game_type == 3 ||
            !orion2re::lua::Has_Active_Galaxy_Generator()) {
            return true;
        }

        auto planets = world_state->planets;
        s_staged_stars stars(MAX_STARS);
        std::vector<s_player> players(
            MOX::player, MOX::player + MAX_PLAYERS);
        std::vector<s_colony> colonies;
        game::s_world_state colony_candidate;
        game::s_colony_slots staged_colonies(&colony_candidate);
        if (MOX::colony.state != nullptr) {
            const auto* live = MOX::colony.state;
            colony_candidate.colonies = live->colonies;
            colony_candidate.colony_order = live->colony_order;
            colony_candidate.vacant_colony_positions = live->vacant_colony_positions;
            colony_candidate.next_colony_id = live->next_colony_id;
            colony_candidate.settlers = live->settlers;
        } else {
            colonies.assign(MOX::colony.fixture, MOX::colony.fixture + MAX_COLONIES);
            staged_colonies = colonies.data();
        }
        std::vector<s_ship_data> ships(
            MOX::ship, MOX::ship + MAX_SHIPS);
        std::vector<s_leader_data> leaders = game::Export_Leaders(world_state);
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

        int16_t candidate_star_count = MOX::NUM_STARS;
        int16_t candidate_colony_count = MOX::NUM_COLONIES;
        int16_t candidate_ship_count = MOX::NUM_SHIPS;
        uint8_t candidate_nebula_count = MOX::NUM_NEBULAS;
        bool valid = false;
        orion2re::lua::game::internal::Advance_Topology_Generation();
        {
            s_galaxy_candidate_scope scope(world_state, &planets,
                stars.view,
                players.data(),
                staged_colonies,
                ships.data(),
                leaders.data(),
                nebulas.data());
            orion2re::lua::game::s_ship_generation_scope generation_scope(
                ship_generations);
            if (!orion2re::lua::Dispatch_Active_Galaxy_Generator(
                    &invoked, diagnostic)) {
                orion2re::lua::game::internal::Restore_Topology_Generation(
                    topology_generation);
                return false;
            }
            valid = invoked && Compact_Galaxy_Candidate(world_state) &&
                Valid_Galaxy_Candidate(world_state);
            if (valid) {
                for (const auto& planet_entry : world_state->planets) {
                const auto planet_idx = planet_entry.first;
                    Refresh_Configured_Planet(world_state, planet_idx);
                }
                GEO::Initialize_Star_In_Nebula_Info();

                candidate_star_count = MOX::NUM_STARS;
                candidate_colony_count = MOX::NUM_COLONIES;
                candidate_ship_count = MOX::NUM_SHIPS;
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

        world_state->planets = std::move(planets);
        stars.Commit();
        memcpy(MOX::player, players.data(), MAX_PLAYERS * sizeof(s_player));
        if (auto* live = MOX::colony.state) {
            live->colonies = std::move(colony_candidate.colonies);
            live->colony_order = colony_candidate.colony_order;
            live->vacant_colony_positions = std::move(colony_candidate.vacant_colony_positions);
            live->next_colony_id = colony_candidate.next_colony_id;
            live->settlers = std::move(colony_candidate.settlers);
        } else {
            std::copy(colonies.begin(), colonies.end(), MOX::colony.fixture);
        }
        memcpy(MOX::ship, ships.data(), MAX_SHIPS * sizeof(s_ship_data));
        game::Import_Leaders(world_state, leaders);
        memcpy(MOX::nebula,
            nebulas.data(),
            MAX_NEBULAS * sizeof(s_nebula));

        MOX::NUM_STARS = candidate_star_count;
        MOX::NUM_COLONIES = candidate_colony_count;
        MOX::NUM_SHIPS = candidate_ship_count;
        MOX::NUM_NEBULAS = candidate_nebula_count;
        orion2re::lua::game::Restore_Ship_Generation_State(
            ship_generations, MAX_SHIPS);
        orion2re::lua::game::internal::Advance_Topology_Generation();
        orion2re::lua::game::internal::Advance_Topology_Generation();
        return true;
    }
} // namespace MAPGEN