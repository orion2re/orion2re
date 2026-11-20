#pragma once

#include <array>
#include <map>
#include <optional>

#include "consts.h"
#include "leader_data.h"
#include "object_ids.h"
#include "planet_data.h"

namespace game {
    struct s_game_state {
        // A leader's ID is its fixed catalog slot, including inactive leaders.
        std::array<s_leader_data, MAX_LEADERS> leaders{};

        std::map<id_planet, s_planet_data> planets;
        uint32_t next_planet_id = 0;

        std::optional<id_planet> Create_Planet(const s_planet_data& planet) {
            const id_planet id{next_planet_id};
            if (planets.size() >= MAX_PLANETS || id == NO_PLANET) return std::nullopt;
            if (!planets.try_emplace(id, planet).second) return std::nullopt;
            ++next_planet_id;
            return id;
        }

        bool operator==(const s_game_state&) const = default;
    };

    inline s_game_state current_state;
} // namespace game
