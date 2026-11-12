#include "pch.h"

namespace {
    s_colony colonies[1];
    s_player players[MAX_PLAYERS];
    s_ship_data ships[3];
    s_leader_data leaders[1];
    s_star_data stars[1];
    int32_t ship_worths[MAX_SHIPS];
    int pre_import_calls;
    int8_t status_during_recalculation;
    int16_t pooled_officer;
    AIDATA::s_state ai;
    bool doom_star_strategic_mode;
    int doom_star_load_count;
} // namespace

namespace TEST_AIDUDES {
    bool colony_researching;
    const game::s_view* World_() {
        static game::s_view world;
        world.players = players;
        world.ships = ships;
        world.stars = stars;
        world.colonies = colonies;
        world.leaders = leaders;
        world.settings = &MOX::settings;
        world.ship_count = &MOX::NUM_SHIPS;
        world.stardate = &MOX::stardate;
        static uint32_t seed;
        world.random_seed = &seed;
        return &world;
    }

    AIDATA::s_state* State_() {
        return &ai;
    }

    aidudes::s_ai_colony_context Context_() {
        return {legacy_world::Player_Id(0), &players[0], &ai.players[0].colony_work, World_()};
    }

    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(players, 0, sizeof(players));
        pre_import_calls = 0;
        colony_researching = false;

        colonies[0].owner = 0;
        colonies[0].n_pops = 1;
        colonies[0].food2_per_farmer = 2;
        colonies[0].max_farms = 1;
        colonies[0].food2_needed_for_our_empire = 2;
    }

    void Reset_Maintenance_Scrap_(bool add_replacement) {
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(leaders, 0, sizeof(leaders));
        memset(stars, 0, sizeof(stars));
        memset(ship_worths, 0, sizeof(ship_worths));
        memset(&MOX::settings, 0, sizeof(MOX::settings));

        status_during_recalculation = -1;
        pooled_officer = -1;
        MOX::NUM_SHIPS = add_replacement ? 2 : 1;
        maintain::ship_worths = &ship_worths;

        players[0].bc = -10;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        stars[0].has_colony = 1;

        ships[0].owner = 0;
        ships[0].status = 0;
        ships[0].location = 0;
        ships[0].d.ship_type = SHIP_TYPE_COMBAT;
        ships[0].d.cost = 40;
        ships[0].d.size = 4;
        ships[0].d.shield_type = 4;
        ships[0].d.armor_type = 4;
        ships[0].d.computer_type = 4;
        ships[0].crew_experience = 100;
        ships[0].officer_index = 0;

        ships[1].owner = 0;
        ships[1].status = 0;
        ships[1].location = 0;
        ships[1].d.ship_type = SHIP_TYPE_COMBAT;
        ships[1].officer_index = -1;
        ship_worths[1] = 1;

        leaders[0].player_index = 0;
        leaders[0].type = LEADER_TYPE_SHIP;
        leaders[0].status = 1;
        leaders[0].location = 0;
    }

    void Reset_Maintenance_Scrap_With_Incomparable_Replacements_() {
        Reset_Maintenance_Scrap_(true);
        MOX::NUM_SHIPS = 3;

        ships[1].d.size = 4;
        ships[1].d.shield_type = 2;
        ships[1].d.armor_type = 2;
        ships[1].d.computer_type = 2;
        ships[1].crew_experience = 20;

        ships[2] = ships[1];
        ships[2].d.size = 2;
        ships[2].d.shield_type = 4;
        ship_worths[2] = 1;
    }

    void Reset_Doom_Star_Update_() {
        memset(colonies, 0, sizeof(colonies));
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        ai = {};
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        doom_star_strategic_mode = false;
        doom_star_load_count = 0;

        ai.players[0].colony_work.colonies.resize(1);
        ai.players[0].colony_work.colonies[0].colony_id = legacy_world::Colony_Id(0);
        colonies[0].producing[0] =
            COLBLDG::Colony_Production_From_Ship_Index(0);
        ships[0].d.ship_type = SHIP_TYPE_COMBAT;
        ships[0].d.size = SHIP_SIZE_DOOM_STAR;
        MOX::settings.strategic_combat_flag = 1;
    }

    void Reset_Doom_Star_Refit_() {
        Reset_Doom_Star_Update_();
        memset(stars, 0, sizeof(stars));

        MOX::stardate = 35036;
        players[0].total_pop = 9;
        ships[0].location = 0;
        stars[0].has_colony = 1;
        ai.players[0].refits.push_back({legacy_world::Ship_Id(0), 1000});
        ai.players[0].empire.colony_count = 2;
    }

    int Pre_Import_Calls_() {
        return pre_import_calls;
    }

    int8_t Status_During_Recalculation_() {
        return status_during_recalculation;
    }

    int16_t Pooled_Officer_() {
        return pooled_officer;
    }

    bool Doom_Star_Strategic_Mode_() {
        return doom_star_strategic_mode;
    }

    int Doom_Star_Load_Count_() {
        return doom_star_load_count;
    }
} // namespace TEST_AIDUDES

