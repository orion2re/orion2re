#include "pch.h"

namespace lostship {
    void Mark_Out_Of_Range_Ships(const game::s_view& world, int16_t player_idx) {
        enum class e_range { outside, extended, basic };
        const int16_t range = world.players[player_idx].ship_range;
        const int16_t extended = (range * 3 + 1) / 2;
        std::map<game::id_star, e_range> fuel_range;
        for (const auto& [key, star] : world.state->stars) {
            auto reach = e_range::outside;
            if (SHIPMOVE::Star_In_Range_Of_Player(world, star, player_idx, extended)) {
                reach = SHIPMOVE::Star_In_Range_Of_Player(world, star, player_idx, range)
                    ? e_range::basic : e_range::extended;
            }
            fuel_range.emplace(key, reach);
        }

        auto& ships = game::Ship_Rows(world);
        for (int row = 0; row < world.Ship_Count(); ++row) {
            auto& ship = ships[row];
            if (ship.status != 0 || ship.owner != player_idx ||
                ship.position.kind != game::e_ship_location::at_star ||
                ship.position.target != game::e_ship_target::star) continue;
            const auto reach = fuel_range.find(ship.position.star_id.get());
            if (reach == fuel_range.end()) continue;
            if (reach->second == e_range::outside || (reach->second == e_range::extended &&
                !struct_::Test_Bit_Field(ship.d.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS))) {
                ship.status = 9;
            }
        }
    }

    void __cdecl Lose_Out_Of_Range_Ships() {
        const game::s_view world{.players = MOX::player, .state = &game::current_state,
            .player_count = &MOX::NUM_PLAYERS};
        for (int16_t player_idx = 0; player_idx < world.Player_Count(); ++player_idx) {
            Mark_Out_Of_Range_Ships(world, player_idx);
            COMBFIND::Process_Retreating_Ships();
        }
    }
}
