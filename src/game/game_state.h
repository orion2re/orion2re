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
#include "star_data.h"

namespace game {
    struct s_game_state {
        // A leader's ID is its fixed catalog slot, including inactive leaders.
        std::array<s_leader_data, MAX_LEADERS> leaders = [] {
            std::array<s_leader_data, MAX_LEADERS> catalog{};
            for (uint32_t i = 0; i < catalog.size(); ++i) catalog[i].id = id_leader{i};
            return catalog;
        }();

        std::map<id_star, s_star_data> stars;
        // Ordered projection for positional tables and transitional interfaces.
        // Rebuilding it does not change the identity of any live record.
        std::vector<id_star> star_order;
        bool star_order_sorted = true;
        uint32_t next_star_id = 0;
        // The legacy ship destination encoding gives Antares a numeric slot,
        // but Antares is not a galaxy star and does not get a canonical ID.
        s_star_data antares_destination{};

        void Reset_Stars() {
            stars.clear();
            star_order.clear();
            star_order_sorted = true;
            next_star_id = 0;
            antares_destination = {};
        }

        std::optional<id_star> Create_Star(s_star_data star = {}) {
            if (stars.size() >= MAX_STARS || next_star_id == NO_STAR.id) return std::nullopt;
            star.id = id_star{next_star_id};
            const auto [it, added] = stars.try_emplace(star.id, star);
            if (!added) return std::nullopt;
            try {
                star_order.push_back(star.id);
            } catch (...) {
                stars.erase(it);
                throw;
            }
            ++next_star_id;
            return star.id;
        }

        id_star Star_Id_At_Position(int32_t position) const {
            if (position < 0 || static_cast<size_t>(position) >= star_order.size()) return NO_STAR;
            return star_order[position];
        }

        int16_t Star_Count() const { return static_cast<int16_t>(star_order.size()); }

        std::optional<id_star> Find_Star_At(int16_t x, int16_t y) const {
            for (const auto& [id, star] : stars) {
                if (star.x == x && star.y == y) return id;
            }
            return std::nullopt;
        }

        int16_t Star_Position(id_star id) const {
            const auto it = star_order_sorted ? std::lower_bound(star_order.begin(), star_order.end(), id)
                                              : std::find(star_order.begin(), star_order.end(), id);
            return it != star_order.end() && *it == id ? static_cast<int16_t>(it - star_order.begin()) : -1;
        }

        std::optional<id_star> Replace_Draft_Star(size_t position) {
            if (position >= star_order.size() || next_star_id == NO_STAR.id) return std::nullopt;
            const id_star id{next_star_id};
            if (!stars.try_emplace(id, s_star_data{.id = id}).second) return std::nullopt;
            stars.erase(star_order[position]);
            star_order[position] = id;
            star_order_sorted = std::is_sorted(star_order.begin(), star_order.end());
            ++next_star_id;
            return id;
        }

        void Rebuild_Star_Order() {
            star_order.clear();
            star_order.reserve(stars.size());
            for (const auto& [id, star] : stars) star_order.push_back(id);
            star_order_sorted = true;
        }

        std::map<id_planet, s_planet_data> planets;
        // Traversal order only; relationships and lookups use the map key.
        std::vector<id_planet> planet_order;
        uint32_t next_planet_id = 0;

        std::map<id_colony, s_colony> colonies;
        uint32_t next_colony_id = 0;
        // Traversal order only; relationships and lookups use the map key.
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
    inline int16_t Current_Star_Count() { return current_state.Star_Count(); }
} // namespace game

inline int16_t s_planet_data::Star_Position(const game::s_game_state& state) const {
    return state.Star_Position(star_id);
}

inline void s_planet_data::Set_Star_Position(int32_t position, const game::s_game_state& state) {
    star_id = state.Star_Id_At_Position(position);
}

inline int16_t s_leader_data::Location_Position(const game::s_game_state& state) const {
    return star_location == game::NO_STAR ? location : state.Star_Position(star_location);
}
