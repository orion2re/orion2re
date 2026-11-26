#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"

namespace {
    test_colonies::Slots<1> colonies;
    auto& players = game::current_state.players;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        rows.resize(3);
        return rows;
    }();
    test_stars::Slots<1> stars;
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
    game::s_game_state* World_() {
        return &game::current_state;
    }

    AIDATA::s_state* State_() {
        return &ai;
    }

    aidudes::s_ai_colony_context Context_() {
        return {legacy_world::Player_Id(0), &players[0], &ai.players[0].colony_work, World_(),
            MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag, MOX::stardate};
    }

    void Reset_() {
        colonies.Reset();
        players = ::player_roster::Make_Roster();
        pre_import_calls = 0;
        colony_researching = false;

        colonies[0].owner = legacy_world::Decode_Player_Slot(0);
        colonies[0].n_pops = 1;
        colonies[0].food2_per_farmer = 2;
        colonies[0].max_farms = 1;
        colonies[0].food2_needed_for_our_empire = 2;
    }

    void Reset_Maintenance_Scrap_(bool add_replacement) {
        players = ::player_roster::Make_Roster();
        ships.assign(3, s_ship_data{});
        game::current_state = game::s_game_state{};
        ships.resize(3);
        stars.Reset();
        memset(ship_worths, 0, sizeof(ship_worths));
        memset(&MOX::settings, 0, sizeof(MOX::settings));

        status_during_recalculation = -1;
        pooled_officer = -1;
        ship_slots::Resize(game::current_state, add_replacement ? 2 : 1);
        maintain::ship_worths = &ship_worths;

        players[0].bc = -10;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        stars[0].has_colony = game::player_bitmap::From_Byte(1);

        ships[0].owner = factions::From_Legacy(0);
        ships[0].status = 0;
        ships[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, -1));
        ships[0].d.ship_type = SHIP_TYPE_COMBAT;
        ships[0].d.cost = 40;
        ships[0].d.size = 4;
        ships[0].d.shield_type = 4;
        ships[0].d.armor_type = 4;
        ships[0].d.computer_type = 4;
        ships[0].crew_experience = 100;
        ships[0].officer_id = game::id_leader{0};

        if (add_replacement) {
            ships[1].owner = factions::From_Legacy(0);
            ships[1].status = 0;
            ships[1].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, -1));
            ships[1].d.ship_type = SHIP_TYPE_COMBAT;
            ships[1].officer_id = game::NO_LEADER;
            ship_worths[1] = 1;
        }

        auto& leader = game::current_state.leaders[0];
        leader.employer = game::id_player{0};
        leader.type = LEADER_TYPE_SHIP;
        leader.status = 1;
        game::Set_Leader_Ship(leader, ships[0], 0);
    }

    void Reset_Maintenance_Scrap_With_Incomparable_Replacements_() {
        Reset_Maintenance_Scrap_(true);
        ship_slots::Resize(game::current_state, 3);

        ships[1].d.size = 4;
        ships[1].d.shield_type = 2;
        ships[1].d.armor_type = 2;
        ships[1].d.computer_type = 2;
        ships[1].crew_experience = 20;

        auto replacement = ships[1];
        replacement.id = game::id_ship{2};
        ships.Replace_Row(2, replacement);
        ships[2].d.size = 2;
        ships[2].d.shield_type = 4;
        ship_worths[2] = 1;
    }

    void Reset_Doom_Star_Update_() {
        colonies.Reset();
        players = ::player_roster::Make_Roster();
        ships.assign(3, s_ship_data{});
        ship_slots::Resize(game::current_state, 1);
        game::current_state.next_ship_id = 91001;
        ships.Set_Key_At_Row(0, game::id_ship{91000});
        ai = {};
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        doom_star_strategic_mode = false;
        doom_star_load_count = 0;

        ai.players[0].colony_work.colonies.resize(1);
        ai.players[0].colony_work.colonies[0].colony_id = game::id_colony{0};
        colonies[0].producing[0] = game::Queued_Ship_Product(ships[0].id);
        ships[0].d.ship_type = SHIP_TYPE_COMBAT;
        ships[0].d.size = SHIP_SIZE_DOOM_STAR;
        MOX::settings.strategic_combat_flag = 1;
    }

    void Reset_Doom_Star_Refit_() {
        Reset_Doom_Star_Update_();
        stars.Reset();

        MOX::stardate = 35036;
        players[0].total_pop = 9;
        ships[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, -1));
        stars[0].has_colony = game::player_bitmap::From_Byte(1);
        ai.players[0].refits.push_back({ships[0].id, 1000});
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
    uint32_t __cdecl Random(uint32_t maximum) {
        return maximum == 900 ? 1 : 9;
    }
} // namespace game_random

namespace INITSHIP {
    void __cdecl Load_Doom_Star_Ship_Design(
        game::id_player player, s_ship_design* design, bool strategic_combat) {
        memset(design, 0, sizeof(*design));
        design->ship_type = SHIP_TYPE_COMBAT;
        design->size = SHIP_SIZE_DOOM_STAR;
        design->previous_owner = factions::From_Player(player);
        design->cost = 100;
        doom_star_strategic_mode = strategic_combat;
        doom_star_load_count++;
    }
} // namespace INITSHIP

namespace EVENTS {
    int16_t __cdecl Event_Check_Colony_Researching(game::id_colony) {
        return TEST_AIDUDES::colony_researching;
    }
} // namespace EVENTS

namespace COLCALC {
    void __cdecl Pre_Import_Computing(game::id_colony, s_colony* colony) {
        pre_import_calls++;
        colony->food2_per_farmer = 0;
        colony->max_farms = 0;

        if (pre_import_calls >= 3) {
            colony->food2_needed_for_our_empire = 0;
        }
    }

    int16_t __cdecl Colony_Pop_Prod_Produced(const s_colony*, const s_colony_pop*, int16_t) {
        return 0;
    }

    void __cdecl Do_Player_Colony_Post_Production(game::id_player) {
        status_during_recalculation = test_ships::Rows(game::current_state)[0].status;
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
    void __cdecl Add_Msg(s_MSG, game::id_player) {
    }
} // namespace MSG_

namespace OFFICER {
    void __cdecl Ai_Assign_Leader_To_Ship(game::s_game_state& state, s_player*, game::id_leader leader_idx, s_ship_data* ship) {
        auto* leader = &state.leaders.at(leader_idx.id);
        if (auto* previous = game::Find_Leader_Ship(state, *leader)) previous->officer_id = game::NO_LEADER;
        game::Set_Leader_Ship(*leader, *ship, -1);
        leader->status = 1;
        ship->officer_id = leader_idx;
    }

    void __cdecl Move_Officer_To_Pool(game::id_leader key) {
        auto* leader = &game::current_state.leaders.at(key.id);
        pooled_officer = leader_slots::Encode(key);

        if (auto* previous = game::Find_Leader_Ship(game::current_state, *leader); previous && previous->officer_id == key) {
            previous->officer_id = game::NO_LEADER;
        }

        leader->status = 0;
        leader->eta = 0;
        leader->Clear_Location();
    }
} // namespace OFFICER
