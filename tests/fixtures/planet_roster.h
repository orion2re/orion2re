#pragma once

#include "game/game_state.h"

namespace test_planets {
    inline s_planet_data& Record(uint32_t key) {
        const game::id_planet id{key};
        auto& state = game::current_state;
        auto [it, inserted] = state.planets.try_emplace(id);
        if (inserted) {
            it->second.id = id;
            state.planet_order.push_back(id);
            state.next_planet_id = std::max(state.next_planet_id, key + 1);
        }
        return it->second;
    }

    inline s_planet_data& Set(uint32_t key, s_planet_data planet) {
        planet.id = game::id_planet{key};
        return Record(key) = planet;
    }

    inline void Resize_Dense(uint32_t count) {
        auto& state = game::current_state;
        std::erase_if(state.planets, [count](const auto& entry) { return entry.first.id >= count; });
        std::erase_if(state.planet_order, [count](auto id) { return id.id >= count; });
        for (uint32_t i = 0; i < count; ++i) Record(i);
    }

    inline void Reset(uint32_t count = 0) {
        game::current_state.Reset_Planets();
        Resize_Dense(count);
    }
}
