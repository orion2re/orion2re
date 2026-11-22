#pragma once

#include <cstdint>

#include "object_ids.h"

namespace game {
    struct s_game_state;
    extern s_game_state current_state;
}

struct s_planet_data {
    game::id_colony colony_id = game::NO_COLONY;
    game::id_star star_id = game::NO_STAR;
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
    game::id_planet id = game::NO_PLANET;

    int16_t Star_Position(const game::s_game_state& state = game::current_state) const;
    void Set_Star_Position(int32_t position, const game::s_game_state& state = game::current_state);
    bool operator==(const s_planet_data&) const = default;
};
