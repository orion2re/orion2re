#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

#include "consts.h"
#include "leader_data.h"
#include "object_ids.h"
#include "planet_data.h"

namespace game {
    struct s_game_state {
        // A leader's ID is its fixed catalog slot, including inactive leaders.
        std::array<s_leader_data, MAX_LEADERS> leaders{};

        std::map<id_planet, s_planet_data> planets;
        // Traversal order only; relationships and lookups use the map key.
        std::vector<id_planet> planet_order;
        uint32_t next_planet_id = 0;

        void Reset_Planets() {
            planets.clear();
            planet_order.clear();
            next_planet_id = 0;
        }

        std::optional<id_planet> Create_Planet(const s_planet_data& planet) {
            const id_planet id{next_planet_id};
            if (planets.size() >= MAX_PLANETS || id == NO_PLANET) return std::nullopt;
            const auto [it, inserted] = planets.try_emplace(id, planet);
            if (!inserted) return std::nullopt;
            try {
                planet_order.push_back(id);
            } catch (...) {
                planets.erase(it);
                throw;
            }
            ++next_planet_id;
            return id;
        }

        bool Erase_Planet(id_planet id) {
            if (planets.erase(id) == 0) return false;
            std::erase(planet_order, id);
            return true;
        }

        bool operator==(const s_game_state&) const = default;
    };

    inline s_game_state current_state;
} // namespace game
