#include "pch.h"
#include "player_roster.h"

namespace lostship {
    void Mark_Out_Of_Range_Ships(game::s_game_state& state, game::id_player player) {
        enum class e_range { outside, extended, basic };
        const auto* record = player_roster::Find(state, player);
        if (!record) return;
        const int16_t range = record->ship_range;
        const int16_t extended = (range * 3 + 1) / 2;
        std::map<game::id_star, e_range> fuel_range;
        for (const auto& [key, star] : state.stars) {
            auto reach = e_range::outside;
            if (SHIPMOVE::Star_In_Range_Of_Player(state, star, player, extended)) {
                reach = SHIPMOVE::Star_In_Range_Of_Player(state, star, player, range)
                    ? e_range::basic : e_range::extended;
            }
            fuel_range.emplace(key, reach);
        }

        for (int row = 0; row < static_cast<int16_t>(state.ship_slots.size()); ++row) {
            if (state.ship_slots[row] == ::game::NO_SHIP) continue;
            auto& ship = ship_slots::At(state, row);
            if (ship.status != 0 || player == game::NO_PLAYER || ship.owner != factions::From_Player(player) ||
                ship.position.kind != game::e_ship_location::at_star ||
                ship.position.target != game::e_ship_target::star) continue;
            const auto reach = fuel_range.find(ship.position.star_id);
            if (reach == fuel_range.end()) continue;
            if (reach->second == e_range::outside || (reach->second == e_range::extended &&
                !struct_::Test_Bit_Field(ship.d.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS))) {
                ship.status = 9;
            }
        }
    }

    void __cdecl Lose_Out_Of_Range_Ships() {
        auto& state = game::current_state;
        for (int slot = 0; slot < state.player_count; ++slot) {
            Mark_Out_Of_Range_Ships(state, game::id_player{static_cast<uint32_t>(slot)});
            COMBFIND::Process_Retreating_Ships();
        }
    }
}
