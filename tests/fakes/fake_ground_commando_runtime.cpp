#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/officer_roster.h"

namespace {
    auto& players = game::current_state.players;
    test_colonies::Slots<2> colonies;

    test_stars::Slots<1> stars;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        rows.resize(2);
        return rows;
    }();
    int16_t officer_ids[MAX_LEADERS];
    int colony_officer;
    s_animation_header animation;
    int32_t last_buffer_entry;
    uint32_t random_result;
    int random_call_count;
    int granted_tech_count;
}

namespace test_ground_commando_runtime {
    std::vector<int16_t> killed_ships;
    std::vector<s_MSG> messages;
    std::vector<int16_t> message_players;
    int stats_updates;
    int16_t stats_player;
    int icon_updates;
    int8_t owner_at_icon_update;
    int seed_queries;
    uint32_t random_seed;
    int rebel_choices;
    uint8_t rebel_weights[MAX_PLAYERS];

    void Reset_() {
        killed_ships.clear();
        messages.clear();
        message_players.clear();
        stats_updates = icon_updates = seed_queries = rebel_choices = 0;
        stats_player = owner_at_icon_update = -1;
        random_seed = 0x1234;
        memset(rebel_weights, 0, sizeof(rebel_weights));
        OFFICER::leader_page = 0;
        game::current_state = game::s_game_state{};
        game::current_state.player_count = 2;
        ship_slots::Resize(game::current_state, 2);
        ships.resize(2);
        colonies.Reset();
        test_planets::Reset(1);
        stars.Reset();
        ships.assign(2, s_ship_data{});
        memset(&animation, 0, sizeof(animation));
        colonies[0].planet_id = game::id_planet{0};
        colonies[0].owner = legacy_world::Decode_Player_Slot(0);
        test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
        players[0].home_planet_id = game::NO_PLANET;
        players[1].home_planet_id = game::NO_PLANET;
        for (int skill_idx = 0; skill_idx < 54; ++skill_idx) {
            MOX::skill_data[skill_idx].cost = 0;
        }
        MOX::skill_data[0].cost = 2;
        MOX::skill_data[1].cost = 4;
        MOX::skill_data[28].cost = 1;
        MOX::skill_data[29].cost = 2;
        MOX::skill_data[28].skill_mask = LEADER_GENERAL_SKILL_MEGAWEALTH;
        MOX::skill_data[29].skill_mask = LEADER_GENERAL_SKILL_MEGAWEALTH2;
        for (int i = 0; i < MAX_LEADERS; ++i) {
            auto& leader = game::current_state.leaders[i];
            leader.status = -1;
            leader.employer = game::NO_PLAYER;
            leader.location = -1;
            officer_ids[i] = -1;
        }
        MOX::local_player = game::id_player{0};
        MOX::officer_scrn_type = LEADER_TYPE_SHIP;
        MOX::n_fltscrn_officers = 0;
        OFFICER::leaders_no_limit = 0;
        COLONY::allow_native_annihilation = 0;
        OFFICER::commando_hire_cost = {1, 3};
        OFFICER::megawealth_doubles_hire_cost = 1;
        colony_officer = -1;
        last_buffer_entry = -1;
        random_result = 1;
        random_call_count = 0;
        granted_tech_count = 0;
    }

    s_colony* Colony_(int index) {
        return &colonies[index];
    }

    s_leader_data* Leader_(int index) {
        return &game::current_state.leaders.at(index);
    }

    s_ship_data* Ship_(int index) {
        return &ships[index];
    }

    void Set_Colony_Officer_(int leader_idx) {
        colony_officer = leader_idx;
    }

    void Set_Officer_Level_(int16_t level) {
        const int16_t level_thresholds[] = {0, 60, 150, 300, 500, 1000};
        int leader_idx = colony_officer >= 0 ? colony_officer : 0;
        auto& leader = game::current_state.leaders.at(leader_idx);
        if (leader.employer == game::NO_PLAYER) leader.employer = game::id_player{0};
        leader.xp = level_thresholds[level];
    }

    void Set_Leader_(int index, int8_t player, int8_t type, int8_t status) {
        auto& leader = game::current_state.leaders.at(index);
        leader.employer = legacy_world::Decode_Player_Slot(player);
        leader.type = type;
        leader.status = status;
    }

    const int16_t* Officer_Ids_() {
        return officer_ids;
    }

    int32_t Last_Buffer_Entry_() {
        return last_buffer_entry;
    }

    void Set_Random_Result_(uint32_t value) {
        random_result = value;
    }

    int Random_Call_Count_() {
        return random_call_count;
    }

    int Granted_Tech_Count_() {
        return granted_tech_count;
    }
} // namespace test_ground_commando_runtime

namespace buffer {
    void* __cdecl Buffer_Reload(const char*, int32_t entry, void*) {
        last_buffer_entry = entry;
        return &animation;
    }
} // namespace buffer

namespace MOX {

    void* global_cache_seg;

    s_settings settings = {};
    game::id_player local_player{0};
    int16_t n_fltscrn_officers = 0;
    int16_t* officer_id_list = officer_ids;
    int8_t officer_scrn_type = LEADER_TYPE_SHIP;
    s_skill_data skill_data[54] = {
        {0, LEADER_GENERAL_SKILL_ASSASSIN, 0, 0, 1, 2, nullptr, nullptr},
        {1, LEADER_GENERAL_SKILL_ASSASSIN2, 0, 0, 1, 4, nullptr, nullptr},
        {2, LEADER_GENERAL_SKILL_COMMANDO, 0, 0, 1, 0, nullptr, nullptr},
        {3, LEADER_GENERAL_SKILL_COMMANDO2, 0, 0, 1, 0, nullptr, nullptr},
    };
}

