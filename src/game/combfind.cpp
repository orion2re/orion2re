#include "pch.h"
#include "legacy_ids.h"

namespace COMBFIND {

    namespace {
        bool combat_in_progress;

        bool __cdecl Planet_Has_Ancient_Artifacts(s_planet_data* planet) {
            return planet->planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        }
    } // namespace

    uint8_t messed_up_fleet_palette;
    uint8_t in_net_update;
    s_combfind_target target;
    uint16_t (*player_needs_to_attack)[MAX_STARS + 1];
    int16_t* battle_stars;
    uint8_t (*star_has_nonplayer)[MAX_STARS + 1];
    int16_t n_battle_stars;
    int8_t (*attack_confirmed)[MAX_PLAYERS][MAX_PLAYERS];
    s_combfind_star_has_player (*star_has_player)[MAX_STARS + 1][MAX_PLAYERS];
    uint8_t* current_star_has_nonplayer;
    s_combfind_star_has_player* current_star_has_player;
    char* research_treaty_msg;
    char* s_attacks_you_at_s_msg;
    char* outpost_destroyed_msg;
    char* combat_is_reported_between_s_and_s_msg;
    char* you_have_following_treaties_msg;
    char* receiving_tribute_msg;
    char* noncombat_ships_destroyed_msg;
    char* colony_seized_by_mind_control_msg;
    char* really_risk_war_msg;
    char* trade_treaty_msg;

    void __cdecl Search_For_Battles() {
        MOX2::action_func_t saved_auto_fn;
        MOX2::Get_Auto_Function(&saved_auto_fn);

        if (MOX::game_type < 2) {
            fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        s_combfind_star_has_player star_has_player_stack[MAX_STARS + 1][MAX_PLAYERS];
        uint8_t star_has_nonplayer_stack[MAX_STARS + 1];
        uint16_t player_needs_to_attack_stack[MAX_STARS + 1];
        int16_t battle_stars_stack[MAX_STARS + 2];
        int8_t attack_confirmed_buf[MAX_PLAYERS][MAX_PLAYERS];

        star_has_player = &star_has_player_stack;
        memset(&star_has_player_stack, 0, sizeof(star_has_player_stack));

        star_has_nonplayer = &star_has_nonplayer_stack;
        memset(&star_has_nonplayer_stack, 0, sizeof(star_has_nonplayer_stack));

        player_needs_to_attack = &player_needs_to_attack_stack;
        memset(&player_needs_to_attack_stack, 0, sizeof(player_needs_to_attack_stack));

        battle_stars = battle_stars_stack;

        NETMOX::Net_Debug_Log(
            "Search_For_Battles: scratch ready player=%d stardate=%u",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate);

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            int8_t owner = MOX::colony[i].owner;
            if (owner != -1) {
                int16_t planet_idx = MOX::colony[i].planet;
                int16_t star_idx = MOX::planet[planet_idx].star_index;
                (*star_has_player)[star_idx][owner].colony_count++;
            }
        }

        (*star_has_nonplayer)[MOX::NUM_STARS] |= 1;

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            int16_t loc = ship->location;
            if (Is_Star_Or_Antares_Location(loc, MOX::NUM_STARS)) {
                if (ship->status == 0) {
                    int8_t owner = ship->owner;
                    if (owner < MAX_PLAYERS) {
                        uint8_t ship_type = ship->d.ship_type;
                        (*star_has_player)[loc][owner].has_player = 1;
                        (*star_has_player)[loc][owner].ship_sizes[ship_type] = 1;
                        if (ship_type == SHIP_TYPE_TRANSPORT) {
                            (*star_has_player)[loc][owner].has_transport = 1;
                        } else if (ship_type == SHIP_TYPE_COMBAT) {
                            (*player_needs_to_attack)[loc] |= (1 << (owner & 0x1F));
                        }
                    } else {
                        (*star_has_nonplayer)[loc] |= (1 << ((owner - MAX_PLAYERS) & 0x1F));
                        (*player_needs_to_attack)[loc] |= (1 << (owner & 0x1F));
                    }
                }
            } else if (ship->d.ship_type == SHIP_TYPE_TRANSPORT && ship->status == 1) {
                int16_t star_idx = loc - SHIP_LOCATION_MOVING_OFFSET;
                if (ship->turns_left == 1) {
                    (*star_has_player)[star_idx][ship->owner].transport_count++;
                } else {
                    (*star_has_player)[star_idx][ship->owner].transport_unk = 1;
                }
            }
        }

        n_battle_stars = 0;
        for (int16_t i = 0; i <= MOX::NUM_STARS; ++i) {
            if ((*player_needs_to_attack)[i] != 0) {
                if (EVENTS::Event_Check_Space_Anomaly_At_Star(i) == 0) {
                    battle_stars[n_battle_stars++] = i;
                }
            }
        }

        RUSS::Shuffle_Sint(battle_stars, n_battle_stars);

