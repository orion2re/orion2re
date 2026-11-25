#include "pch.h"
#include "ship_ids.h"
#include "battle_results_serialization.h"
#include "combat_fleets.h"
#include "fleet_movement.h"

using game::current_state;
#include "legacy_ids.h"

namespace COMBFIND {

    namespace {
        bool combat_in_progress;

        s_net_player_info Local_Combat_Info() {
            const auto& net = NETMOX::net;
            return {static_cast<int8_t>(net.sync_state), net.combat_destination,
                net.combat_defender_id, net.combat_planet_id, net.combat_target_player_id, net.combat_type,
                net.combat_attacker_id, net.combat_result, net.diplomacy_target_id};
        }

        void Send_Strategic_Results(int16_t peer, uint8_t kind, const s_battle_results& results) {
            uint8_t bytes[SERIAL::BATTLE_RESULTS_BYTES];
            SERIAL::Writer_ writer(bytes, sizeof(bytes), "battle results overflow");
            SERIAL::Write_Battle_Results(writer, results);
            NETMOX::Mox_Send_Message(peer, kind, bytes, writer.Length());
        }

        bool __cdecl Planet_Has_Ancient_Artifacts(s_planet_data* planet) {
            return planet->planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
        }
    } // namespace

    uint8_t messed_up_fleet_palette;
    uint8_t in_net_update;
    s_combfind_target target;
    game::owner_bitmap (*player_needs_to_attack)[MAX_STARS + 1];
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
        game::owner_bitmap player_needs_to_attack_stack[MAX_STARS + 1]{};
        int16_t battle_stars_stack[MAX_STARS + 2];
        int8_t attack_confirmed_buf[MAX_PLAYERS][MAX_PLAYERS];

        star_has_player = &star_has_player_stack;
        memset(&star_has_player_stack, 0, sizeof(star_has_player_stack));

        star_has_nonplayer = &star_has_nonplayer_stack;
        memset(&star_has_nonplayer_stack, 0, sizeof(star_has_nonplayer_stack));

        player_needs_to_attack = &player_needs_to_attack_stack;

        battle_stars = battle_stars_stack;

