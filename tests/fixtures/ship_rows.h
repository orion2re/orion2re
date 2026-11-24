#pragma once

#include "game/ship_ids.h"

namespace test_ships {
    // Transitional row-based fixture setup; production owns a map and ID slots.
    struct s_rows {
        game::s_game_state& state;

        auto size() const { return state.ship_slots.size(); }
        bool empty() const { return state.ship_slots.empty(); }
        const s_ship_data& operator[](size_t row) const { return ship_slots::At(std::as_const(state), row); }
        s_ship_data& operator[](size_t row) {
            if (row >= size()) resize(row + 1);
            return ship_slots::At(state, row);
        }
        void resize(size_t extent) { ship_slots::Resize(state, extent); }
        void Replace_Row(size_t row, s_ship_data ship) {
            if (row >= size()) resize(row + 1);
            ship_slots::Put(state, row, ship);
        }
        void Set_Key_At_Row(size_t row, game::id_ship key) {
            if (row >= size()) resize(row + 1);
            ship_slots::Set_Key(state, row, key);
        }
        void Swap_Rows(size_t first, size_t second) {
            if (first == second) return;
            std::swap(state.ship_slots.at(first), state.ship_slots.at(second));
            auto a = state.legacy_ship_rows.extract(first);
            auto b = state.legacy_ship_rows.extract(second);
            if (a) { a.key() = second; state.legacy_ship_rows.insert(std::move(a)); }
            if (b) { b.key() = first; state.legacy_ship_rows.insert(std::move(b)); }
        }
        auto Copy_Rows() const {
            std::vector<s_ship_data> result;
            result.reserve(size());
            for (size_t i = 0; i < size(); ++i) result.push_back((*this)[i]);
            return result;
        }
        void assign(size_t count, const s_ship_data& ship) {
            resize(0);
            resize(count);
            if (ship == s_ship_data{}) return;
            for (size_t i = 0; i < count; ++i) Replace_Row(i, ship);
        }
        template<class Iterator>
        void assign(Iterator first, Iterator last) {
            resize(0);
            for (; first != last; ++first) Replace_Row(size(), *first);
        }
        const auto& Keyed_Records() const { return state.ships; }
        const auto& Unkeyed_Rows() const { return state.legacy_ship_rows; }
        int Row_Of(const s_ship_data& ship) const { return ship_slots::Row_Of(state, ship); }
    };

    inline s_rows Rows(game::s_game_state& state = game::current_state) { return {state}; }
}