        if (MOX::game_type < 2) { /* Case 0 (Single), Case 1 (Host) */
            memset(attack_confirmed_buf, 0, sizeof(attack_confirmed_buf));
            attack_confirmed = &attack_confirmed_buf;

            while (n_battle_stars > 0) {
                int16_t star_idx, attacker_idx;
                Pick_Star_And_Attacker(&star_idx, &attacker_idx);
                if (star_idx != -1) {
                    int16_t defender_idx = -1;
                    int16_t colony_idx = -1;
                    int8_t unk_flag = 0;

                    if (attacker_idx < MAX_PLAYERS) {
                        if (MOX::player[attacker_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                            Get_AI_Target(star_idx, attacker_idx, &defender_idx, &colony_idx, &unk_flag);
                        } else {
                            if ((MOX::star[star_idx].ignore_combat_ships & (1 << (attacker_idx & 0x1F))) == 0) {
                                Get_Local_Human_Target(star_idx, attacker_idx, &defender_idx, &colony_idx);
                                if (colony_idx == -1) {
                                    if (defender_idx == -1) {
                                        MOX::star[star_idx].ignore_combat_ships |= (1 << (attacker_idx & 0x1F));
                                    }
                                }
                            }
                        }
                    } else {
                        Get_NPC_Target(star_idx, attacker_idx, &defender_idx, &colony_idx);
                    }

                    int16_t final_defender_idx = defender_idx;
                    if (colony_idx != -1) {
                        final_defender_idx = MOX::colony[colony_idx].owner;
                        MOX::player[final_defender_idx].last_attacker = (uint8_t)attacker_idx;
                    }

                    if (attacker_idx == 12 /* Antarans */ && colony_idx != -1) {
                        int16_t bribe_planet_idx = -1;
                        if (MOX::player[final_defender_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            bribe_planet_idx = MAINPUPS::Dragon_New_Star_Popup(final_defender_idx, star_idx);
                        }
                        if (bribe_planet_idx != -1) {
                            // BUG: final_defender_idx was set to -1 before being passed to Do_Dragon_Bribe
                            Do_Dragon_Bribe(final_defender_idx, star_idx, bribe_planet_idx);
                            final_defender_idx = -1;
                        }
                    }

                    if (final_defender_idx != -1) {
                        int16_t planet_idx = COMBFIND::Target_Colony_To_Planet(colony_idx);
                        MOX::star[star_idx].ignore_combat_ships = 0;
                        Do_1_Combat(star_idx, attacker_idx, defender_idx, planet_idx, nullptr, unk_flag);
                        current_star_has_player = 0;
                        current_star_has_nonplayer = 0;
                        Update_Player_Needs_To_Attack(star_idx, attacker_idx, final_defender_idx);
                    }
                } else {
                    break;
                }
            }
            attack_confirmed = nullptr;
        } else if (MOX::game_type < 4) {
            NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
            Client_Combat_Loop();
            NETMOX::Net_Debug_Log(
                "Search_For_Battles: combat loop exited sync=%u pending_stars=%d",
                (unsigned int)NETMOX::net.sync_state,
                (int)n_battle_stars);
            NETMOX::Mox_Send_Message((int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF, NET_PACKET_COMBAT_PHASE_COMPLETE, nullptr, 0);
        }

        Process_Retreating_Ships();

        if (messed_up_fleet_palette) {
            MAINSCR::Set_Reload_Fleet_Box_Palette_Flag();
            messed_up_fleet_palette = 0;
        }

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            Do_System_Discoveries_At_Star(&MOX::star[i]);
        }

        MOX2::Put_Auto_Function(&saved_auto_fn);
        NETMOX::Net_Debug_Log(
            "Search_For_Battles: releasing scratch pending_stars=%d sync=%u",
            (int)n_battle_stars,
            (unsigned int)NETMOX::net.sync_state);
        n_battle_stars = 0;
        star_has_player = nullptr;
        star_has_nonplayer = nullptr;
        player_needs_to_attack = nullptr;
        battle_stars = nullptr;

        COLCALC::Check_For_Eliminated_Players();
    }

    void __cdecl Process_Retreating_Ships(void) {
        struct s_msg {
            char type;
            char pad1;
            int16_t star_idx;
            char reason;
            short count;
            char killed_officer_count;
            char pad2[13];
        };

        constexpr int16_t retreat_location_count = MAX_STARS + 1;
        s_retreat_report report[MAX_PLAYERS * retreat_location_count];
        int16_t player_retreat_target[MAX_PLAYERS];
        int16_t ship_indices[MAX_SHIPS];
        s_ship_move_info move_info;

        memset(report, 0, sizeof(report));
        memset(player_retreat_target, 0xFF, sizeof(player_retreat_target));

        for (int16_t i = MOX::NUM_SHIPS - 1; i >= 0; --i) {
            s_ship_data* ship = &MOX::ship[i];

            if (ship->location == MOX::NUM_STARS) {
                int8_t owner = ship->owner;
                if (player_retreat_target[owner] == -1) {
                    player_retreat_target[owner] = 0x48; /* 'H' */
                    uint32_t valid_star_count = 0;
                    for (int16_t j = 0; j < MOX::NUM_STARS; ++j) {
                        if ((MOX::star[j].has_dimensional_portal >> owner) & 1) {
                            valid_star_count++;
                            if (game_random::Random(valid_star_count) == 1) {
                                player_retreat_target[owner] = j;
                            }
                        }
                    }
                }

                if (player_retreat_target[owner] == 0x48) {
                    int16_t leader_idx = ship->officer_index;
                    if (leader_idx != -1) {
                        if (MOX::leaders[leader_idx].eta < 1) {
                            MOX::leaders[leader_idx].location = -1;
                            ship->officer_index = -1;
                            OFFICER::Kill_Officer(leader_idx);
                            report[owner * retreat_location_count + MOX::NUM_STARS].killed_officers++;
                        } else {
                            OFFICER::Move_Officer_To_Pool(leader_idx);
                        }
                    }
                    report[owner * retreat_location_count + MOX::NUM_STARS].count++;
                    report[owner * retreat_location_count + MOX::NUM_STARS].reason = 0;
                    SHIPS::Kill_Ship(i);
                } else {
                    int16_t target_loc = player_retreat_target[owner];
                    ship->location = target_loc;
                    ship->x = MOX::star[target_loc].x;
                    ship->y = MOX::star[target_loc].y;
                    ship->status = 0;
                }
            } else if (ship->status == 9) {
                int group_count = 1;
                int16_t loc = ship->location;
                int8_t owner = ship->owner;
                ship_indices[0] = i;
                ship->mission = 0;

                for (int16_t j = i - 1; j >= 0; --j) {
                    s_ship_data* other = &MOX::ship[j];
                    if (other->status == 9 && other->location == loc && other->owner == owner) {
                        ship_indices[group_count++] = j;
                        other->mission = 0;
                    }
                }

                int16_t retreat_star = Closest_Other_Colony_Star(owner, loc);
                bool retreat_failed = false;
                int8_t fail_reason = -1;

                if (retreat_star == -1) {
                    retreat_failed = true;
                    fail_reason = 2;
                } else {
                    ship_indices[group_count] = -1;
                    SHIPMOVE::Ships_Try_To_Move_To(ship_indices, retreat_star, &move_info);
                    if (move_info.moving == 0) {
                        retreat_failed = true;
                        fail_reason = (move_info.hyperspace_flux == 0) ? 2 : 1;
                    }
                }

                if (retreat_failed) {
                    for (int k = 0; k < group_count; ++k) {
                        int16_t idx = ship_indices[k];
                        int16_t leader_idx = MOX::ship[idx].officer_index;
                        if (leader_idx != -1) {
                            if (MOX::leaders[leader_idx].eta < 1) {
                                OFFICER::Kill_Officer(leader_idx);
                                report[owner * retreat_location_count + MOX::ship[i].location].killed_officers++;
                            } else {
                                OFFICER::Move_Officer_To_Pool(leader_idx);
                            }
                        }
                        // ORACLE: The original re-reads the outer ship's location after it may have
                        // been killed. A location of -1 aliases the preceding player's final slot.
                        // Fix this attribution bug later even though doing so diverges from the oracle.
                        s_retreat_report& retreat =
                            report[owner * retreat_location_count + MOX::ship[i].location];
                        retreat.count++;
                        retreat.reason = fail_reason;
                        SHIPS::Kill_Ship(idx);
                    }
                } else {
                    ship_indices[group_count] = -1;
                    SHIPMOVE::Make_Ships_Move_To(ship_indices, retreat_star, &move_info);
                    for (int k = 0; k < group_count; ++k) {
                        int16_t idx = ship_indices[k];
                        MOX::ship[idx].location -= SHIP_LOCATION_MOVING_OFFSET;
                        MOX::ship[idx].status = 1;
                    }
                }
            } else if (ship->owner >= MAX_PLAYERS) {
                if (ship->owner == 8 && ship->status == 0) {
                    antarans::Restock_Antaran_Ship((int16_t)ship->d.size);
                    SHIPS::Kill_Ship(i);
                } else if (ship->owner >= MAX_PLAYERS && ship->status == 0 && ship->d.speed != 0) {
                    if (Wandering_Monster_Retires(ship->owner)) {
                        SHIPS::Kill_Ship(i);
                        NETMOX::Russ_Event_Monster_Update((int16_t)ship->owner, -1, -1);
                    }
                }
            }
        }

        for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
            for (int16_t s = 0; s < MOX::NUM_STARS + 1; ++s) {
                s_retreat_report& retreat = report[p * retreat_location_count + s];
                if (retreat.count != 0) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_FAILED_RETREAT;
                    msg.u.retreat.star_idx = s;
                    msg.u.retreat.reason = retreat.reason;
                    msg.u.retreat.ships_lost = retreat.count;
                    msg.u.retreat.leader_count = retreat.killed_officers;
                    MSG_::Add_Msg(msg, p);
                }
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    void __cdecl Client_Combat_Loop() {
        int8_t local_attack_confirmed[MAX_PLAYERS][MAX_PLAYERS];
        s_battle_results local_battle_results_1;
        s_battle_results local_battle_results_2;
        s_battle_results local_battle_results_main;
        int16_t target_colony;
        int16_t new_star_target;
        int16_t result;

        NETMOX::net.combat_results_data = &local_battle_results_main;
        memset(local_attack_confirmed, 0, sizeof(local_attack_confirmed));
        COMBFIND::attack_confirmed = (int8_t(*)[MAX_PLAYERS][MAX_PLAYERS])local_attack_confirmed;

        while (NETMOX::net.sync_state >= NET_SYNC_STATE_COMBAT_READY && NETMOX::net.sync_state < NET_SYNC_STATE_COMBAT_COMPLETE) {
            RUSS::Mox_Update();
            MULTPLAY::Chat_Box_Input_Loop();
            RUSS::Mox_Update();

            switch (NETMOX::net.sync_state) {
                case NET_SYNC_STATE_PICKING_COMBAT_TARGET:
                    fields::Assign_Auto_Function(SCORE::nullsub, 1);
                    video::Set_Page_Off();
                    MOX::dont_draw_main_screen_to_back = 1;
                    MAINSCR::Draw_Mini_Main_Screen();
                    video::Copy_Off_To_Back();
                    video::Set_Refresh_Stencil();
                    ERICNET::Mox_Play_Sound(51 /* 0x33 */);
                    COMBFIND::Get_Local_Human_Target(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, &NETMOX::net.combat_defender_id, &target_colony);
                    NETMOX::net.combat_planet_id = COMBFIND::Target_Colony_To_Planet(target_colony);
                    MULTPLAY::Load_Net_Next_Turn_Screen(0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TARGET_SENT;
                    if (target_colony == -1) {
                        if (NETMOX::net.combat_defender_id == -1) {
                            NETMOX::net.combat_target_player_id = -1;
                            NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                        } else {
                            NETMOX::net.combat_target_player_id = NETMOX::net.combat_defender_id;
                        }
                    } else {
                        NETMOX::net.combat_target_player_id = MOX::colony[target_colony].owner;
                    }
                    NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_COMBAT_TARGET_CHOSEN, &NETMOX::net.combat_defender_id, sizeof(NETMOX::net.combat_defender_id) + sizeof(NETMOX::net.combat_planet_id) + sizeof(NETMOX::net.combat_target_player_id));
                    break;

                case NET_SYNC_STATE_COMBAT_RESOLUTION_DISPATCH:
                    if (NETMOX::net.combat_attacker_id < MAX_PLAYERS && MOX::player[(int)NETMOX::net.combat_attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER;
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER_VS_NONHUMAN;
                    }
                    ERICNET::Mox_Play_Sound(51 /* 0x33 */);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER:
                    memset(&local_battle_results_1, 0, sizeof(s_battle_results));
                    memcpy(&local_battle_results_1, &NETMOX::net.sync_state, sizeof(local_battle_results_1.player_info));
                    local_battle_results_1.planet_id = NETMOX::net.combat_planet_id;
                    result = COMBFIND::Do_1_Combat(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, &local_battle_results_1, 0);
                    local_battle_results_1.player_info.combat_result = (int8_t)result;
                    NETMOX::Mox_Send_Message(MOX::player[NETMOX::net.combat_target_player_id].network_player_id, NET_PACKET_STRATEGIC_RESULTS_READY, &local_battle_results_1, sizeof(local_battle_results_1));
                    while (NETMOX::net.sync_state != NET_SYNC_STATE_COMBAT_READY) {
                        RUSS::Mox_Update();
                    }
                    NETMOX::Send_Battle_Results(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, NETMOX::net.combat_target_player_id, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, (int16_t)local_battle_results_1.player_info.combat_result);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER:
                    result = COMBFIND::Do_1_Combat(NETMOX::net.combat_star_id, NETMOX::net.combat_attacker_id, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, nullptr, 0);
                    if (NETMOX::net.sync_state != NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                        while (NETMOX::net.sync_state != NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED) {
                            RUSS::Mox_Update();
                        }
                    }
                    COMBFIND::Bomb_Results_Popups(NETMOX::net.combat_results_data);
                    NETMOX::Mox_Send_Message(MOX::player[(int)NETMOX::net.combat_attacker_id].network_player_id, NET_PACKET_STRATEGIC_RESULTS_ACK, nullptr, 0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Send_Battle_Results(NETMOX::net.combat_star_id, NETMOX::net.combat_attacker_id, MOX::PLAYER_NUM, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, result);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER_VS_NONHUMAN:
                    result = COMBFIND::Do_1_Combat(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, nullptr, 0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Send_Battle_Results(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, NETMOX::net.combat_target_player_id, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, result);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER_VS_NONHUMAN:
                    result = COMBFIND::Do_1_Combat(NETMOX::net.combat_star_id, NETMOX::net.combat_attacker_id, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, nullptr, 0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    MULTPLAY::Load_Net_Next_Turn_Screen(1);
                    NETMOX::Send_Battle_Results(NETMOX::net.combat_star_id, NETMOX::net.combat_attacker_id, MOX::PLAYER_NUM, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, result);
                    break;

                case NET_SYNC_STATE_COMBAT_STRATEGIC_ATTACKER:
                    memset(&local_battle_results_2, 0, sizeof(s_battle_results));
                    memcpy(&local_battle_results_2, &NETMOX::net.sync_state, sizeof(local_battle_results_2.player_info));
                    local_battle_results_2.planet_id = NETMOX::net.combat_planet_id;
                    result = COMBFIND::Do_1_Combat(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, &local_battle_results_2, 0);
                    local_battle_results_2.player_info.combat_result = (int8_t)result;
                    MULTPLAY::Load_Net_Next_Turn_Screen(1);
                    if (NETMOX::net.combat_target_player_id < MAX_PLAYERS && MOX::player[NETMOX::net.combat_target_player_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        NETMOX::Mox_Send_Message(MOX::player[NETMOX::net.combat_target_player_id].network_player_id, NET_PACKET_STRATEGIC_BATTLE_RESULTS, &local_battle_results_2, sizeof(local_battle_results_2));
                    }
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Send_Battle_Results(NETMOX::net.combat_star_id, MOX::PLAYER_NUM, NETMOX::net.combat_target_player_id, NETMOX::net.combat_defender_id, NETMOX::net.combat_planet_id, (int16_t)local_battle_results_2.player_info.combat_result);
                    break;

                case NET_SYNC_STATE_COMBAT_RESULTS_DISPLAY:
                    COMBFIND::Strategic_Results_Popup(NETMOX::net.combat_results_data);
                    COMBFIND::Bomb_Results_Popups(NETMOX::net.combat_results_data);
                    MULTPLAY::Load_Net_Next_Turn_Screen(0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_COMBAT_RESULTS_ACK, nullptr, 0);
                    break;

                case NET_SYNC_STATE_DRAGON_BRIBE_PROMPT:
                    result = MAINPUPS::Dragon_New_Star_Popup(MOX::PLAYER_NUM, NETMOX::net.combat_star_id);
                    new_star_target = result;
                    MULTPLAY::Load_Net_Next_Turn_Screen(0);
                    if (new_star_target == -1) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_DRAGON_BRIBE_DECLINED;
                        NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_DRAGON_NO_BRIBE, nullptr, 0);
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                        NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_DRAGON_BRIBE_DESTINATION, &new_star_target, sizeof(new_star_target));
                    }
                    break;

                default:
                    break;
            }
        }

        NETMOX::net.combat_results_data = nullptr;
        COMBFIND::attack_confirmed = nullptr;
        RUSS::Mox_Update();
    }

    void __cdecl Do_Dragon_Bribe(int player_idx, int16_t star_idx, int16_t dest_idx) {
        int16_t ship_indices[2];
        s_ship_move_info move_info;

        ship_indices[0] = -1;
        ship_indices[1] = -1;

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            // Owner 12 is the Dragon/Guardian
            if (MOX::ship[i].owner == 12 && MOX::ship[i].location == star_idx) {
                ship_indices[0] = i;
            }
        }

        SHIPMOVE::Ships_Try_To_Move_To(ship_indices, dest_idx, &move_info);
        SHIPMOVE::Make_Ships_Move_To(ship_indices, dest_idx, &move_info);

        MOX::player[player_idx].bc -= static_cast<int32_t>(dragon_bribe_bc);

        (*star_has_nonplayer)[star_idx] &= 0xEF;
    }

    int16_t __cdecl Closest_Other_Colony_Star(int16_t player_idx, int16_t star_idx) {
        uint32_t min_dist_sq = 0xFFFFFFFF;
        int16_t closest_star_idx = -1;

        s_star_data* src_star = &MOX::star[star_idx];
        int32_t src_x = src_star->x;
        int32_t src_y = src_star->y;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];

            if (colony->owner == player_idx) {
                int16_t target_star_idx = MOX::planet[colony->planet].star_index;

                if (target_star_idx != star_idx && target_star_idx != closest_star_idx) {
                    if (!struct_::Test_Bit_Field(src_star->black_hole_blocks, target_star_idx)) {
                        s_star_data* target_star = &MOX::star[target_star_idx];

                        int32_t dx = (int32_t)target_star->x - src_x;
                        int32_t dy = (int32_t)target_star->y - src_y;
                        uint32_t dist_sq = (uint32_t)dx * (uint32_t)dx + (uint32_t)dy * (uint32_t)dy;

                        if (dist_sq < min_dist_sq) {
                            min_dist_sq = dist_sq;
                            closest_star_idx = target_star_idx;
                        }
                    }
                }
            }
        }

        return closest_star_idx;
    }

    /**
 * Builds the current ship and colony target lists for one attacker at a star.
 *
 * @param star_idx Star containing potential combat targets.
 * @param player_idx Attacker whose legal targets are being collected.
 */
    void __cdecl Collect_Targets_For_Combat(int16_t star_idx, int16_t player_idx) {
        target.num_colony_targets = 0;

        if (star_idx < MOX::NUM_STARS) {
            for (int i = 0; i < 5; ++i) {
                int16_t planet_idx = MOX::star[star_idx].planet_index[i];
                if (planet_idx >= 0) {
                    int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                    if (colony_idx >= 0) {
                        if (Player_May_Attack_Player(player_idx, MOX::colony[colony_idx].owner)) {
                            target.colony_targets[target.num_colony_targets] = colony_idx;
                            target.num_colony_targets++;
                        }
                    }
                }
            }
        }

        target.num_player_targets = 0;
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (player_idx != i) {
                s_combfind_star_has_player* status = &(*star_has_player)[star_idx][i];
                if (status->has_player && status->colony_count == 0) {
                    if (Player_May_Attack_Player(player_idx, i)) {
                        target.player_targets[target.num_player_targets] = i;
                        target.num_player_targets++;
                    }
                }
            }
        }

        uint8_t mask = (*star_has_nonplayer)[star_idx];
        if (mask != 0) {
            for (int i = 0; i < 7; ++i) {
                // Indices 8-14 represent non-player factions/monsters
                if (player_idx != (i + MAX_PLAYERS)) {
                    if ((mask >> i) & 1) {
                        target.player_targets[target.num_player_targets] = (int16_t)i + MAX_PLAYERS;
                        target.num_player_targets++;
                    }
                }
            }
        }
    }

    int16_t __cdecl Pick_Attacker_At_Star(int16_t star_idx) {
        uint16_t needs_attack = (uint16_t)(*player_needs_to_attack)[star_idx];
        int32_t attacker_idx = -1;
        int32_t count = 0;

        if (needs_attack == 0) {
            return -1;
        }

        // Check for special entities (bits 8-14)
        if (needs_attack >= (1u << MAX_PLAYERS) /* bit 8 set */) {
            for (int32_t i = MAX_PLAYERS; i < 15; ++i) {
                if ((needs_attack >> i) & 1) {
                    count++;
                    // Reservoir sampling for equal probability distribution
                    if (game_random::Random(count) == 1) {
                        attacker_idx = i;
                    }
                }
            }
            return (int16_t)attacker_idx;
        } else {
            // Check for players (bits 0-7)
            for (int32_t i = 0; i < (int32_t)MOX::NUM_PLAYERS; ++i) {
                if ((needs_attack >> i) & 1) {
                    count++;
                    if (game_random::Random(count) == 1) {
                        attacker_idx = i;
                    }
                }
            }
            return (int16_t)attacker_idx;
        }
    }

