#pragma once

#include <iterator>
#include <stdexcept>

#include "game_state.h"
#include "faction_ids.h"

namespace ship_slots {
    inline const s_ship_data& At(const game::s_game_state& state, size_t row) {
        const auto key = state.ship_slots.at(row);
        if (key != game::NO_SHIP) return state.ships.at(key);
        static const s_ship_data empty{};
        const auto legacy = state.legacy_ship_rows.find(row);
        return legacy == state.legacy_ship_rows.end() ? empty : legacy->second;
    }

    inline s_ship_data& At(game::s_game_state& state, size_t row) {
        const auto key = state.ship_slots.at(row);
        if (key != game::NO_SHIP) return state.ships.at(key);
        return state.legacy_ship_rows.try_emplace(row).first->second;
    }

    inline void Put(game::s_game_state& state, size_t row, s_ship_data ship) {
        auto& slot = state.ship_slots.at(row);
        if (slot == ship.id && ship.id != game::NO_SHIP) {
            state.ships.at(ship.id) = ship;
            return;
        }
        if (ship.id == game::NO_SHIP) {
            if (ship == s_ship_data{}) state.legacy_ship_rows.erase(row);
            else state.legacy_ship_rows.insert_or_assign(row, ship);
        } else {
            if (!state.ships.emplace(ship.id, ship).second) throw std::logic_error("duplicate ship key");
            state.legacy_ship_rows.erase(row);
        }
        if (slot != game::NO_SHIP) state.ships.erase(slot);
        slot = ship.id;
    }

    inline int Row(const game::s_game_state& state, game::id_ship key) {
        if (key == game::NO_SHIP) return -1;
        const auto entry = std::ranges::find(state.ship_slots, key);
        return entry == state.ship_slots.end() ? -1 : static_cast<int>(entry - state.ship_slots.begin());
    }

    inline int Row_Of(const game::s_game_state& state, const s_ship_data& ship) {
        if (ship.id != game::NO_SHIP) {
            const auto found = state.ships.find(ship.id);
            return found != state.ships.end() && &found->second == &ship ? Row(state, ship.id) : -1;
        }
        for (const auto& [row, legacy] : state.legacy_ship_rows) if (&legacy == &ship) return static_cast<int>(row);
        return -1;
    }

    inline void Resize(game::s_game_state& state, size_t extent) {
        for (size_t row = extent; row < state.ship_slots.size(); ++row) {
            if (const auto key = state.ship_slots[row]; key != game::NO_SHIP) state.ships.erase(key);
        }
        state.legacy_ship_rows.erase(state.legacy_ship_rows.lower_bound(extent), state.legacy_ship_rows.end());
        state.ship_slots.resize(extent, game::NO_SHIP);
    }

    inline void Set_Key(game::s_game_state& state, size_t row, game::id_ship key) {
        auto ship = At(std::as_const(state), row);
        ship.id = key;
        Put(state, row, ship);
    }

    template<class Visitor>
    void Visit_Records(game::s_game_state& state, Visitor visit) {
        for (auto& [key, ship] : state.ships) visit(ship);
        for (auto& [row, ship] : state.legacy_ship_rows) visit(ship);
    }

    inline s_ship_data Vacant(game::id_faction owner = {}) {
        s_ship_data ship{};
        ship.owner = owner;
        ship.status = 5;
        ship.position.Set(game::NO_SHIP_LOCATION);
        ship.officer_index = -1;
        return ship;
    }

    inline game::id_faction Owner(const game::s_game_state& state, int row) {
        return row < std::ssize(state.ship_slots) ? At(state, row).owner : state.spare_ship_owners.at(row);
    }

    inline void Reserve(game::s_game_state& state, int row, game::id_faction owner) {
        if (row < std::ssize(state.ship_slots)) At(state, row).owner = owner;
        else state.spare_ship_owners.at(row) = owner;
    }

    inline void Grow(game::s_game_state& state, int16_t extent) {
        const auto old_extent = std::ssize(state.ship_slots);
        if (extent <= old_extent) return;
        state.ship_slots.resize(extent, game::NO_SHIP);
        for (int row = old_extent; row < extent; ++row) {
            state.legacy_ship_rows.insert_or_assign(row, Vacant(state.spare_ship_owners[row]));
            state.spare_ship_owners[row] = {};
        }
    }
}
