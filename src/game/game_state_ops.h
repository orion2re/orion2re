#pragma once

#include "game_state.h"

namespace game {
    inline void Reset_Stars(s_game_state& state) {
        state.stars.clear();
        state.star_order.clear();
        state.star_order_sorted = true;
        state.next_star_id = 0;
        state.antares_destination = {};
    }

    inline std::optional<id_star> Create_Star(s_game_state& state, s_star_data star = {}) {
        if (state.stars.size() >= MAX_STARS || state.next_star_id == NO_STAR.id) return std::nullopt;
        star.id = id_star{state.next_star_id};
        const auto [it, added] = state.stars.try_emplace(star.id, star);
        if (!added) return std::nullopt;
        try {
            state.star_order.push_back(star.id);
        } catch (...) {
            state.stars.erase(it);
            throw;
        }
        ++state.next_star_id;
        return star.id;
    }

    inline std::optional<id_star> Replace_Draft_Star(s_game_state& state, size_t position) {
        if (position >= state.star_order.size() || state.next_star_id == NO_STAR.id) return std::nullopt;
        const id_star key{state.next_star_id};
        if (!state.stars.try_emplace(key, s_star_data{.id = key}).second) return std::nullopt;
        state.stars.erase(state.star_order[position]);
        state.star_order[position] = key;
        state.star_order_sorted = std::is_sorted(state.star_order.begin(), state.star_order.end());
        ++state.next_star_id;
        return key;
    }

    inline void Rebuild_Star_Order(s_game_state& state) {
        state.star_order.clear();
        state.star_order.reserve(state.stars.size());
        for (const auto& [key, star] : state.stars) state.star_order.push_back(key);
        state.star_order_sorted = true;
    }

    inline void Reset_Colonies(s_game_state& state) {
        state.colonies.clear();
        state.next_colony_id = 0;
        for (auto& reservations : state.colony_reservations) reservations.clear();
    }

    inline std::optional<id_colony> Create_Colony(s_game_state& state, const s_colony& colony) {
        const id_colony key{state.next_colony_id};
        if (state.colonies.size() >= MAX_COLONIES || key == NO_COLONY) return std::nullopt;
        const auto [it, inserted] = state.colonies.try_emplace(key, colony);
        if (!inserted) return std::nullopt;
        it->second.id = key;
        ++state.next_colony_id;
        return key;
    }

    inline void Reset_Planets(s_game_state& state) {
        state.planets.clear();
        state.planet_order.clear();
        state.next_planet_id = 0;
    }

    inline std::optional<id_planet> Create_Planet(s_game_state& state, const s_planet_data& planet) {
        const id_planet key{state.next_planet_id};
        if (state.planets.size() >= MAX_PLANETS || key == NO_PLANET) return std::nullopt;
        const auto [it, inserted] = state.planets.try_emplace(key, planet);
        if (!inserted) return std::nullopt;
        it->second.id = key;
        try {
            state.planet_order.push_back(key);
        } catch (...) {
            state.planets.erase(it);
            throw;
        }
        ++state.next_planet_id;
        return key;
    }

    inline bool Erase_Planet(s_game_state& state, id_planet key) {
        if (state.planets.erase(key) == 0) return false;
        std::erase(state.planet_order, key);
        return true;
    }
}