    void __cdecl Pick_Star_And_Attacker(int16_t* star_idx_ptr, int16_t* attacker_idx_ptr) {
        int16_t star_list_idx = n_battle_stars - 1;
        *star_idx_ptr = -1;

        while (star_list_idx >= 0) {
            RUSS::Mox_Update();

            // Get star index from the pending battle stars list
            int16_t star_idx = battle_stars[star_list_idx];
            *star_idx_ptr = star_idx;

            if ((MOX::game_type == 2 || MOX::game_type == 3) && COMBFIND::Some_Star_Player_Is_Busy(*star_idx_ptr)) {
                // Skip star if player logic is currently busy (e.g. multiplayer/hotseat)
                star_list_idx--;
                *star_idx_ptr = -1;
            } else {
                do {
                    *attacker_idx_ptr = COMBFIND::Pick_Attacker_At_Star(*star_idx_ptr);

                    if (*attacker_idx_ptr == -1) {
                        // No valid attacker found at this star, remove the star from the list by swapping with last
                        int16_t current_list_idx = star_list_idx;
                        *star_idx_ptr = -1;
                        n_battle_stars--;
                        star_list_idx--;
                        battle_stars[current_list_idx] = battle_stars[n_battle_stars];
                    } else {
                        // Mark this player as processed for this star in the current pick
                        (*player_needs_to_attack)[*star_idx_ptr] &= ~(1 << *attacker_idx_ptr);

                        COMBFIND::Collect_Targets_For_Combat(*star_idx_ptr, *attacker_idx_ptr);

                        // If no valid targets (player or colony) were found, try to find another attacker at the same star
                        if (target.num_player_targets == 0 && target.num_colony_targets == 0) {
                            *attacker_idx_ptr = -1;
                        }
                    }
                } while (*attacker_idx_ptr == -1 && *star_idx_ptr != -1);
            }

            // If a valid star and attacker pair was found, exit
            if (*star_idx_ptr != -1) {
                break;
            }
        }
    }

    int16_t __cdecl Find_Planet_At_Star_Satisfying(s_star_data* star, bool(__cdecl* predicate)(s_planet_data*)) {
        for (int16_t i = 0; i < 5; ++i) {
            int16_t planet_idx = star->planet_index[i];

            if (planet_idx != -1) {
                // Accessing the planet data via the global planet array
                if (predicate(&MOX::planet[planet_idx])) {
                    return planet_idx;
                }
            }
        }

        return -1;
    }

    bool __cdecl Planet_Has_Splinter_Colony(s_planet_data* planet) {
        return planet->planet_special == SYSTEM_SPECIAL_SPLINTER_COLONY;
    }

    /**
 * Returns the colony index occupying a planet, or -1 when none exists.
 *
 * @param planet_idx Planet index in the global planet array.
 * @return Colony index for that planet, or -1.
 */
    int16_t __cdecl Target_Planet_To_Colony(int16_t planet_idx) {
        if (planet_idx == -1) {
            return -1;
        }

        return MOX::planet[planet_idx].colony_index;
    }

    s_colony* __cdecl Free_Colony_Slot() {
        int16_t i = 0;
        s_colony* col = MOX::colony;

        if (i < MOX::NUM_COLONIES) {
            do {
                // Check if the colony slot is empty
                if (col->planet == -1) {
                    break;
                }
                i++;
                col++;
            } while (i < MOX::NUM_COLONIES);

            // Found a free slot within the existing array bounds
            if (i < MOX::NUM_COLONIES) {
                return col;
            }
        }

        // No free slot found; expand the colony count and return the next slot
        MOX::NUM_COLONIES = i + 1;
        return col;
    }