        NETMOX::Net_Debug_Log(
            "Search_For_Battles: scratch ready player=%d stardate=%u",
            (int)MOX::Legacy_Local_Player_Slot(),
            (unsigned int)MOX::stardate);

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            int8_t owner = legacy_world::Encode_Player_Slot(roster_colony.owner);
            if (owner != -1) {
                const auto planet_idx = roster_colony.planet_id;
                int16_t star_idx = game::Star_Row(current_state, current_state.planets.at(planet_idx).star_id);
                (*star_has_player)[star_idx][owner].colony_count++;
            }
        }

        (*star_has_nonplayer)[game::ANTARES_POSITION] |= 1;

        for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
            if (game::current_state.ship_slots[i] == ::game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(game::current_state, i);
            const int16_t loc = game::Ship_Orbit_Row(current_state, ship->position);
            if (Is_Star_Or_Antares_Location(loc, std::ssize(current_state.stars))) {
                if (ship->status == 0) {
                    int8_t owner = factions::Legacy_Code(ship->owner);
                    if (owner < MAX_PLAYERS) {
                        uint8_t ship_type = ship->d.ship_type;
                        (*star_has_player)[loc][owner].has_player = 1;
                        (*star_has_player)[loc][owner].ship_sizes[ship_type] = 1;
                        if (ship_type == SHIP_TYPE_TRANSPORT) {
                            (*star_has_player)[loc][owner].has_transport = 1;
                        } else if (ship_type == SHIP_TYPE_COMBAT) {
                            (*player_needs_to_attack)[loc].set(owner);
                        }
                    } else {
                        (*star_has_nonplayer)[loc] |= (1 << ((owner - MAX_PLAYERS) & 0x1F));
                        (*player_needs_to_attack)[loc].set(owner);
                    }
                }
            } else if (ship->d.ship_type == SHIP_TYPE_TRANSPORT && ship->status == 1) {
                const int16_t star_idx = game::Ship_Target_Row(current_state, ship->position);
                if (ship->position.kind != game::e_ship_location::in_flight || !Is_Star_Or_Antares_Location(star_idx, std::ssize(current_state.stars))) continue;
                if (ship->turns_left == 1) {
                    (*star_has_player)[star_idx][factions::Legacy_Code(ship->owner)].transport_count++;
                } else {
                    (*star_has_player)[star_idx][factions::Legacy_Code(ship->owner)].transport_unk = 1;
                }
            }
        }

        n_battle_stars = 0;
        for (int16_t position = 0; position <= std::ssize(current_state.stars); ++position) {
            const auto i = position == std::ssize(current_state.stars) ? game::ANTARES_POSITION : position;
            if ((*player_needs_to_attack)[i].any()) {
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
                int16_t star_idx;
                auto attacker = game::NO_OWNER;
                Pick_Star_And_Attacker(&star_idx, &attacker);
                const auto attacker_idx = attacker.code;
                if (star_idx != -1) {
                    int16_t defender_idx = -1;
                    game::id_colony colony_idx = game::NO_COLONY;
                    int8_t unk_flag = 0;

                    if (attacker_idx < MAX_PLAYERS) {
                        if (game::current_state.players[attacker_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                            auto defender = game::NO_OWNER;
                            Get_AI_Target(star_idx, current_state.players[attacker_idx].id, &defender, &colony_idx, &unk_flag);
                            defender_idx = defender.code;
                        } else {
                            auto& location = game::Destination_Record_At_Row(current_state, star_idx);
                            if ((location.ignore_combat_ships.test_slot(attacker_idx & 0x1F)) == 0) {
                                auto defender = game::NO_OWNER;
                                Get_Local_Human_Target(star_idx, current_state.players[attacker_idx].id, &defender, &colony_idx);
                                defender_idx = defender.code;
                                if (colony_idx == game::NO_COLONY) {
                                    if (defender_idx == -1) {
                                        location.ignore_combat_ships.set_slot(attacker_idx & 0x1F);
                                    }
                                }
                            }
                        }
                    } else {
                        auto target_owner = game::NO_OWNER;
                        Get_NPC_Target(star_idx, attacker, &target_owner, &colony_idx);
                        defender_idx = target_owner.code;
                    }

                    int16_t final_defender_idx = defender_idx;
                    if (colony_idx != game::NO_COLONY) {
                        final_defender_idx = legacy_world::Encode_Player_Slot(game::current_state.colonies.at(colony_idx).owner);
                        game::current_state.players.at(final_defender_idx).last_attacker = attacker;
                    }

                    if (attacker_idx == 12 /* Antarans */ && colony_idx != game::NO_COLONY) {
                        int16_t bribe_planet_idx = -1;
                        if (game::current_state.players[final_defender_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            bribe_planet_idx = MAINPUPS::Dragon_New_Star_Popup(game::current_state.players[final_defender_idx].id, star_idx);
                        }
                        if (bribe_planet_idx != -1) {
                            Do_Dragon_Bribe(current_state.players[final_defender_idx].id, star_idx, bribe_planet_idx);
                            final_defender_idx = -1;
                        }
                    }

                    if (final_defender_idx != -1) {
                        const auto planet_idx = COMBFIND::Target_Colony_To_Planet(colony_idx);
                        game::Destination_Record_At_Row(current_state, star_idx).ignore_combat_ships.reset();
                        Do_1_Combat(star_idx, attacker, factions::From_Legacy(defender_idx), planet_idx, nullptr, unk_flag);
                        current_star_has_player = 0;
                        current_star_has_nonplayer = 0;
                        Update_Player_Needs_To_Attack(star_idx, attacker, factions::From_Legacy(final_defender_idx));
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

        for (const auto star_id : current_state.star_order) {
            Do_System_Discoveries_At_Star(&game::current_state.stars.at(star_id));
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
        std::vector<game::id_ship> retreating;
        auto& world = current_state;
        s_ship_move_info move_info;

        memset(report, 0, sizeof(report));
        memset(player_retreat_target, 0xFF, sizeof(player_retreat_target));

        for (int16_t i = static_cast<int16_t>(game::current_state.ship_slots.size()) - 1; i >= 0; --i) {
            if (game::current_state.ship_slots[i] == game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(game::current_state, i);

            if (game::Ship_Orbit_Row(current_state, ship->position) == game::ANTARES_POSITION) {
                int8_t owner = factions::Legacy_Code(ship->owner);
                if (player_retreat_target[owner] == -1) {
                    player_retreat_target[owner] = 0x48; /* 'H' */
                    uint32_t valid_star_count = 0;
                    for (int16_t j = 0; j < std::ssize(current_state.star_order); ++j) {
                        const auto target_id = current_state.star_order[j];
                        if (game::current_state.stars.at(target_id).has_dimensional_portal.test_slot(owner)) {
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
                        auto& leader = current_state.leaders.at(leader_idx);
                        if (leader.eta < 1) {
                            leader.Clear_Location();
                            ship->officer_index = -1;
                            OFFICER::Kill_Officer(leader_idx);
                            report[owner * retreat_location_count + game::ANTARES_POSITION].killed_officers++;
                        } else {
                            OFFICER::Move_Officer_To_Pool(leader_idx);
                        }
                    }
                    report[owner * retreat_location_count + game::ANTARES_POSITION].count++;
                    report[owner * retreat_location_count + game::ANTARES_POSITION].reason = 0;
                    SHIPS::Kill_Ship(*ship);
                } else {
                    int16_t target_loc = player_retreat_target[owner];
                    game::Set_Ship_Orbit_At_Row(current_state, ship->position, target_loc, game::ANTARES_POSITION);
                    ship->x = game::current_state.stars.at(game::Star_Key_At_Row(current_state, target_loc)).x;
                    ship->y = game::current_state.stars.at(game::Star_Key_At_Row(current_state, target_loc)).y;
                    ship->status = 0;
                }
            } else if (ship->status == 9) {
                int16_t loc = game::Ship_Orbit_Row(current_state, ship->position);
                int8_t owner = factions::Legacy_Code(ship->owner);
                retreating.clear();
                retreating.push_back(game::Ship_Key_At_Row(world, i));
                ship->mission = 0;

                for (int16_t j = i - 1; j >= 0; --j) {
                    if (game::current_state.ship_slots[j] == game::NO_SHIP) continue;
                    s_ship_data* other = &ship_slots::At(game::current_state, j);
                    if (other->status == 9 && other->position == ship->position && factions::Legacy_Code(other->owner) == owner) {
                        retreating.push_back(game::Ship_Key_At_Row(world, j));
                        other->mission = 0;
                    }
                }

                int16_t retreat_star = Closest_Other_Colony_Star(world, factions::Player(ship->owner), loc);
                const auto destination = game::Destination_At_Row(current_state, retreat_star);
                bool retreat_failed = false;
                int8_t fail_reason = -1;

                if (retreat_star == -1) {
                    retreat_failed = true;
                    fail_reason = 2;
                } else {
                    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, retreating, destination, &move_info);
                    if (move_info.moving == 0) {
                        retreat_failed = true;
                        fail_reason = (move_info.hyperspace_flux == 0) ? 2 : 1;
                    }
                }

                if (retreat_failed) {
                    for (const auto key : retreating) {
                        const auto record_entry = world.ships.find(key);
                        auto* record = record_entry == world.ships.end() ? nullptr : &record_entry->second;
                        if (!record) continue;
                        auto& casualty = *record;
                        int16_t leader_idx = casualty.officer_index;
                        if (leader_idx != -1) {
                            if (current_state.leaders.at(leader_idx).eta < 1) {
                                OFFICER::Kill_Officer(leader_idx);
                                report[owner * retreat_location_count + game::Ship_Orbit_Row(current_state, ship_slots::At(game::current_state, i).position)].killed_officers++;
                            } else {
                                OFFICER::Move_Officer_To_Pool(leader_idx);
                            }
                        }
                        // ORACLE: The original re-reads the outer ship's location after it may have
                        // been killed. A location of -1 aliases the preceding player's final slot.
                        // Fix this attribution bug later even though doing so diverges from the oracle.
                        s_retreat_report& retreat =
                            report[owner * retreat_location_count + game::Ship_Orbit_Row(current_state, ship_slots::At(game::current_state, i).position)];
                        retreat.count++;
                        retreat.reason = fail_reason;
                        SHIPS::Kill_Ship(casualty);
                    }
                } else if (SHIPMOVE::Order_Fleet_Move(game::current_state, retreating, destination, &move_info)) {
                    for (const auto key : retreating) {
                        const auto moved_entry = world.ships.find(key);
                        auto* moved = moved_entry == world.ships.end() ? nullptr : &moved_entry->second;
                        if (!moved) continue;
                        if (moved->position.kind == game::e_ship_location::in_wormhole)
                            moved->position.kind = game::e_ship_location::in_flight;
                        moved->status = 1;
                    }
                }
            } else if (factions::Legacy_Code(ship->owner) >= MAX_PLAYERS) {
                if (factions::Legacy_Code(ship->owner) == 8 && ship->status == 0) {
                    antarans::Restock_Antaran_Ship((int16_t)ship->d.size);
                    SHIPS::Kill_Ship(*ship);
                } else if (factions::Legacy_Code(ship->owner) >= MAX_PLAYERS && ship->status == 0 && ship->d.speed != 0) {
                    if (Wandering_Monster_Retires(factions::Legacy_Code(ship->owner))) {
                        SHIPS::Kill_Ship(*ship);
                        NETMOX::Russ_Event_Monster_Update(ship->owner, game::NO_PLANET);
                    }
                }
            }
        }

        for (int p = 0; p < game::current_state.player_count; ++p) {
            for (int16_t position = 0; position <= std::ssize(current_state.stars); ++position) {
                const auto s = position == std::ssize(current_state.stars) ? game::ANTARES_POSITION : position;
                s_retreat_report& retreat = report[p * retreat_location_count + s];
                if (retreat.count != 0) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_FAILED_RETREAT;
                    msg.u.retreat.destination = game::Destination_At_Row(current_state, s);
                    msg.u.retreat.reason = retreat.reason;
                    msg.u.retreat.ships_lost = retreat.count;
                    msg.u.retreat.leader_count = retreat.killed_officers;
                    MSG_::Add_Msg(msg, game::current_state.players[p].id);
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
        game::id_colony target_colony;
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
                case NET_SYNC_STATE_PICKING_COMBAT_TARGET: {
                    fields::Assign_Auto_Function(SCORE::nullsub, 1);
                    video::Set_Page_Off();
                    MOX::dont_draw_main_screen_to_back = 1;
                    MAINSCR::Draw_Mini_Main_Screen();
                    video::Copy_Off_To_Back();
                    video::Set_Refresh_Stencil();
                    ERICNET::Mox_Play_Sound(51 /* 0x33 */);
                    auto defender = game::NO_OWNER;
                    COMBFIND::Get_Local_Human_Target(game::Destination_Row(current_state, NETMOX::net.combat_destination), MOX::local_player, &defender, &target_colony);
                    NETMOX::net.combat_defender_id = defender;
                    NETMOX::net.combat_planet_id = COMBFIND::Target_Colony_To_Planet(target_colony);
                    MULTPLAY::Load_Net_Next_Turn_Screen(0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TARGET_SENT;
                    if (target_colony == game::NO_COLONY) {
                        if (factions::Legacy_Code(NETMOX::net.combat_defender_id.get()) == -1) {
                            NETMOX::net.combat_target_player_id = factions::From_Legacy(-1);
                            NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                        } else {
                            NETMOX::net.combat_target_player_id = NETMOX::net.combat_defender_id;
                        }
                    } else {
                        NETMOX::net.combat_target_player_id = factions::From_Player(game::current_state.colonies.at(target_colony).owner);
                    }
                    uint8_t target[8];
                    SERIAL::Writer_ writer(target, sizeof(target), "combat target overflow");
                    writer.Write_Int16(factions::Legacy_Code(NETMOX::net.combat_defender_id.get()));
                    writer.Write_Uint32(NETMOX::net.combat_planet_id.get().id);
                    writer.Write_Int16(factions::Legacy_Code(NETMOX::net.combat_target_player_id.get()));
                    NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_COMBAT_TARGET_CHOSEN, target, writer.Length());
                    break;
                }

                case NET_SYNC_STATE_COMBAT_RESOLUTION_DISPATCH:
                    if (factions::Legacy_Code(NETMOX::net.combat_attacker_id.get()) < MAX_PLAYERS && game::current_state.players[(int)factions::Legacy_Code(NETMOX::net.combat_attacker_id.get())].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER;
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER_VS_NONHUMAN;
                    }
                    ERICNET::Mox_Play_Sound(51 /* 0x33 */);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER:
                    local_battle_results_1 = {};
                    local_battle_results_1.player_info = Local_Combat_Info();
                    local_battle_results_1.planet_id = NETMOX::net.combat_planet_id;
                    result = COMBFIND::Do_1_Combat(game::Destination_Row(current_state, NETMOX::net.combat_destination), factions::From_Player(MOX::local_player), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), &local_battle_results_1, 0);
                    local_battle_results_1.player_info.combat_result = (int8_t)result;
                    Send_Strategic_Results(game::current_state.players[factions::Legacy_Code(NETMOX::net.combat_target_player_id.get())].network_player_id, NET_PACKET_STRATEGIC_RESULTS_READY, local_battle_results_1);
                    while (NETMOX::net.sync_state != NET_SYNC_STATE_COMBAT_READY) {
                        RUSS::Mox_Update();
                    }
                    NETMOX::Send_Battle_Results(game::Destination_Row(current_state, NETMOX::net.combat_destination), factions::From_Player(MOX::local_player), NETMOX::net.combat_target_player_id.get(), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), (int16_t)local_battle_results_1.player_info.combat_result);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER:
                    result = COMBFIND::Do_1_Combat(game::Destination_Row(current_state, NETMOX::net.combat_destination), NETMOX::net.combat_attacker_id.get(), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), nullptr, 0);
                    if (NETMOX::net.sync_state != NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                        while (NETMOX::net.sync_state != NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED) {
                            RUSS::Mox_Update();
                        }
                    }
                    COMBFIND::Bomb_Results_Popups(NETMOX::net.combat_results_data);
                    NETMOX::Mox_Send_Message(game::current_state.players[(int)factions::Legacy_Code(NETMOX::net.combat_attacker_id.get())].network_player_id, NET_PACKET_STRATEGIC_RESULTS_ACK, nullptr, 0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Send_Battle_Results(game::Destination_Row(current_state, NETMOX::net.combat_destination), NETMOX::net.combat_attacker_id.get(), factions::From_Player(MOX::local_player), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), result);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER_VS_NONHUMAN:
                    result = COMBFIND::Do_1_Combat(game::Destination_Row(current_state, NETMOX::net.combat_destination), factions::From_Player(MOX::local_player), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), nullptr, 0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Send_Battle_Results(game::Destination_Row(current_state, NETMOX::net.combat_destination), factions::From_Player(MOX::local_player), NETMOX::net.combat_target_player_id.get(), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), result);
                    break;

                case NET_SYNC_STATE_COMBAT_TACTICAL_DEFENDER_VS_NONHUMAN:
                    result = COMBFIND::Do_1_Combat(game::Destination_Row(current_state, NETMOX::net.combat_destination), NETMOX::net.combat_attacker_id.get(), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), nullptr, 0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    MULTPLAY::Load_Net_Next_Turn_Screen(1);
                    NETMOX::Send_Battle_Results(game::Destination_Row(current_state, NETMOX::net.combat_destination), NETMOX::net.combat_attacker_id.get(), factions::From_Player(MOX::local_player), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), result);
                    break;

                case NET_SYNC_STATE_COMBAT_STRATEGIC_ATTACKER:
                    local_battle_results_2 = {};
                    local_battle_results_2.player_info = Local_Combat_Info();
                    local_battle_results_2.planet_id = NETMOX::net.combat_planet_id;
                    result = COMBFIND::Do_1_Combat(game::Destination_Row(current_state, NETMOX::net.combat_destination), factions::From_Player(MOX::local_player), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), &local_battle_results_2, 0);
                    local_battle_results_2.player_info.combat_result = (int8_t)result;
                    MULTPLAY::Load_Net_Next_Turn_Screen(1);
                    if (factions::Legacy_Code(NETMOX::net.combat_target_player_id.get()) < MAX_PLAYERS && game::current_state.players[factions::Legacy_Code(NETMOX::net.combat_target_player_id.get())].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        Send_Strategic_Results(game::current_state.players[factions::Legacy_Code(NETMOX::net.combat_target_player_id.get())].network_player_id, NET_PACKET_STRATEGIC_BATTLE_RESULTS, local_battle_results_2);
                    }
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Send_Battle_Results(game::Destination_Row(current_state, NETMOX::net.combat_destination), factions::From_Player(MOX::local_player), NETMOX::net.combat_target_player_id.get(), NETMOX::net.combat_defender_id.get(), NETMOX::net.combat_planet_id.get(), (int16_t)local_battle_results_2.player_info.combat_result);
                    break;

                case NET_SYNC_STATE_COMBAT_RESULTS_DISPLAY:
                    COMBFIND::Strategic_Results_Popup(NETMOX::net.combat_results_data);
                    COMBFIND::Bomb_Results_Popups(NETMOX::net.combat_results_data);
                    MULTPLAY::Load_Net_Next_Turn_Screen(0);
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                    NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_COMBAT_RESULTS_ACK, nullptr, 0);
                    break;

                case NET_SYNC_STATE_DRAGON_BRIBE_PROMPT:
                    result = MAINPUPS::Dragon_New_Star_Popup(MOX::local_player, game::Destination_Row(current_state, NETMOX::net.combat_destination));
                    new_star_target = result;
                    MULTPLAY::Load_Net_Next_Turn_Screen(0);
                    if (new_star_target == -1) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_DRAGON_BRIBE_DECLINED;
                        NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_DRAGON_NO_BRIBE, nullptr, 0);
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                        uint8_t payload[sizeof(uint32_t)];
                        SERIAL::Writer_ writer(payload, sizeof(payload), "invalid Dragon bribe destination");
                        writer.Write_Uint32(game::Star_Key_At_Row(current_state, new_star_target).id);
                        NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_DRAGON_BRIBE_DESTINATION, payload, writer.Length());
                    }
                    break;

                default:
                    break;
            }
        }

        NETMOX::net.combat_results_data = nullptr;
        COMBFIND::attack_confirmed = nullptr;
        game::received_combat_order.reset();
        RUSS::Mox_Update();
    }

    void __cdecl Do_Dragon_Bribe(game::id_player player, int16_t star_idx, int16_t dest_idx) {
        auto& record = current_state.players.at(player.id);
        auto& world = current_state;
        auto key = game::NO_SHIP;

        for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
            if (game::current_state.ship_slots[i] == ::game::NO_SHIP) continue;
            // Owner 12 is the Dragon/Guardian
            if (factions::Legacy_Code(ship_slots::At(game::current_state, i).owner) == 12 && game::Ship_Orbit_Row(current_state, ship_slots::At(game::current_state, i).position) == star_idx) {
                key = game::Ship_Key_At_Row(world, i);
            }
        }

        const game::id_ship ships[] = {key};
        const auto destination = game::Destination_At_Row(current_state, dest_idx);
        s_ship_move_info move_info{};
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, destination, &move_info);
        SHIPMOVE::Order_Fleet_Move(game::current_state, ships, destination, &move_info);

        record.bc -= static_cast<int32_t>(dragon_bribe_bc);

        (*star_has_nonplayer)[star_idx] &= 0xEF;
    }

    int16_t Closest_Other_Colony_Star(const game::s_game_state& state, game::id_player player, int16_t star_idx) {
        uint32_t min_dist_sq = 0xFFFFFFFF;
        int16_t closest_star_idx = -1;

        const s_star_data* src_star = &state.stars.at(game::Star_Key_At_Row(state, star_idx));
        int32_t src_x = src_star->x;
        int32_t src_y = src_star->y;

        for (auto& [roster_id, roster_colony] : state.colonies) {
            const s_colony* colony = &roster_colony;

            if (colony->owner == player) {
                int16_t target_star_idx = game::Star_Row(state, state.planets.at(colony->planet_id).star_id);

                if (target_star_idx != star_idx && target_star_idx != closest_star_idx) {
                    if (!struct_::Read_Bit_Field(src_star->black_hole_blocks, target_star_idx)) {
                        const s_star_data* target_star = &state.stars.at(game::Star_Key_At_Row(state, target_star_idx));

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
    void __cdecl Collect_Targets_For_Combat(int16_t star_idx, game::owner_slot attacker) {
        const auto player_idx = attacker.code;
        target.num_colony_targets = 0;

        if (star_idx < std::ssize(current_state.stars)) {
            for (int i = 0; i < 5; ++i) {
                const auto planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).orbit_planet_ids[i];
                if (planet_idx != game::NO_PLANET) {
                    game::id_colony colony_idx = current_state.planets.at(planet_idx).colony_id;
                    if (colony_idx != game::NO_COLONY) {
                        if (Player_May_Attack_Player(attacker, current_state.colonies.at(colony_idx).owner)) {
                            target.colony_targets[target.num_colony_targets] = colony_idx;
                            target.num_colony_targets++;
                        }
                    }
                }
            }
        }

        target.num_player_targets = 0;
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (player_idx != i) {
                s_combfind_star_has_player* status = &(*star_has_player)[star_idx][i];
                if (status->has_player && status->colony_count == 0) {
                    if (Player_May_Attack_Player(attacker, current_state.players[i].id)) {
                        target.faction_targets[target.num_player_targets] = factions::From_Player(current_state.players.at(i).id);
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
                        target.faction_targets[target.num_player_targets] = game::owner_slot{i + MAX_PLAYERS};
                        target.num_player_targets++;
                    }
                }
            }
        }
    }

    game::owner_slot __cdecl Pick_Attacker_At_Star(int16_t star_idx) {
        const auto needs_attack = (*player_needs_to_attack)[star_idx];
        auto attacker = game::NO_OWNER;
        int32_t count = 0;

        if (needs_attack.none()) {
            return game::NO_OWNER;
        }

        // Check for special entities (bits 8-14)
        if ((needs_attack >> MAX_PLAYERS).any()) {
            for (int32_t i = MAX_PLAYERS; i < 15; ++i) {
                if (needs_attack.test(i)) {
                    count++;
                    // Reservoir sampling for equal probability distribution
                    if (game_random::Random(count) == 1) {
                        attacker = game::owner_slot{i};
                    }
                }
            }
            return attacker;
        } else {
            // Check for players (bits 0-7)
            for (int32_t i = 0; i < (int32_t)game::current_state.player_count; ++i) {
                if (needs_attack.test(i)) {
                    count++;
                    if (game_random::Random(count) == 1) {
                        attacker = factions::From_Player(current_state.players[i].id);
                    }
                }
            }
            return attacker;
        }
    }

    void __cdecl Pick_Star_And_Attacker(int16_t* star_idx_ptr, game::owner_slot* attacker) {
        int16_t star_list_idx = n_battle_stars - 1;
        *star_idx_ptr = -1;
        *attacker = game::NO_OWNER;

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
                    *attacker = COMBFIND::Pick_Attacker_At_Star(*star_idx_ptr);

                    if (*attacker == game::NO_OWNER) {
                        // No valid attacker found at this star, remove the star from the list by swapping with last
                        int16_t current_list_idx = star_list_idx;
                        *star_idx_ptr = -1;
                        n_battle_stars--;
                        star_list_idx--;
                        battle_stars[current_list_idx] = battle_stars[n_battle_stars];
                    } else {
                        // Mark this player as processed for this star in the current pick
                        (*player_needs_to_attack)[*star_idx_ptr].reset(attacker->code);

                        COMBFIND::Collect_Targets_For_Combat(*star_idx_ptr, *attacker);

                        // If no valid targets (player or colony) were found, try to find another attacker at the same star
                        if (target.num_player_targets == 0 && target.num_colony_targets == 0) {
                            *attacker = game::NO_OWNER;
                        }
                    }
                } while (*attacker == game::NO_OWNER && *star_idx_ptr != -1);
            }

            // If a valid star and attacker pair was found, exit
            if (*star_idx_ptr != -1) {
                break;
            }
        }
    }

    static game::id_planet Find_Planet_At_Star_Satisfying(game::s_game_state& state, const s_star_data& star,
        bool(__cdecl* predicate)(s_planet_data*)) {
        for (const auto& link : star.orbit_planet_ids) {
            const auto id = link;
            if (id != game::NO_PLANET && predicate(&state.planets.at(id))) return id;
        }
        return game::NO_PLANET;
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
    game::id_colony __cdecl Target_Planet_To_Colony(game::id_planet planet_idx) {
        if (planet_idx == game::NO_PLANET) {
            return game::NO_COLONY;
        }

        return current_state.planets.at(planet_idx).colony_id;
    }

    void __cdecl Do_System_Discoveries_At_Star(s_star_data* star) {
        int16_t star_idx;
        int eligible_player_count;
        uint8_t player_present[MAX_PLAYERS];
        int16_t player_idx;
        int16_t limit;
        int n_techs;
        int total_valid_techs;
        s_MSG msg = {};

        star_idx = game::Star_Row(game::current_state, star->id);
        Reset_One_Shot_System_Special(star);

        if (star->system_special != SYSTEM_SPECIAL_NO_SPECIAL && (*star_has_nonplayer)[star_idx] == 0) {
            eligible_player_count = 0;
            memset(player_present, 0, sizeof(player_present));

            for (int i = 0; i < game::current_state.player_count; ++i) {
                if ((*star_has_player)[star_idx][i].has_player != 0) {
                    player_present[i] = 1;
                    eligible_player_count++;
                }
            }

            if (eligible_player_count > 0) {
                player_idx = RUSS::Get_Weighted_Choice_Char(player_present, game::current_state.player_count);

                switch (star->system_special) {
                    case SYSTEM_SPECIAL_SPACE_DEBRIS:
                        game::current_state.players[player_idx].bc += system_discovery_bc.space_debris;
                        star->system_special = player_idx + 50;
                        break;

                    case SYSTEM_SPECIAL_PIRATE_CACHE:
                        game::current_state.players[player_idx].bc += system_discovery_bc.pirate_cache;
                        star->system_special = player_idx + 60;
                        break;

                    case SYSTEM_SPECIAL_SPLINTER_COLONY: {
                        const auto id = Find_Planet_At_Star_Satisfying(current_state, *star, Planet_Has_Splinter_Colony);
                        if (id == game::NO_PLANET) break;
                        const auto colony_id = INITGAME::Init_Colony(current_state, game::NO_COLONY, id, current_state.players[player_idx].id);
                        if (colony_id == game::NO_COLONY) break;
                        star->system_special = player_idx + 70;
                        (*star_has_player)[star_idx][player_idx].colony_count = 1;
                        auto* colony = &game::current_state.colonies.at(colony_id);

                        limit = COLCALC::Colony_Race_Pop_Limit(colony, player_idx);
                        colony->n_pops = (uint8_t)limit;
                        if (colony->n_pops > 3) {
                            colony->n_pops = 3;
                        }

                        memset(colony->buildings, 0, sizeof(colony->buildings));
                        std::ranges::fill(colony->pop, s_colony_pop{});

                        for (int i = 0; i < colony->n_pops; ++i) {
                            auto pop_val = colony->pop[i];

                            pop_val.group = static_cast<uint8_t>(colony->owner.id);
                            pop_val.original_owner = colony->owner;

                            if (current_state.planets.at(id).food_per_farmer == 0 || game::current_state.players[player_idx].traits[TRAIT_EATS_MINERALS] != 0) {
                                pop_val.profession = game_random::Random(2) == 1 ? POP::PROF_WORKER : POP::PROF_SCIENTIST;
                            } else if (game::current_state.players[player_idx].traits[TRAIT_CYBERNETIC] == 0) {
                                pop_val.profession = POP::PROF_FARMER;
                            } else if (i == 0) {
                                pop_val.profession = POP::PROF_WORKER;
                            } else {
                                pop_val.profession = POP::PROF_FARMER;
                            }

                            pop_val.assigned = true;
                            colony->pop[i] = pop_val;
                        }

                        COLCALC::Compute_Star_Colony_Stuff(star);
                        COLCALC::Colony_Calculation(colony_id);
                        colony->just_produced = COLONY_PRODUCTION_JUST_COLONIZED;
                        break;
                    }

                    case SYSTEM_SPECIAL_MAROONED_HERO:
                        star->system_special = player_idx + 80;
                        break;

                    case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS:
                        if (star->artifacts_gave_app == 0) {
                            uint8_t* picked_tech_ids;
                            int r;

                            star->system_special = player_idx + 90;
                            star->artifacts_gave_app = 1;
                            const auto id = Find_Planet_At_Star_Satisfying(
                                current_state, *star, Planet_Has_Ancient_Artifacts);

                            if (!MAPGEN::special_generation_config.artifacts_grant_technology) {
                                break;
                            }

                            msg.msg_type = MSG_TYPE_ARTIFACTS;
                            msg.u.artifacts.destination = game::Destination_At_Row(current_state, star_idx);
                            picked_tech_ids = (uint8_t*)&msg.u.artifacts.tech1;
                            picked_tech_ids[0] = 0;
                            picked_tech_ids[1] = 0;

                            r = game_random::Random(4);
                            n_techs = ((r + ((r >> 31) & 3)) >> 2) + 1;

                            for (int remaining = n_techs; remaining != 0; --remaining) {
                                total_valid_techs = 0;
                                for (int tech_id = 1; tech_id < 204; ++tech_id) {
                                    if (Player_Tech_Field_Status_(&game::current_state.players[player_idx], TECHDATA::technology_applications[tech_id].tech_field_id) == 2 &&
                                        COLCALC::Player_May_Get_Tech_App(&game::current_state.players[player_idx], tech_id)) {
                                        total_valid_techs++;
                                        if (game_random::Random(total_valid_techs) == 1) {
                                            picked_tech_ids[remaining - 1] = (uint8_t)tech_id;
                                        }
                                    }
                                }
                                if (total_valid_techs != 0) {
                                    const int16_t tech_app_id = picked_tech_ids[remaining - 1];
                                    COLCALC::Player_Gets_Tech_App(&game::current_state.players[player_idx], tech_app_id);

                                    s_report report = {};
                                    report.type = 3;
                                    report.recipient = current_state.players[player_idx].id;
                                    report.d.artifacts.planet_id = id;
                                    report.d.artifacts.tech_app_id = tech_app_id;
                                    REPORT::Add_Report(&report);
                                }
                            }

                            if (picked_tech_ids[0] != 0) {
                                MSG_::Add_Msg(msg, game::current_state.players[player_idx].id);
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
    void __cdecl Get_Local_Human_Target(int16_t star_idx, game::id_player player, game::owner_slot* target_owner, game::id_colony* target_colony_idx_out) {
        SYS::Clear_System_Display_Loaded_Flag();

        *target_colony_idx_out = game::NO_COLONY;
        *target_owner = game::NO_OWNER;

        if (star_idx == game::ANTARES_POSITION) {
            *target_owner = game::owner_slot{8};
            return;
        }

        if (COMBFIND::target.num_colony_targets > 0 || COMBFIND::target.num_player_targets > 0) {
            video::Set_Page_Off();
            MAINSCR::Draw_Mini_Main_Screen();
            video::Copy_Off_To_Back();
            RUSS::Mox_Sync_Update();
            MAINSCR::Draw_Mini_Main_Screen();

            REPORT::something_interesting_happened[player.id] = 1;
            current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).visited.set(player);
            MOX::local_player = player;
            MAINSCR::Update_Ship_Icons_In_Between_Turns();

            while (true) {
                auto target_id = game::NO_OWNER;
                *target_colony_idx_out = game::NO_COLONY;
                *target_owner = game::NO_OWNER;

                int16_t selection = MAINPUPS::Defense_Colony_Selection_Popup(star_idx, MOX::local_player);

                if (selection != -1) {
                    COMBFIND::messed_up_fleet_palette = 1;
                    if (selection < 15) {
                        target_id = factions::From_Legacy(selection);
                        *target_owner = target_id;
                    } else {
                        const auto planet = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).orbit_planet_ids[selection - 15];
                        const auto colony_idx = Target_Planet_To_Colony(planet);
                        if (colony_idx == game::NO_COLONY) continue;
                        *target_colony_idx_out = colony_idx;
                        target_id = factions::From_Player(current_state.colonies.at(colony_idx).owner);
                    }
                }

                if (target_id == game::NO_OWNER) {
                    return;
                }

                if (COMBFIND::Human_Confirms_Attack(player, target_id)) {
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
    void __cdecl Get_AI_Target(int16_t star_idx, game::id_player attacker, game::owner_slot* target_owner, game::id_colony* target_colony, int8_t* is_sneak_attack) {
        const auto player_idx = attacker.id;
        *is_sneak_attack = 0;
        *target_owner = game::NO_OWNER;
        *target_colony = game::NO_COLONY;

        if (game::ANTARES_POSITION == star_idx) {
            *target_owner = game::owner_slot{8};
            return;
        }

        if (COMBFIND::target.num_colony_targets > 0 || COMBFIND::target.num_player_targets > 0) {
            s_player* player = &game::current_state.players[player_idx];
            auto sneak_attack_target = game::NO_PLAYER;

            const auto attack_planet = player->sneak_attack_planet;
            if (attack_planet != game::NO_PLANET && star_idx >= 0 && star_idx < std::ssize(current_state.stars)) {
                for (int orbit = 0; orbit < 5; ++orbit) {
                    if (game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).orbit_planet_ids[orbit] != attack_planet) continue;
                    sneak_attack_target = player->sneak_attack_player;
                    player->sneak_attack_planet = game::NO_PLANET;
                    player->sneak_attack_player = game::NO_PLAYER;
                    player->war_buildup_duration = 0;
                    break;
                }
            }

            uint32_t bonus_info[4];
            uint32_t our_strength[15];
            uint32_t enemy_strength[15];

            COMBFIND::Get_Ship_Strength_Info(enemy_strength, our_strength, bonus_info, star_idx, attacker);

            uint32_t best_colony_score = 0;
            game::id_colony best_colony_idx = game::NO_COLONY;

            COMBFIND::Get_Best_Colony_Target(enemy_strength, our_strength, bonus_info, star_idx, attacker, &best_colony_score, &best_colony_idx, sneak_attack_target);

            uint32_t best_player_score = 0;
            auto best_owner = game::NO_OWNER;

            // Only consider player targets if there is no sneak attack target at this star or no valid colony target found
            if (sneak_attack_target == game::NO_PLAYER || best_colony_idx == game::NO_COLONY) {
                for (int16_t i = 0; i < COMBFIND::target.num_player_targets; ++i) {
                    const auto enemy = COMBFIND::target.faction_targets[i].get();
                    const auto enemy_idx = enemy.code;
                    const bool is_sneak_target = sneak_attack_target != game::NO_PLAYER && factions::From_Player(sneak_attack_target) == enemy;
                    uint32_t enemy_str = enemy_strength[enemy_idx];
                    uint32_t our_str = our_strength[enemy_idx];

                    if (enemy_str <= our_str) {
                        bool is_valid = false;
                        if (enemy_str < our_str) {
                            is_valid = true;
                        } else {
                            s_star_data* star = &game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx));
                            uint8_t player_bit = (uint8_t)player_idx;
                            uint8_t enemy_bit = (uint8_t)enemy_idx;
                            // Check if player has a colony here and if it is currently blockaded by the enemy
                            if ((star->has_colony.test_slot(player_bit & 0x1F)) &&
                                (star->blockaded_by[player_bit].test_slot(enemy_bit & 0x1F))) {
                                is_valid = true;
                            } else if (is_sneak_target) {
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
                            if (is_sneak_target) {
                                score = 0xFFFFFFFF;
                            } else {
                                score = (our_str << 4) / enemy_str;
                            }

                            if (best_player_score < score) {
                                best_player_score = score;
                                best_owner = enemy;
                            }
                        }
                    }
                }
            }

            if (best_colony_score < best_player_score) {
                *target_owner = best_owner;
                if (sneak_attack_target != game::NO_PLAYER && best_owner == factions::From_Player(sneak_attack_target)) {
                    *is_sneak_attack = 1;
                }
            } else if (best_colony_score != 0) {
                *target_colony = best_colony_idx;
                if (sneak_attack_target != game::NO_PLAYER && current_state.colonies.at(best_colony_idx).owner == sneak_attack_target) {
                    *is_sneak_attack = 1;
                }
            }
        }
    }

    void __cdecl Update_Player_Needs_To_Attack(int16_t star_idx, game::owner_slot first, game::owner_slot second) {
        for (const auto owner : {first, second}) {
            const auto slot = owner.code;
            if (slot < 0 || slot >= 15) continue;
            const bool present = slot < MAX_PLAYERS
                ? (*star_has_player)[star_idx][slot].has_player != 0
                : (((*star_has_nonplayer)[star_idx] >> slot) & 1) != 0;
            if (!present) (*player_needs_to_attack)[star_idx].reset(slot);
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
    void __cdecl Get_NPC_Target(int16_t star_idx, game::owner_slot attacker, game::owner_slot* target_owner, game::id_colony* target_colony_idx_out) {
        int lucky_count;
        int non_lucky_count;
        game::id_colony lucky_target;
        game::id_colony non_lucky_target;
        int i;

        *target_colony_idx_out = game::NO_COLONY;
        *target_owner = game::NO_OWNER;

        if (attacker == game::owner_slot{13}) {
            return;
        }

        if (game::ANTARES_POSITION == star_idx) {
            *target_owner = game::owner_slot{8};
            return;
        }

        if (COMBFIND::target.num_colony_targets > 0) {
            lucky_count = 0;
            non_lucky_count = 0;
            lucky_target = game::NO_COLONY;
            non_lucky_target = game::NO_COLONY;

            for (i = 0; i < (int)COMBFIND::target.num_colony_targets; ++i) {
                game::id_colony colony_idx = COMBFIND::target.colony_targets[i].get();
                const auto owner = current_state.colonies.at(colony_idx).owner;

                if (current_state.players.at(owner.id).traits[TRAIT_LUCKY] != 0) {
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

            if (non_lucky_target != game::NO_COLONY) {
                *target_colony_idx_out = non_lucky_target;
            } else {
                *target_colony_idx_out = lucky_target;
            }
            return;
        }

        if (COMBFIND::target.num_player_targets > 0) {
            lucky_count = 0;
            non_lucky_count = 0;
            auto lucky_player = game::NO_OWNER;
            auto non_lucky_player = game::NO_OWNER;

            for (i = 0; i < (int)COMBFIND::target.num_player_targets; ++i) {
                const auto faction = COMBFIND::target.faction_targets[i].get();
                const auto player = factions::Player(faction);

                // If player is valid index and not lucky, prioritize them
                if (player != game::NO_PLAYER && current_state.players.at(player.id).traits[TRAIT_LUCKY] == 0) {
                    non_lucky_count++;
                    if (game_random::Random(non_lucky_count) == 1) {
                        non_lucky_player = faction;
                    }
                } else {
                    lucky_count++;
                    if (game_random::Random(lucky_count) == 1) {
                        lucky_player = faction;
                    }
                }
            }

            *target_owner = non_lucky_player != game::NO_OWNER ? non_lucky_player : lucky_player;
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
    int8_t __cdecl Human_Confirms_Attack(game::id_player player, game::owner_slot target) {
        const auto opponent = factions::Player(target);
        const auto player_idx = player.id;
        const auto target_idx = opponent.id;
        char message_buffer[400];
        int8_t confirmed = 1;

        if (player_idx < MAX_PLAYERS && opponent != game::NO_PLAYER && (*attack_confirmed)[player_idx][target_idx] == 0) {
            int treaty_count = 0;
            strlcpy(message_buffer, you_have_following_treaties_msg, sizeof(message_buffer));
            strlcat(message_buffer, "\n", sizeof(message_buffer));

            // Check standard treaties (e.g., Non-Aggression, Alliance)
            int8_t treaty_type = game::current_state.players[player_idx].treaty[target_idx];
            if (treaty_type > 0 && treaty_type < 4) {
                treaty_count = 1;
                strlcat(message_buffer, MOX::treaty_labels[treaty_type], sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            // Check trade treaty
            if (game::current_state.players[player_idx].trade_treaty[target_idx] != 0) {
                treaty_count++;
                strlcat(message_buffer, trade_treaty_msg, sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            // Check research treaty
            if (game::current_state.players[player_idx].research_treaty[target_idx] != 0) {
                treaty_count++;
                strlcat(message_buffer, research_treaty_msg, sizeof(message_buffer));
                strlcat(message_buffer, "\n", sizeof(message_buffer));
            }

            // Check if player is receiving tribute from the target
            if (game::current_state.players[target_idx].tribute_treaty[player_idx] != 0) {
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

    int8_t __cdecl Player_May_Attack_Player(game::owner_slot attacking_owner, game::id_player target) {
        const auto attacker_idx = attacking_owner.code;
        const auto target_idx = target.id;
        if (attacker_idx < 0 || target_idx >= MAX_PLAYERS || attacking_owner == factions::From_Player(target)) {
            return 0;
        }

        // Indices >= 8 usually represent Antarans, Guardians, or space monsters
        if (attacker_idx >= MAX_PLAYERS) {
            return 1;
        }

        s_player* attacker = &game::current_state.players[attacker_idx];

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

    game::id_planet __cdecl Target_Colony_To_Planet(game::id_colony colony_idx) {
        if (colony_idx == game::NO_COLONY) {
            return game::NO_PLANET;
        }

        return game::current_state.colonies.at(colony_idx).planet_id;
    }

    int8_t __cdecl Some_Star_Player_Is_Busy(int16_t star_idx) {
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (game::current_state.players[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                s_combfind_star_has_player& star_info = (*COMBFIND::star_has_player)[star_idx][i];

                if (star_info.colony_count != 0 || star_info.has_player != 0) {
                    if (NETMOX::net.player_net_info[i].combat_attacker_id.get().code != -1) {
                        return 1;
                    }

                    if (MOX::Legacy_Local_Player_Slot() == i) {
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

    static int16_t Russ_Combat(game::owner_slot attacker, game::owner_slot defender, game::s_combat_fleets& fleets,
        int16_t star_idx, game::id_colony colony_idx, int8_t is_sneak_attack, s_battle_results* results);
    static void Kill_Noncombat_Ships(std::span<const game::id_ship> keys, int16_t star_idx, game::id_player player);
    static void Do_Attacker_Beat_Colony_Stuff(std::span<const game::id_ship> keys, game::owner_slot attacker, game::id_colony colony_id, s_battle_results* results);

    int16_t __cdecl Do_1_Combat(int16_t star_idx, game::owner_slot attacking_owner, game::owner_slot defending_owner, game::id_planet planet_idx, s_battle_results* result, int8_t is_sneak_attack) {
        const auto plr2_idx = attacking_owner.code;
        if (combat_in_progress) {
            exitm::Exit_With_Message("nested combat detected");
        }
        combat_in_progress = true;

        COMBINIT::Reset_Satellite_Capture();

        game::id_colony colony_idx;
        char msg_buf[200];
        s_battle_results local_results;

        colony_idx = Target_Planet_To_Colony(planet_idx);

        if (defending_owner == game::NO_OWNER) {
            defending_owner = factions::From_Player(current_state.colonies.at(colony_idx).owner);
        }
        const auto plr1_idx = defending_owner.code;

        if (plr2_idx < MAX_PLAYERS && plr1_idx < MAX_PLAYERS) {
            game::current_state.players[plr2_idx].time_since_last_attack[plr1_idx] = 0;
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
            if (MOX::Legacy_Local_Player_Slot() == plr1_idx && MOX::settings.strategic_combat_flag == 0) {
                char* star_or_planet_name;
                if (planet_idx == game::NO_PLANET) {
                    star_or_planet_name = RUSS::Star_Name(star_idx);
                } else {
                    star_or_planet_name = HACCESS::Get_Planet_Name(current_state, planet_idx);
                }
                const char* attacker_name = COLCALC::Combatant_Name(attacking_owner);
                snprintf(msg_buf, sizeof(msg_buf), s_attacks_you_at_s_msg, attacker_name, star_or_planet_name);
                TEXTBOX::Timed_Text_Box(
                    nullptr,
                    msg_buf,
                    confirm_battle_message_timeout);
            }
        }

        const auto gather_fleets = [&] {
            return game::Select_Combat_Fleets(current_state, star_idx, attacking_owner, defending_owner,
                game::Received_Combat_Order(current_state, star_idx));
        };
        auto fleets = gather_fleets();

        short combat_status = Russ_Combat(attacking_owner, defending_owner, fleets, star_idx, colony_idx, is_sneak_attack, result);
        result->player_info.combat_result = (int8_t)combat_status;

        colony_idx = Target_Planet_To_Colony(planet_idx);

        if (MOX::game_type > 1 && MOX::game_type < 4) {
            if (MOX::Legacy_Local_Player_Slot() == plr2_idx || MOX::Legacy_Local_Player_Slot() == plr1_idx) {
                MULTPLAY::Load_Net_Next_Turn_Screen(1);
                MOX::FULL_DRAW_SCREEN_FLAG = 1;
                MULTPLAY::Chat_Box_Input_Loop();
            }
        }

        if (result->player_info.combat_result >= 0) {
            if (const auto player = factions::Player(defending_owner); player != game::NO_PLAYER) {
                if (current_star_has_player[plr1_idx].ship_sizes[SHIP_TYPE_COMBAT] == 0 && colony_idx == game::NO_COLONY) {
                    Kill_Noncombat_Ships(fleets.defender, star_idx, player);
                } else {
                    Determine_Retreat_Ships(star_idx, player, colony_idx);
                }
            } else {
                *current_star_has_nonplayer &= ~(1 << (plr1_idx - MAX_PLAYERS));
            }
        }

        if (result->player_info.combat_result <= 0) {
            if (const auto player = factions::Player(attacking_owner); player != game::NO_PLAYER) {
                Determine_Retreat_Ships(star_idx, player, game::NO_COLONY);
            } else {
                *current_star_has_nonplayer &= ~(1 << (plr2_idx - MAX_PLAYERS));
            }

            if (plr2_idx == 9) { // Guardian defeated
                game::current_state.players[plr1_idx].captured_orion = 1;
                OFFICER::Give_Player_Loknar_Special(factions::Player(defending_owner), star_idx, result->info.orion_techs, &result->info.rescued_hero_id);
                if (game::current_state.players[plr1_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::game_type == 0 || MOX::game_type == 1 || MOX::settings.strategic_combat_flag == 0) {
                        ERIC::Play_Take_Orion_Flic();
                        OFFICER::Rescued_Loknar_Popup(result->info.orion_techs, result->info.rescued_hero_id);
                    }
                }
            }

            if (result->player_info.combat_result < 0) {
                fleets = gather_fleets();
                if (fleets.defender.empty()) {
                    if (plr1_idx < MAX_PLAYERS) {
                        Reset_Combat_Ships_At_Current_Star_Has_Player(factions::Player(defending_owner));
                    } else {
                        *current_star_has_nonplayer &= ~(1 << (plr1_idx - MAX_PLAYERS));
                    }
                }
            }
        } else {
            if (plr1_idx == 9) { // Guardian defeated by attacker
                game::current_state.players[plr2_idx].captured_orion = 1;
                OFFICER::Give_Player_Loknar_Special(factions::Player(attacking_owner), star_idx, result->info.orion_techs, &result->info.rescued_hero_id);
                if (game::current_state.players[plr2_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::game_type == 0 || MOX::game_type == 1 || MOX::settings.strategic_combat_flag == 0) {
                        ERIC::Play_Take_Orion_Flic();
                        OFFICER::Rescued_Loknar_Popup(result->info.orion_techs, result->info.rescued_hero_id);
                    }
                }
            }

            if (star_idx == game::ANTARES_POSITION) { // Antares victory
                game::current_state.players[plr2_idx].captured_antares = 1;
                for (auto& waiting : *player_needs_to_attack) waiting.reset();
            } else {
                fleets = gather_fleets();
                if (fleets.attacker.empty()) {
                    if (plr2_idx < MAX_PLAYERS) {
                        Reset_Combat_Ships_At_Current_Star_Has_Player(factions::Player(attacking_owner));
                    } else {
                        *current_star_has_nonplayer &= ~(1 << (plr2_idx - MAX_PLAYERS));
                    }
                }
                if (colony_idx != game::NO_COLONY && legacy_world::Encode_Player_Slot(game::current_state.colonies.at(colony_idx).owner) != -1) {
                    Do_Attacker_Beat_Colony_Stuff(fleets.attacker, attacking_owner, colony_idx, result);
                }
            }
        }

        const int16_t combat_result = (int16_t)result->player_info.combat_result;
        COMBINIT::Reset_Satellite_Capture();
        combat_in_progress = false;
        return combat_result;
    }

    static int16_t Russ_Combat(game::owner_slot attacking_owner, game::owner_slot defending_owner, game::s_combat_fleets& fleets,
        int16_t star_idx, game::id_colony colony_idx, int8_t is_sneak_attack, s_battle_results* results) {
        const auto attacker_idx = attacking_owner.code;
        const auto defender_idx = defending_owner.code;
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

        if (attacker_idx < MAX_PLAYERS && game::current_state.players[attacker_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            plr1_is_human = true;
        } else {
            plr1_is_human = false;
        }

        if (defender_idx < MAX_PLAYERS && game::current_state.players[defender_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            plr2_is_human = true;
        } else {
            plr2_is_human = false;
        }

        // Early return if no actual combat possible and not at special star location
        if (fleets.attacker.empty() ||
            (fleets.defender.empty() && colony_idx == game::NO_COLONY && star_idx != game::ANTARES_POSITION)) {
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
                DIP_SCRN::Show_Sneak_Attack_Message(game::current_state.players[attacker_idx].id);
            }
            MAINPUPS::Player_Attacked_Popup(attacking_owner, defending_owner, star_idx, colony_idx);
            COMBFIND::messed_up_fleet_palette = 1;
        }

        // Force strategic combat if no human players are involved
        if (strategic_combat_flag == 0) {
            if (!plr1_is_human && !plr2_is_human) {
                strategic_combat_flag = 1;
            }
        }

        auto& world = current_state;
        const auto refresh_fleets = [&] {
            fleets.attacker = game::Active_Combat_Keys(world, fleets.attacker, star_idx, attacking_owner);
            fleets.defender = game::Active_Combat_Keys(world, fleets.defender, star_idx, defending_owner);
            return !fleets.attacker.empty() && (!fleets.defender.empty() || colony_idx != game::NO_COLONY || star_idx == game::ANTARES_POSITION) &&
                (colony_idx == game::NO_COLONY || current_state.colonies.contains(colony_idx));
        };
        const auto run_tactical = [&](int16_t retreat_flag) -> int16_t {
            return refresh_fleets() ? COMBINIT::Tactical_Combat(fleets.attacker, fleets.defender, star_idx, colony_idx, retreat_flag) : 0;
        };

        if (strategic_combat_flag != 0) {
            if (!refresh_fleets()) return 0;
            combat_result_code = COMBAT::Strategic_Combat(fleets.attacker, fleets.defender, star_idx, colony_idx,
                results->info.summary, sizeof(results->info.summary), results->info.details, sizeof(results->info.details));

            // Show result dialog if human is involved or it is a local single player game
            if (not_multiplayer || MOX::Legacy_Local_Player_Slot() == attacker_idx) {
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
                        retreat_flag = AI_Can_Retreat(current_state.players[defender_idx].id, colony_idx);
                    }
                } else {
                    retreat_flag = AI_Can_Retreat(current_state.players[attacker_idx].id, game::NO_COLONY);
                }
            } else {
                retreat_flag = 0;
            }

            // Multiplayer synchronization for tactical combat
            if (plr1_is_human && plr2_is_human && (MOX::game_type == 2 || MOX::game_type == 3)) {
                int16_t remote_plr_idx = (MOX::Legacy_Local_Player_Slot() == attacker_idx) ? defender_idx : attacker_idx;
                int16_t net_id = game::current_state.players[remote_plr_idx].network_player_id;

                NETMOX::Mox_Send_Message(net_id, NET_PACKET_TACTICAL_COMBAT_START, nullptr, 0);

                while (NETMOX::net.tactical_combat_sync_state == NET_TACTICAL_COMBAT_SYNC_WAITING_FOR_START) {
                    RUSS::Mox_Update();
                }

                combat_result_code = run_tactical(retreat_flag);

                NETMOX::Mox_Send_Message(net_id, NET_PACKET_TACTICAL_COMBAT_END, nullptr, 0);

                while (NETMOX::net.tactical_combat_sync_state == NET_TACTICAL_COMBAT_SYNC_STARTED) {
                    RUSS::Mox_Update();
                }

                NETMOX::net.tactical_combat_sync_state = NET_TACTICAL_COMBAT_SYNC_WAITING_FOR_START;
            } else {
                combat_result_code = run_tactical(retreat_flag);
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        if (combat_result_code < 0) {
            return -1;
        }

        return (combat_result_code > 0);
    }

    static void Kill_Noncombat_Ships(std::span<const game::id_ship> keys, int16_t star_idx, game::id_player player) {
        if (keys.empty()) return;
        const auto owner_idx = player.id;
        const auto owner = factions::From_Player(player);

        if (game::current_state.players[owner_idx].objectives == PLAYER_OBJECTIVE_HUMAN &&
            MOX::game_type >= 0 && MOX::game_type <= 1) {
            GENDRAW::Message_Box(noncombat_ships_destroyed_msg);
        }

        auto& world = current_state;
        for (const auto key : keys) {
            const auto ship_entry = world.ships.find(key);
            auto* ship = ship_entry == world.ships.end() ? nullptr : &ship_entry->second;
            if (ship && ship->owner == owner && ship->status == 0 &&
                game::Ship_Orbit_Row(current_state, ship->position) == star_idx) SHIPS::Kill_Ship(*ship);
        }

        current_star_has_player[owner_idx].has_player = 0;
        MAINSCR::Update_Ship_Icons_In_Between_Turns();
    }

    void __cdecl Determine_Retreat_Ships(int16_t star_idx, game::id_player player, game::id_colony colony_idx) {
        if (player.id >= MAX_PLAYERS) return;
        const auto p_idx = player.id;
        const auto owner = factions::From_Player(player);
        bool should_retreat_or_die = false;
        const auto ships = game::Ships_In_Battle_Orbit(current_state, star_idx, game::Received_Combat_Order(current_state, star_idx));

        if (colony_idx != game::NO_COLONY || COMBFIND::current_star_has_player[p_idx].colony_count == 0) {
            if (COMBFIND::current_star_has_player[p_idx].colony_count < 2) {
                should_retreat_or_die = true;
            }
        }

        if (should_retreat_or_die) {
            bool destroy_ships_in_flux = false;
            if (game::current_state.players[p_idx].traits[TRAIT_TRANS_DIMENSIONAL] == 0) {
                if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
                    destroy_ships_in_flux = true;
                }
            }

            if (destroy_ships_in_flux) {
                // Under Hyperspace Flux, non-trans-dimensional player fleets at this star are destroyed instead of retreating.
                for (const auto key : ships) {
                    const auto ship_entry = current_state.ships.find(key);
                    auto* ship = ship_entry == current_state.ships.end() ? nullptr : &ship_entry->second;
                    if (ship && ship->owner == owner && ship->status == 0) {
                        if (ship->officer_index != -1) {
                            if (current_state.leaders.at(ship->officer_index).eta <= 0) {
                                OFFICER::Kill_Officer(ship->officer_index);
                            } else {
                                OFFICER::Move_Officer_To_Pool(ship->officer_index);
                            }
                        }
                        SHIPS::Kill_Ship(*ship);
                    }
                }
                COMBFIND::current_star_has_player[p_idx].has_player = 0;
                return;
            } else {
                // Otherwise the fleet enters retreat status.
                for (const auto key : ships) {
                    const auto ship_entry = current_state.ships.find(key);
                    auto* ship = ship_entry == current_state.ships.end() ? nullptr : &ship_entry->second;
                    if (ship && ship->owner == owner && ship->status == 0) {
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
        for (const auto key : ships) {
            const auto ship_entry = current_state.ships.find(key);
            const auto* ship = ship_entry == current_state.ships.end() ? nullptr : &ship_entry->second;
            if (ship && ship->owner == owner && ship->status == 0) {
                ships_remaining = true;
                break;
            }
        }

        if (!ships_remaining) {
            COMBFIND::current_star_has_player[p_idx].has_player = 0;
        }
    }

    void __cdecl Reset_Combat_Ships_At_Current_Star_Has_Player(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return;
        const auto player_idx = player.id;
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
    int8_t __cdecl AI_Can_Retreat(game::id_player player, game::id_colony excluded_colony_idx) {
        if (player.id >= MAX_PLAYERS) return 0;
        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            if (roster_colony.owner == player && roster_id != excluded_colony_idx) {
                return 1;
            }
        }
        return 0;
    }

    void __cdecl Get_Ship_Strength_Info(uint32_t* enemy_strength, uint32_t* player_strength, uint32_t* shield_strength, int16_t star_idx, game::id_player player) {
        const auto player_owner = factions::From_Player(player);
        uint32_t ship_size_sum = 0;

        memset(enemy_strength, 0, 15 * sizeof(uint32_t));
        memset(player_strength, 0, 15 * sizeof(uint32_t));
        memset(shield_strength, 0, 4 * sizeof(uint32_t));

        for (const auto key : game::Ships_In_Battle_Orbit(current_state, star_idx, game::Received_Combat_Order(current_state, star_idx))) {
            const auto ship_entry = std::as_const(current_state).ships.find(key);
            const auto* ship = ship_entry == std::as_const(current_state).ships.end() ? nullptr : &ship_entry->second;
            if (ship && ship->status == 0) {
                int16_t owner = factions::Legacy_Code(ship->owner);

                if (ship->owner == player_owner) {
                    ship_size_sum += 1U << (ship->d.size & 0x1F);

                    for (int16_t p = 0; p < game::current_state.player_count; ++p) {
                        if (player != current_state.players[p].id) {
                            if ((*star_has_player)[star_idx][p].has_player != 0 ||
                                (*star_has_player)[star_idx][p].colony_count != 0) {
                                player_strength[p] += AIPOWER::Ship_Strength_Vs_Ships(&current_state, MOX::settings.strategic_combat_flag, key, factions::From_Player(current_state.players[p].id));
                            }
                        }
                    }

                    for (int16_t np = MAX_PLAYERS; np < 15; ++np) {
                        if ((((*star_has_nonplayer)[star_idx] >> (np - MAX_PLAYERS)) & 1) != 0 &&
                            !(*player_needs_to_attack)[star_idx].test(np)) {
                            player_strength[np] += AIPOWER::Ship_Strength_Vs_Ships(&current_state, MOX::settings.strategic_combat_flag, key, factions::From_Legacy(np));
                        }
                    }

                    uint16_t shield[4];
                    AIPOWER::Ship_Strength_Vs_Planetary_Shield(&game::current_state, MOX::settings.strategic_combat_flag, key, shield);

                    for (int j = 0; j < 4; ++j) {
                        shield_strength[j] += shield[j];
                    }
                } else if (owner >= 0 && owner < 15) {
                    enemy_strength[owner] += AIPOWER::Ship_Strength_Vs_Ships(&current_state, MOX::settings.strategic_combat_flag, key, player_owner);
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
        game::id_player player,
        uint32_t* best_score,
        game::id_colony* best_colony_idx,
        game::id_player preferred_owner) {
        *best_score = 0;
        *best_colony_idx = game::NO_COLONY;

        const auto player_idx = player.id;
        uint32_t best_pref_score = 0;
        game::id_colony best_pref_colony = game::NO_COLONY;

        if (target.num_colony_targets > 0) {
            for (int16_t i = 0; i < target.num_colony_targets; ++i) {
                game::id_colony colony_idx = target.colony_targets[i].get();
                s_colony* colony = &game::current_state.colonies.at(colony_idx);

                int16_t shield_val = COLCALC::Colony_Planetary_Shield(colony);
                const auto owner = colony->owner.id;

                uint32_t our_space_str = our_space_str_arr[owner];
                uint32_t our_ground_str = our_ground_str_arr[shield_val];

                uint32_t enemy_space_str_vs = AIPOWER::Colony_Space_Strength_Vs_Player(&current_state, MOX::settings.strategic_combat_flag, colony_idx, player) & 0xFFFF;
                uint32_t enemy_space_str = enemy_fleet_space_str_arr[owner] + enemy_space_str_vs;

                uint32_t enemy_ground_str_vs = AIPOWER::Colony_Ground_Strength_Vs_Player(&current_state, MOX::settings.strategic_combat_flag, colony_idx, player) & 0xFFFF;
                uint32_t enemy_ground_str = enemy_ground_str_vs;

                bool player_is_omni = false;
                if (game::current_state.players[player_idx].traits[TRAIT_TELEPATHIC] != 0 && game::current_state.players[owner].traits[TRAIT_TELEPATHIC] == 0) {
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
                                POP::Group_Player(colony->pop[0]) != player ||
                                (*star_has_player)[star_idx][player_idx].ship_sizes[SHIP_TYPE_TRANSPORT] != 0 ||
                                game::current_state.players[player_idx].personality == PLAYER_PERSONALITY_RUTHLESS) {
                                is_valid = true;
                            }
                        }
                    }
                }

                if (is_valid) {
                    uint32_t worth = (uint32_t)AIMOVE::Enemy_Colony_Worth_To_Player(&AIDATA::state, &current_state, colony_idx, player);

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

                    if (colony->owner == preferred_owner && score > best_pref_score) {
                        best_pref_score = score;
                        best_pref_colony = colony_idx;
                    }
                }
            }
        }

        if (preferred_owner != game::NO_PLAYER && best_pref_colony != game::NO_COLONY) {
            *best_score = best_pref_score;
            *best_colony_idx = best_pref_colony;
        }
    }

    static void Do_Attacker_Beat_Colony_Stuff(std::span<const game::id_ship> keys, game::owner_slot attacking_owner, game::id_colony colony_idx, s_battle_results* results) {
        if (keys.empty()) return;
        const auto attacker_id = attacking_owner.code;
        const auto attacker = factions::Player(attacking_owner);
        auto* colony = &current_state.colonies.at(colony_idx);
        const auto planet_idx = colony->planet_id;
        const auto star = current_state.planets.at(planet_idx).star_id;
        int16_t star_idx = game::Star_Row(current_state, star);
        const auto defender = colony->owner;
        const auto colony_owner = legacy_world::Encode_Player_Slot(defender);
        const auto colony_id = colony_idx;
        auto& world = current_state;
        const auto orbit = game::Destination_At_Row(current_state, star_idx);
        const auto find_attacker = [&](game::id_ship key) -> s_ship_data* {
            const auto ship_entry = world.ships.find(key);
            auto* ship = ship_entry == world.ships.end() ? nullptr : &ship_entry->second;
            if (!ship || ship->owner != attacking_owner || ship->status != 0 || ship->position.kind != game::e_ship_location::at_star) return nullptr;
            if (orbit.kind == game::e_star_destination::antares) {
                return ship->position.target == game::e_ship_target::antares ? ship : nullptr;
            }
            return ship->position.target == game::e_ship_target::star && ship->position.star_id.get() == star ? ship : nullptr;
        };
        const auto active_keys = [&] {
            std::vector<game::id_ship> active;
            for (const auto key : keys) if (find_attacker(key)) active.push_back(key);
            return active;
        };
        const auto has_stellar_converter = [&](std::span<const game::id_ship> fleet) {
            return std::ranges::any_of(fleet, [&](auto key) {
                const auto* ship = find_attacker(key);
                return ship && INITSHIP::Ship_Has_Weapon(*ship, WEAPON_STELLAR_CONVERTER);
            });
        };
        const auto find_target = [&]() -> s_colony* {
            const auto it = current_state.colonies.find(colony_id);
            if (it == current_state.colonies.end() || it->second.planet_id != planet_idx) return nullptr;
            return &it->second;
        };

        if (colony->outpost_flag != 0) {
            results->outpost_destroyed_flag = 1;
            BOMB::Destroy_Colony(current_state, colony_id, 1);
            COMBFIND::current_star_has_player[colony_owner].colony_count--;
        } else if (attacker.id < MAX_PLAYERS) {
            if (game::current_state.players[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                bool is_active_player = false;
                if ((MOX::game_type != 2 && MOX::game_type != 3) || (MOX::Legacy_Local_Player_Slot() == attacker_id)) {
                    is_active_player = true;
                }

                if (is_active_player) {
                    auto attackers = active_keys();
                    if (attackers.empty()) return;
                    int32_t bombardment_res = Do_Bombardment(attacking_owner, colony_idx);
                    bool has_stellar = has_stellar_converter(attackers);

                    invasion::Get_Invasion_Info(attacker, attackers, colony_id, colony, &results->info.attacker_invasion, &results->info.defender_invasion);
                    int seed = game_random::Get_Random_Seed();
                    int16_t selection = MAINPUPS::Colony_Selection_Popup(&results->info.attacker_invasion, &results->info.defender_invasion, seed, colony_idx, bombardment_res, has_stellar);

                    bool loop_done = false;
                    do {
                        if (selection < 1 || selection > 4) break;
                        colony = find_target();
                        attackers = active_keys();
                        if (!colony || legacy_world::Encode_Player_Slot(colony->owner) != colony_owner || attackers.empty()) break;
                        has_stellar = has_stellar_converter(attackers);
                        switch (selection) {
                            case 1: { // Stellar Converter
                                if (!has_stellar) {
                                    loop_done = true;
                                    break;
                                }
                                int8_t old_n_pops = colony->n_pops;
                                results->info.planet_destroyed_flag = 1;
                                BOMB::Destroy_Colony(current_state, colony_id, 2);
                                ERIC::Play_Planetary_Destruction_Flic();
                                COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                current_state.planets.at(planet_idx).planet_type = PLANET_TYPE_ASTEROID;
                                if (current_state.planets.at(planet_idx).planet_special != SYSTEM_SPECIAL_NO_SPECIAL) {
                                    current_state.planets.at(planet_idx).planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
                                    game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                                }
                                NETMOX::Russ_Change_Relations(-10000, attacker, defender, 10, star_idx, old_n_pops);
                                loop_done = true;
                                break;
                            }
                            case 2: { // Bombing
                                int bomb_seed = game_random::Get_Random_Seed();
                                memset(&results->bomb, 0, sizeof(s_combat_bomb_result));
                                results->bomb.attacker = factions::From_Legacy(attacker_id);
                                results->bomb.colony_id = colony_id;
                                COMBAT::Bombardment(&results->bomb);

                                invasion::Get_Invasion_Info(attacker, attackers, colony_id, colony, &results->info.attacker_invasion, &results->info.defender_invasion);
                                selection = MAINPUPS::Colony_Bombing_Popup(&results->bomb, &results->info.attacker_invasion, &results->info.defender_invasion, bomb_seed, colony_idx, bombardment_res, has_stellar);
                                NETMOX::Russ_Change_Relations(-10000, attacker, defender, 10, star_idx, results->bomb.population_killed);

                                if (results->bomb.colony_wiped_out != 0) {
                                    if (const auto* target = find_target(); target && legacy_world::Encode_Player_Slot(target->owner) == colony_owner) {
                                        BOMB::Destroy_Colony(current_state, colony_id, 1);
                                        COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                    }
                                    loop_done = true;
                                } else {
                                    if (results->info.attacker_invasion.unit_counts[1] == 0 && !has_stellar && !ERIC::Player_Can_Mind_Control_Colony(attacker, colony_idx)) {
                                        loop_done = true;
                                    }
                                }
                                bombardment_res = -1;
                                break;
                            }
                            case 3: { // Invasion
                                if (!invasion::Invade(attacker, attackers, colony_idx, &results->info)) {
                                    loop_done = true;
                                    break;
                                }
                                int combat_seed = game_random::Get_Random_Seed();
                                ERIC::Force_Fade_To_Colony_Palette(1);
                                COLGCBT::Colony_Combat_Screen(&results->info.attacker_invasion, &results->info.defender_invasion, results->info.ground_defense_strength, colony_idx);
                                ERIC::Force_Fade_To_Colony_Palette(0);
                                game_random::Set_Random_Seed(combat_seed);

                                if (results->info.captured_tech_id != 0) {
                                    SCIENCE::Show_Off_Captured_Tech_At_Planet(results->info.captured_techs_data, results->info.captured_tech_id, results->info.captured_tech_planet_id.get());
                                }
                                NETMOX::Russ_Change_Relations(-10000, attacker, defender, 10, star_idx, 0);

                                if (const auto* target = find_target(); target && legacy_world::Encode_Player_Slot(target->owner) == attacker_id) {
                                    COMBFIND::current_star_has_player[attacker_id].colony_count++;
                                    COMBFIND::current_star_has_player[colony_owner].colony_count--;
                                }
                                COMBFIND::messed_up_fleet_palette = 1;
                                loop_done = true;
                                break;
                            }
                            case 4: { // Surrender/Mind Control
                                invasion::Change_Colony_Ownership(colony_id, colony, attacker, &results->info);
                                results->info.is_mind_controlled = 1;
                                const auto remaining_pop = colony->n_pops;
                                if (results->info.captured_tech_id != 0) {
                                    SCIENCE::Show_Off_Captured_Tech_At_Planet(results->info.captured_techs_data, results->info.captured_tech_id, planet_idx);
                                }
                                NETMOX::Russ_Change_Relations(-10000, attacker, defender, 10, star_idx, remaining_pop);
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
                        COMBINIT::Apply_Bio_Weapon_Penalty(attacker, star_idx, results->bomb.bio_pop_killed);
                    }

                    if ((MOX::game_type > 1 && MOX::game_type < 4) && (MOX::Legacy_Local_Player_Slot() == attacker_id || MOX::Legacy_Local_Player_Slot() == colony_owner)) {
                        MULTPLAY::Load_Net_Next_Turn_Screen(0);
                        MOX::FULL_DRAW_SCREEN_FLAG = 1;
                        MULTPLAY::Chat_Box_Input_Loop();
                    }
                }
            } else {
                if (ERIC::Player_Can_Mind_Control_Colony(attacker, colony_idx)) {
                    results->info.is_mind_controlled = 1;
                    invasion::Change_Colony_Ownership(colony_id, colony, attacker, &results->info);
                    COMBFIND::current_star_has_player[attacker_id].colony_count++;
                    COMBFIND::current_star_has_player[colony_owner].colony_count--;
                } else {
                    const auto attackers = active_keys();
                    if (attackers.empty()) return;
                    bool has_stellar = has_stellar_converter(attackers);
                    int16_t enemy_star_idx = game::Star_Row(current_state, current_state.planets.at(colony->planet_id).star_id);
                    bool knows_attacker = false;
                    uint8_t surrendered = 0;
                    uint8_t terminal_destroyed = 0;

                    for (int i = 0; i < colony->n_pops; ++i) {
                        if (POP::Group_Player(colony->pop[i]) == attacker) {
                            knows_attacker = true;
                            break;
                        }
                    }

                    memset(&results->bomb, 0, sizeof(s_combat_bomb_result));
                    results->bomb.attacker = factions::From_Legacy(attacker_id);
                    results->bomb.colony_id = colony_id;
                    COMBAT::Bombardment(&results->bomb);

                    int bomb_hits = results->bomb.bomb_hits_remaining;
                    int bio_hits = results->bomb.bio_hits_remaining;

                    if (bomb_hits != 0 || bio_hits != 0 || has_stellar) {
                        bool surrender = false;
                        if (game::current_state.players[colony_owner].objectives != PLAYER_OBJECTIVE_HUMAN && MOX::settings.game_difficulty > GAME_DIFFICULTY_AVERAGE) {
                            if (COLCALC::May_Surrender_Star(game::current_state, game::Star_Key_At_Row(game::current_state, enemy_star_idx), defender, attacker) && (int)(colony->n_pops * 2) <= bomb_hits) {
                                surrender = true;
                            }
                        }

                        if (surrender) {
                            surrendered = 1;
                            if (attacker_id >= 0 && attacker_id < MAX_PLAYERS) {
                                game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).surrender_to[colony_owner] =
                                    game::star_surrender_order::Proposal(attacker);
                                diplomac::Declare_Peace(defender, attacker);
                            }
                        } else if (knows_attacker) {
                            bool success = false;
                            bool done = false;
                            do {
                                if (aibomb::AI_Invasion_Will_Succeed(attacker, attackers, colony_id, colony)) {
                                    success = true;
                                } else {
                                    if (colony->n_pops == 1 && POP::Group_Player(colony->pop[0]) == attacker &&
                                        game::current_state.players[attacker_id].personality != PLAYER_PERSONALITY_RUTHLESS && (COMBFIND::Player_Owns_Transports(attacker, star_idx) || (game::current_state.players[attacker_id].traits[TRAIT_TELEPATHIC] != 0 && game::current_state.players[colony_owner].traits[TRAIT_TELEPATHIC] == 0))) {
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
                            int stars_in_range = N_Enemy_Stars_In_Range(attacker, colony->owner);
                            if (stars_in_range < 2 && N_Enemy_Stars(colony->owner) != 1) {
                                bool success = false;
                                bool done = false;
                                do {
                                    if (aibomb::AI_Invasion_Will_Succeed(attacker, attackers, colony_id, colony)) {
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
                                BOMB::Destroy_Colony(current_state, colony_id, 2);
                            } else {
                                BOMB::Apply_Bomb_Hits(&results->bomb);
                            }
                        }

                        if (results->bomb.bio_pop_killed != 0) {
                            COMBINIT::Apply_Bio_Weapon_Penalty(attacker, star_idx, results->bomb.bio_pop_killed);
                        }

                        if (game::current_state.players[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
                            (MOX::settings.strategic_combat_flag == 0 || (MOX::game_type != 2 && MOX::game_type != 3) ||
                                game::current_state.players[colony_owner].network_player_id == NETMOX::net.local_net_id) &&
                            results->bomb.damage_occurred != 0) {
                            COLBOMB::Colony_Bombing_Screen(&results->bomb);
                        }

                        NETMOX::Russ_Change_Relations(-10000, attacker, defender, 10, star_idx, results->bomb.population_killed);

                        if (results->bomb.colony_wiped_out != 0) {
                            terminal_destroyed = 1;
                            BOMB::Destroy_Colony(current_state, colony_id, 1);
                        }
                    }

                    colony = find_target();
                    const auto remaining = active_keys();
                    if (!terminal_destroyed && !surrendered && colony && legacy_world::Encode_Player_Slot(colony->owner) == colony_owner &&
                        invasion::Invade(attacker, remaining, colony_idx, &results->info)) {
                        if (game::current_state.players[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
                            (MOX::settings.strategic_combat_flag == 0 || (MOX::game_type != 2 && MOX::game_type != 3) ||
                                game::current_state.players[colony_owner].network_player_id == NETMOX::net.local_net_id)) {
                            int invade_seed = game_random::Get_Random_Seed();
                            ERIC::Force_Fade_To_Colony_Palette(1);
                            COLGCBT::Colony_Combat_Screen(&results->info.attacker_invasion, &results->info.defender_invasion, results->info.ground_defense_strength, colony_idx);
                            ERIC::Force_Fade_To_Colony_Palette(0);
                            game_random::Set_Random_Seed(invade_seed);
                        }

                        NETMOX::Russ_Change_Relations(-10000, attacker, defender, 10, star_idx, 0);

                        if (const auto* target = find_target(); target && legacy_world::Encode_Player_Slot(target->owner) == attacker_id) {
                            COMBFIND::current_star_has_player[attacker_id].colony_count++;
                            COMBFIND::current_star_has_player[colony_owner].colony_count--;
                        }

                        if (game::current_state.players[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN || game::current_state.players[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                            COMBFIND::messed_up_fleet_palette = 1;
                        }
                    }
                }
            }
        } else {
            if (attacker_id > 9 && attacker_id < 15) {
                NETMOX::Russ_Event_Monster_Update(attacking_owner, colony->planet_id);
            }

            memset(&results->bomb, 0, sizeof(s_combat_bomb_result));
            results->bomb.attacker = factions::From_Legacy(attacker_id);
            results->bomb.colony_id = colony_id;
            if (attacker_id == 10) { // Amoeba
                results->bomb.bomb_hits_remaining = 20;
                results->bomb.bio_hits_remaining = 0;
            }

            COMBAT::Bombardment(&results->bomb);
            BOMB::Apply_Bomb_Hits(&results->bomb);

            const bool amoeba_attacked_colony =
                attacker_id == NONPLAYER_SHIP_TYPE_AMOEBA;

            if (amoeba_attacked_colony) {
                current_state.planets.at(planet_idx).climate = PLANET_CLIMATE_TOXIC;
                current_state.planets.at(planet_idx).food_per_farmer = 0;
                if (results->bomb.colony_wiped_out == 0) {
                    COLCALC::Colony_Environmental_Stuff(colony);
                    invasion::Enforce_Population_Limits_At_Colony(colony);
                    COLCALC::Colony_Calculation(colony_idx);
                }
            }

            if (game::current_state.players[colony_owner].objectives == PLAYER_OBJECTIVE_HUMAN &&
                (MOX::settings.strategic_combat_flag == 0 || (MOX::game_type != 2 && MOX::game_type != 3) ||
                    game::current_state.players[colony_owner].network_player_id == NETMOX::net.local_net_id) &&
                results->bomb.damage_occurred != 0) {
                if (attacker_id == NONPLAYER_SHIP_TYPE_AMOEBA) {
                    ERIC::Play_Amoeba_Flic();
                    COMBFIND::messed_up_fleet_palette = 1;
                }
                COLBOMB::Colony_Bombing_Screen(&results->bomb);
            }

            if (amoeba_attacked_colony &&
                Wandering_Monster_Retires(attacker_id, true)) {
                for (const auto key : keys) if (auto* ship = find_attacker(key)) SHIPS::Kill_Ship(*ship);
                NETMOX::Russ_Event_Monster_Update(game::owner_slot{NONPLAYER_SHIP_TYPE_AMOEBA}, game::NO_PLANET);
            }

            if (results->bomb.colony_wiped_out != 0) {
                BOMB::Destroy_Colony(current_state, colony_id, 1);
            }
        }

        if (attacker_id == 8) { // Antarans
            for (const auto key : keys) {
                if (auto* ship = find_attacker(key)) {
                    antarans::Restock_Antaran_Ship(ship->d.size);
                    SHIPS::Kill_Ship(*ship);
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
                battle_results->info.orion_techs,
                battle_results->info.rescued_hero_id);
        }

        MULTPLAY::Load_Net_Next_Turn_Screen(0);
    }

    void __cdecl Bomb_Results_Popups(s_battle_results* res) {
        if (res->outpost_destroyed_flag == 0) {
            if (res->info.planet_destroyed_flag == 0) {
                if (res->bomb.damage_occurred != 0) {
                    if (res->bomb.attacker.get().code == 10) {
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
                current_state.planets.at(res->player_info.combat_planet_id.get()).colony_id);

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

    int32_t __cdecl Do_Bombardment(game::owner_slot attacker, game::id_colony colony_idx) {
        s_combat_bomb_result result;

        memset(&result, 0, sizeof(s_combat_bomb_result));

        result.attacker = attacker;
        result.colony_id = colony_idx;

        COMBAT::Bombardment(&result);

        return result.bomb_hits_remaining + result.bio_hits_remaining;
    }

    int32_t __cdecl N_Enemy_Stars_In_Range(game::id_player player, game::id_player enemy) {
        int32_t count = 0;

        for (auto& [key, star] : current_state.stars) {
            if (star.has_colony.test(enemy)) {
                if (SHIPMOVE::Star_In_Normal_Range_Of_Player(current_state, star, player)) {
                    count++;
                }
            }
        }

        return count;
    }

    int32_t __cdecl N_Enemy_Stars(game::id_player player) {
        int32_t count = 0;

        for (const auto& [key, star] : current_state.stars) {
            if (star.has_colony.test(player)) {
                count++;
            }
        }

        return count;
    }

    bool Player_Owns_Transports(game::id_player player, int16_t star_idx) {
        if (player.id >= MAX_PLAYERS) return false;
        const auto owner = factions::From_Player(player);
        const auto is_owned_transport = [&](const s_ship_data& ship) {
            return ship.owner == owner && ship.d.ship_type == SHIP_TYPE_TRANSPORT && ship.status < 5;
        };
        if (const auto* order = game::Received_Combat_Order(current_state, star_idx)) {
            return std::ranges::any_of(order->keys, [&](game::id_ship key) {
                const auto ship_entry = current_state.ships.find(key);
                const auto* ship = ship_entry == current_state.ships.end() ? nullptr : &ship_entry->second;
                return ship && is_owned_transport(*ship);
            });
        }
        return game::Any_Ship(current_state, is_owned_transport);
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

        if (game::current_state.player_count > 0) {
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                RUSS::Mox_Update();
                uint8_t state = NETMOX::net.player_net_info[i].player_state;

                switch (state) {
                    case NET_PLAYER_STATE_COMBAT_TARGET_CHOSEN: {
                        int16_t target_player_id = NETMOX::net.player_net_info[i].combat_target_player_id.get().code;
                        if (target_player_id == -1) {
                            NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
                            for (int16_t j = 0; j < game::current_state.player_count; ++j) {
                                if ((uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id.get().code == i) {
                                    RUSS::Mox_Update();
                                    NETMOX::net.player_net_info[j].combat_attacker_id = game::NO_OWNER;
                                }
                            }
                            NETMOX::net.sync_phase_active_flag = 0;
                            if (auto star = current_state.stars.find(NETMOX::net.player_net_info[i].combat_destination.star_id.get()); star != current_state.stars.end())
                                star->second.ignore_combat_ships.set_slot(i);
                        } else {
                            if (auto star = current_state.stars.find(NETMOX::net.player_net_info[i].combat_destination.star_id.get()); star != current_state.stars.end())
                                star->second.ignore_combat_ships.reset();

                            NETMOX::net.player_net_info[target_player_id].combat_defender_id = NETMOX::net.player_net_info[i].combat_defender_id;
                            NETMOX::net.player_net_info[target_player_id].combat_planet_id = NETMOX::net.player_net_info[i].combat_planet_id;
                            NETMOX::net.player_net_info[target_player_id].combat_target_player_id = NETMOX::net.player_net_info[i].combat_target_player_id;

                            for (int16_t j = 0; j < game::current_state.player_count; ++j) {
                                if (j != i && j != target_player_id && (uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id.get().code == i) {
                                    RUSS::Mox_Update();
                                    NETMOX::net.player_net_info[j].combat_attacker_id = game::NO_OWNER;
                                }
                            }

                            GNN_Reports_Combat(factions::From_Player(current_state.players[i].id), factions::From_Legacy(target_player_id));

                            uint8_t msg_type = 0;
                            if (MOX::settings.strategic_combat_flag == 0) {
                                if (target_player_id < MAX_PLAYERS && game::current_state.players[target_player_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                                    NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_TACTICAL_ATTACKER;
                                    NETMOX::net.player_net_info[target_player_id].player_state = NET_PLAYER_STATE_COMBAT_TACTICAL_DEFENDER;
                                    const auto colony_id = Target_Planet_To_Colony(NETMOX::net.player_net_info[i].combat_planet_id.get());
                                    NETMOX::Send_You_Are_Target_Info(
                                        current_state.players[target_player_id].id, factions::From_Player(current_state.players[i].id), NETMOX::net.player_net_info[i].combat_defender_id.get(), colony_id, game::Destination_Row(current_state, NETMOX::net.player_net_info[i].combat_destination));
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
                            NETMOX::Mox_Send_Message((int16_t)game::current_state.players[i].network_player_id, msg_type, nullptr, 0);
                        }
                        break;
                    }
                    case NET_PLAYER_STATE_HUMAN_DIPLOMACY_ACTIVE: {
                        const auto attacker = NETMOX::net.player_net_info[i].combat_attacker_id.get();
                        const auto owner = factions::From_Player(current_state.players[i].id);
                        if (attacker == owner) {
                            Update_Player_Needs_To_Attack(
                                game::Destination_Row(current_state, NETMOX::net.player_net_info[i].combat_destination),
                                owner,
                                NETMOX::net.player_net_info[i].combat_target_player_id.get());
                        } else {
                            Update_Player_Needs_To_Attack(
                                game::Destination_Row(current_state, NETMOX::net.player_net_info[i].combat_destination),
                                attacker,
                                owner);
                        }
                        NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
                        NETMOX::net.player_net_info[i].combat_attacker_id = game::NO_OWNER;
                        NETMOX::net.sync_phase_active_flag = 0;
                        break;
                    }
                    case NET_PLAYER_STATE_HUMAN_DIPLOMACY_IGNORED: {
                        Do_Nonhuman_Attack_Against_Human(current_state.players[i].id);
                        if (const auto position = game::Destination_Row(current_state, NETMOX::net.player_net_info[i].combat_destination); position >= 0)
                            (*star_has_nonplayer)[position] &= 0xef;
                        NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_RESULTS_POPUP;
                        NETMOX::net.sync_phase_active_flag = 0;
                        NETMOX::net.player_net_info[12].combat_attacker_id = game::NO_OWNER;
                        break;
                    }
                    case NET_PLAYER_STATE_DRAGON_BRIBE_DESTINATION: {
                        Do_Dragon_Bribe(
                            current_state.players[i].id,
                            game::Destination_Row(current_state, NETMOX::net.player_net_info[i].combat_destination),
                            game::Star_Row(current_state, NETMOX::net.dragon_bribe_destination_star_id.get()));
                        NETMOX::net.player_net_info[i].combat_attacker_id = game::NO_OWNER;
                        NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
                        NETMOX::net.sync_phase_active_flag = 0;
                        NETMOX::net.player_net_info[12].combat_attacker_id = game::NO_OWNER;
                        break;
                    }
                }
            }
        }

        if (n_battle_stars == 0) {
            bool waiting_for_combat_ready_player = false;
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                if (game::current_state.players[i].objectives == PLAYER_OBJECTIVE_HUMAN && NETMOX::net.player_net_info[i].player_state != NET_PLAYER_STATE_COMBAT_READY) {
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

        uint8_t state = NETMOX::net.player_net_info[MOX::Legacy_Local_Player_Slot()].player_state;
        if (state == NET_PLAYER_STATE_PICK_TARGET_INFO || NETMOX::net.sync_phase_active_flag != 0) {
            in_net_update = 0;
            RUSS::Mox_Update();
            return;
        }

        int16_t star_id;
        auto attacker = game::NO_OWNER;
        Pick_Star_And_Attacker(&star_id, &attacker);
        const auto attacker_id = attacker.code;

        if (star_id == -1) {
            NETMOX::net.sync_phase_active_flag = 1;
            in_net_update = 0;
            RUSS::Mox_Update();
            return;
        }

        int8_t is_sneak_attack = 0;
        auto target_defender = game::NO_OWNER;
        game::id_colony target_colony_idx = game::NO_COLONY;

        for (int16_t j = 0; j < target.num_player_targets; ++j) {
            const auto target_player = target.faction_targets[j].get().code;
            NETMOX::net.player_net_info[target_player].combat_destination = game::Destination_At_Row(current_state, star_id);
            NETMOX::net.player_net_info[target_player].combat_attacker_id = factions::From_Legacy(attacker_id);
        }

        for (int16_t j = 0; j < target.num_colony_targets; ++j) {
            game::id_colony target_colony = target.colony_targets[j].get();
            int8_t owner = legacy_world::Encode_Player_Slot(game::current_state.colonies.at(target_colony).owner);
            NETMOX::net.player_net_info[owner].combat_destination = game::Destination_At_Row(current_state, star_id);
            NETMOX::net.player_net_info[owner].combat_attacker_id = factions::From_Legacy(attacker_id);
        }

        NETMOX::net.player_net_info[attacker_id].combat_destination = game::Destination_At_Row(current_state, star_id);
        NETMOX::net.player_net_info[attacker_id].combat_attacker_id = factions::From_Legacy(attacker_id);

        if (attacker_id < MAX_PLAYERS) {
            if (game::current_state.players[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if ((game::Destination_Record_At_Row(current_state, star_id).ignore_combat_ships.test_slot(attacker_id)) == 0) {
                    NETMOX::net.player_net_info[attacker_id].player_state = NET_PLAYER_STATE_PICK_TARGET_INFO;
                    NETMOX::Send_Pick_Target_Info(current_state.players[attacker_id].id, star_id, &target);
                    in_net_update = 0;
                    RUSS::Mox_Update();
                    return;
                }
            }
        }

        int16_t target_sync_owner_id;
        if (attacker_id < MAX_PLAYERS && game::current_state.players[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
            target_sync_owner_id = -1;
        } else {
            if (attacker_id < MAX_PLAYERS) {
                Get_AI_Target(star_id, current_state.players[attacker_id].id, &target_defender, &target_colony_idx, &is_sneak_attack);
            } else {
                Get_NPC_Target(star_id, attacker, &target_defender, &target_colony_idx);
            }
            if (target_colony_idx != game::NO_COLONY) {
                target_sync_owner_id = legacy_world::Encode_Player_Slot(game::current_state.colonies.at(target_colony_idx).owner);
                game::current_state.players.at(target_sync_owner_id).last_attacker = game::owner_slot{attacker_id};
            } else {
                target_sync_owner_id = target_defender.code;
            }
        }

        NETMOX::net.player_net_info[attacker_id].combat_defender_id = target_defender;
        const auto target_planet_id = Target_Colony_To_Planet(target_colony_idx);
        NETMOX::net.player_net_info[attacker_id].combat_planet_id = target_planet_id;
        NETMOX::net.player_net_info[attacker_id].combat_target_player_id = factions::From_Legacy(target_sync_owner_id);

        if (target_sync_owner_id == -1) {
            NETMOX::net.sync_phase_active_flag = 0;
            for (int16_t j = 0; j < game::current_state.player_count; ++j) {
                if ((uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id.get().code == attacker_id &&
                    NETMOX::net.player_net_info[j].combat_destination == game::Destination_At_Row(current_state, star_id)) {
                    RUSS::Mox_Update();
                    NETMOX::net.player_net_info[j].combat_attacker_id = game::NO_OWNER;
                }
            }
        } else {
            game::Destination_Record_At_Row(current_state, star_id).ignore_combat_ships.reset();
            NETMOX::net.player_net_info[target_sync_owner_id].combat_defender_id = NETMOX::net.player_net_info[attacker_id].combat_defender_id;
            NETMOX::net.player_net_info[target_sync_owner_id].combat_planet_id = NETMOX::net.player_net_info[attacker_id].combat_planet_id;
            NETMOX::net.player_net_info[target_sync_owner_id].combat_target_player_id = NETMOX::net.player_net_info[attacker_id].combat_target_player_id;

            for (int16_t j = 0; j < game::current_state.player_count; ++j) {
                if (j != attacker_id &&
                    j != target_sync_owner_id &&
                    (uint8_t)NETMOX::net.player_net_info[j].combat_attacker_id.get().code == attacker_id &&
                    NETMOX::net.player_net_info[j].combat_destination == game::Destination_At_Row(current_state, star_id)) {
                    RUSS::Mox_Update();
                    NETMOX::net.player_net_info[j].combat_attacker_id = game::NO_OWNER;
                }
            }

            if (target_sync_owner_id < MAX_PLAYERS && game::current_state.players[target_sync_owner_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (attacker_id == 12 && target_colony_idx != game::NO_COLONY) {
                    NETMOX::net.player_net_info[target_sync_owner_id].player_state = NET_PLAYER_STATE_HUMAN_DIPLOMACY_READY_DATA;
                    NETMOX::Send_Dragon_Info(target_colony_idx);
                } else {
                    Do_Nonhuman_Attack_Against_Human(current_state.players[target_sync_owner_id].id);
                }
            } else {
                const auto target_planet_id = Target_Colony_To_Planet(target_colony_idx);
                Do_1_Combat(star_id, attacker, target_defender, target_planet_id, nullptr, 0);
                Update_Player_Needs_To_Attack(star_id, attacker, factions::From_Legacy(target_sync_owner_id));
                NETMOX::net.sync_phase_active_flag = 0;
                NETMOX::net.player_net_info[attacker_id].combat_attacker_id = game::NO_OWNER;
                NETMOX::net.player_net_info[target_sync_owner_id].combat_attacker_id = game::NO_OWNER;
            }
        }

        in_net_update = 0;
        RUSS::Mox_Update();
    }

    void __cdecl Do_Nonhuman_Attack_Against_Human(game::id_player player) {
        const auto player_idx = player.id;
        s_battle_results battle_results;

        int8_t attacker_id = (int8_t)NETMOX::net.player_net_info[player_idx].combat_attacker_id.get().code;
        int16_t combat_star_id = game::Destination_Row(current_state, NETMOX::net.player_net_info[player_idx].combat_destination);

        const auto attacker = NETMOX::net.player_net_info[player_idx].combat_attacker_id.get();
        const auto defender = factions::From_Player(player);
        GNN_Reports_Combat(attacker, defender);

        if (MOX::settings.strategic_combat_flag == 0) {
            const auto colony_id = Target_Planet_To_Colony(NETMOX::net.player_net_info[player_idx].combat_planet_id.get());

            NETMOX::Send_You_Are_Target_Info(
                player,
                NETMOX::net.player_net_info[player_idx].combat_attacker_id.get(),
                NETMOX::net.player_net_info[player_idx].combat_defender_id.get(),
                colony_id,
                combat_star_id);

            NETMOX::net.player_net_info[player_idx].player_state = NET_PLAYER_STATE_BATTLE_RESULTS_ACK;
            return;
        }

        battle_results = {};
        battle_results.player_info = NETMOX::net.player_net_info[player_idx];

        battle_results.planet_id = battle_results.player_info.combat_planet_id;

        battle_results.player_info.combat_result = (int8_t)Do_1_Combat(
            combat_star_id,
            factions::From_Legacy(attacker_id),
            NETMOX::net.player_net_info[player_idx].combat_defender_id.get(),
            NETMOX::net.player_net_info[player_idx].combat_planet_id.get(),
            &battle_results,
            0);

        Update_Player_Needs_To_Attack(combat_star_id, attacker, defender);

        NETMOX::net.sync_phase_active_flag = 0;
        NETMOX::net.player_net_info[attacker_id].combat_attacker_id = game::NO_OWNER;
        NETMOX::net.player_net_info[player_idx].player_state = NET_PLAYER_STATE_COMBAT_RESULTS_POPUP;

        Send_Strategic_Results(
            game::current_state.players[player_idx].network_player_id,
            NET_PACKET_STRATEGIC_BATTLE_RESULTS,
            battle_results);
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

    void __cdecl GNN_Reports_Combat(game::owner_slot first, game::owner_slot second) {
        char buf[200];
        if (game_random::Random(2) != 1) std::swap(first, second);

        const char* name2 = COLCALC::Combatant_Name(second);
        const char* name1 = COLCALC::Combatant_Name(first);

        snprintf(buf, sizeof(buf), combat_is_reported_between_s_and_s_msg, name1, name2);

        CHAT::Send_GNN_Chat_Msg(buf);
    }

} // namespace COMBFIND
