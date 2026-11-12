#include "pch.h"

namespace {
    HOMEGEN::s_star_dist star_distances[MAX_STARS];
    s_planet_data planets[MAX_PLANETS];
    s_star_data stars[MAX_STARS + 1];
    int32_t movement_count;
    int32_t orbit_roll_count;
    int32_t satellite_generation_count;
    int32_t normal_planet_generation_count;
    int32_t non_black_hole_generation_count;
    int32_t special_generation_count;
    int32_t spectral_generation_count;
    int16_t configured_satellite_count;
    int8_t generated_spectral_class;
}

namespace TEST_HOMEGEN {
    void Reset_() {
        memset(star_distances, 0, sizeof(star_distances));
        memset(planets, 0, sizeof(planets));
        memset(stars, 0, sizeof(stars));
        memset(MOX::star_class_table, 0, sizeof(MOX::star_class_table));
        memset(MAPGEN::mineral_class, 0, sizeof(MAPGEN::mineral_class));
        memset(MAPGEN::planet_gravity, 0, sizeof(MAPGEN::planet_gravity));

        for (int16_t star_idx = 0; star_idx <= MAX_STARS; ++star_idx) {
            for (int16_t orbit = 0; orbit < 5; ++orbit) {
                stars[star_idx].planet_index[orbit] = -1;
            }
        }

        stars[1].planet_index[2] = 0;
        planets[0].star_index = 1;
        planets[0].orbit = 2;
        planets[0].planet_type = PLANET_TYPE_ASTEROID;

        MOX::planet_count = 1;
        MAPGEN::make_food_planet = 0;
        movement_count = 0;
        orbit_roll_count = 0;
        satellite_generation_count = 0;
        normal_planet_generation_count = 0;
        non_black_hole_generation_count = 0;
        special_generation_count = 0;
        spectral_generation_count = 0;
        configured_satellite_count = 0;
        generated_spectral_class = STAR_CLASS_G;
    }

    int32_t Satellite_Generation_Count_() {
        return satellite_generation_count;
    }

    void Set_Satellite_Count_(int16_t satellite_count) {
        configured_satellite_count = satellite_count;
    }

    int32_t Normal_Planet_Generation_Count_() {
        return normal_planet_generation_count;
    }

    void Set_Generated_Spectral_Class_(int8_t spectral_class) {
        generated_spectral_class = spectral_class;
    }

    int32_t Non_Black_Hole_Generation_Count_() {
        return non_black_hole_generation_count;
    }

    int32_t Special_Generation_Count_() {
        return special_generation_count;
    }

    int32_t Spectral_Generation_Count_() {
        return spectral_generation_count;
    }
}

namespace MOX {
    int16_t NUM_PLAYERS = 1;
    int16_t NUM_STARS = 2;
    void* global_data_seg = nullptr;
    int16_t planet_count = 1;
    s_planet_data* planet = planets;
    s_star_data* star = stars;
    s_settings settings = {};
    uint8_t star_class_table[STAR_CLASS_COUNT][GALAXY_AGE_COUNT];
}

namespace allocate {
    void* __cdecl Allocate_First_Block(void*, uint32_t) {
        return star_distances;
    }
}

namespace CMBTMIS {
    void __cdecl Coord_Movement(int16_t, int16_t, int16_t, int16_t, uint8_t, int16_t* dx, int16_t* dy) {
        *dx = 0;
        *dy = 0;
        movement_count++;
    }
}

namespace GEO {
    int32_t __cdecl Parsecs_Between_Stars(int16_t, int16_t) {
        return movement_count == 0 ? 5 : 4;
    }
}

namespace HAROLD {
    bool __cdecl Habitable_Spectral_Class(int16_t star_idx) {
        return MOX::star[star_idx].spectral_class < STAR_CLASS_BLACK_HOLE;
    }

    uint8_t __cdecl N_Planets_In_System(int16_t star_idx) {
        uint8_t count = 0;
        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            int16_t planet_idx = MOX::star[star_idx].planet_index[orbit];
            if (planet_idx >= 0 && MOX::planet[planet_idx].planet_type == PLANET_TYPE_PLANET) {
                count++;
            }
        }
        return count;
    }
}

namespace HACCESS {
    void __cdecl Race_Name(int16_t, char* buffer, uint32_t buffer_size) {
        snprintf(buffer, buffer_size, "%s", "Monster");
    }
}

namespace MAPGEN {
    uint8_t make_food_planet = 0;
    s_home_system_config home_system_config = {3};
    int8_t mineral_class[10][6];
    int8_t planet_gravity[MINERAL_RESOURCE_COUNT][PLANET_SIZE_COUNT];

    int8_t __cdecl Generate_Spectral_Class(int16_t) {
        spectral_generation_count++;
        return generated_spectral_class;
    }

    int8_t __cdecl Generate_Non_Black_Hole_Spectral_Class(int16_t) {
        non_black_hole_generation_count++;
        return generated_spectral_class;
    }

    int16_t __cdecl Generate_Number_Of_Satellites(int16_t) {
        return configured_satellite_count;
    }

    void __cdecl Planet_Generation(int16_t star_idx) {
        normal_planet_generation_count++;
        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            if (MOX::star[star_idx].planet_index[orbit] != -1) {
                continue;
            }

            int16_t planet_idx = MOX::planet_count;
            MOX::star[star_idx].planet_index[orbit] = planet_idx;
            MOX::planet[planet_idx].planet_type = PLANET_TYPE_ASTEROID;
            Generate_Satellite(star_idx, orbit, PLANET_TYPE_ASTEROID);
            MOX::planet_count++;
            return;
        }
    }

    void __cdecl Generate_Satellite(int16_t star_idx, int16_t orbit, int16_t) {
        MOX::planet[MOX::planet_count].star_index = star_idx;
        MOX::planet[MOX::planet_count].orbit = (int8_t)orbit;
        satellite_generation_count++;
    }

    void __cdecl Get_A_Planet_Or_Star_Special(int16_t, int16_t*, int8_t* special_out,
                                                uint8_t, int32_t) {
        special_generation_count++;
        *special_out = SYSTEM_SPECIAL_NO_SPECIAL;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t limit) {
        if (limit == 5) {
            orbit_roll_count++;
            return orbit_roll_count == 1 ? 3 : 4;
        }
        return 1;
    }
}