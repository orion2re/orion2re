#pragma once

struct s_antaran;

#include <iterator>
#include <stdexcept>

#include "game_state.h"
#include "faction_ids.h"

namespace ship_slots {
    // saved_extent preserves the prefix restored by the disk codec while rebuilding its tail.
    void Allocate_Reservations(game::s_game_state& state, game::id_player local, int game_type,
        uint32_t stardate, const s_antaran& antarans, size_t saved_extent = 0);
    inline const s_ship_data& At(const game::s_game_state& state, size_t row) {
        return state.ships.at(state.ship_slots.at(row));
    }

    inline s_ship_data& At(game::s_game_state& state, size_t row) {
        return state.ships.at(state.ship_slots.at(row));
    }

    inline bool Occupied(const game::s_game_state& state, size_t row) {
        return row < state.ship_slots.size() && state.ship_slots[row] != game::NO_SHIP;
    }

    inline bool Available(const game::s_game_state& state, size_t row) {
        return !Occupied(state, row) || At(state, row).status == 5;
    }

    inline void Put(game::s_game_state& state, size_t row, s_ship_data ship) {
        auto& slot = state.ship_slots.at(row);
        if (slot == ship.id && ship.id != game::NO_SHIP) {
            state.ships.at(ship.id) = ship;
            return;
        }
        if (ship.id == game::NO_SHIP) {
            state.spare_ship_owners.at(row) = ship.owner;
        } else {
            if (!state.ships.emplace(ship.id, ship).second) throw std::logic_error("duplicate ship key");
            state.spare_ship_owners.at(row) = {};
        }
        if (slot != game::NO_SHIP) state.ships.erase(slot);
        slot = ship.id;
    }

    inline int Slot(const game::s_game_state& state, game::id_ship key) {
        if (key == game::NO_SHIP) return -1;
        const auto entry = std::ranges::find(state.ship_slots, key);
        return entry == state.ship_slots.end() ? -1 : static_cast<int>(entry - state.ship_slots.begin());
    }

    inline int Row(const game::s_game_state& state, game::id_ship key) { return Slot(state, key); }

    inline int Row_Of(const game::s_game_state& state, const s_ship_data& ship) {
        if (ship.id != game::NO_SHIP) {
            const auto found = state.ships.find(ship.id);
            return found != state.ships.end() && &found->second == &ship ? Slot(state, ship.id) : -1;
        }
        return -1;
    }

    inline void Resize(game::s_game_state& state, size_t extent) {
        for (size_t row = extent; row < state.ship_slots.size(); ++row) {
            if (const auto key = state.ship_slots[row]; key != game::NO_SHIP) state.ships.erase(key);
        }
        state.ship_slots.resize(extent, game::NO_SHIP);
    }

    template<class Visitor>
    void Visit_Records(game::s_game_state& state, Visitor visit) {
        for (auto& [key, ship] : state.ships) visit(ship);
    }

    inline s_ship_data Vacant(game::owner_slot owner = {}) {
        s_ship_data ship{};
        ship.owner = owner;
        ship.status = 5;
        ship.position.Set(game::NO_SHIP_LOCATION);
        ship.officer_id = game::NO_LEADER;
        return ship;
    }

    inline game::owner_slot Owner(const game::s_game_state& state, int row) {
        return Occupied(state, row) ? At(state, row).owner : state.spare_ship_owners.at(row);
    }

    inline void Reserve(game::s_game_state& state, int row, game::owner_slot owner) {
        if (Occupied(state, row)) At(state, row).owner = owner;
        else state.spare_ship_owners.at(row) = owner;
    }

    inline void Grow(game::s_game_state& state, int16_t extent) {
        if (extent > std::ssize(state.ship_slots)) state.ship_slots.resize(extent, game::NO_SHIP);
    }
}
