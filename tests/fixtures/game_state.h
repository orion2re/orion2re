#pragma once

#include "pch.h"
#include "../fixtures/planet_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/star_roster.h"
#include "game/legacy_world.h"
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
        std::vector<s_player> players = std::vector<s_player>(MAX_PLAYERS);
        game::s_ship_roster& ships = game::current_state.ship_records;
        test_stars::Slots<MAX_STARS + 1> stars;
        test_colonies::Slots<MAX_COLONIES> colonies;
        s_current_design design = {};

        s_player* old_players = MOX::player;
        game::s_game_state old_state = std::move(game::current_state);
        s_current_design* old_design = MOX::design;
        s_settings old_settings = MOX::settings;
        int16_t old_player_num = MOX::PLAYER_NUM;
        int16_t old_player_count = MOX::NUM_PLAYERS;
        int32_t old_stardate = MOX::stardate;
        uint32_t old_seed = game_random::Get_Random_Seed();
        game::s_view old_world = game::current;

        s_game_state() {
            MOX::player = players.data();
            game::current_state = game::s_game_state{};
            ships.resize(MAX_SHIPS);
            MOX::design = &design;
            MOX::settings = {};
            MOX::PLAYER_NUM = 0;
            MOX::NUM_PLAYERS = 0;
            game::current_state.ship_count = 0;
            test_stars::Resize(0);
            MOX::stardate = 35000;
            for (auto& leader : game::current_state.leaders) {
                leader.player_index = -1;
                leader.status = -1;
            }
            game::current = legacy_world::World();
        }

        ~s_game_state() {
            MOX::player = old_players;
            game::current_state = std::move(old_state);
            MOX::design = old_design;
            MOX::settings = old_settings;
            MOX::PLAYER_NUM = old_player_num;
            MOX::NUM_PLAYERS = old_player_count;
            MOX::stardate = old_stardate;
            game_random::Set_Random_Seed(old_seed);
            game::current = old_world;
        }

        s_game_state(const s_game_state&) = delete;
        s_game_state& operator=(const s_game_state&) = delete;
    };
} // namespace test
