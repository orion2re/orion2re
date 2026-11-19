#include "pch.h"

namespace HOMEGEN {
    s_star_dist* star_list;

    static void Add_Planet_To_System(game::s_world_state* world_state, int16_t star_idx) {
        const int16_t orbit = Usable_Orbit(world_state, star_idx);
        auto id = MOX::star[star_idx].planet_index[orbit];
        if (id == game::NO_PLANET) {
            auto created = game::Create_Planet(world_state, s_planet_data{});
            if (!created) return;
            id = *created;
        }
        Make_One_Planet(world_state, star_idx, legacy_world::Planet_Id(id), orbit, 0);
    }

    static void Generate_Normal_Satellites(::game::s_world_state* world_state, int16_t star_idx) {
        int16_t satellite_count = MAPGEN::Generate_Number_Of_Satellites(star_idx);
        for (int16_t i = 0; i < satellite_count; ++i) {
            if (::game::Planet_Count(world_state) >= MAX_PLANETS) {
                break;
            }

            bool has_free_orbit = false;
            for (int16_t orbit = 0; orbit < 5; ++orbit) {
                if (MOX::star[star_idx].planet_index[orbit] == ::game::NO_PLANET) {
                    has_free_orbit = true;
                    break;
                }
            }
            if (!has_free_orbit) {
                break;
            }

            MAPGEN::Planet_Generation(world_state, star_idx);
        }
    }

