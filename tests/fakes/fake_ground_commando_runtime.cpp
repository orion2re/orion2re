#include "pch.h"

#include "doctest/doctest.h"
#include "game/officer_roster.h"

namespace {
    s_leader_data leaders[MAX_LEADERS];
    s_player players[2];
    s_colony colonies[2];
    s_planet_data planets[1];
    s_star_data stars[1];
    s_ship_data ships[2];
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
        MOX::NUM_PLAYERS = 2;
        MOX::NUM_SHIPS = 2;
        OFFICER::leader_page = 0;
        memset(leaders, 0, sizeof(leaders));
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(stars, 0, sizeof(stars));
        memset(ships, 0, sizeof(ships));
        memset(&animation, 0, sizeof(animation));
        colonies[0].planet = 0;
        MOX::NUM_COLONIES = 1;
        colonies[0].owner = 0;
        planets[0].star_index = 0;
        players[0].home_planet_id = -1;
        players[1].home_planet_id = -1;
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
            leaders[i].status = -1;
            leaders[i].player_index = -1;
            leaders[i].location = -1;
            officer_ids[i] = -1;
        }
        MOX::PLAYER_NUM = 0;
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
        return &leaders[index];
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
        leaders[leader_idx].xp = level_thresholds[level];
    }

    void Set_Leader_(int index, int8_t player, int8_t type, int8_t status) {
        leaders[index].player_index = player;
        leaders[index].type = type;
        leaders[index].status = status;
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
    s_leader_data* leaders = ::leaders;
    s_player* player = players;
    s_colony* colony = colonies;
    s_star_data* star = stars;
    void* global_cache_seg;
    s_planet_data* planet = planets;
    s_ship_data* ship = ships;
    s_settings settings = {};
    int16_t NUM_COLONIES = 1;
    int16_t NUM_PLAYERS = 2;
    int16_t NUM_SHIPS = 2;
    int16_t PLAYER_NUM = 0;
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
    int32_t __cdecl Player_Best_Armor(s_player*) {
        return 0;
    }

    int32_t __cdecl Player_Best_Rifle(s_player*) {
        return 0;
    }

    int32_t __cdecl Player_Best_Personal_Shield(s_player*) {
        return 0;
    }
}

namespace COLCALC {
    void __cdecl Update_Player_Stats(s_player* player) {
        ++test_ground_commando_runtime::stats_updates;
        test_ground_commando_runtime::stats_player = (int16_t)(player - MOX::player);
    }

    s_population_capacity population_capacity = {
        {5, 10, 15, 20, 25},
        {25, 25, 25, 25, 25, 25, 40, 60, 80, 100},
    };

    int __cdecl Colony_Officer(s_colony*) {
        return colony_officer;
    }

    int16_t __cdecl Colony_Race_Pop_Limit(s_colony*, int16_t) {
        return MAX_COLONY_POPULATION;
    }

    int16_t __cdecl Colony_Food2_Per_Farmer(
        const s_colony*, int16_t, s_colony_job_production*) {
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
        DOCTEST_REQUIRE(count == MOX::NUM_PLAYERS);
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
    void __cdecl Kill_Ship(int16_t ship_idx) {
        DOCTEST_REQUIRE(ship_idx >= 0);
        DOCTEST_REQUIRE(ship_idx < 2);
        test_ground_commando_runtime::killed_ships.push_back(ship_idx);
        MOX::ship[ship_idx].status = 5;
    }
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        ++test_ground_commando_runtime::icon_updates;
        test_ground_commando_runtime::owner_at_icon_update = MOX::colony[0].owner;
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG message, int16_t player) {
        test_ground_commando_runtime::messages.push_back(message);
        test_ground_commando_runtime::message_players.push_back(player);
    }
}

namespace COMBINIT {
    int16_t __cdecl Take_Captured_Satellite(int16_t) {
        return -1;
    }
} // namespace COMBINIT

namespace ERICNET {
    void __cdecl Add_Building(int16_t, int16_t) {
    }

    void __cdecl Remove_Building(int16_t colony_idx, int16_t building_idx) {
        MOX::colony[colony_idx].buildings[building_idx] = 0;
    }
} // namespace ERICNET

namespace BOMB {
    int32_t __cdecl Handle_Leader_At_Lost_Colony(s_colony*) {
        return -1;
    }
} // namespace BOMB

namespace COLBLDG {
    void __cdecl Clear_Colony_Queue(int16_t) {
    }
} // namespace COLBLDG
