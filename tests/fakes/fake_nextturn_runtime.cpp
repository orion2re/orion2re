#include "pch.h"

#include "game/ai_legacy.h"

namespace AI_LEGACY {
    game_world::s_view World() { return {}; }
}

#include "lua/lua_game.h"

namespace {
    s_player players[MAX_PLAYERS];
    int colony_calculation_count;
    bool population_growth_scope_active;
}

namespace TEST_NEXTTURN {
    int32_t observed_event_bc;
    int32_t observed_event_research;
    bool colony_changes_saw_current_economy;
    bool pirate_raid_was_eligible;
    bool computer_virus_was_eligible;
    int16_t officer_check_order[MAX_PLAYERS];
    int16_t officer_check_count;
    int16_t officer_offer_player;
    bool colony_calculation_population_growth_scope[2];

    void Reset_() {
        memset(players, 0, sizeof(players));
        MOX::settings = {};
        MOX::antarans = {};
        MOX::game_type = 2;
        MOX::NUM_PLAYERS = 1;
        MOX::stardate = 35000;
        MOX::player[0].bc = 90;
        MOX::player[0].research_accumulated = 5;
        RUSS::all_ai_game = 1;
        MAINSCR::in_auto_next_turn_mode = 1;
        colony_calculation_count = 0;
        observed_event_bc = -1;
        observed_event_research = -1;
        colony_changes_saw_current_economy = false;
        pirate_raid_was_eligible = false;
        computer_virus_was_eligible = false;
        memset(officer_check_order, 0xFF, sizeof(officer_check_order));
        officer_check_count = 0;
        officer_offer_player = -1;
        population_growth_scope_active = false;
        memset(colony_calculation_population_growth_scope,
            0,
            sizeof(colony_calculation_population_growth_scope));
    }
}

namespace MOX {
    int8_t game_type;
    int16_t NUM_PLAYERS;
    int32_t stardate;
    s_antaran antarans;
    s_settings settings;
    s_mouse_list mouse_list_arrow;
    s_player* player = players;
}

namespace NETMOX {
    s_network net;
}

namespace orion2re::lua::game {
    s_authoritative_population_growth_scope::
        s_authoritative_population_growth_scope(bool active_value)
        : active(active_value) {
        population_growth_scope_active = active_value;
    }

    s_authoritative_population_growth_scope::
        ~s_authoritative_population_growth_scope() {
        population_growth_scope_active = false;
    }
} // namespace orion2re::lua::game

namespace RUSS {
    int8_t all_ai_game;
    void __cdecl Shuffle_Sint(int16_t*, int16_t) {}
}

namespace MAINSCR {
    uint8_t in_auto_next_turn_mode;
}

namespace COLCALC {
    void __cdecl All_Colony_Calculations() {
        TEST_NEXTTURN::colony_calculation_population_growth_scope[colony_calculation_count] = population_growth_scope_active;
        if (colony_calculation_count++ == 0) {
            MOX::player[0].surplus_bc = 20;
            MOX::player[0].research_produced = 7;
        }
    }

    void __cdecl Apply_All_Player_Changes() {
        s_player& player = MOX::player[0];
        player.bc += player.surplus_bc;
        player.research_accumulated += player.research_produced;
    }

    void __cdecl Apply_All_Colony_Changes() {
        TEST_NEXTTURN::colony_changes_saw_current_economy =
            MOX::player[0].bc == 110 &&
            MOX::player[0].research_accumulated == 12;
    }

    void __cdecl Do_Surrenders() {}
    void __cdecl Compute_Blockades() {}
}

namespace EVENTS {
    void __cdecl Clear_Deferred_Monster_Event_Target() {}

    void __cdecl Determine_Event() {
        TEST_NEXTTURN::observed_event_bc = MOX::player[0].bc;
        TEST_NEXTTURN::observed_event_research = MOX::player[0].research_accumulated;
        TEST_NEXTTURN::pirate_raid_was_eligible = MOX::player[0].bc >= 100;
        TEST_NEXTTURN::computer_virus_was_eligible = MOX::player[0].research_accumulated >= 10;
    }

    void __cdecl Event_Twiddle() {}
}