    bool __cdecl Star_Has_Planet(::game::s_world_state* world_state, int16_t star_index) {
        if (MOX::star[star_index].spectral_class == STAR_CLASS_BLACK_HOLE) {
            return false;
        }

        bool planet_found = false;
        for (int i = 0; i < 5; ++i) {
            if (planet_found) {
                return true;
            }

            ::game::id_planet planet_idx = MOX::star[star_index].planet_index[i];
            if (planet_idx != ::game::NO_PLANET) {
                if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).planet_type == PLANET_TYPE_PLANET) {
                    planet_found = true;
                }
            }
        }

        return planet_found;
    }

    void __cdecl Guarantee_A_Planet_At_Min_Dist(game::s_world_state* world_state, int16_t origin_star_idx, int16_t min_dist, int16_t* player_home_stars) {
        star_list = (s_star_dist*)allocate::Allocate_First_Block(
            MOX::global_data_seg, sizeof(*star_list) * MAX_STARS);

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            star_list[i].dist = 999;
            star_list[i].index = 999;
        }

        bool planet_found = false;
        for (int16_t i = 1; i < MOX::NUM_STARS; ++i) {
            if (i == origin_star_idx) continue;

            star_list[i].index = i;
            int16_t dist = GEO::Parsecs_Between_Stars(origin_star_idx, i);
            star_list[i].dist = dist;

            if (Star_Has_Planet(world_state, i) && dist <= min_dist) {
                planet_found = true;
                break;
            }
        }

        if (!planet_found) {
            qsort(star_list, MOX::NUM_STARS, sizeof(*star_list), qsort_star_dist_list);

            int16_t list_idx = 0;
            int16_t target_star_idx;
            while (true) {
                target_star_idx = star_list[list_idx].index;
                bool is_home_star = false;
                for (int16_t p = 0; p < MOX::NUM_PLAYERS; ++p) {
                    if (player_home_stars[p] == target_star_idx) {
                        is_home_star = true;
                        break;
                    }
                }
                if (!is_home_star) {
                    break;
                }
                ++list_idx;
                if (MOX::NUM_STARS <= list_idx) {
                    break;
                }
            }

            if (!Star_Has_Planet(world_state, target_star_idx)) {
                s_star_data* target_star = &MOX::star[target_star_idx];
                const bool generate_normal_satellites =
                    !HAROLD::Habitable_Spectral_Class(target_star_idx);
                if (generate_normal_satellites) {
                    target_star->spectral_class =
                        MAPGEN::Generate_Non_Black_Hole_Spectral_Class(MOX::settings.galaxy_age);
                }
                if (target_star->spectral_class >= STAR_CLASS_DWARF) {
                    target_star->spectral_class = STAR_CLASS_M;
                }
                if (generate_normal_satellites) {
                    Generate_Normal_Satellites(world_state, target_star_idx);
                }

                int32_t r_size = game_random::Random(10) - 1;
                if (r_size < 3)
                    target_star->size = 0;
                else if (r_size < 7)
                    target_star->size = 1;
                else
                    target_star->size = 2;

                target_star->pict_type = (char)(game_random::Random(3) - 1);

                int32_t planets_to_add = 2 - (int32_t)HAROLD::N_Planets_In_System(world_state, target_star_idx);
                for (int32_t j = 0; j < planets_to_add; ++j) {
                    Add_Planet_To_System(world_state, target_star_idx);
                }
            }

            if (list_idx < MOX::NUM_STARS) {
                do {
                    int16_t delta_x, delta_y;
                    CMBTMIS::Coord_Movement(
                        MOX::star[target_star_idx].x, MOX::star[target_star_idx].y, MOX::star[origin_star_idx].x, MOX::star[origin_star_idx].y, 10, &delta_x, &delta_y);
                    MOX::star[target_star_idx].x += delta_x;
                    MOX::star[target_star_idx].y += delta_y;
                } while (GEO::Parsecs_Between_Stars(origin_star_idx, target_star_idx) > min_dist);
            }
        }
    }

    int32_t __cdecl qsort_star_dist_list(const void* star_dist_a, const void* star_dist_b) {
        const s_star_dist* left = (const s_star_dist*)star_dist_a;
        const s_star_dist* right = (const s_star_dist*)star_dist_b;

        if (left->dist == right->dist) {
            return 0;
        }

        return (left->dist > right->dist) ? 1 : -1;
    }

    void Make_One_Planet(game::s_world_state* world_state, int16_t star_idx, game::id_planet planet_idx, int16_t orbit_idx, int8_t is_food_planet) {
        auto& star = MOX::star[star_idx];
        if (is_food_planet != 0 && star.spectral_class >= STAR_CLASS_DWARF) {
            star.spectral_class = MAPGEN::Generate_Non_Black_Hole_Spectral_Class(MOX::settings.galaxy_age);
        }
        auto& planet = game::Planet(world_state, legacy_world::Planet_Id(planet_idx));
        planet = {};
        planet.colony_index = -1;
        MAPGEN::make_food_planet = static_cast<uint8_t>(is_food_planet);
        MAPGEN::Generate_Satellite(world_state, legacy_world::Planet_Id(planet_idx), star_idx, orbit_idx, PLANET_TYPE_PLANET);
        MAPGEN::make_food_planet = 0;
        star.planet_index[orbit_idx] = planet_idx;
    }

    int16_t __cdecl Usable_Orbit(::game::s_world_state* world_state, int16_t star_idx) {
        int16_t result_orbit = -1;
        bool found = false;

        // Try 101 times to find a random usable orbit
        for (int i = 0; i <= 100; ++i) {
            if (found) break;

            int r = game_random::Random(5);
            int16_t orbit_candidate = (int16_t)(r - 1);

            if (orbit_candidate < 0 || orbit_candidate > 4) continue;

            ::game::id_planet p_idx = MOX::star[star_idx].planet_index[orbit_candidate];

            if (p_idx == ::game::NO_PLANET) {
                result_orbit = orbit_candidate;
                found = true;
            } else if (p_idx != ::game::NO_PLANET && (::game::Find_Planet(world_state, legacy_world::Planet_Id(p_idx)) != nullptr)) {
                if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).planet_type != PLANET_TYPE_PLANET) {
                    result_orbit = orbit_candidate;
                    found = true;
                }
            }
        }

        // Sequential search fallback if random attempts fail
        if (!found) {
            for (int16_t i = 0; i < 5; ++i) {
                ::game::id_planet p_idx = MOX::star[star_idx].planet_index[i];
                if (p_idx == ::game::NO_PLANET || ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).planet_type != PLANET_TYPE_PLANET) {
                    result_orbit = i;
                    found = true;
                    break;
                }
            }
        }

        // Final fallback: just pick a random orbit if search failed
        if (result_orbit == -1) {
            result_orbit = (int16_t)(game_random::Random(5) - 1);
        }

        return result_orbit;
    }

    void __cdecl Set_Special_For_Star_Guarded_By_Monster(::game::s_world_state* world_state, s_star_data* star, int16_t monster_type) {
        char race_name[200];
        int16_t star_idx = MOX::star.Position_Of(star);
        int16_t need_new_special = 0;

        HACCESS::Race_Name(monster_type, race_name, sizeof(race_name));

        int32_t monster_mask = 2;
        if (monster_type == 12) {
            monster_mask = 0xA96;
        } else if (monster_type == 13) {
            monster_mask = 0xF4E;
        }

        bool has_planet_special = false;
        for (int i = 0; i < 5; ++i) {
            ::game::id_planet p_idx = star->planet_index[i];
            if (p_idx != ::game::NO_PLANET && ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).planet_special > SYSTEM_SPECIAL_NO_SPECIAL) {
                has_planet_special = true;
                break;
            }
        }

        if (has_planet_special) {
            if ((monster_mask >> ((uint8_t)star->system_special & 0x1F)) & 1) {
                for (int i = 0; i < 5; ++i) {
                    ::game::id_planet p_idx = star->planet_index[i];
                    if (p_idx != ::game::NO_PLANET) {
                        ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    }
                }
                need_new_special = 1;
            }
        } else {
            need_new_special = 1;
        }

        if (need_new_special) {
            MAPGEN::Get_A_Planet_Or_Star_Special(world_state, 
                star_idx, &need_new_special, &star->system_special, 1, monster_mask);
        }
    }

    void __cdecl Enforce_3_Planet_Minimum(game::s_world_state* world_state, int16_t star_idx) {
        int8_t planets_in_system = HAROLD::N_Planets_In_System(world_state, star_idx);

        const int8_t minimum_planets = (int8_t)MAPGEN::home_system_config.minimum_planets;
        if (planets_in_system < minimum_planets) {
            // Ensure the star has a spectral class capable of hosting a system
            if (!HAROLD::Habitable_Spectral_Class(star_idx)) {
                for (int attempt = 0; attempt < 200; ++attempt) {
                    MOX::star[star_idx].spectral_class = MAPGEN::Generate_Spectral_Class(MOX::settings.galaxy_age);
                    if (HAROLD::Habitable_Spectral_Class(star_idx)) break;
                }
                if (!HAROLD::Habitable_Spectral_Class(star_idx)) {
                    MOX::star[star_idx].spectral_class = STAR_CLASS_M;
                }
                Generate_Normal_Satellites(world_state, star_idx);
            }

            planets_in_system = HAROLD::N_Planets_In_System(world_state, star_idx);

            int32_t planets_to_add = minimum_planets - (int32_t)planets_in_system;
            for (int32_t i = 0; i < planets_to_add; ++i) {
                Add_Planet_To_System(world_state, star_idx);
            }
        }
    }

    void __cdecl Enforce_Mineral_Richness(::game::s_world_state* world_state, ::game::id_planet planet_index, int16_t mineral_class) {
        s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_index)));
        planet->mineral_class = (int8_t)mineral_class;

        const auto star_idx = planet->Star_Position(MOX::star);
        s_star_data* star = &MOX::star[star_idx];

        bool richness_is_valid = false;
        for (int attempt = 0; attempt < 200; ++attempt) {
            // Only spectral classes 0-4 have specific mineral restrictions checked via table
            if (star->spectral_class < STAR_CLASS_DWARF) {
                for (int i = 0; i < 10; ++i) {
                    if (MAPGEN::mineral_class[i][star->spectral_class] ==
                        (int8_t)mineral_class) {
                        richness_is_valid = true;
                        break;
                    }
                }
            }

            if (richness_is_valid) break;

            // If class is Dwarf (>=5) or richness not allowed for this class, regenerate spectral class
            star->spectral_class = MAPGEN::Generate_Spectral_Class((int16_t)MOX::settings.galaxy_age);
        }
        if (!richness_is_valid) {
            const int galaxy_age = MOX::settings.galaxy_age;
            for (int spectral_class = STAR_CLASS_B;
                 spectral_class < STAR_CLASS_DWARF;
                 ++spectral_class) {
                if (MOX::star_class_table[spectral_class][galaxy_age] == 0) {
                    continue;
                }

                for (int roll = 0; roll < 10; ++roll) {
                    if (MAPGEN::mineral_class[roll][spectral_class] ==
                        (int8_t)mineral_class) {
                        star->spectral_class = (int8_t)spectral_class;
                        richness_is_valid = true;
                        break;
                    }
                }

                if (richness_is_valid) break;
            }
        }

        // Update gravity class based on mineral class and planet size
        if (planet->mineral_class >= MINERAL_RESOURCE_ULTRA_POOR &&
            planet->mineral_class < MINERAL_RESOURCE_COUNT &&
            planet->size >= PLANET_SIZE_TINY && planet->size < PLANET_SIZE_COUNT) {
            planet->gravity_class =
                MAPGEN::planet_gravity[planet->mineral_class][planet->size];
        } else {
            planet->gravity_class = PLANET_GRAVITY_NORMAL_G;
        }
    }
} // namespace HOMEGEN
