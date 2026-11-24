#pragma once

#include "pch.h"

#include <memory>
#include "../fixtures/planet_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/star_roster.h"
#include "game/legacy_ids.h"
#include "game/ship_ids.h"
#include "lua/lua_game_ship_identity.h"

namespace test {
    template<typename T>
    struct s_saved_value {
        T& target;
        T saved;

        explicit s_saved_value(T& value) : target(value), saved(value) {}
        ~s_saved_value() { target = saved; }
        s_saved_value(const s_saved_value&) = delete;
        s_saved_value& operator=(const s_saved_value&) = delete;
    };

    template<typename T, size_t N>
    struct s_saved_array {
        T (&target)[N];
        std::vector<T> saved;

        explicit s_saved_array(T (&value)[N]) : target(value), saved(value, value + N) {}
        ~s_saved_array() { std::copy(saved.begin(), saved.end(), target); }
        s_saved_array(const s_saved_array&) = delete;
        s_saved_array& operator=(const s_saved_array&) = delete;
    };

    struct s_game_state {
        std::array<s_player, MAX_PLAYERS>& players = game::current_state.players;
        game::s_ship_roster& ships = game::current_state.ship_records;
        test_stars::Slots<MAX_STARS + 1> stars;
        test_colonies::Slots<MAX_COLONIES> colonies;
        s_current_design design = {};

        std::unique_ptr<game::s_game_state> old_state = std::make_unique<game::s_game_state>(std::move(game::current_state));
        s_current_design* old_design = MOX::design;
        s_settings old_settings = MOX::settings;
        const game::id_player old_player_num = MOX::local_player;
        int32_t old_stardate = MOX::stardate;
        uint32_t old_seed = game_random::Get_Random_Seed();

        s_game_state() {
            game::current_state = std::move(*std::make_unique<game::s_game_state>());
            ships.resize(MAX_SHIPS);
            MOX::design = &design;
            MOX::settings = {};
            MOX::local_player = game::id_player{0};
            game::current_state.ship_count = 0;
            test_stars::Resize(0);
            MOX::stardate = 35000;
            for (auto& leader : game::current_state.leaders) {
                leader.employer = game::NO_PLAYER;
                leader.status = -1;
            }
        }

        ~s_game_state() {
            game::current_state = std::move(*old_state);
            MOX::design = old_design;
            MOX::settings = old_settings;
            MOX::local_player = old_player_num;
            MOX::stardate = old_stardate;
            game_random::Set_Random_Seed(old_seed);
        }

        s_game_state(const s_game_state&) = delete;
        s_game_state& operator=(const s_game_state&) = delete;
    };
} // namespace test
