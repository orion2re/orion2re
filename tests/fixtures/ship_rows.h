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
            if (!ship_slots::Occupied(state, row)) {
                Replace_Row(row, s_ship_data{.id = Implicit_Key_(row)});
            }
            return ship_slots::At(state, row);
        }
        void resize(size_t extent) { ship_slots::Resize(state, extent); }
        void Replace_Row(size_t row, s_ship_data ship) {
            if (row >= size()) resize(row + 1);
            if (ship.id == game::NO_SHIP && ship.status != 5 && ship != s_ship_data{}) {
                ship.id = ship_slots::Occupied(state, row) ? state.ship_slots[row] : Implicit_Key_(row);
            }
            ship_slots::Put(state, row, ship);
        }
        void Set_Key_At_Row(size_t row, game::id_ship key) {
            if (row >= size()) resize(row + 1);
            auto ship = ship_slots::Occupied(state, row) ? ship_slots::At(state, row) : s_ship_data{};
            ship.id = key;
            ship_slots::Put(state, row, ship);
        }
        void Swap_Rows(size_t first, size_t second) {
            if (first == second) return;
            std::swap(state.ship_slots.at(first), state.ship_slots.at(second));
            if (first < MAX_SHIPS && second < MAX_SHIPS) {
                std::swap(state.spare_ship_owners[first], state.spare_ship_owners[second]);
            }
        }
        auto Copy_Rows() const {
            std::vector<s_ship_data> result;
            result.reserve(size());
            for (size_t i = 0; i < size(); ++i) {
                result.push_back(ship_slots::Occupied(state, i) ? (*this)[i] : ship_slots::Vacant(ship_slots::Owner(state, i)));
            }
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
        int Row_Of(const s_ship_data& ship) const { return ship_slots::Row_Of(state, ship); }

    private:
        game::id_ship Implicit_Key_(size_t row) const {
            game::id_ship key{static_cast<uint32_t>(row)};
            while (state.ships.contains(key)) key.id += MAX_SHIPS;
            return key;
        }
    };

    inline s_rows Rows(game::s_game_state& state = game::current_state) { return {state}; }
}
