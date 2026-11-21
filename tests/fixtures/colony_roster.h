#pragma once

#include "game/game_state.h"

namespace test_colonies {
    inline s_colony& Record(uint32_t key, game::s_game_state& state = game::current_state) {
        state.next_colony_id = std::max(state.next_colony_id, key + 1);
        return state.colonies[game::id_colony{key}];
    }

    inline void Reset(uint32_t count = 0) {
        game::current_state.Reset_Colonies();
        for (uint32_t i = 0; i < count; ++i) Record(i);
    }

    template <size_t N>
    struct Slots {
        s_colony& operator[](uint32_t key) const { return Record(key); }
        constexpr size_t size() const { return N; }
        void Reset() const { test_colonies::Reset(N < MAX_COLONIES ? N : 0); }
    };
}
