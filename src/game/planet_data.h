#pragma once

#include <cstdint>

#include "object_ids.h"

struct s_planet_data {
    game::id_colony colony_id = game::NO_COLONY;
    int16_t star_index;
    int8_t orbit;
    int8_t planet_type;
    int8_t size;
    int8_t gravity_class;
    int8_t group;
    int8_t climate;
    int8_t climate_bg_type;
    int8_t mineral_class;
    uint8_t food_per_farmer;
    uint8_t n_terraforms;
    uint8_t max_farms;
    uint8_t max_population;
    int8_t planet_special;
    int8_t environmental_alterations;

    bool operator==(const s_planet_data&) const = default;
};
