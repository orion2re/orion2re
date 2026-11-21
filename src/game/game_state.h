#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

#include "colony_data.h"
#include "consts.h"
#include "leader_data.h"
#include "object_ids.h"
#include "planet_data.h"

namespace game {
    struct s_game_state {
        // A leader's ID is its fixed catalog slot, including inactive leaders.
        std::array<s_leader_data, MAX_LEADERS> leaders = [] {
            std::array<s_leader_data, MAX_LEADERS> catalog{};
            for (uint32_t i = 0; i < catalog.size(); ++i) catalog[i].id = id_leader{i};
            return catalog;
        }();

        std::map<id_planet, s_planet_data> planets;
        // Traversal order only; relationships and lookups use the map key.
        std::vector<id_planet> planet_order;
        uint32_t next_planet_id = 0;

        std::map<id_colony, s_colony> colonies;
        uint32_t next_colony_id = 0;
        std::array<std::vector<id_colony>, MAX_PLAYERS> colony_reservations;

        void Reset_Colonies() {
            colonies.clear();
            next_colony_id = 0;
            for (auto& reservations : colony_reservations) reservations.clear();
        }

        std::optional<id_colony> Create_Colony(const s_colony& colony) {
            const id_colony id{next_colony_id};
            if (colonies.size() >= MAX_COLONIES || id == NO_COLONY) return std::nullopt;
            const auto [it, inserted] = colonies.try_emplace(id, colony);
            if (!inserted) return std::nullopt;
            it->second.id = id;
            ++next_colony_id;
            return id;
        }

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
            it->second.id = id;
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
