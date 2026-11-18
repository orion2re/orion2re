#include "../fixtures/planet_state.h"
#include "pch.h"

namespace {
    HOMEGEN::s_star_dist star_distances[MAX_STARS];

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
        game::world.planets.clear();
        game::world.next_planet_id = 0;
        memset(stars, 0, sizeof(stars));
        memset(MOX::star_class_table, 0, sizeof(MOX::star_class_table));
        memset(MAPGEN::mineral_class, 0, sizeof(MAPGEN::mineral_class));
        memset(MAPGEN::planet_gravity, 0, sizeof(MAPGEN::planet_gravity));

        for (int16_t star_idx = 0; star_idx <= MAX_STARS; ++star_idx) {
            for (int16_t orbit = 0; orbit < 5; ++orbit) {
                stars[star_idx].planet_index[orbit] = ::game::NO_PLANET;
            }
        }

        stars[1].planet_index[2] = game::id_planet{0};
        test::Planet(&game::world, legacy_world::Planet_Id(0)).star_index = 1;
        test::Planet(&game::world, legacy_world::Planet_Id(0)).orbit = 2;
        test::Planet(&game::world, legacy_world::Planet_Id(0)).planet_type = PLANET_TYPE_ASTEROID;

        test::Resize_Planets(&::game::world, 1);
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

    uint8_t __cdecl N_Planets_In_System(::game::s_world_state* world_state, int16_t star_idx) {
        uint8_t count = 0;
        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            ::game::id_planet planet_idx = MOX::star[star_idx].planet_index[orbit];
            if (planet_idx != ::game::NO_PLANET && test::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).planet_type == PLANET_TYPE_PLANET) {
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

    void __cdecl Planet_Generation(::game::s_world_state* world_state, int16_t star_idx) {
        normal_planet_generation_count++;
        for (int16_t orbit = 0; orbit < 5; ++orbit) {
            if (MOX::star[star_idx].planet_index[orbit] != ::game::NO_PLANET) {
                continue;
            }

            const auto planet_idx = game::Create_Planet(world_state, {});
            if (!planet_idx) return;
            MOX::star[star_idx].planet_index[orbit] = *planet_idx;
            Generate_Satellite(world_state, *planet_idx, star_idx, orbit, PLANET_TYPE_ASTEROID);
            return;
        }
    }

    void Generate_Satellite(game::s_world_state* world_state, game::id_planet id, int16_t star_idx, int16_t orbit, int16_t type) {
        auto& planet = game::Planet(world_state, id);
        planet.star_index = star_idx;
        planet.orbit = static_cast<int8_t>(orbit);
        planet.planet_type = static_cast<int8_t>(type);
        satellite_generation_count++;
    }

    void __cdecl Get_A_Planet_Or_Star_Special(::game::s_world_state* world_state, int16_t, int16_t*, int8_t* special_out,
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