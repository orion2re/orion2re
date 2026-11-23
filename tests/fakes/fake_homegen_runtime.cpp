#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"

namespace {
    HOMEGEN::s_star_dist star_distances[MAX_STARS];

    test_stars::Slots<MAX_STARS + 1> stars;
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
        test_planets::Reset(0);
        stars.Reset();
        memset(MOX::star_class_table, 0, sizeof(MOX::star_class_table));
        memset(MAPGEN::mineral_class, 0, sizeof(MAPGEN::mineral_class));
        memset(MAPGEN::planet_gravity, 0, sizeof(MAPGEN::planet_gravity));

        test_stars::Resize(2);

        stars[1].orbit_planet_ids[2] = game::id_planet{0};
        test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(1)};
        test_planets::Record(0).orbit = 2;
        test_planets::Record(0).planet_type = PLANET_TYPE_ASTEROID;

        test_planets::Resize_Dense(1);
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
    [[maybe_unused]] const bool initial_player_count = [] { game::current_state.player_count = 1; return true; }();

    void* global_data_seg = nullptr;


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
        return test_stars::Record(star_idx).spectral_class < STAR_CLASS_BLACK_HOLE;
    }

    uint8_t __cdecl N_Planets_In_System(int16_t star_idx) {
        uint8_t count = 0;
        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            const auto planet_id = test_stars::Record(star_idx).orbit_planet_ids[orbit];
            if (planet_id != game::NO_PLANET && game::current_state.planets.at(planet_id).planet_type == PLANET_TYPE_PLANET) {
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
            if (test_stars::Record(star_idx).orbit_planet_ids[orbit] != game::NO_PLANET) {
                continue;
            }

            int16_t planet_idx = static_cast<int16_t>(game::current_state.next_planet_id);
            test_stars::Record(star_idx).orbit_planet_ids[orbit] = game::id_planet{static_cast<uint32_t>(planet_idx)};
            test_planets::Record(planet_idx).planet_type = PLANET_TYPE_ASTEROID;
            test_planets::Record(planet_idx).orbit = static_cast<int8_t>(orbit);
            Generate_Satellite(test_planets::Record(planet_idx), star_idx);
            return;
        }
    }

    void __cdecl Generate_Satellite(s_planet_data& planet, int16_t star_idx) {
        planet.star_id = game::id_star{static_cast<uint32_t>(star_idx)};
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
