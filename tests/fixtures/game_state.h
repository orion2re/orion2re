#pragma once

#include "pch.h"
#include "lua/lua_game_ship_identity.h"

namespace test {
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
        std::vector<s_ship_data> ships = std::vector<s_ship_data>(MAX_SHIPS);
        std::vector<s_star_data> stars = std::vector<s_star_data>(MAX_STARS + 1);
        std::vector<s_planet_data> planets = std::vector<s_planet_data>(MAX_PLANETS);
        std::vector<s_colony> colonies = std::vector<s_colony>(MAX_COLONIES);
        std::vector<s_leader_data> leaders = std::vector<s_leader_data>(MAX_LEADERS);
        s_current_design design = {};
        std::vector<uint64_t> generations = std::vector<uint64_t>(MAX_SHIPS);
        orion2re::lua::game::s_ship_generation_scope generation_scope{generations.data()};

        s_player* old_players = MOX::player;
        s_ship_data* old_ships = MOX::ship;
        s_star_data* old_stars = MOX::star;
        s_planet_data* old_planets = MOX::planet;
        s_colony* old_colonies = MOX::colony;
        s_leader_data* old_leaders = MOX::leaders;
        s_current_design* old_design = MOX::design;
        s_settings old_settings = MOX::settings;
        int16_t old_player_num = MOX::PLAYER_NUM;
        int16_t old_player_count = MOX::NUM_PLAYERS;
        int16_t old_ship_count = MOX::NUM_SHIPS;
        int16_t old_star_count = MOX::NUM_STARS;
        int16_t old_planet_count = MOX::planet_count;
        int16_t old_colony_count = MOX::NUM_COLONIES;
        int32_t old_stardate = MOX::stardate;
        uint32_t old_seed = game_random::Get_Random_Seed();

        s_game_state() {
            MOX::player = players.data();
            MOX::ship = ships.data();
            MOX::star = stars.data();
            MOX::planet = planets.data();
            MOX::colony = colonies.data();
            MOX::leaders = leaders.data();
            MOX::design = &design;
            MOX::settings = {};
            MOX::PLAYER_NUM = 0;
            MOX::NUM_PLAYERS = 0;
            MOX::NUM_SHIPS = 0;
            MOX::NUM_STARS = 0;
            MOX::planet_count = 0;
            MOX::NUM_COLONIES = 0;
            MOX::stardate = 35000;
            for (auto& leader : leaders) {
                leader.player_index = -1;
                leader.status = -1;
            }
        }

        ~s_game_state() {
            MOX::player = old_players;
            MOX::ship = old_ships;
            MOX::star = old_stars;
            MOX::planet = old_planets;
            MOX::colony = old_colonies;
            MOX::leaders = old_leaders;
            MOX::design = old_design;
            MOX::settings = old_settings;
            MOX::PLAYER_NUM = old_player_num;
            MOX::NUM_PLAYERS = old_player_count;
            MOX::NUM_SHIPS = old_ship_count;
            MOX::NUM_STARS = old_star_count;
            MOX::planet_count = old_planet_count;
            MOX::NUM_COLONIES = old_colony_count;
            MOX::stardate = old_stardate;
            game_random::Set_Random_Seed(old_seed);
        }

        s_game_state(const s_game_state&) = delete;
        s_game_state& operator=(const s_game_state&) = delete;
    };
} // namespace test