namespace TECHDATA {
    s_tech_armor_data armor[ARMOR_COUNT] = {};
    s_tech_data_rifle rifles[6] = {};
    s_tech_data_personal_shield personal_shields[2] = {};
    s_tech_app_data technology_applications[TECH_APP_COUNT] = {};
    s_tech_field_data technology_fields[TECH_FIELD_COUNT] = {};
    s_tech_building_data buildings[BUILDING_COUNT] = {};
}

namespace aitech {
    int32_t __cdecl Player_Best_Armor(const s_tech_armor_data*, const s_player*) {
        return 0;
    }

    int32_t __cdecl Player_Best_Rifle(const s_tech_data_rifle*, const s_player*) {
        return 0;
    }

    int32_t __cdecl Player_Best_Personal_Shield(const s_tech_data_personal_shield*, const s_player*) {
        return 0;
    }
}

namespace COLCALC {
    void __cdecl Update_Player_Stats(s_player* player) {
        ++test_ground_commando_runtime::stats_updates;
        test_ground_commando_runtime::stats_player = (int16_t)(player - game::current_state.players.data());
    }

    s_population_capacity population_capacity = {
        {5, 10, 15, 20, 25},
        {25, 25, 25, 25, 25, 25, 40, 60, 80, 100},
    };

    game::id_leader __cdecl Colony_Officer(const s_colony*) {
        return leader_slots::Decode(colony_officer);
    }

    int16_t __cdecl Colony_Race_Pop_Limit(const s_colony*, int16_t) {
        return MAX_COLONY_POPULATION;
    }

    int16_t __cdecl Colony_Food2_Per_Farmer(
        const s_colony*, game::id_player, s_colony_job_production*) {
        return 1;
    }

    bool __cdecl Player_May_Get_Tech_App(s_player* player, int tech_app) {
        return player->tech_applications[tech_app] !=
            TECH_RESEARCH_STATUS_RESEARCHED;
    }

    void __cdecl Player_Gets_Tech_App(s_player* player, int16_t tech_app) {
        player->tech_applications[tech_app] = TECH_RESEARCH_STATUS_RESEARCHED;
        ++granted_tech_count;
    }

    void __cdecl Do_Colony_Owner_Change_Star_Colony_Stuff(s_colony*) {
    }

    void __cdecl Colony_Environmental_Stuff(s_colony*) {
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {
    }
}

namespace RUSS {
    int16_t __cdecl Get_Weighted_Choice_Char(uint8_t* weights, int16_t count) {
        DOCTEST_REQUIRE(count == game::current_state.player_count);
        DOCTEST_REQUIRE(count <= MAX_PLAYERS);
        ++test_ground_commando_runtime::rebel_choices;
        std::copy_n(weights, count, test_ground_commando_runtime::rebel_weights);
        for (int16_t i = 0; i < count; ++i) {
            if (weights[i] != 0) {
                return i;
            }
        }
        return -1;
    }

    void __cdecl Shuffle(void*, int16_t, int16_t) {
    }

    int16_t __cdecl Get_Weighted_Choice_Short(
        const uint16_t* weights, int16_t count) {
        for (int16_t index = 0; index < count; ++index) {
            if (weights[index] != 0) {
                return index;
            }
        }
        return -1;
    }
} // namespace RUSS

namespace game_random {
    uint32_t __cdecl Get_Random_Seed() {
        ++test_ground_commando_runtime::seed_queries;
        return test_ground_commando_runtime::random_seed;
    }

    uint32_t __cdecl Random(uint32_t) {
        ++random_call_count;
        return random_result;
    }
} // namespace game_random

namespace SHIPS {
    void Kill_Ship(s_ship_data& ship) {
        const auto ship_idx = test_ships::Rows(game::current_state).Row_Of(ship);
        DOCTEST_REQUIRE(ship_idx >= 0);
        DOCTEST_REQUIRE(ship_idx < 2);
        test_ground_commando_runtime::killed_ships.push_back(ship_idx);
        ship.status = 5;
    }
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        ++test_ground_commando_runtime::icon_updates;
        test_ground_commando_runtime::owner_at_icon_update = legacy_world::Encode_Player_Slot(test_colonies::Record(0).owner);
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG message, game::id_player player) {
        test_ground_commando_runtime::messages.push_back(message);
        test_ground_commando_runtime::message_players.push_back(legacy_world::Encode_Player_Slot(player));
    }
}

namespace COMBINIT {
    int16_t __cdecl Take_Captured_Satellite(game::id_colony) {
        return -1;
    }
} // namespace COMBINIT

namespace ERICNET {
    void __cdecl Add_Building(game::id_colony, int16_t) {
    }

    void __cdecl Remove_Building(game::s_game_state&, game::id_colony id, int16_t building_idx) {
        const auto colony_idx = static_cast<int16_t>(id.id);
        test_colonies::Record(colony_idx).buildings[building_idx] = 0;
    }
} // namespace ERICNET

namespace BOMB {
    game::id_leader __cdecl Handle_Leader_At_Lost_Colony(const game::s_game_state&, game::id_colony) {
        return game::NO_LEADER;
    }
} // namespace BOMB

namespace COLBLDG {
    void __cdecl Clear_Colony_Queue(game::s_game_state&, game::id_colony) {
    }
} // namespace COLBLDG
