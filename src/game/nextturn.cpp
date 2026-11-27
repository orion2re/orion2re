#include "pch.h"

#include "legacy_ids.h"
#include "ship_ids.h"

#include "lua/lua_game.h"

namespace NEXTTURN {

    bool Validate_Turn_Inputs(game::s_game_state* world) {
        if (const char* reason = AIDATA::Validate_Inputs(world)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot calculate next turn: %s", reason);
            AIDATA::Reset(&AIDATA::state);
            return false;
        }
        return true;
    }


    bool Next_Turn_Calc() {
        auto* world = &game::current_state;
        if (!Validate_Turn_Inputs(world)) return false;
        if (MOX::game_type >= 0 && MOX::game_type <= 1) {
            EVENTS::Clear_Deferred_Monster_Event_Target();
            fields::Assign_Auto_Function(SCORE::nullsub, 1);
        }

        MSG_::Allocate_Msg_Slots();

        if (MOX::settings.xenons_exist != 0 && MOX::antarans.field6_0x3c == 0) {
            antarans::Antaran_Invasion_Check();
        }

        AIDATA::s_state* ai = &AIDATA::state;
        RUSS::Mox_Update();
        if (!AIDATA::Compute_AI_Data(ai, world, MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag, MOX::stardate)) return false;
        DIP_SCRN::Resolve_Delayed_Diplomacy_Orders();
        DIP_SCRN::Set_Opportunity_Attacks();
        diplomac::Clear_Diplomacy_Messages();
        npcdiplo::NPC_Diplomacy();
        diplomac::Diplomacy_Growth();
        aileader::Do_AI_Leaders(ai, world);
        AIBUILD::Compute_Empire_Building_Needs(ai, world, MOX::settings.game_difficulty);
        aidudes::All_Colony_AI(ai, world, MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag, MOX::stardate);
        AIMOVE::Move_All_AI(ai, world, MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag);
        COLONIZE::All_AI_Colonize();
        aitech::All_AI_Tech_Select(ai, world, {MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag != 0,
            MOX::stardate, MOX::settings.xenons_exist != 0});
        RUSS::Mox_Update();
        maintain::Make_Scrap_Ships_Dead();
        REPORT::Initialize_Reports();
        TRADE::Process_Trade_And_Research_Agreements();
        SHIPMOVE::Move_All_Ships_Toward_Stars();
        spy::Resolve_Spies(ai->players.data());
        {
            orion2re::lua::game::s_authoritative_population_growth_scope
                population_growth_scope(
                    MOX::game_type != 2 && MOX::game_type != 3 ||
                    NETMOX::net.local_net_id != -1 &&
                        NETMOX::net.local_net_id ==
                            NETMOX::net.net_host_id);
            COLCALC::All_Colony_Calculations();
        }
        COLCALC::Apply_All_Player_Changes();
        COLCALC::Apply_All_Colony_Changes();
        COLCALC::Do_Surrenders();
        AIDATA::Complete_Turn(ai);
        COMBFIND::Search_For_Battles();
        aitech::Apply_Evolutionary_Upgrades(world);
        EVENTS::Determine_Event();
        SHIPMOVE::Unmark_Just_Built_AI_Ships();
        ERICNET::Do_All_Ships_XP_Check();
        EVENTS::Event_Twiddle();
        lostship::Lose_Out_Of_Range_Ships();
        COLCALC::Compute_Blockades();
        SETTLER::Move_Settlers();
        COLCALC::All_Colony_Calculations();
        SHIPMOVE::Allocate_New_Ship_Slots();
        OFFICER::Check_For_Officer_Level();
        OFFICER::Decrement_Officer_ETA();
        invasion::Check_All_Rebellions();
        INITSHIP::Repair_Ships_At_Colonies(*world);
        COLONIZE::Allocate_New_Colony_Slots();
        CONTACT::Compute_Contacts();
        diplomac::Determine_First_Contacts();
        SHIPMOVE::Reset_Needless_Ignore_Combat_Ships_Flags();
        diplomac::Determine_Diplomacy_Messages();
        diplomac::End_Of_Turn_Diplomacy_Adjustments();

        if (MOX::game_type == 0 && RUSS::all_ai_game == 0) {
            council::Check_For_Council_Meeting();
        }

        uint8_t checked_players[MAX_PLAYERS] = {};
        for (int16_t checked_count = 0;
             checked_count < game::current_state.player_count;
             ++checked_count) {
            int16_t unchecked_rank = (int16_t)game_random::Random(
                (uint32_t)(game::current_state.player_count - checked_count));

            for (int16_t player_idx = 0;
                 player_idx < game::current_state.player_count;
                 ++player_idx) {
                if (checked_players[player_idx] != 0) {
                    continue;
                }

                --unchecked_rank;
                if (unchecked_rank != 0) {
                    continue;
                }

                OFFICER::Random_Officer_Check(game::current_state.players[player_idx].id);
                checked_players[player_idx] = 1;
                break;
            }
        }

        MSG_::Add_Ongoing_Event_Msgs();

        if (capture::Check_Release_Version() == 0) {
            OFFICER::Assert_Marooned_Leaders();
        }

        BILL::Record_History();
        MOX::stardate++;

        if (MAINSCR::in_auto_next_turn_mode == 0) {
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        }
        return true;
    }

} // namespace NEXTTURN