namespace MOX {
    s_colony* colony = colonies;
    s_player* player = players;
    s_ship_data* ship = ships;
    s_leader_data* leaders = ::leaders;
    s_star_data* star = stars;
    int16_t NUM_SHIPS;
    int32_t stardate;
    s_settings settings;
} // namespace MOX

namespace AIBUILD {
    s_rules rules{.ship_upgrade_bonus = {}};
}

namespace maintain {
    int32_t (*ship_worths)[MAX_SHIPS];
}

namespace game_random {
    uint32_t __cdecl Random_From_Seed(uint32_t*, uint32_t maximum) { return Random(maximum); }
    uint32_t __cdecl Random(uint32_t maximum) {
        return maximum == 900 ? 1 : 9;
    }
} // namespace game_random

namespace INITSHIP {
    void __cdecl Load_Doom_Star_Ship_Design(
        int16_t owner_idx, s_ship_design* design, bool strategic_combat) {
        memset(design, 0, sizeof(*design));
        design->ship_type = SHIP_TYPE_COMBAT;
        design->size = SHIP_SIZE_DOOM_STAR;
        design->previous_owner = static_cast<uint8_t>(owner_idx);
        design->cost = 100;
        doom_star_strategic_mode = strategic_combat;
        doom_star_load_count++;
    }
} // namespace INITSHIP

namespace EVENTS {
    int16_t __cdecl Event_Check_Colony_Researching(int16_t) {
        return TEST_AIDUDES::colony_researching;
    }
} // namespace EVENTS

namespace COLCALC {
    void __cdecl Pre_Import_Computing(s_colony* colony) {
        pre_import_calls++;
        colony->food2_per_farmer = 0;
        colony->max_farms = 0;

        if (pre_import_calls >= 3) {
            colony->food2_needed_for_our_empire = 0;
        }
    }

    int16_t __cdecl Colony_Pop_Prod_Produced(const s_colony*, const uint32_t*, int16_t) {
        return 0;
    }

    void __cdecl Do_Player_Colony_Post_Production(int16_t) {
        status_during_recalculation = MOX::ship[0].status;
    }
} // namespace COLCALC

namespace HAROLD {
    int16_t __cdecl XY_To_Star_Id(int16_t, int16_t) {
        return 0;
    }
} // namespace HAROLD

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
    }
} // namespace MAINSCR

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, int16_t) {
    }
} // namespace MSG_

namespace OFFICER {
    bool __cdecl Ships_In_Same_Place(int16_t, int16_t) {
        return true;
    }

    void __cdecl Ai_Assign_Leader_To_Ship(s_player*, s_leader_data* leader, s_ship_data* ship) {
        int16_t leader_idx = (int16_t)(leader - MOX::leaders);
        int16_t ship_idx = (int16_t)(ship - MOX::ship);

        MOX::ship[leader->location].officer_index = -1;
        leader->location = ship_idx;
        leader->status = 1;
        ship->officer_index = leader_idx;
    }

    void __cdecl Move_Officer_To_Pool(int16_t leader_idx) {
        s_leader_data* leader = &MOX::leaders[leader_idx];
        pooled_officer = leader_idx;

        if (leader->location >= 0 && MOX::ship[leader->location].officer_index == leader_idx) {
            MOX::ship[leader->location].officer_index = -1;
        }

        leader->status = 0;
        leader->eta = 0;
        leader->location = -1;
    }
} // namespace OFFICER