    void __cdecl Do_System_Discoveries_At_Star(s_star_data* star) {
        int16_t star_idx;
        int eligible_player_count;
        uint8_t player_present[MAX_PLAYERS];
        int16_t player_idx;
        s_colony* colony;
        int16_t planet_idx;
        int16_t limit;
        int n_techs;
        int total_valid_techs;
        s_MSG msg = {};

        star_idx = (int16_t)(star - MOX::star);
        Reset_One_Shot_System_Special(star);

        if (star->system_special != SYSTEM_SPECIAL_NO_SPECIAL && (*star_has_nonplayer)[star_idx] == 0) {
            eligible_player_count = 0;
            memset(player_present, 0, sizeof(player_present));

            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if ((*star_has_player)[star_idx][i].has_player != 0) {
                    player_present[i] = 1;
                    eligible_player_count++;
                }
            }

            if (eligible_player_count > 0) {
                player_idx = RUSS::Get_Weighted_Choice_Char(player_present, MOX::NUM_PLAYERS);

                switch (star->system_special) {
                    case SYSTEM_SPECIAL_SPACE_DEBRIS:
                        MOX::player[player_idx].bc += system_discovery_bc.space_debris;
                        star->system_special = player_idx + 50;
                        break;

                    case SYSTEM_SPECIAL_PIRATE_CACHE:
                        MOX::player[player_idx].bc += system_discovery_bc.pirate_cache;
                        star->system_special = player_idx + 60;
                        break;

                    case SYSTEM_SPECIAL_SPLINTER_COLONY:
                        star->system_special = player_idx + 70;
                        (*star_has_player)[star_idx][player_idx].colony_count = 1;
                        colony = Free_Colony_Slot();
                        planet_idx = Find_Planet_At_Star_Satisfying(star, Planet_Has_Splinter_Colony);
                        INITGAME::Init_Colony((int16_t)(colony - MOX::colony), planet_idx, (uint8_t)player_idx);

                        limit = COLCALC::Colony_Race_Pop_Limit(colony, player_idx);
                        colony->n_pops = (uint8_t)limit;
                        if (colony->n_pops > 3) {
                            colony->n_pops = 3;
                        }

                        memset(colony->buildings, 0, sizeof(colony->buildings));
                        memset(colony->pop, 0, sizeof(colony->pop));

                        for (int i = 0; i < colony->n_pops; ++i) {
                            uint32_t pop_val = colony->pop[i];

                            // Set owner bits (0-3) and source planet bits (4-6)
                            pop_val = (pop_val & ~0xF) | (player_idx & 0xF);
                            pop_val = (pop_val & ~0x70) | ((player_idx << 4) & 0x70);

                            if (MOX::planet[planet_idx].food_per_farmer == 0 || MOX::player[player_idx].traits[TRAIT_EATS_MINERALS] != 0) {
                                int r = game_random::Random(2);
                                pop_val = (((r << 7) ^ pop_val) & 0x180) ^ pop_val;
                            } else if (MOX::player[player_idx].traits[TRAIT_CYBERNETIC] == 0) {
                                pop_val &= ~0x180;
                            } else if (i == 0) {
                                pop_val = (pop_val & ~0x100) | 0x80;
                            } else {
                                pop_val &= ~0x180;
                            }

                            pop_val |= 0x200; // Population unit exists flag
                            colony->pop[i] = pop_val;
                        }

                        COLCALC::Compute_Star_Colony_Stuff(star);
                        COLCALC::Colony_Calculation((int16_t)(colony - MOX::colony));
                        colony->just_produced = COLONY_PRODUCTION_JUST_COLONIZED;
                        break;

                    case SYSTEM_SPECIAL_MAROONED_HERO:
                        star->system_special = player_idx + 80;
                        break;

                    case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS:
                        if (star->artifacts_gave_app == 0) {
                            uint8_t* picked_tech_ids;
                            int r;

                            star->system_special = player_idx + 90;
                            star->artifacts_gave_app = 1;
                            planet_idx = Find_Planet_At_Star_Satisfying(
                                star, Planet_Has_Ancient_Artifacts);

                            if (!MAPGEN::special_generation_config.artifacts_grant_technology) {
                                break;
                            }

                            msg.msg_type = MSG_TYPE_ARTIFACTS;
                            msg.u.artifacts.star_idx = star_idx;
                            picked_tech_ids = (uint8_t*)&msg.u.artifacts.tech1;
                            picked_tech_ids[0] = 0;
                            picked_tech_ids[1] = 0;

                            r = game_random::Random(4);
                            n_techs = ((r + ((r >> 31) & 3)) >> 2) + 1;

                            for (int remaining = n_techs; remaining != 0; --remaining) {
                                total_valid_techs = 0;
                                for (int tech_id = 1; tech_id < 204; ++tech_id) {
                                    if (Player_Tech_Field_Status_(&MOX::player[player_idx], TECHDATA::technology_applications[tech_id].tech_field_id) == 2 &&
                                        COLCALC::Player_May_Get_Tech_App(&MOX::player[player_idx], tech_id)) {
                                        total_valid_techs++;
                                        if (game_random::Random(total_valid_techs) == 1) {
                                            picked_tech_ids[remaining - 1] = (uint8_t)tech_id;
                                        }
                                    }
                                }
                                if (total_valid_techs != 0) {
                                    const int16_t tech_app_id = picked_tech_ids[remaining - 1];
                                    COLCALC::Player_Gets_Tech_App(&MOX::player[player_idx], tech_app_id);

                                    s_report report = {};
                                    report.type = 3;
                                    report.player_idx = player_idx;
                                    report.d.artifacts.planet_idx = planet_idx;
                                    report.d.artifacts.tech_app_id = tech_app_id;
                                    REPORT::Add_Report(&report);
                                }
                            }

                            if (picked_tech_ids[0] != 0) {
                                MSG_::Add_Msg(msg, player_idx);
                            }
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    }

    /**
 * Prompts a local human player to choose a combat target at a star.
 *
 * @param star_idx Star containing the pending combat.
 * @param player_idx Human attacker.
 * @param target_player_idx_out Receives the chosen ship-only defender, or -1.
 * @param target_colony_idx_out Receives the chosen colony target, or -1.
 */
    void __cdecl Get_Local_Human_Target(int16_t star_idx, int16_t player_idx, int16_t* target_player_idx_out, int16_t* target_colony_idx_out) {
        SYS::Clear_System_Display_Loaded_Flag();

        *target_colony_idx_out = -1;
        *target_player_idx_out = -1;

        if (star_idx == MOX::NUM_STARS) {
            *target_player_idx_out = 8;
            return;
        }

        if (COMBFIND::target.num_colony_targets > 0 || COMBFIND::target.num_player_targets > 0) {
            video::Set_Page_Off();
            MAINSCR::Draw_Mini_Main_Screen();
            video::Copy_Off_To_Back();
            RUSS::Mox_Sync_Update();
            MAINSCR::Draw_Mini_Main_Screen();

            REPORT::something_interesting_happened[player_idx] = 1;
            MOX::star[star_idx].visited |= (1 << player_idx);
            MOX::PLAYER_NUM = player_idx;
            MAINSCR::Update_Ship_Icons_In_Between_Turns();

            int16_t target_id;
            while (true) {
                target_id = -1;
                *target_colony_idx_out = -1;
                *target_player_idx_out = -1;

                int16_t selection = MAINPUPS::Defense_Colony_Selection_Popup(star_idx, player_idx);

                if (selection != -1) {
                    COMBFIND::messed_up_fleet_palette = 1;
                    if (selection < 15) {
                        target_id = selection;
                        *target_player_idx_out = selection;
                    } else {
                        int16_t colony_idx = selection - 15;
                        *target_colony_idx_out = colony_idx;
                        target_id = MOX::colony[colony_idx].owner;
                    }
                }

                if (target_id == -1) {
                    return;
                }

                if (COMBFIND::Human_Confirms_Attack(player_idx, target_id)) {
                    break;
                }
            }
        }
    }

    /**
 * Selects the best AI combat target available at a star.
 *
 * @param star_idx Star containing the pending combat.
 * @param player_idx AI attacker.
 * @param target_player Receives the chosen ship-only defender, or -1.
 * @param target_colony Receives the chosen colony target, or -1.
 * @param is_sneak_attack Receives nonzero when the selected attack is a sneak attack.
 */
    void __cdecl Get_AI_Target(int16_t star_idx, int16_t player_idx, int16_t* target_player, int16_t* target_colony, int8_t* is_sneak_attack) {
        *is_sneak_attack = 0;
        *target_player = -1;
        *target_colony = -1;

        if (MOX::NUM_STARS == star_idx) {
            *target_player = 8 /* STAR_ORION */;
            return;
        }

        if (COMBFIND::target.num_colony_targets > 0 || COMBFIND::target.num_player_targets > 0) {
            s_player* player = &MOX::player[player_idx];
            int16_t sneak_attack_target = -1;

            if (player->sneak_attack_planet != -1) {
                if (MOX::planet[player->sneak_attack_planet].star_index == star_idx) {
                    sneak_attack_target = (int16_t)player->sneak_attack_player;
                    player->sneak_attack_planet = -1;
                    player->sneak_attack_player = -1;
                    player->war_buildup_duration = 0;
                }
            }

            uint32_t bonus_info[4];
            uint32_t our_strength[15];
            uint32_t enemy_strength[15];

            COMBFIND::Get_Ship_Strength_Info(enemy_strength, our_strength, bonus_info, star_idx, player_idx);

            uint32_t best_colony_score = 0;
            int16_t best_colony_idx = -1;

            COMBFIND::Get_Best_Colony_Target(enemy_strength, our_strength, bonus_info, star_idx, player_idx, &best_colony_score, &best_colony_idx, sneak_attack_target);

            uint32_t best_player_score = 0;
            int16_t best_player_idx = -1;

            // Only consider player targets if there is no sneak attack target at this star or no valid colony target found
            if (sneak_attack_target == -1 || best_colony_idx == -1) {
                for (int16_t i = 0; i < COMBFIND::target.num_player_targets; ++i) {
                    int16_t enemy_idx = COMBFIND::target.player_targets[i];
                    uint32_t enemy_str = enemy_strength[enemy_idx];
                    uint32_t our_str = our_strength[enemy_idx];

                    if (enemy_str <= our_str) {
                        bool is_valid = false;
                        if (enemy_str < our_str) {
                            is_valid = true;
                        } else {
                            s_star_data* star = &MOX::star[star_idx];
                            uint8_t player_bit = (uint8_t)player_idx;
                            uint8_t enemy_bit = (uint8_t)enemy_idx;
                            // Check if player has a colony here and if it is currently blockaded by the enemy
                            if (((star->has_colony >> (player_bit & 0x1F)) & 1) &&
                                ((star->blockaded_by[player_bit] >> (enemy_bit & 0x1F)) & 1)) {
                                is_valid = true;
                            } else if (enemy_idx == sneak_attack_target) {
                                is_valid = true;
                            } else {
                                switch (player->personality) {
                                    case PLAYER_PERSONALITY_XENOPHOBIC:
                                    case PLAYER_PERSONALITY_RUTHLESS:
                                        is_valid = true;
                                        break;
                                    case PLAYER_PERSONALITY_AGGRESSIVE:
                                    case PLAYER_PERSONALITY_ERRATIC:
                                        is_valid = (game_random::Random(2) - 1) != 0;
                                        break;
                                }
                            }
                        }

                        if (is_valid) {
                            // Normalize strengths for scoring if the ratio is overwhelming
                            if (our_str > enemy_str * 3) {
                                our_str = 3;
                                enemy_str = 1;
                            } else if (our_str == 0) {
                                enemy_str = 1;
                            }

                            uint32_t score;
                            if (enemy_idx == sneak_attack_target) {
                                score = 0xFFFFFFFF;
                            } else {
                                score = (our_str << 4) / enemy_str;
                            }

                            if (best_player_score < score) {
                                best_player_score = score;
                                best_player_idx = enemy_idx;
                            }
                        }
                    }
                }
            }

            if (best_colony_score < best_player_score) {
                *target_player = best_player_idx;
                if (best_player_idx == sneak_attack_target) {
                    *is_sneak_attack = 1;
                }
            } else if (best_colony_score != 0) {
                *target_colony = best_colony_idx;
                if (MOX::colony[best_colony_idx].owner == sneak_attack_target) {
                    *is_sneak_attack = 1;
                }
            }
        }
    }

    void __cdecl Update_Player_Needs_To_Attack(int16_t star_idx, int player_a_idx, int player_b_idx) {
        // Handle first player
        if (player_a_idx < MAX_PLAYERS) {
            // Regular player: check presence via star-player status struct
            if ((*star_has_player)[star_idx][player_a_idx].has_player == 0) {
                (*player_needs_to_attack)[star_idx] &= ~(1 << (player_a_idx & 0x1f));
            }
        } else {
            // Non-player/Monster: check presence via bitmask
            if ((((*star_has_nonplayer)[star_idx] >> (player_a_idx & 0x1f)) & 1) == 0) {
                (*player_needs_to_attack)[star_idx] &= ~(1 << (player_a_idx & 0x1f));
            }
        }

        // Handle second player
        if (player_b_idx < MAX_PLAYERS) {
            // Regular player: check presence via star-player status struct
            if ((*star_has_player)[star_idx][player_b_idx].has_player == 0) {
                (*player_needs_to_attack)[star_idx] &= ~(1 << (player_b_idx & 0x1f));
            }
        } else {
            // Non-player/Monster: check presence via bitmask
            if ((((*star_has_nonplayer)[star_idx] >> (player_b_idx & 0x1f)) & 1) == 0) {
                (*player_needs_to_attack)[star_idx] &= ~(1 << (player_b_idx & 0x1f));
            }
        }
    }

    /**
 * Chooses a non-player combat target at a star.
 *
 * @param star_idx Star containing the pending combat.
 * @param attacker_idx Non-player attacker identifier.
 * @param target_player_idx_out Receives the chosen ship-only defender, or -1.
 * @param target_colony_idx_out Receives the chosen colony target, or -1.
 */
    void __cdecl Get_NPC_Target(int16_t star_idx, int16_t attacker_idx, int16_t* target_player_idx_out, int16_t* target_colony_idx_out) {
        int lucky_count;
        int non_lucky_count;
        int16_t lucky_target;
        int16_t non_lucky_target;
        int i;

        *target_colony_idx_out = -1;
        *target_player_idx_out = -1;

        if (attacker_idx == 13 /* EVENT_TYPE_13 */) {
            return;
        }

        if (MOX::NUM_STARS == star_idx) {
            *target_player_idx_out = 8;
            return;
        }

        if (COMBFIND::target.num_colony_targets > 0) {
            lucky_count = 0;
            non_lucky_count = 0;
            lucky_target = -1;
            non_lucky_target = -1;

            for (i = 0; i < (int)COMBFIND::target.num_colony_targets; ++i) {
                int16_t colony_idx = COMBFIND::target.colony_targets[i];
                int8_t owner = MOX::colony[colony_idx].owner;

                if (MOX::player[owner].traits[TRAIT_LUCKY] != 0) {
                    lucky_count++;
                    if (game_random::Random(lucky_count) == 1) {
                        lucky_target = colony_idx;
                    }
                } else {
                    non_lucky_count++;
                    if (game_random::Random(non_lucky_count) == 1) {
                        non_lucky_target = colony_idx;
                    }
                }
            }

            if (non_lucky_target != -1) {
                *target_colony_idx_out = non_lucky_target;
            } else {
                *target_colony_idx_out = lucky_target;
            }
            return;
        }

        if (COMBFIND::target.num_player_targets > 0) {
            lucky_count = 0;
            non_lucky_count = 0;
            lucky_target = -1;
            non_lucky_target = -1;

            for (i = 0; i < (int)COMBFIND::target.num_player_targets; ++i) {
                int16_t player_idx = COMBFIND::target.player_targets[i];

                // If player is valid index and not lucky, prioritize them
                if (player_idx < MAX_PLAYERS && MOX::player[player_idx].traits[TRAIT_LUCKY] == 0) {
                    non_lucky_count++;
                    if (game_random::Random(non_lucky_count) == 1) {
                        non_lucky_target = player_idx;
                    }
                } else {
                    lucky_count++;
                    if (game_random::Random(lucky_count) == 1) {
                        lucky_target = player_idx;
                    }
                }
            }

            if (non_lucky_target != -1) {
                *target_player_idx_out = non_lucky_target;
            } else {
                *target_player_idx_out = lucky_target;
            }
        }
    }

    /**
 * Assigns COMBFIND string pointers from a packed strings block.
 *
 * @param strings_block Start of the packed strings block.
 * @return Pointer to the first byte after the strings consumed by this unit.
 */
    char* __cdecl Load_Combfind_Strings(char* strings_block) {
        char* current = strings_block;

        noncombat_ships_destroyed_msg = current;
        current += strlen(current) + 1;

        you_have_following_treaties_msg = current;
        current += strlen(current) + 1;

        trade_treaty_msg = current;
        current += strlen(current) + 1;

        research_treaty_msg = current;
        current += strlen(current) + 1;

        receiving_tribute_msg = current;
        current += strlen(current) + 1;

        really_risk_war_msg = current;
        current += strlen(current) + 1;

        outpost_destroyed_msg = current;
        current += strlen(current) + 1;

        colony_seized_by_mind_control_msg = current;
        current += strlen(current) + 1;

        s_attacks_you_at_s_msg = current;
        current += strlen(current) + 1;

        combat_is_reported_between_s_and_s_msg = current;
        current += strlen(current) + 1;

        return current;
    }

    /**
 * Asks a human player to confirm an attack that may break diplomatic agreements.
 *
 * @param player_idx Attacking player.
 * @param target_idx Proposed target player.
 * @return Nonzero when the attack is confirmed.
 */
    int8_t __cdecl Human_Confirms_Attack(int player_idx, int target_idx) {
        char message_buffer[400];
        int8_t confirmed = 1;

        if (target_idx < MAX_PLAYERS && (*attack_confirmed)[player_idx][target_idx] == 0) {
            int treaty_count = 0;
            strlcpy(message_buffer, you_have_following_treaties_msg, sizeof(message_buffer));
            strlcat(message_buffer, "\n", sizeof(message_buffer));

            // Check standard treaties (e.g., Non-Aggression, Alliance)
            int8_t treaty_type = MOX::player[player_idx].treaty[target_idx];
            if (treaty_type > 0 && treaty_type < 4) {
                treaty_count = 1;
                strlcat(message_buffer, MOX::treaty_labels[treaty_type], sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            // Check trade treaty
            if (MOX::player[player_idx].trade_treaty[target_idx] != 0) {
                treaty_count++;
                strlcat(message_buffer, trade_treaty_msg, sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            // Check research treaty
            if (MOX::player[player_idx].research_treaty[target_idx] != 0) {
                treaty_count++;
                strlcat(message_buffer, research_treaty_msg, sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            // Check if player is receiving tribute from the target
            if (MOX::player[target_idx].tribute_treaty[player_idx] != 0) {
                treaty_count++;
                strlcat(message_buffer, receiving_tribute_msg, sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            if (treaty_count != 0) {
                strlcat(message_buffer, really_risk_war_msg, sizeof(message_buffer));
                confirmed = (int8_t)GENDRAW::Confirmation_Box(message_buffer);

                if (confirmed != 0) {
                    (*attack_confirmed)[player_idx][target_idx] = 1;
                }
            }
        }

        return confirmed;
    }

    /**
 * Copies the current target-selection state from another target record.
 *
 * @param src Source target-selection state.
 */
    void __cdecl Copy_Target_Info(s_combfind_target* src) {
        target = *src;
    }

    int8_t __cdecl Player_May_Attack_Player(int attacker_idx, int target_idx) {
        if (target_idx == attacker_idx) {
            return 0;
        }

        // Indices >= 8 usually represent Antarans, Guardians, or space monsters
        if (attacker_idx >= MAX_PLAYERS) {
            return 1;
        }

        s_player* attacker = &MOX::player[attacker_idx];

        // 0x64 ('d') represents human player
        if (attacker->objectives == PLAYER_OBJECTIVE_HUMAN) {
            return 1;
        }

        int8_t treaty_status = attacker->treaty[target_idx];
        if (treaty_status != 0) {
            // Specific treaty values allow combat (e.g., War or specific hostilities)
            if (treaty_status >= 4 && treaty_status <= 6) {
                return 1;
            }
            return 0;
        }

        // If no formal treaty exists, check for non-aggression or active trade/research ties
        if (attacker->stop_blockading_duration[target_idx] == 0 &&
            attacker->trade_treaty[target_idx] == 0 &&
            attacker->research_treaty[target_idx] == 0) {
            // Personality 0 is likely Ruthless/Aggressive
            if (attacker->personality == PLAYER_PERSONALITY_XENOPHOBIC) {
                return 1;
            }

            // Personality 3 is likely erratic or opportunistic, having a small chance to attack
            if (attacker->personality == PLAYER_PERSONALITY_ERRATIC) {
                if (game_random::Random(10) == 1) {
                    return 1;
                }
            }
        }

        return 0;
    }

    int16_t __cdecl Target_Colony_To_Planet(int16_t colony_idx) {
        if (colony_idx == -1) {
            return -1;
        }

        return MOX::colony[colony_idx].planet;
    }

    int8_t __cdecl Some_Star_Player_Is_Busy(int16_t star_idx) {
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                s_combfind_star_has_player& star_info = (*COMBFIND::star_has_player)[star_idx][i];

                if (star_info.colony_count != 0 || star_info.has_player != 0) {
                    if (NETMOX::net.player_net_info[i].combat_attacker_id != -1) {
                        return 1;
                    }

                    if (MOX::PLAYER_NUM == i) {
                        if (NETMOX::net.network_data_ptr != nullptr) {
                            return 1;
                        }
                    }
                }
            }
        }

        return 0;
    }

    void __cdecl Decode_Star_Player_Presence(NETPROTO::Packet_Reader_* reader, int16_t star_idx) {
        s_combfind_star_has_player* dest;

        if (star_idx == -1) {
            dest = current_star_has_player;
        } else {
            dest = (*star_has_player)[star_idx];
        }

        reader->Read_Bytes(dest, MAX_PLAYERS * sizeof(*dest));

        if (star_idx == -1) {
            *current_star_has_nonplayer = reader->Read_Uint8();
        } else {
            (*star_has_nonplayer)[star_idx] = reader->Read_Uint8();
        }
    }

    void __cdecl Get_Combat_Ships(int16_t* ships, int16_t star_idx, int16_t plr1_idx, int16_t* plr1_offset, int16_t* plr1_count, int16_t plr2_idx, int16_t* plr2_offset, int16_t* plr2_count) {
        *plr1_offset = 0;
        *plr1_count = 0;
        *plr2_offset = MAX_SHIPS;
        *plr2_count = 0;

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];

            if (ship->location == star_idx && ship->status == 0) {
                if (ship->owner == (int8_t)plr1_idx) {
                    ships[*plr1_count] = i;
                    *plr1_count = *plr1_count + 1;
                } else if (ship->owner == (int8_t)plr2_idx) {
                    *plr2_offset = *plr2_offset - 1;
                    ships[*plr2_offset] = i;
                    *plr2_count = *plr2_count + 1;
                }
            }
        }
    }

    int16_t __cdecl Do_1_Combat(int16_t star_idx, int16_t plr2_idx, int16_t plr1_idx, int16_t planet_idx, s_battle_results* result, int8_t is_sneak_attack) {
        if (combat_in_progress) {
            exitm::Exit_With_Message("nested combat detected");
        }
        combat_in_progress = true;

        COMBINIT::Reset_Satellite_Capture();

        int16_t colony_idx;
        int16_t plr1_ship_offset;
        int16_t plr2_ship_offset;
        int16_t plr1_ship_count;
        int16_t plr2_ship_count;
        char msg_buf[200];
        int16_t ships[MAX_SHIPS];
        s_battle_results local_results;

        colony_idx = Target_Planet_To_Colony(planet_idx);

        if (plr1_idx == -1) {
            plr1_idx = MOX::colony[colony_idx].owner;
        }

        if (plr2_idx < MAX_PLAYERS && plr1_idx < MAX_PLAYERS) {
            MOX::player[plr2_idx].time_since_last_attack[plr1_idx] = 0;
        }

        if (result == nullptr) {
            result = &local_results;
            memset(result, 0, sizeof(*result));
        }

        if (MOX::game_type < 2 || MOX::game_type > 3) {
            current_star_has_player = &(*star_has_player)[star_idx][0];
            current_star_has_nonplayer = &(*star_has_nonplayer)[star_idx];
        } else {
            if (NETMOX::net.local_net_id == NETMOX::net.net_host_id) {
                current_star_has_player = &(*star_has_player)[star_idx][0];
                current_star_has_nonplayer = &(*star_has_nonplayer)[star_idx];
            }
            if (MOX::PLAYER_NUM == plr1_idx && MOX::settings.strategic_combat_flag == 0) {
                char* star_or_planet_name;
                if (planet_idx == -1) {
                    star_or_planet_name = RUSS::Star_Name(star_idx);
                } else {
                    star_or_planet_name = HACCESS::Get_Planet_Name(planet_idx);
                }
                char* attacker_name = COLCALC::Player_Name(plr2_idx);
                snprintf(msg_buf, sizeof(msg_buf), s_attacks_you_at_s_msg, attacker_name, star_or_planet_name);
                TEXTBOX::Timed_Text_Box(
                    nullptr,
                    msg_buf,
                    confirm_battle_message_timeout);
            }
        }

        Get_Combat_Ships(ships, star_idx, plr2_idx, &plr2_ship_offset, &plr2_ship_count, plr1_idx, &plr1_ship_offset, &plr1_ship_count);

        short combat_status = Russ_Combat(plr2_idx, &ships[plr2_ship_offset], plr2_ship_count, plr1_idx, &ships[plr1_ship_offset], plr1_ship_count, star_idx, colony_idx, is_sneak_attack, result);
        result->player_info.combat_result = (int8_t)combat_status;

        colony_idx = Target_Planet_To_Colony(planet_idx);

        if (MOX::game_type > 1 && MOX::game_type < 4) {
            if (MOX::PLAYER_NUM == plr2_idx || MOX::PLAYER_NUM == plr1_idx) {
                MULTPLAY::Load_Net_Next_Turn_Screen(1);
                MOX::FULL_DRAW_SCREEN_FLAG = 1;
                MULTPLAY::Chat_Box_Input_Loop();
            }
        }

        if (result->player_info.combat_result >= 0) {
            if (plr1_idx < MAX_PLAYERS) {
                if (current_star_has_player[plr1_idx].ship_sizes[SHIP_TYPE_COMBAT] == 0 && colony_idx == -1) {
                    Kill_Noncombat_Ships(&ships[plr1_ship_offset], plr1_ship_count);
                } else {
                    Determine_Retreat_Ships(star_idx, plr1_idx, colony_idx);
                }
            } else {
                *current_star_has_nonplayer &= ~(1 << (plr1_idx - MAX_PLAYERS));
            }
        }

        if (result->player_info.combat_result <= 0) {
            if (plr2_idx < MAX_PLAYERS) {
                Determine_Retreat_Ships(star_idx, plr2_idx, -1);
            } else {
                *current_star_has_nonplayer &= ~(1 << (plr2_idx - MAX_PLAYERS));
            }

            if (plr2_idx == 9) { // Guardian defeated
                MOX::player[plr1_idx].captured_orion = 1;
                OFFICER::Give_Player_Loknar_Special(plr1_idx, result->info.orion_techs, &result->info.rescued_hero_id);
                if (MOX::player[plr1_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::game_type == 0 || MOX::game_type == 1 || MOX::settings.strategic_combat_flag == 0) {
                        ERIC::Play_Take_Orion_Flic();
                        OFFICER::Rescued_Loknar_Popup(plr1_idx, result->info.orion_techs, result->info.rescued_hero_id);
                    }
                }
            }

            if (result->player_info.combat_result < 0) {
                Get_Combat_Ships(ships, star_idx, plr2_idx, &plr2_ship_offset, &plr2_ship_count, plr1_idx, &plr1_ship_offset, &plr1_ship_count);
                if (plr1_ship_count == 0) {
                    if (plr1_idx < MAX_PLAYERS) {
                        Reset_Combat_Ships_At_Current_Star_Has_Player(plr1_idx);
                    } else {
                        *current_star_has_nonplayer &= ~(1 << (plr1_idx - MAX_PLAYERS));
                    }
                }
            }
        } else {
            if (plr1_idx == 9) { // Guardian defeated by attacker
                MOX::player[plr2_idx].captured_orion = 1;
                OFFICER::Give_Player_Loknar_Special(plr2_idx, result->info.orion_techs, &result->info.rescued_hero_id);
                if (MOX::player[plr2_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::game_type == 0 || MOX::game_type == 1 || MOX::settings.strategic_combat_flag == 0) {
                        ERIC::Play_Take_Orion_Flic();
                        OFFICER::Rescued_Loknar_Popup(plr2_idx, result->info.orion_techs, result->info.rescued_hero_id);
                    }
                }
            }

            if (star_idx == MOX::NUM_STARS) { // Antares victory
                MOX::player[plr2_idx].captured_antares = 1;
                memset(*player_needs_to_attack, 0, sizeof(*player_needs_to_attack));
            } else {
                Get_Combat_Ships(ships, star_idx, plr2_idx, &plr2_ship_offset, &plr2_ship_count, plr1_idx, &plr1_ship_offset, &plr1_ship_count);
                if (plr2_ship_count == 0) {
                    if (plr2_idx < MAX_PLAYERS) {
                        Reset_Combat_Ships_At_Current_Star_Has_Player(plr2_idx);
                    } else {
                        *current_star_has_nonplayer &= ~(1 << (plr2_idx - MAX_PLAYERS));
                    }
                }
                if (colony_idx != -1 && MOX::colony[colony_idx].owner != -1) {
                    Do_Attacker_Beat_Colony_Stuff(&ships[plr2_ship_offset], plr2_ship_count, colony_idx, result);
                }
            }
        }

        const int16_t combat_result = (int16_t)result->player_info.combat_result;
        COMBINIT::Reset_Satellite_Capture();
        combat_in_progress = false;
        return combat_result;
    }

    int16_t __cdecl Russ_Combat(int16_t attacker_idx, int16_t* attacker_ships, int16_t attacker_count, int16_t defender_idx, int16_t* defender_ships, int16_t defender_count, int16_t star_idx, int16_t colony_idx, int8_t is_sneak_attack, s_battle_results* results) {
        bool not_multiplayer;
        bool plr1_is_human;
        bool plr2_is_human;
        uint8_t strategic_combat_flag;
        int16_t combat_result_code;

        if (MOX::game_type == 2 || MOX::game_type == 3) {
            not_multiplayer = false;
        } else {
            not_multiplayer = true;
        }

        strategic_combat_flag = MOX::settings.strategic_combat_flag;

        if (attacker_idx < MAX_PLAYERS && MOX::player[attacker_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            plr1_is_human = true;
        } else {
            plr1_is_human = false;
        }

        if (defender_idx < MAX_PLAYERS && MOX::player[defender_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            plr2_is_human = true;
        } else {
            plr2_is_human = false;
        }

        // Early return if no actual combat possible and not at special star location
        if (((attacker_count == 0) || (defender_count == 0 && colony_idx == -1)) && (star_idx != MOX::NUM_STARS)) {
            return 0;
        }

        if (attacker_idx < MAX_PLAYERS) {
            REPORT::something_interesting_happened[attacker_idx] = 1;
        }
        if (defender_idx < MAX_PLAYERS) {
            REPORT::something_interesting_happened[defender_idx] = 1;
        }

        // Display attacker alerts in single player mode
        if (not_multiplayer && plr2_is_human) {
            if (is_sneak_attack != 0) {
                DIP_SCRN::Show_Sneak_Attack_Message(attacker_idx, defender_idx);
            }
            MAINPUPS::Player_Attacked_Popup(attacker_idx, defender_idx, star_idx, colony_idx);
            COMBFIND::messed_up_fleet_palette = 1;
        }

        // Force strategic combat if no human players are involved
        if (strategic_combat_flag == 0) {
            if (!plr1_is_human && !plr2_is_human) {
                strategic_combat_flag = 1;
            }
        }

        if (strategic_combat_flag != 0) {
            combat_result_code = COMBAT::Strategic_Combat(attacker_ships, attacker_count, defender_ships, defender_count, star_idx, colony_idx, results->info.summary, sizeof(results->info.summary), results->info.details, sizeof(results->info.details));

            // Show result dialog if human is involved or it is a local single player game
            if (not_multiplayer || MOX::PLAYER_NUM == attacker_idx) {
                if (plr2_is_human || plr1_is_human) {
                    MAINSCR::Update_Ship_Icons_In_Between_Turns();
                    COMBAT::Message_Box_Titled(results->info.summary, results->info.details);
                    MAINSCR::Update_Ship_Icons_In_Between_Turns();
                    COMBFIND::messed_up_fleet_palette = 1;
                }
            }
        } else {
            int8_t retreat_flag = 1;

            if (attacker_idx < MAX_PLAYERS && defender_idx < MAX_PLAYERS) {
                if (plr1_is_human) {
                    if (!plr2_is_human) {
                        retreat_flag = AI_Can_Retreat((int8_t)defender_idx, colony_idx);
                    }
                } else {
                    retreat_flag = AI_Can_Retreat((int8_t)attacker_idx, -1);
                }
            } else {
                retreat_flag = 0;
            }

            // Multiplayer synchronization for tactical combat
            if (plr1_is_human && plr2_is_human && (MOX::game_type == 2 || MOX::game_type == 3)) {
                int16_t remote_plr_idx = (MOX::PLAYER_NUM == attacker_idx) ? defender_idx : attacker_idx;
                int16_t net_id = MOX::player[remote_plr_idx].network_player_id;

                NETMOX::Mox_Send_Message(net_id, NET_PACKET_TACTICAL_COMBAT_START, nullptr, 0);

                while (NETMOX::net.tactical_combat_sync_state == NET_TACTICAL_COMBAT_SYNC_WAITING_FOR_START) {
                    RUSS::Mox_Update();
                }

                combat_result_code = COMBINIT::Tactical_Combat(attacker_ships, attacker_count, defender_ships, defender_count, star_idx, colony_idx, retreat_flag);

                NETMOX::Mox_Send_Message(net_id, NET_PACKET_TACTICAL_COMBAT_END, nullptr, 0);

                while (NETMOX::net.tactical_combat_sync_state == NET_TACTICAL_COMBAT_SYNC_STARTED) {
                    RUSS::Mox_Update();
                }

                NETMOX::net.tactical_combat_sync_state = NET_TACTICAL_COMBAT_SYNC_WAITING_FOR_START;
            } else {
                combat_result_code = COMBINIT::Tactical_Combat(attacker_ships, attacker_count, defender_ships, defender_count, star_idx, colony_idx, retreat_flag);
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        if (combat_result_code < 0) {
            return -1;
        }

        return (combat_result_code > 0);
    }

    void __cdecl Kill_Noncombat_Ships(int16_t* ship_indices, int16_t count) {
        if (count <= 0) {
            return;
        }

        int8_t owner_idx = MOX::ship[ship_indices[0]].owner;

        if (MOX::player[owner_idx].objectives == PLAYER_OBJECTIVE_HUMAN &&
            MOX::game_type >= 0 && MOX::game_type <= 1) {
            GENDRAW::Message_Box(noncombat_ships_destroyed_msg);
        }

        for (int16_t i = 0; i < count; ++i) {
            SHIPS::Kill_Ship(ship_indices[i]);
        }

        current_star_has_player[owner_idx].has_player = 0;
        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    void __cdecl Determine_Retreat_Ships(int16_t star_idx, int16_t player_idx, int16_t colony_idx) {
        int32_t p_idx = (int32_t)player_idx;
        bool should_retreat_or_die = false;

        if (colony_idx != -1 || COMBFIND::current_star_has_player[p_idx].colony_count == 0) {
            if (COMBFIND::current_star_has_player[p_idx].colony_count < 2) {
                should_retreat_or_die = true;
            }
        }

        if (should_retreat_or_die) {
            bool destroy_ships_in_flux = false;
            if (player_idx < MAX_PLAYERS && MOX::player[p_idx].traits[29 /* TRAIT_TRANS_DIMENSIONAL */] == 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                    destroy_ships_in_flux = true;
                }
            }

            if (destroy_ships_in_flux) {
                // Under Hyperspace Flux, non-trans-dimensional player fleets at this star are destroyed instead of retreating.
                for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                    s_ship_data* ship = &MOX::ship[i];
                    if (ship->owner == player_idx && ship->location == star_idx && ship->status == 0) {
                        if (ship->officer_index != -1) {
                            if (MOX::leaders[ship->officer_index].eta <= 0) {
                                OFFICER::Kill_Officer(ship->officer_index);
                            } else {
                                OFFICER::Move_Officer_To_Pool(ship->officer_index);
                            }
                        }
                        SHIPS::Kill_Ship(i);
                    }
                }
                COMBFIND::current_star_has_player[p_idx].has_player = 0;
                return;
            } else {
                // Otherwise the fleet enters retreat status.
                for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                    s_ship_data* ship = &MOX::ship[i];
                    if (ship->owner == player_idx && ship->location == star_idx && ship->status == 0) {
                        ship->status = 9; // Status 9: retreating/travelling
                    }
                }
                COMBFIND::current_star_has_player[p_idx].has_player = 0;
                MAINSCR::Update_Ship_Icons_In_Between_Turns();
                return;
            }
        }

        // Default case: verify if any active ships remain at the star
        bool ships_remaining = false;
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->owner == player_idx && ship->location == star_idx && ship->status == 0) {
                ships_remaining = true;
                break;
            }
        }

        if (!ships_remaining) {
            COMBFIND::current_star_has_player[p_idx].has_player = 0;
        }
    }

    void __cdecl Reset_Combat_Ships_At_Current_Star_Has_Player(int16_t player_idx) {
        current_star_has_player[player_idx].ship_sizes[SHIP_TYPE_COMBAT] = 0;
        current_star_has_player[player_idx].has_player = 0;

        for (int i = 0; i < SHIP_TYPE_COUNT; ++i) {
            if (current_star_has_player[player_idx].ship_sizes[i] != 0) {
                current_star_has_player[player_idx].has_player = 1;
            }
        }
    }

    /**
 * Checks if the specified player owns any colony other than the one at excluded_colony_idx.
 * This is typically used to determine if an AI player can retreat (needs at least one other colony to retreat to).
 */
    int8_t __cdecl AI_Can_Retreat(int8_t player_idx, int16_t excluded_colony_idx) {
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            if (MOX::colony[i].owner == player_idx && i != excluded_colony_idx) {
                return 1;
            }
        }
        return 0;
    }

    void __cdecl Get_Ship_Strength_Info(uint32_t* enemy_strength, uint32_t* player_strength, uint32_t* shield_strength, int16_t star_idx, int16_t player_idx) {
        uint32_t ship_size_sum = 0;

        memset(enemy_strength, 0, 15 * sizeof(uint32_t));
        memset(player_strength, 0, 15 * sizeof(uint32_t));
        memset(shield_strength, 0, 4 * sizeof(uint32_t));

        int16_t num_ships = MOX::NUM_SHIPS;
        s_ship_data* ship = MOX::ship;

        for (int16_t i = 0; i < num_ships; ++i, ++ship) {
            if (ship->location == star_idx && ship->status == 0) {
                int16_t owner = ship->owner;

                if (player_idx == owner) {
                    ship_size_sum += 1U << (ship->d.size & 0x1F);

                    for (int16_t p = 0; p < MOX::NUM_PLAYERS; ++p) {
                        if (player_idx != p) {
                            if ((*star_has_player)[star_idx][p].has_player != 0 ||
                                (*star_has_player)[star_idx][p].colony_count != 0) {
                                player_strength[p] += AIPOWER::Ship_Strength_Vs_Ships(&game::current, legacy_world::Ship_Id(i), legacy_world::Player_Id(p));
                            }
                        }
                    }

                    for (int16_t np = MAX_PLAYERS; np < 15; ++np) {
                        if ((((*star_has_nonplayer)[star_idx] >> (np - MAX_PLAYERS)) & 1) != 0 &&
                            (((*player_needs_to_attack)[star_idx] >> np) & 1) == 0) {
                            player_strength[np] += AIPOWER::Ship_Strength_Vs_Ships(&game::current, legacy_world::Ship_Id(i), legacy_world::Player_Id(np));
                        }
                    }

                    uint16_t shield[4];
                    AIPOWER::Ship_Strength_Vs_Planetary_Shield(&game::current, legacy_world::Ship_Id(i), shield);

                    for (int j = 0; j < 4; ++j) {
                        shield_strength[j] += shield[j];
                    }
                } else {
                    enemy_strength[owner] += AIPOWER::Ship_Strength_Vs_Ships(&game::current, legacy_world::Ship_Id(i), legacy_world::Player_Id(player_idx));
                }
            }
        }

        if (ship_size_sum > 150) {
            for (int i = 0; i < 15; ++i) {
                uint32_t val = player_strength[i];
                uint32_t new_val = (val / 150) * ship_size_sum;
                player_strength[i] = new_val;
                if (val > new_val) {
                    player_strength[i] = val;
                }
            }
        }
    }

    void __cdecl Get_Best_Colony_Target(
        uint32_t* enemy_fleet_space_str_arr,
        uint32_t* our_space_str_arr,
        uint32_t* our_ground_str_arr,
        int16_t star_idx,
        int16_t player_idx,
        uint32_t* best_score,
        int16_t* best_colony_idx,
        int32_t preferred_owner) {
        *best_score = 0;
        *best_colony_idx = -1;

        uint32_t best_pref_score = 0;
        int16_t best_pref_colony = -1;

        if (target.num_colony_targets > 0) {
            for (int16_t i = 0; i < target.num_colony_targets; ++i) {
                int16_t colony_idx = target.colony_targets[i];
                s_colony* colony = &MOX::colony[colony_idx];

                int16_t shield_val = COLCALC::Colony_Planetary_Shield(colony);
                int32_t owner = colony->owner;

                uint32_t our_space_str = our_space_str_arr[owner];
                uint32_t our_ground_str = our_ground_str_arr[shield_val];

                uint32_t enemy_space_str_vs = AIPOWER::Colony_Space_Strength_Vs_Player(&game::current, legacy_world::Colony_Id(colony_idx), legacy_world::Player_Id(player_idx)) & 0xFFFF;
                uint32_t enemy_space_str = enemy_fleet_space_str_arr[owner] + enemy_space_str_vs;

                uint32_t enemy_ground_str_vs = AIPOWER::Colony_Ground_Strength_Vs_Player(&game::current, legacy_world::Colony_Id(colony_idx), legacy_world::Player_Id(player_idx)) & 0xFFFF;
                uint32_t enemy_ground_str = enemy_ground_str_vs;

                bool player_is_omni = false;
                if (MOX::player[player_idx].traits[TRAIT_TELEPATHIC] != 0 && MOX::player[owner].traits[TRAIT_TELEPATHIC] == 0) {
                    player_is_omni = true;
                }

                bool is_valid = false;
                if (our_space_str != 0 || our_ground_str != 0 ||
                    (*star_has_player)[star_idx][player_idx].has_transport != 0 ||
                    player_is_omni ||
                    colony->outpost_flag != 0) {
                    if (our_space_str > enemy_space_str && our_ground_str > enemy_ground_str) {
                        bool condition2 = ((*star_has_player)[star_idx][owner].colony_count < 2 ||
                            our_ground_str != 0 ||
                            (*star_has_player)[star_idx][player_idx].has_transport != 0 ||
                            player_is_omni);

                        if (condition2 || enemy_space_str_vs != 0) {
                            if (colony->n_pops != 1 ||
                                (colony->pop[0] & 0xF) != player_idx ||
                                (*star_has_player)[star_idx][player_idx].ship_sizes[SHIP_TYPE_TRANSPORT] != 0 ||
                                MOX::player[player_idx].personality == PLAYER_PERSONALITY_RUTHLESS) {
                                is_valid = true;
                            }
                        }
                    }
                }

                if (is_valid) {
                    uint32_t worth = (uint32_t)AIMOVE::Enemy_Colony_Worth_To_Player(&AIDATA::state, &game::current, legacy_world::Colony_Id(colony_idx), legacy_world::Player_Id(player_idx));

                    if (enemy_space_str * 3 < our_space_str) {
                        our_space_str = 3;
                        enemy_space_str = 1;
                    } else if (our_space_str == 0) {
                        our_space_str = 1;
                        enemy_space_str = 1;
                    }

                    if (enemy_ground_str * 3 < our_ground_str) {
                        our_ground_str = 3;
                        enemy_ground_str = 1;
                    } else if (our_ground_str == 0) {
                        our_ground_str = 1;
                        enemy_ground_str = 1;
                    }

                    uint32_t enemy_space_str_scaled = enemy_space_str;
                    if (enemy_space_str_scaled > 0xFFFF) {
                        enemy_space_str_scaled >>= 8;
                        our_space_str >>= 8;
                    }

                    uint32_t enemy_ground_str_scaled = enemy_ground_str;
                    if (enemy_ground_str_scaled > 0xFFFF) {
                        enemy_ground_str_scaled >>= 8;
                        our_ground_str >>= 8;
                    }

                    uint32_t score = ((worth * our_space_str) / enemy_space_str_scaled * our_ground_str) / enemy_ground_str_scaled;

                    if (score > *best_score) {
                        *best_score = score;
                        *best_colony_idx = colony_idx;
                    }

                    if (owner == preferred_owner && score > best_pref_score) {
                        best_pref_score = score;
                        best_pref_colony = colony_idx;
                    }
                }
            }
        }

        if (preferred_owner != -1 && best_pref_colony != -1) {
            *best_score = best_pref_score;
            *best_colony_idx = best_pref_colony;
        }
    }

    void __cdecl Do_Attacker_Beat_Colony_Stuff(int16_t* ship_list, int n_ships, int16_t colony_idx, s_battle_results* results) {
        s_ship_data* ships = MOX::ship;
        int16_t ship_idx = *ship_list;
        int8_t attacker_id = ships[ship_idx].owner;
        s_colony* colony = &MOX::colony[colony_idx];
        int16_t planet_idx = colony->planet;
        int16_t star_idx = MOX::planet[planet_idx].star_index;
        int8_t colony_owner = colony->owner;

        if (colony->outpost_flag != 0) {
            results->outpost_destroyed_flag = 1;
            BOMB::Destroy_Colony(colony, 1);
            COMBFIND::current_star_has_player[colony_owner].colony_count--;
        } else if (attacker_id < MAX_PLAYERS) {
            if (MOX::player[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                bool is_active_player = false;
                if ((MOX::game_type != 2 && MOX::game_type != 3) || (MOX::PLAYER_NUM == attacker_id)) {
                    is_active_player = true;
                }

                if (is_active_player) {
                    int32_t bombardment_res = Do_Bombardment(attacker_id, colony_idx);
                    bool has_stellar = Has_Stellar_Converter(ship_list, n_ships);

                    invasion::Get_Invasion_Info(ship_list, n_ships, colony, &results->info.attacker_invasion, &results->info.defender_invasion);
                    int seed = game_random::Get_Random_Seed();
                    int16_t selection = MAINPUPS::Colony_Selection_Popup(&results->info.attacker_invasion, &results->info.defender_invasion, seed, colony_idx, bombardment_res, has_stellar);

                    bool loop_done = false;
                    do {
                        switch (selection) {
                            case 1: { // Stellar Converter
                                int8_t old_n_pops = colony->n_pops;
                                results->info.planet_destroyed_flag = 1;
                                BOMB::Destroy_Colony(colony, 2);
                                ERIC::Play_Planetary_Destruction_Flic();
                                COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                MOX::planet[planet_idx].planet_type = PLANET_TYPE_ASTEROID;
                                if (MOX::planet[planet_idx].planet_special != SYSTEM_SPECIAL_NO_SPECIAL) {
                                    MOX::planet[planet_idx].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
                                    MOX::star[star_idx].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                                }
                                NETMOX::Russ_Change_Relations(-10000, attacker_id, colony_owner, 10, star_idx, old_n_pops);
                                loop_done = true;
                                break;
                            }
                            case 2: { // Bombing
                                int bomb_seed = game_random::Get_Random_Seed();
                                memset(&results->bomb, 0, sizeof(s_combat_bomb_result));
                                results->bomb.attacker_idx = attacker_id;
                                results->bomb.colony_idx = colony_idx;
                                COMBAT::Bombardment(&results->bomb);

                                invasion::Get_Invasion_Info(ship_list, n_ships, colony, &results->info.attacker_invasion, &results->info.defender_invasion);
                                selection = MAINPUPS::Colony_Bombing_Popup(&results->bomb, &results->info.attacker_invasion, &results->info.defender_invasion, bomb_seed, colony_idx, bombardment_res, has_stellar);
                                NETMOX::Russ_Change_Relations(-10000, attacker_id, colony_owner, 10, star_idx, results->bomb.population_killed);

                                if (results->bomb.colony_wiped_out != 0) {
                                    BOMB::Destroy_Colony(colony, 1);
                                    COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                    loop_done = true;
                                } else {
                                    if (results->info.attacker_invasion.unit_counts[1] == 0 && !has_stellar && !ERIC::Player_Can_Mind_Control_Colony(attacker_id, colony_idx)) {
                                        loop_done = true;
                                    }
                                }
                                bombardment_res = -1;
                                break;
                            }
                            case 3: { // Invasion
                                invasion::Invade(ship_list, n_ships, colony_idx, &results->info);
                                int combat_seed = game_random::Get_Random_Seed();
                                ERIC::Force_Fade_To_Colony_Palette(1);
                                COLGCBT::Colony_Combat_Screen(&results->info.attacker_invasion, &results->info.defender_invasion, results->info.ground_defense_strength, colony_idx);
                                ERIC::Force_Fade_To_Colony_Palette(0);
                                game_random::Set_Random_Seed(combat_seed);

                                if (results->info.captured_tech_id != 0) {
                                    SCIENCE::Show_Off_Captured_Tech(results->info.captured_techs_data, results->info.captured_tech_id, results->info.captured_tech_planet_id);
                                }
                                NETMOX::Russ_Change_Relations(-10000, attacker_id, colony_owner, 10, star_idx, 0);

                                if (colony->owner == attacker_id) {
                                    COMBFIND::current_star_has_player[attacker_id].colony_count++;
                                    COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                }
                                COMBFIND::messed_up_fleet_palette = 1;
                                loop_done = true;
                                break;
                            }
                            case 4: { // Surrender/Mind Control
                                invasion::Change_Colony_Ownership(colony, attacker_id, &results->info);
                                results->info.is_mind_controlled = 1;
                                if (results->info.captured_tech_id != 0) {
                                    SCIENCE::Show_Off_Captured_Tech(results->info.captured_techs_data, results->info.captured_tech_id, colony->planet);
                                }
                                NETMOX::Russ_Change_Relations(-10000, attacker_id, colony_owner, 10, star_idx, colony->n_pops);
                                COMBFIND::current_star_has_player[attacker_id].colony_count++;
                                COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                loop_done = true;
                                break;
                            }
                            case -1:
                            default: {
                                loop_done = true;
                                break;
                            }
                        }
                    } while (!loop_done);

                    if (results->bomb.bio_pop_killed != 0) {
                        COMBINIT::Apply_Bio_Weapon_Penalty(attacker_id, star_idx, results->bomb.bio_pop_killed);
                    }

                    if ((MOX::game_type > 1 && MOX::game_type < 4) && (MOX::PLAYER_NUM == attacker_id || MOX::PLAYER_NUM == colony_owner)) {
                        MULTPLAY::Load_Net_Next_Turn_Screen(0);
                        MOX::FULL_DRAW_SCREEN_FLAG = 1;
                        MULTPLAY::Chat_Box_Input_Loop();
                    }
                }
            } else {
                if (ERIC::Player_Can_Mind_Control_Colony(attacker_id, colony_idx)) {
                    results->info.is_mind_controlled = 1;
                    invasion::Change_Colony_Ownership(colony, attacker_id, &results->info);
                    COMBFIND::current_star_has_player[attacker_id].colony_count++;
                    COMBFIND::current_star_has_player[colony_owner].colony_count--;
                } else {
                    bool has_stellar = Has_Stellar_Converter(ship_list, n_ships);
                    int16_t enemy_star_idx = MOX::planet[colony->planet].star_index;
                    bool knows_attacker = false;
                    uint8_t surrendered = 0;
                    uint8_t terminal_destroyed = 0;

                    for (int i = 0; i < colony->n_pops; ++i) {
                        if ((colony->pop[i] & 0xF) == (uint32_t)attacker_id) {
                            knows_attacker = true;
                            break;
                        }
                    }

                    memset(&results->bomb, 0, sizeof(s_combat_bomb_result));
                    results->bomb.attacker_idx = attacker_id;
                    results->bomb.colony_idx = colony_idx;
                    COMBAT::Bombardment(&results->bomb);

                    int bomb_hits = results->bomb.bomb_hits_remaining;
                    int bio_hits = results->bomb.bio_hits_remaining;

                    if (bomb_hits != 0 || bio_hits != 0 || has_stellar) {
                        bool surrender = false;
                        if (MOX::player[colony_owner].objectives != PLAYER_OBJECTIVE_HUMAN && MOX::settings.game_difficulty > GAME_DIFFICULTY_AVERAGE) {
                            if (COLCALC::May_Surrender_Star(enemy_star_idx, colony_owner, attacker_id) && (int)(colony->n_pops * 2) <= bomb_hits) {
                                surrender = true;
                            }
                        }

                        if (surrender) {
                            surrendered = 1;
                            if (attacker_id >= 0 && attacker_id < MAX_PLAYERS) {
                                MOX::star[star_idx].surrender_to[colony_owner] = attacker_id + MAX_PLAYERS;
                                diplomac::Declare_Peace(colony_owner, attacker_id);
                            }
                        } else if (knows_attacker) {
                            bool success = false;
                            bool done = false;
                            do {
                                if (aibomb::AI_Invasion_Will_Succeed(attacker_id, ship_list, n_ships, colony)) {
                                    success = true;
                                } else {
                                    if (colony->n_pops == 1 && (colony->pop[0] & 0xF) == (uint32_t)attacker_id &&
                                        MOX::player[attacker_id].personality != PLAYER_PERSONALITY_RUTHLESS && (COMBFIND::Player_Owns_Transports(attacker_id) || (MOX::player[attacker_id].traits[TRAIT_TELEPATHIC] != 0 && MOX::player[colony_owner].traits[TRAIT_TELEPATHIC] == 0))) {
                                        done = true;
                                    } else if (bio_hits == 0) {
                                        int used = BOMB::Resolve_Bomb_Hit(colony, &results->bomb, bomb_hits);
                                        if (used == 0)
                                            bomb_hits = 0;
                                        else
                                            bomb_hits -= used;
                                    } else {
                                        BOMB::Resolve_Biological_Hit(colony, &results->bomb);
                                        bio_hits--;
                                    }
                                }
                            } while (!success && !done && (bio_hits + bomb_hits != 0));
                        } else {
                            int stars_in_range = N_Enemy_Stars_In_Range(attacker_id, colony_owner);
                            if (stars_in_range < 2 && N_Enemy_Stars(colony_owner) != 1) {
                                bool success = false;
                                bool done = false;
                                do {
                                    if (aibomb::AI_Invasion_Will_Succeed(attacker_id, ship_list, n_ships, colony)) {
                                        success = true;
                                    } else {
                                        if (colony->n_pops == 1) {
                                            done = true;
                                        } else if (bio_hits == 0) {
                                            int used = BOMB::Resolve_Bomb_Hit(colony, &results->bomb, bomb_hits);
                                            if (used == 0)
                                                bomb_hits = 0;
                                            else
                                                bomb_hits -= used;
                                        } else {
                                            BOMB::Resolve_Biological_Hit(colony, &results->bomb);
                                            bio_hits--;
                                        }
                                    }
                                } while (!success && !done && (bio_hits + bomb_hits != 0));
                            } else if (has_stellar) {
                                results->info.planet_destroyed_flag = 1;
                                terminal_destroyed = 1;
                                BOMB::Destroy_Colony(colony, 2);
                            } else {
                                BOMB::Apply_Bomb_Hits(&results->bomb);
                            }
                        }

                        if (results->bomb.bio_pop_killed != 0) {
                            COMBINIT::Apply_Bio_Weapon_Penalty(attacker_id, star_idx, results->bomb.bio_pop_killed);
                        }

                        if (MOX::player[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
                            (MOX::settings.strategic_combat_flag == 0 || (MOX::game_type != 2 && MOX::game_type != 3) ||
                                MOX::player[colony_owner].network_player_id == NETMOX::net.local_net_id) &&
                            results->bomb.damage_occurred != 0) {
                            COLBOMB::Colony_Bombing_Screen(&results->bomb);
                        }

                        NETMOX::Russ_Change_Relations(-10000, attacker_id, colony_owner, 10, star_idx, results->bomb.population_killed);

                        if (results->bomb.colony_wiped_out != 0) {
                            terminal_destroyed = 1;
                            BOMB::Destroy_Colony(colony, 1);
                        }
                    }

                    if (!terminal_destroyed && !surrendered &&
                        invasion::Has_Defending_Troops(attacker_id, ship_list, n_ships, colony_idx)) {
                        invasion::Invade(ship_list, n_ships, colony_idx, &results->info);
                        if (MOX::player[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
                            (MOX::settings.strategic_combat_flag == 0 || (MOX::game_type != 2 && MOX::game_type != 3) ||
                                MOX::player[colony_owner].network_player_id == NETMOX::net.local_net_id)) {
                            int invade_seed = game_random::Get_Random_Seed();
                            ERIC::Force_Fade_To_Colony_Palette(1);
                            COLGCBT::Colony_Combat_Screen(&results->info.attacker_invasion, &results->info.defender_invasion, results->info.ground_defense_strength, colony_idx);
                            ERIC::Force_Fade_To_Colony_Palette(0);
                            game_random::Set_Random_Seed(invade_seed);
                        }

                        NETMOX::Russ_Change_Relations(-10000, attacker_id, colony_owner, 10, star_idx, 0);

                        if (colony->owner == attacker_id) {
                            COMBFIND::current_star_has_player[attacker_id].colony_count++;
                            COMBFIND::current_star_has_player[colony_owner].colony_count--;
                        }

                        if (MOX::player[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN || MOX::player[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            COMBFIND::messed_up_fleet_palette = 1;
                        }
                    }
                }
            }
        } else {
            if (attacker_id > 9 && attacker_id < 15) {
                NETMOX::Russ_Event_Monster_Update(attacker_id, colony->planet, -1);
            }

            memset(&results->bomb, 0, sizeof(s_combat_bomb_result));
            results->bomb.attacker_idx = attacker_id;
            results->bomb.colony_idx = colony_idx;
            if (attacker_id == 10) { // Amoeba
                results->bomb.bomb_hits_remaining = 20;
                results->bomb.bio_hits_remaining = 0;
            }

            COMBAT::Bombardment(&results->bomb);
            BOMB::Apply_Bomb_Hits(&results->bomb);

            const bool amoeba_attacked_colony =
                attacker_id == NONPLAYER_SHIP_TYPE_AMOEBA;

            if (amoeba_attacked_colony) {
                MOX::planet[planet_idx].climate = PLANET_CLIMATE_TOXIC;
                MOX::planet[planet_idx].food_per_farmer = 0;
                if (results->bomb.colony_wiped_out == 0) {
                    COLCALC::Colony_Environmental_Stuff(colony);
                    invasion::Enforce_Population_Limits_At_Colony(colony);
                    COLCALC::Colony_Calculation(colony_idx);
                }
            }

            if (MOX::player[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
                (MOX::settings.strategic_combat_flag == 0 || (MOX::game_type != 2 && MOX::game_type != 3) ||
                    MOX::player[colony_owner].network_player_id == NETMOX::net.local_net_id) &&
                results->bomb.damage_occurred != 0) {
                if (attacker_id == NONPLAYER_SHIP_TYPE_AMOEBA) {
                    ERIC::Play_Amoeba_Flic();
                    COMBFIND::messed_up_fleet_palette = 1;
                }
                COLBOMB::Colony_Bombing_Screen(&results->bomb);
            }

            if (amoeba_attacked_colony &&
                Wandering_Monster_Retires(attacker_id, true)) {
                for (int i = 0; i < n_ships; ++i) {
                    SHIPS::Kill_Ship(ship_list[i]);
                }
                NETMOX::Russ_Event_Monster_Update(NONPLAYER_SHIP_TYPE_AMOEBA, -1, -1);
            }

            if (results->bomb.colony_wiped_out != 0) {
                BOMB::Destroy_Colony(colony, 1);
            }
        }

        if (attacker_id == 8) { // Antarans
            if (n_ships > 0) {
                int count = n_ships;
                int16_t* list = ship_list;
                while (count > 0) {
                    antarans::Restock_Antaran_Ship(MOX::ship[*list].d.size);
                    SHIPS::Kill_Ship(*list);
                    list++;
                    count--;
                }
            }
            uint8_t* star_has_nonplayer = COMBFIND::current_star_has_nonplayer;
            if (COMBFIND::star_has_nonplayer != nullptr) {
                star_has_nonplayer = &(*COMBFIND::star_has_nonplayer)[star_idx];
            }
            // Multiplayer clients only track the active combat star through current_star_has_nonplayer.
            if (star_has_nonplayer != nullptr) {
                *star_has_nonplayer &= 0xFE;
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    void __cdecl Strategic_Results_Popup(s_battle_results* battle_results) {
        ERICNET::Mox_Play_Sound(0x33);
        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        COMBAT::Message_Box_Titled(battle_results->info.summary, battle_results->info.details);

        if (battle_results->info.rescued_hero_id != 0) {
            ERIC::Play_Take_Orion_Flic();
            OFFICER::Rescued_Loknar_Popup(
                MOX::PLAYER_NUM,
                battle_results->info.orion_techs,
                battle_results->info.rescued_hero_id);
        }

        MULTPLAY::Load_Net_Next_Turn_Screen(0);
    }

    void __cdecl Bomb_Results_Popups(s_battle_results* res) {
        if (res->outpost_destroyed_flag == 0) {
            if (res->info.planet_destroyed_flag == 0) {
                if (res->bomb.damage_occurred != 0) {
                    if (res->bomb.attacker_idx == 10) {
                        ERIC::Play_Amoeba_Flic();
                    }
                    COLBOMB::Colony_Bombing_Screen(&res->bomb);
                }
            } else {
                ERIC::Play_Planetary_Destruction_Flic();
                messed_up_fleet_palette = 1;
            }
        } else {
            TEXTBOX::Timed_Text_Box(
                nullptr,
                outpost_destroyed_msg,
                confirm_battle_message_timeout);
        }

        if (res->info.attacker_invasion.unit_counts[1] != 0) {
            uint32_t seed = game_random::Get_Random_Seed();

            COLGCBT::Colony_Combat_Screen(
                &res->info.attacker_invasion,
                &res->info.defender_invasion,
                res->info.ground_defense_strength,
                MOX::planet[res->player_info.combat_planet_id].colony_index);

            MULTPLAY::Load_Net_Next_Turn_Screen(1);
            game_random::Set_Random_Seed(seed);
        }

        if (res->info.is_mind_controlled != 0) {
            TEXTBOX::Timed_Text_Box(
                nullptr,
                colony_seized_by_mind_control_msg,
                confirm_battle_message_timeout);
        }
    }

    int32_t __cdecl Do_Bombardment(int8_t attacker_id, int16_t colony_idx) {
        s_combat_bomb_result result;

        memset(&result, 0, sizeof(s_combat_bomb_result));

        result.attacker_idx = attacker_id;
        result.colony_idx = colony_idx;

        COMBAT::Bombardment(&result);

        return result.bomb_hits_remaining + result.bio_hits_remaining;
    }

    uint8_t __cdecl Has_Stellar_Converter(int16_t* ship_list, int16_t ship_count) {
        for (int32_t i = 0; i < ship_count; ++i) {
            int16_t ship_idx = ship_list[i];
            if (MOX::ship[ship_idx].status == 0) {
                if (INITSHIP::Ship_Has_Weapon(ship_idx, WEAPON_STELLAR_CONVERTER) != 0) {
                    return 1;
                }
            }
        }
        return 0;
    }

    int32_t __cdecl N_Enemy_Stars_In_Range(int16_t player_idx, uint8_t enemy_player_idx) {
        int32_t count = 0;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];

            if ((star->has_colony >> enemy_player_idx) & 1) {
                if (SHIPMOVE::Star_In_Normal_Range_Of_Player(star, player_idx)) {
                    count++;
                }
            }
        }

        return count;
    }

    int32_t __cdecl N_Enemy_Stars(int8_t player_idx) {
        int32_t count = 0;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if ((MOX::star[i].has_colony >> player_idx) & 1) {
                count++;
            }
        }

        return count;
    }

    int8_t __cdecl Player_Owns_Transports(int player_idx) {
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            if (MOX::ship[i].owner == player_idx &&
                MOX::ship[i].d.ship_type == SHIP_TYPE_TRANSPORT &&
                MOX::ship[i].status < 5) {
                return 1;
            }
        }
        return 0;
    }

    void __cdecl Do_Net_Combat_Update(bool unused) {
        if (combat_in_progress) {
            return;
        }

        if (battle_stars == nullptr ||
            player_needs_to_attack == nullptr ||
            star_has_player == nullptr ||
            star_has_nonplayer == nullptr) {
            if (n_battle_stars != 0) {
                NETMOX::Net_Debug_Log(
                    "Do_Net_Combat_Update: deferred outside battle search pending_stars=%d sync=%u",
                    (int)n_battle_stars,
                    (unsigned int)NETMOX::net.sync_state);
            }
            return;
        }

        (void)unused;
        RUSS::Mox_Update();

        if (in_net_update != 0) {
            RUSS::Mox_Update();
            return;
        }

        in_net_update = 1;

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                RUSS::Mox_Update();
                uint8_t state = NETMOX::net.player_net_info[i].player_state;

                switch (state) {
                    case NET_PLAYER_STATE_COMBAT_TARGET_CHOSEN: {
                        int16_t target_player_id = NETMOX::net.player_net_info[i].combat_target_player_id;
                        if (target_player_id == -1) {
                            NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
                            for (int16_t j = 0; j < MOX::NUM_PLAYERS; ++j) {
                                if ((uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id == i) {
                                    RUSS::Mox_Update();
                                    NETMOX::net.player_net_info[j].combat_attacker_id = (undefined)0xff;
                                }
                            }
                            NETMOX::net.sync_phase_active_flag = 0;
                            MOX::star[NETMOX::net.player_net_info[i].combat_star_id].ignore_combat_ships |= (1 << i);
                        } else {
                            MOX::star[NETMOX::net.player_net_info[i].combat_star_id].ignore_combat_ships = 0;

                            NETMOX::net.player_net_info[target_player_id].combat_defender_id = NETMOX::net.player_net_info[i].combat_defender_id;
                            NETMOX::net.player_net_info[target_player_id].combat_planet_id = NETMOX::net.player_net_info[i].combat_planet_id;
                            NETMOX::net.player_net_info[target_player_id].combat_target_player_id = NETMOX::net.player_net_info[i].combat_target_player_id;

                            for (int16_t j = 0; j < MOX::NUM_PLAYERS; ++j) {
                                if (j != i && j != target_player_id && (uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id == i) {
                                    RUSS::Mox_Update();
                                    NETMOX::net.player_net_info[j].combat_attacker_id = (undefined)0xff;
                                }
                            }

                            GNN_Reports_Combat(i, target_player_id);

                            uint8_t msg_type = 0;
                            if (MOX::settings.strategic_combat_flag == 0) {
                                if (target_player_id < MAX_PLAYERS && MOX::player[target_player_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                    NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_TACTICAL_ATTACKER;
                                    NETMOX::net.player_net_info[target_player_id].player_state = NET_PLAYER_STATE_COMBAT_TACTICAL_DEFENDER;
                                    int16_t colony_id = Target_Planet_To_Colony(NETMOX::net.player_net_info[i].combat_planet_id);
                                    NETMOX::Send_You_Are_Target_Info(
                                        target_player_id, i, NETMOX::net.player_net_info[i].combat_defender_id, colony_id, NETMOX::net.player_net_info[i].combat_star_id);
                                    msg_type = NET_PACKET_COMBAT_RESOLUTION_BEGIN;
                                } else {
                                    NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_STRATEGIC_DEFENDER;
                                    msg_type = NET_PACKET_COMBAT_RESOLUTION_BEGIN;
                                }
                            } else {
                                NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_STRATEGIC_ATTACKER;
                                NETMOX::net.player_net_info[target_player_id].player_state = NET_PLAYER_STATE_COMBAT_RESULTS_POPUP;
                                msg_type = NET_PACKET_COMBAT_RESOLUTION_BEGIN;
                            }
                            NETMOX::Mox_Send_Message((int16_t)MOX::player[i].network_player_id, msg_type, nullptr, 0);
                        }
                        break;
                    }
                    case NET_PLAYER_STATE_HUMAN_DIPLOMACY_ACTIVE: {
                        int16_t sync_phase_ready_flag = (int8_t)NETMOX::net.player_net_info[i].combat_attacker_id;
                        if (sync_phase_ready_flag == i) {
                            Update_Player_Needs_To_Attack(
                                NETMOX::net.player_net_info[i].combat_star_id,
                                i,
                                NETMOX::net.player_net_info[i].combat_target_player_id);
                        } else {
                            Update_Player_Needs_To_Attack(
                                NETMOX::net.player_net_info[i].combat_star_id,
                                sync_phase_ready_flag,
                                i);
                        }
                        NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
                        NETMOX::net.player_net_info[i].combat_attacker_id = (undefined)0xff;
                        NETMOX::net.sync_phase_active_flag = 0;
                        break;
                    }
                    case NET_PLAYER_STATE_HUMAN_DIPLOMACY_IGNORED: {
                        Do_Nonhuman_Attack_Against_Human(i);
                        (*star_has_nonplayer)[NETMOX::net.player_net_info[i].combat_star_id] &= 0xef;
                        NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_RESULTS_POPUP;
                        NETMOX::net.sync_phase_active_flag = 0;
                        NETMOX::net.player_net_info[12].combat_attacker_id = -1;
                        break;
                    }
                    case NET_PLAYER_STATE_DRAGON_BRIBE_DESTINATION: {
                        Do_Dragon_Bribe(
                            i,
                            NETMOX::net.player_net_info[i].combat_star_id,
                            NETMOX::net.dragon_bribe_destination_star_id);
                        NETMOX::net.player_net_info[i].combat_attacker_id = (undefined)0xff;
                        NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
                        NETMOX::net.sync_phase_active_flag = 0;
                        NETMOX::net.player_net_info[12].combat_attacker_id = -1;
                        break;
                    }
                }
            }
        }

        if (n_battle_stars == 0) {
            bool waiting_for_combat_ready_player = false;
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN && NETMOX::net.player_net_info[i].player_state != NET_PLAYER_STATE_COMBAT_READY) {
                    RUSS::Mox_Update();
                    waiting_for_combat_ready_player = true;
                }
            }
            if (!waiting_for_combat_ready_player) {
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_COMPLETE;
            }
            in_net_update = 0;
            RUSS::Mox_Update();
            return;
        }

        uint8_t state = NETMOX::net.player_net_info[MOX::PLAYER_NUM].player_state;
        if (state == NET_PLAYER_STATE_PICK_TARGET_INFO || NETMOX::net.sync_phase_active_flag != 0) {
            in_net_update = 0;
            RUSS::Mox_Update();
            return;
        }

        int16_t star_id;
        int16_t attacker_id;
        Pick_Star_And_Attacker(&star_id, &attacker_id);

        if (star_id == -1) {
            NETMOX::net.sync_phase_active_flag = 1;
            in_net_update = 0;
            RUSS::Mox_Update();
            return;
        }

        int8_t is_sneak_attack = 0;
        int16_t target_defender_id = -1;
        int16_t target_colony_idx = -1;

        for (int16_t j = 0; j < target.num_player_targets; ++j) {
            int16_t target_player = target.player_targets[j];
            NETMOX::net.player_net_info[target_player].combat_star_id = star_id;
            NETMOX::net.player_net_info[target_player].combat_attacker_id = (undefined)attacker_id;
        }

        for (int16_t j = 0; j < target.num_colony_targets; ++j) {
            int16_t target_colony = target.colony_targets[j];
            int8_t owner = MOX::colony[target_colony].owner;
            NETMOX::net.player_net_info[owner].combat_star_id = star_id;
            NETMOX::net.player_net_info[owner].combat_attacker_id = (undefined)attacker_id;
        }

        NETMOX::net.player_net_info[attacker_id].combat_star_id = star_id;
        NETMOX::net.player_net_info[attacker_id].combat_attacker_id = (undefined)attacker_id;

        if (attacker_id < MAX_PLAYERS) {
            if (MOX::player[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if ((MOX::star[star_id].ignore_combat_ships & (1 << attacker_id)) == 0) {
                    NETMOX::net.player_net_info[attacker_id].player_state = NET_PLAYER_STATE_PICK_TARGET_INFO;
                    NETMOX::Send_Pick_Target_Info(attacker_id, star_id, &target);
                    in_net_update = 0;
                    RUSS::Mox_Update();
                    return;
                }
            }
        }

        int16_t target_sync_owner_id;
        if (attacker_id < MAX_PLAYERS && MOX::player[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
            target_sync_owner_id = -1;
        } else {
            if (attacker_id < MAX_PLAYERS) {
                Get_AI_Target(star_id, attacker_id, &target_defender_id, &target_colony_idx, &is_sneak_attack);
            } else {
                Get_NPC_Target(star_id, attacker_id, &target_defender_id, &target_colony_idx);
            }
            if (target_colony_idx != -1) {
                target_sync_owner_id = MOX::colony[target_colony_idx].owner;
                MOX::player[target_sync_owner_id].last_attacker = (int8_t)attacker_id;
            } else {
                target_sync_owner_id = target_defender_id;
            }
        }

        NETMOX::net.player_net_info[attacker_id].combat_defender_id = target_defender_id;
        int16_t target_planet_id = Target_Colony_To_Planet(target_colony_idx);
        NETMOX::net.player_net_info[attacker_id].combat_planet_id = target_planet_id;
        NETMOX::net.player_net_info[attacker_id].combat_target_player_id = target_sync_owner_id;

        if (target_sync_owner_id == -1) {
            NETMOX::net.sync_phase_active_flag = 0;
            for (int16_t j = 0; j < MOX::NUM_PLAYERS; ++j) {
                if ((uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id == attacker_id &&
                    NETMOX::net.player_net_info[j].combat_star_id == star_id) {
                    RUSS::Mox_Update();
                    NETMOX::net.player_net_info[j].combat_attacker_id = (undefined)0xff;
                }
            }
        } else {
            MOX::star[star_id].ignore_combat_ships = 0;
            NETMOX::net.player_net_info[target_sync_owner_id].combat_defender_id = NETMOX::net.player_net_info[attacker_id].combat_defender_id;
            NETMOX::net.player_net_info[target_sync_owner_id].combat_planet_id = NETMOX::net.player_net_info[attacker_id].combat_planet_id;
            NETMOX::net.player_net_info[target_sync_owner_id].combat_target_player_id = NETMOX::net.player_net_info[attacker_id].combat_target_player_id;

            for (int16_t j = 0; j < MOX::NUM_PLAYERS; ++j) {
                if (j != attacker_id &&
                    j != target_sync_owner_id &&
                    (uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id == attacker_id &&
                    NETMOX::net.player_net_info[j].combat_star_id == star_id) {
                    RUSS::Mox_Update();
                    NETMOX::net.player_net_info[j].combat_attacker_id = (undefined)0xff;
                }
            }

            if (target_sync_owner_id < MAX_PLAYERS && MOX::player[target_sync_owner_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (attacker_id == 12 && target_colony_idx != -1) {
                    NETMOX::net.player_net_info[target_sync_owner_id].player_state = NET_PLAYER_STATE_HUMAN_DIPLOMACY_READY_DATA;
                    NETMOX::Send_Dragon_Info(target_colony_idx);
                } else {
                    Do_Nonhuman_Attack_Against_Human(target_sync_owner_id);
                }
            } else {
                int16_t target_planet_id = Target_Colony_To_Planet(target_colony_idx);
                Do_1_Combat(star_id, attacker_id, target_defender_id, target_planet_id, nullptr, 0);
                Update_Player_Needs_To_Attack(star_id, attacker_id, target_sync_owner_id);
                NETMOX::net.sync_phase_active_flag = 0;
                NETMOX::net.player_net_info[attacker_id].combat_attacker_id = (undefined)0xff;
                NETMOX::net.player_net_info[target_sync_owner_id].combat_attacker_id = (undefined)0xff;
            }
        }

        in_net_update = 0;
        RUSS::Mox_Update();
    }

    void __cdecl Do_Nonhuman_Attack_Against_Human(int player_idx) {
        s_battle_results battle_results;

        int8_t attacker_id = (int8_t)NETMOX::net.player_net_info[player_idx].combat_attacker_id;
        int16_t combat_star_id = NETMOX::net.player_net_info[player_idx].combat_star_id;

        GNN_Reports_Combat(attacker_id, player_idx);

        if (MOX::settings.strategic_combat_flag == 0) {
            int16_t colony_id = Target_Planet_To_Colony(NETMOX::net.player_net_info[player_idx].combat_planet_id);

            NETMOX::Send_You_Are_Target_Info(
                (int16_t)player_idx,
                (int16_t)attacker_id,
                NETMOX::net.player_net_info[player_idx].combat_defender_id,
                colony_id,
                combat_star_id);

            NETMOX::net.player_net_info[player_idx].player_state = NET_PLAYER_STATE_BATTLE_RESULTS_ACK;
            return;
        }

        memset(&battle_results, 0, sizeof(s_battle_results));

        // The leading s_net_player_info in s_battle_results maps exactly to the synchronized player record.
        memcpy(&battle_results, &NETMOX::net.player_net_info[player_idx], sizeof(NETMOX::net.player_net_info[player_idx]));

        battle_results.planet_id = battle_results.player_info.combat_planet_id;

        battle_results.player_info.combat_result = (int8_t)Do_1_Combat(
            combat_star_id,
            (int16_t)attacker_id,
            NETMOX::net.player_net_info[player_idx].combat_defender_id,
            NETMOX::net.player_net_info[player_idx].combat_planet_id,
            &battle_results,
            0);

        Update_Player_Needs_To_Attack(combat_star_id, attacker_id, player_idx);

        NETMOX::net.sync_phase_active_flag = 0;
        NETMOX::net.player_net_info[attacker_id].combat_attacker_id = (undefined)0xFF; // -1
        NETMOX::net.player_net_info[player_idx].player_state = NET_PLAYER_STATE_COMBAT_RESULTS_POPUP;

        NETMOX::Mox_Send_Message(
            MOX::player[player_idx].network_player_id,
            NET_PACKET_STRATEGIC_BATTLE_RESULTS,
            &battle_results,
            (uint16_t)sizeof(s_battle_results));
    }

    void __cdecl Send_Star_Player_Presence(NETPROTO::Packet_Writer_* writer, int16_t star_idx) {
        s_combfind_star_has_player* src;
        if (star_idx == -1) {
            src = current_star_has_player;
        } else {
            src = (*star_has_player)[star_idx];
        }

        writer->Write_Bytes(src, MAX_PLAYERS * sizeof(*src));

        if (star_idx != -1) {
            writer->Write_Uint8((*star_has_nonplayer)[star_idx]);
        } else {
            writer->Write_Uint8(*current_star_has_nonplayer);
        }
    }

    void __cdecl GNN_Reports_Combat(int player1_idx, int player2_idx) {
        char buf[200];
        int p1;
        int p2;

        if (game_random::Random(2) == 1) {
            p1 = player1_idx;
            p2 = player2_idx;
        } else {
            p1 = player2_idx;
            p2 = player1_idx;
        }

        char* name2 = COLCALC::Player_Name(p2);
        char* name1 = COLCALC::Player_Name(p1);

        snprintf(buf, sizeof(buf), combat_is_reported_between_s_and_s_msg, name1, name2);

        CHAT::Send_GNN_Chat_Msg(buf);
    }

} // namespace COMBFIND