namespace fields {
    void __cdecl Assign_Auto_Function(MOX2::action_func_t, int16_t) {}
}

namespace SCORE {
    void __cdecl nullsub() {}
}

namespace MSG_ {
    void __cdecl Allocate_Msg_Slots() {}
    void __cdecl Add_Ongoing_Event_Msgs() {}
}

namespace antarans {
    void __cdecl Antaran_Invasion_Check() {}
}

namespace AIDATA {
    s_state state;
    void __cdecl Compute_AI_Data(s_state*, const game_world::s_view*) {}
    void Complete_Turn(s_state*) {}
}

namespace DIP_SCRN {
    void Resolve_Delayed_Diplomacy_Orders() {}
    void __cdecl Set_Opportunity_Attacks() {}
}

namespace diplomac {
    void Clear_Diplomacy_Messages() {}
    void __cdecl Diplomacy_Growth() {}
    void Determine_First_Contacts() {}
    void Determine_Diplomacy_Messages() {}
    void __cdecl End_Of_Turn_Diplomacy_Adjustments() {}
}

namespace npcdiplo {
    void __cdecl NPC_Diplomacy() {}
}

namespace aileader {
    void __cdecl Do_AI_Leaders(AIDATA::s_state*, const game_world::s_view*) {}
}

namespace AIBUILD {
    void __cdecl Compute_Empire_Building_Needs(AIDATA::s_state*, const game_world::s_view*) {}
}

namespace aidudes {
    void __cdecl All_Colony_AI(AIDATA::s_state*, const game_world::s_view*) {}
}

namespace AIMOVE {
    void __cdecl Move_All_AI(AIDATA::s_state*, const game_world::s_view*) {}
}

namespace COLONIZE {
    void __cdecl All_AI_Colonize() {}
    void Allocate_New_Colony_Slots() {}
}

namespace aitech {
    void __cdecl All_AI_Tech_Select(AIDATA::s_state*, const game_world::s_view*) {}
    void __cdecl Apply_Evolutionary_Upgrades(const game_world::s_view*) {}
}

namespace maintain {
    void __cdecl Make_Scrap_Ships_Dead() {}
}

namespace REPORT {
    void __cdecl Initialize_Reports() {}
}

namespace TRADE {
    void __cdecl Process_Trade_And_Research_Agreements() {}
}

namespace SHIPMOVE {
    void __cdecl Move_All_Ships_Toward_Stars() {}
    void __cdecl Unmark_Just_Built_AI_Ships() {}
    void __cdecl Allocate_New_Ship_Slots() {}
    void __cdecl Reset_Needless_Ignore_Combat_Ships_Flags() {}
}

namespace spy {
    void __cdecl Resolve_Spies(const AIDATA::s_player_assessment*) {}
}

namespace COMBFIND {
    void __cdecl Search_For_Battles() {}
}

namespace ERICNET {
    void __cdecl Do_All_Ships_XP_Check() {}
}

namespace lostship {
    void __cdecl Lose_Out_Of_Range_Ships() {}
}

namespace SETTLER {
    void __cdecl Move_Settlers() {}
}

namespace OFFICER {
    void __cdecl Check_For_Officer_Level() {}
    void __cdecl Decrement_Officer_ETA() {}
    void __cdecl Random_Officer_Check(int16_t player_idx) {
        TEST_NEXTTURN::officer_check_order[
            TEST_NEXTTURN::officer_check_count++] = player_idx;

        if ((int32_t)game_random::Random(100) - 1 < 10) {
            TEST_NEXTTURN::officer_offer_player = player_idx;
        }
    }
    void __cdecl Assert_Marooned_Leaders() {}
}

namespace invasion {
    void __cdecl Check_All_Rebellions() {}
}

namespace INITSHIP {
    void __cdecl Repair_Ships_At_Colonies() {}
}

namespace CONTACT {
    void __cdecl Compute_Contacts() {}
}

namespace council {
    void Check_For_Council_Meeting() {}
}

namespace capture {
    uint16_t __cdecl Check_Release_Version() {
        return 1;
    }
}

namespace BILL {
    void __cdecl Record_History() {}
}

namespace mouse {
    void __cdecl Set_Mouse_List(s_mouse_list*, int16_t) {}
}