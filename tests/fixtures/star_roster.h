#pragma once

#include <cstring>

#include "game/game_state.h"

namespace test_stars {
    inline const s_star_data& At_Position(int32_t position) {
        const auto& state = game::current_state;
        return state.stars.at(game::Star_Key_At_Row(state, position));
    }

    inline s_star_data& Record(uint32_t key, game::s_game_state& state = game::current_state) {
        const game::id_star id{key};
        state.next_star_id = std::max(state.next_star_id, key + 1);
        const auto [it, inserted] = state.stars.try_emplace(id, s_star_data{.id = id});
        if (inserted) state.star_order.insert(std::lower_bound(state.star_order.begin(), state.star_order.end(), id), id);
        return it->second;
    }

    inline s_star_data& Set(uint32_t key, s_star_data star, game::s_game_state& state = game::current_state) {
        star.id = game::id_star{key};
        return Record(key, state) = star;
    }

    inline void Resize(uint32_t count) {
        auto& state = game::current_state;
        while (state.star_order.size() > count) {
            state.stars.erase(state.star_order.back());
            state.star_order.pop_back();
        }
        while (state.star_order.size() < count) {
            const game::id_star key{static_cast<uint32_t>(state.star_order.size())};
            Record(state.stars.contains(key) ? state.next_star_id : key.id);
        }
    }

    inline void Reset(uint32_t count = 0, bool erased_fields = false) {
        game::Reset_Stars(game::current_state);
        for (uint32_t i = 0; i < count; ++i) {
            auto& star = Record(i);
            if (erased_fields) {
                std::memset(&star, 0xff, sizeof(star));
                star.id = game::id_star{i};
            }
        }
    }

    template<size_t N>
    struct Slots {
        struct Iterator {
            uint32_t index;
            s_star_data& operator*() const { return Record(index); }
            void operator++() { ++index; }
            bool operator==(const Iterator&) const = default;
        };
        s_star_data& operator[](uint32_t key) const { return Record(key); }
        constexpr size_t size() const { return N; }
        void Reset() const { test_stars::Reset(N < MAX_STARS ? N : 0); }
        Iterator begin() const { return {0}; }
        Iterator end() const { return {N}; }
    };
}
