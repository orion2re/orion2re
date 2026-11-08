#include "pch.h"

#include "legacy_world.h"

#include "lua/lua_game.h"

namespace NEXTTURN {

    static bool Validate_Turn_Inputs(const game_world::s_view* world) {
        if (const char* reason = AIDATA::Validate_Inputs(world)) {
            fprintf(stderr, "Cannot calculate next turn: %s\n", reason);
            AIDATA::Reset(&AIDATA::state);
            return false;
        }
        return true;
    }

    int16_t Get_Winner() {
        if (diplodef::council_winner != -1) {
            return (int16_t)diplodef::council_winner;
        }

        int16_t winner_idx = -1;
        int16_t num_players = (int16_t)MOX::NUM_PLAYERS;

        for (int16_t i = 0; i < num_players; ++i) {
            if (MOX::player[i].captured_antares != 0 || MOX::player[i].eliminated == 0) {
                winner_idx = i;
            }
        }

        return winner_idx;
    }

    void Do_Autosave() {
        int32_t autosave_interval = 1;

        if (MOX::game_type == 2 || MOX::game_type == 3) {
            autosave_interval = 1;
        }

        if (MOX::settings.auto_saves != 0) {
            if (((int32_t)MOX::stardate % autosave_interval) == 0) {
                strlcpy(MOX::save_game_description[9].description, "(Auto Save)", sizeof(MOX::save_game_description[9].description));
                FILEDEF::Save_Game(9);
            }
        }
    }

    void Next_Turn() {
        const auto world = legacy_world::World();
        if (!Validate_Turn_Inputs(&world)) return;
        if (MOX::game_type == 0 || MOX::game_type == 1) {
            Do_Autosave();
        }

        HAROLD::Confirm_Planet_And_Star_Index_Link(HAROLD::s_in_next_turn);
        if (!Next_Turn_Calc()) return;

        MOX::current_screen = MOX::return_screen;

        if (MOX::game_type != 2 && MOX::game_type != 3 && RUSS::all_ai_game == 0) {
            if (COLCALC::N_Living_Human_Players() < 1) {
                ERIC::Fast_Fade_Out();
                video::Set_Page_Off();
                graphics::Fill(0, 0, 0x27f, 0x1df, 0);
                RUSS::Mox_Sync_Update();
                video::Copy_Off_To_Back();
                video::Set_Refresh_Stencil();
                ERIC::Play_Loose_Flic();
                SCORE::End_Of_Game_Hi_Score(MOX::PLAYER_NUM);
                MOX::current_screen = SCREEN_MAIN_MENU;
            } else {
                if (COLCALC::N_Living_Players() < 2) {
                    int16_t winner = Get_Winner();
                    ERIC::Fast_Fade_Out();
                    ERIC::Play_Normal_Win_Flic();
                    SCORE::End_Of_Game_Hi_Score(winner);
                    MOX::current_screen = SCREEN_MAIN_MENU;
                } else if (diplodef::council_winner != -1) {
                    if (MOX::PLAYER_NUM != diplodef::council_winner) {
                        ERIC::Play_Loose_Flic();
                    }
                    SCORE::End_Of_Game_Hi_Score(MOX::PLAYER_NUM);
                    MOX::current_screen = SCREEN_MAIN_MENU;
                } else {
                    for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        if (MOX::player[i].captured_antares != 0) {
                            fields::Deactivate_Auto_Function();
                            ERIC::Play_All_Flics();
                            SCORE::End_Of_Game_Hi_Score((int16_t)i);
                            MOX::current_screen = SCREEN_MAIN_MENU;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            MOX::player[i].player_done_flags = 0;
        }

        MOX::old_system_index = -1;
    }

    bool Next_Turn_Calc() {
        game_world::current = legacy_world::World();
        const auto* world = &game_world::current;
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
        if (!AIDATA::Compute_AI_Data(ai, world)) return false;
        DIP_SCRN::Resolve_Delayed_Diplomacy_Orders();
        DIP_SCRN::Set_Opportunity_Attacks();
        diplomac::Clear_Diplomacy_Messages();
        npcdiplo::NPC_Diplomacy();
        diplomac::Diplomacy_Growth();
        aileader::Do_AI_Leaders(ai, world);
        AIBUILD::Compute_Empire_Building_Needs(ai, world);
        aidudes::All_Colony_AI(ai, world);
        AIMOVE::Move_All_AI(ai, world);
        COLONIZE::All_AI_Colonize();
        aitech::All_AI_Tech_Select(ai, world);
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
        INITSHIP::Repair_Ships_At_Colonies();
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
             checked_count < MOX::NUM_PLAYERS;
             ++checked_count) {
            int16_t unchecked_rank = (int16_t)game_random::Random(
                (uint32_t)(MOX::NUM_PLAYERS - checked_count));

            for (int16_t player_idx = 0;
                 player_idx < MOX::NUM_PLAYERS;
                 ++player_idx) {
                if (checked_players[player_idx] != 0) {
                    continue;
                }

                --unchecked_rank;
                if (unchecked_rank != 0) {
                    continue;
                }

                OFFICER::Random_Officer_Check(player_idx);
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