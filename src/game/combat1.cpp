#include "pch.h"

#include "config/config_loader.h"
#include "game/boarding.h"
#include "tactical_events.h"
#include "tactical_view.h"
#include "tactical_control.h"

namespace COMBAT1 {
    static int16_t movement_reserve[MAX_COMBAT_SHIPS] = {};
    static bool strict_wait_stage;
    static bool strict_ship_waited[MAX_COMBAT_SHIPS];
    static int16_t strict_initial_order[MAX_COMBAT_SHIPS];
    static bool time_warp_phase_active;
    static bool side_waiting[15];
    static int16_t side_wait_button;
    static int16_t reload_config_button;
    static int16_t reload_interface_button;

    static int16_t Advance_Next_Ship(
        int16_t active_player_id, COMBAT_SHIP_ACTION action);
    static int16_t Finish_Next_Ship(int16_t result);

    static bool Process_Interface_Reload_Input(int16_t input_value) {
        const bool reload_all = input_value == reload_config_button;
        if (!reload_all && input_value != reload_interface_button) {
            return false;
        }
        tactical_control::Close_Decision();

        config::s_cfg_diagnostic diagnostic;
        const bool network_game = MOX::game_type == 2 || MOX::game_type == 3;
        if (reload_all && network_game) {
            GENDRAW::Warning_Box(
                "You may not reload config in a multiplayer game.");
            return true;
        }
        if (!(reload_all
                    ? config::Reload_All_Active_Config_Groups(&diagnostic)
                    : config::Reload_Active_Config_Groups(
                          network_game, &diagnostic))) {
            GENDRAW::Warning_Box(diagnostic.message.c_str());
        } else if (reload_all) {
            GENDRAW::Message_Box("config reloaded");
        }
        return true;
    }

    static int16_t Get_Tactical_Input() {
        for (;;) {
            tactical_events::Service_Reads(true);
            const int16_t input_value = fields::Get_Input();
            if (!Process_Interface_Reload_Input(input_value)) {
                return input_value;
            }
        }
    }

    static int16_t Maximum_Combat_Speed(int16_t ship_idx) {
        s_combat_data& ship = MOX::combat_data[ship_idx];
        int16_t speed = (uint8_t)ship.combat_speed_base;

        if (struct_::Test_Bit_Field(
                ship.special_device_flags, SPECIAL_AUGMENTED_ENGINES) != 0) {
            speed += DESIGN::augmented_engines_speed_bonus;
        }

        if (ship.owner >= 0 && ship.owner < MAX_PLAYERS &&
            MOX::player[ship.owner].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
            speed += td_combat_speed_bonus;
        }

        return speed;
    }

    int32_t end_of_turn_button;

    uint16_t start_of_turn;

    static bool Is_Human_Combat_Owner(int8_t owner) {
        return owner >= 0 &&
            owner < MAX_PLAYERS &&
            owner < MOX::NUM_PLAYERS &&
            MOX::player[owner].objectives == PLAYER_OBJECTIVE_HUMAN;
    }

    static int Compare_Combat_Ships(int16_t ship1_idx, int16_t ship2_idx) {
        if (ship1_idx < 0 || ship2_idx < 0) {
            return ship1_idx - ship2_idx;
        }

        const s_combat_data* ship1 = &MOX::combat_data[ship1_idx];
        const s_combat_data* ship2 = &MOX::combat_data[ship2_idx];
        const int initiative1 = ship1->tac_val_attack + ship1->current_speed * 10;
        const int initiative2 = ship2->tac_val_attack + ship2->current_speed * 10;
        if (initiative1 != initiative2) {
            return initiative2 - initiative1;
        }

        const bool monster1 = ship1->owner > MAX_PLAYERS;
        const bool monster2 = ship2->owner > MAX_PLAYERS;
        if (monster1 != monster2) {
            return monster1 ? -1 : 1;
        }
        if (ship1->crew_quality != ship2->crew_quality) {
            return ship2->crew_quality - ship1->crew_quality;
        }

        const bool combat_only1 = ship1->ship_idx < 0;
        const bool combat_only2 = ship2->ship_idx < 0;
        if (combat_only1 && combat_only2) {
            return ship2_idx - ship1_idx;
        }
        if (combat_only1 != combat_only2) {
            return combat_only1 ? -1 : 1;
        }

        const int experience1 = MOX::ship[ship1->ship_idx].crew_experience;
        const int experience2 = MOX::ship[ship2->ship_idx].crew_experience;
        if (experience1 != experience2) {
            return experience2 - experience1;
        }
        return ship1->ship_idx - ship2->ship_idx;
    }

    void Recalculate_Combat_Ship(int16_t ship_idx, bool tractor_applied) {
        if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
            return;
        }

        s_combat_data& ship = MOX::combat_data[ship_idx];
        const int16_t old_speed = ship.current_speed;
        const int16_t new_speed = Calc_Current_Speed(ship_idx);
        ship.current_speed = new_speed;

        if (interactive_combat_stats != 0) {
            ship.movement_left = Adjust_Movement_After_Speed_Change(
                old_speed,
                new_speed,
                Maximum_Combat_Speed(ship_idx),
                ship.movement_left,
                &movement_reserve[ship_idx]);
            ship.tac_val_attack = CMBTFIR2::Offensive_Combat_Bonus(ship_idx);
            ship.tac_val_defense = CMBTFIR2::Defensive_Combat_Bonus(ship_idx, 0);
            ship.tac_val_evade = CMBTFIR2::Defensive_Combat_Bonus(ship_idx, 1);
        } else if (non_ics_tractor_and_bhg_mode != 0 &&
            ship.movement_left > new_speed &&
            (tractor_applied || ship.black_hole_source_idx != COMBAT_SHIP_NONE)) {
            ship.movement_left = new_speed;
        }
    }

    void __cdecl Add_Tactical_Combat_Fields(void) {
        reload_config_button = -100;
        reload_interface_button = -100;
        int16_t auto_combat_idx = 0;
        const char* info_hotkey;
        int16_t info_y_coord;

        if (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count) {
            if (MOX::combat_data[MOX::cur_ship].owner != MOX::combat_attacker_player) {
                auto_combat_idx = 1;
            }
        }

        Set_Combat_Help_Fields();

        if (MOX::auto_combat_flag[auto_combat_idx] == 1 ||
            (MOX::game_type != 1 && MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count &&
                MOX::combat_data[MOX::cur_ship].owner != MOX::PLAYER_NUM &&
                MOX::take_control_flag == 0)) {
            for (int i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                MOX::missile_field[i] = -1000;
            }
            MOX::take_control_button = -1000;
            MOX::space_field = -1000;
            MOX::map_field = -1000;
            MOX::msl_warning_button = -1000;
            MOX::speedx2_button = -100;
            MOX::draw_legal_boxes_button = -100;
            MOX::msl_impact_warning_button = -100;
            MOX::self_destruct_button = -100;
            MOX::shields_button = -100;
            MOX::grid_button = -100;
            MOX::scan_button = -100;
            COMBAT1::end_of_turn_button = -100;
            MOX::board_button = -100;
            MOX::flee_button = -100;
            MOX::wait_button = -100;
            side_wait_button = -100;
            MOX::done_button = -100;
            for (int i = 0; i < 8; ++i) {
                MOX::weapon_button[i] = -1000;
            }
            MOX::center_button = -1000;
            MOX::weapons_button = -1000;
            MOX::specials_button = -1000;
            MOX::cmbt_info_button = -1000;
            MOX::super_fast_button = -1000;
            return;
        }

        fields::Clear_Fields();
        reload_config_button = fields::Add_Multi_Hot_Key_Field("RE");
        reload_interface_button = fields::Add_Multi_Hot_Key_Field("RI");

        if (MOX::order_info_flag == 1) {
            Add_Missile_Fields();
            MOX::take_control_button = fields::Add_Hot_Key("[");
            MOX::space_field = fields::Add_Grid_Field(0, 0, 10, 10, 64, 36, &MOX::xpos, &MOX::ypos, 0);

            if (MOX::missile_warning == 1) {
                MOX::msl_warning_button = fields::Add_Hidden_Field(505, 370, 622, 464, "", 0);
                MOX::map_field = -1000;
            } else {
                MOX::map_field = fields::Add_Grid_Field(500, 364, 1, 1, 121, 102, &MOX::map_x, &MOX::map_y, 0);
                MOX::msl_warning_button = -1000;
            }

            MOX::weapons_button = fields::Add_Multi_Button_Field(114, 361, "", (s_animation_header*)MOX::weapons_button_seg, &MOX::ship_status_display, 0, "", 41);
            MOX::specials_button = fields::Add_Multi_Button_Field(187, 361, "", (s_animation_header*)MOX::specials_button_seg, &MOX::ship_status_display, 1, "", 41);
            MOX::auto_button = fields::Add_Radio_Button_Field(273, 364, "", (s_animation_header*)MOX::auto_button_seg, &MOX::auto_cmbt_button_value, "a", 40);
            MOX::scan_button = disable_scan == 0
                ? fields::Add_Radio_Button_Field(337, 364, "", (s_animation_header*)MOX::scan_button_seg, &MOX::skan_flag, "s", 40)
                : -100;
            MOX::board_button = fields::Add_Radio_Button_Field(273, 392, "", (s_animation_header*)MOX::board_button_seg, &MOX::board_flag, "b", 40);
            MOX::flee_button = fields::Add_Button_Field(337, 392, "", (s_animation_header*)MOX::flee_button_seg, "r", 40);
            if (ship_initiative == 2 && strict_wait_stage) {
                MOX::wait_button = -100;
            } else {
                MOX::wait_button = fields::Add_Button_Field(273, 420, "", (s_animation_header*)MOX::wait_button_seg, "w", 40);
            }
            side_wait_button = Add_Combat_Side_Wait_Field();
            MOX::done_button = fields::Add_Button_Field(337, 420, "", (s_animation_header*)MOX::cmbt_done_button_seg, "d", 40);
            COMBAT1::end_of_turn_button = (int)fields::Add_Hot_Key("t");

            MOX::draw_legal_boxes_button = -100;
            MOX::msl_impact_warning_button = -100;
            MOX::speedx2_button = -100;
            MOX::shields_button = -100;
            MOX::grid_button = -100;
            MOX::self_destruct_button = -100;

            if (MOX::combat_attacker_player < MAX_PLAYERS && MOX::player[MOX::combat_attacker_player].objectives == PLAYER_OBJECTIVE_HUMAN &&
                MOX::combat_defender_player < MAX_PLAYERS && MOX::player[MOX::combat_defender_player].objectives == PLAYER_OBJECTIVE_HUMAN) {
                MOX::super_fast_button = -1000;
            } else {
                MOX::super_fast_button = fields::Add_Hot_Key("z");
            }

            info_hotkey = "O";
            info_y_coord = 448;
        } else {
            for (int i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                MOX::missile_field[i] = -1000;
            }
            MOX::take_control_button = -100;
            MOX::space_field = -100;
            MOX::map_field = fields::Add_Grid_Field(500, 364, 1, 1, 121, 102, &MOX::map_x, &MOX::map_y, 0);
            MOX::msl_warning_button = -100;
            MOX::weapons_button = -100;
            MOX::specials_button = -100;
            MOX::msl_impact_warning_button = fields::Add_Hidden_Field(264, 383, 407, 396, "", 41);
            MOX::speedx2_button = fields::Add_Hidden_Field(264, 399, 407, 412, "", 41);
            MOX::draw_legal_boxes_button = fields::Add_Hidden_Field(264, 415, 407, 428, "", 41);
            MOX::shields_button = fields::Add_Hidden_Field(264, 431, 407, 444, "", 41);
            MOX::grid_button = fields::Add_Hidden_Field(264, 447, 407, 460, "", 41);
            MOX::self_destruct_button = fields::Add_Button_Field(283, 348, "", (s_animation_header*)MOX::self_destruct_seg, "", 40);
            MOX::auto_button = -100;
            MOX::scan_button = -100;
            COMBAT1::end_of_turn_button = -100;
            MOX::super_fast_button = -1000;
            MOX::board_button = -100;
            MOX::flee_button = -100;
            MOX::wait_button = -100;
            side_wait_button = -100;
            MOX::done_button = -100;
            info_hotkey = "\x1B";
            info_y_coord = 301;
        }

        MOX::cmbt_info_button = fields::Add_Button_Field(298, info_y_coord, "", (s_animation_header*)MOX::cmbt_info_button_seg, info_hotkey, 40);

        int16_t weapon_y = 389;
        for (int i = 0; i < 8; ++i) {
            MOX::weapon_button[i] = fields::Add_Hidden_Field(120, weapon_y - 10, 255, weapon_y, "", 41);
            weapon_y += 11;
        }

        MOX::center_button = fields::Add_Hidden_Field(15, 379, 100, 460, "c", 41);
    }

    int32_t __cdecl Send_Tactical_Combat_Message(int16_t player_idx, int16_t message_code, int16_t arg1, int16_t arg2, int16_t arg3) {
        s_net_tactical_combat_message combat_msg;

        combat_msg.viewport_x = MOX::cur_combat_x;
        combat_msg.viewport_y = MOX::cur_combat_y;
        combat_msg.random_seed = game_random::Get_Random_Seed();
        combat_msg.message_code = message_code;
        combat_msg.arg1 = arg1;
        combat_msg.arg2 = arg2;
        combat_msg.arg3 = arg3;

        int16_t net_id = MOX::player[player_idx].network_player_id;

        NETMOX::Net_Debug_Log(
            "tactical action send local_player=%d target_player=%d target_net_id=%d action=%d args=%d,%d,%d turn=%d cur_ship=%d cur_ptr=%d",
            (int)MOX::PLAYER_NUM,
            (int)player_idx,
            (int)net_id,
            (int)message_code,
            arg1,
            arg2,
            arg3,
            (int)MOX::combat_turn,
            MOX::cur_ship,
            (int)MOX::cur_ptr);
        return NETMOX::Mox_Send_Message(net_id, TACTICAL_NET_PACKET_ACTION, &combat_msg, sizeof(combat_msg));
    }

    int32_t Send_Combat_Side_Wait_Message(int16_t player_idx) {
        return Send_Tactical_Combat_Message(
            player_idx, TACTICAL_NET_MSG_SIDE_WAIT, 0, 0, 0);
    }

    int16_t __cdecl Ship_Controlled_By_Computer(int16_t ship_idx) {
        int16_t is_computer = 0;
        int8_t owner = MOX::combat_data[ship_idx].owner;

        if (((owner >= MAX_PLAYERS) || (MOX::player[owner].objectives != PLAYER_OBJECTIVE_HUMAN) || (MOX::auto_combat == 1)) &&
            (MOX::take_control_flag == 0)) {
            is_computer = 1;
        }

        return is_computer;
    }

    int16_t __cdecl Combat_Ship_Is_Functional(int16_t ship_idx) {
        int16_t is_functional = 0;

        if (MOX::combat_data[ship_idx].unit_status == 0 &&
            MOX::combat_data[ship_idx].is_captured == 0) {
            if (!CMBTMOV1::Ship_Is_In_Stasis(ship_idx)) {
                is_functional = 1;
            }
        }

        return is_functional;
    }

    bool Combat_Ship_Participates_In_Pass(int16_t ship_idx, bool time_warp_only) {
        return Combat_Ship_Is_Functional(ship_idx) != 0 &&
            (!time_warp_only ||
                COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_TIME_WARP_FACILITATOR) != 0);
    }

    bool Combat_Ship_Participates_In_Current_Pass(int16_t ship_idx) {
        return Combat_Ship_Participates_In_Pass(
            ship_idx, time_warp_phase_active);
    }

    bool Combat_Ship_Participates_In_Turn_Order(
        int16_t ship_idx, bool time_warp_only) {
        const s_combat_data* ship = &MOX::combat_data[ship_idx];
        return ship->unit_status == 0 && ship->is_captured == 0 &&
            (MOX::combat_init != 0 ||
                CMBTMOV1::Ship_Is_In_Stasis(ship_idx) == 0) &&
            (!time_warp_only ||
                COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_TIME_WARP_FACILITATOR) != 0);
    }

    static void Process_Retreating_Ships(int16_t active_player_id, int16_t time_warp_only) {
        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (!Combat_Ship_Participates_In_Pass(i, time_warp_only != 0)) {
                continue;
            }

            s_combat_data* ship = &MOX::combat_data[i];
            if ((MOX::combat_init == 0 && ship->owner != active_player_id) ||
                ship->is_retreating != 1) {
                continue;
            }

            if (ship->current_speed <= 0 || Enemy_Dissipator_Exists(ship->owner) != 0) {
                ship->is_retreating = 0;
                continue;
            }

            if (MOX::net_flag != 0 && MOX::combat_init != 0) {
                NETMOX::Net_Debug_Log(
                    "Wait_Until_Net_Opponent_Finished: caller=%s:%d",
                    __FILE__,
                    __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }

            MOX::cur_ship = i;
            Retreat_Ship(i);
        }
    }

    static void End_Side_Turn() {
        const int8_t owner = MOX::combat_data[MOX::cur_ship].owner;

        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                break;
            }
            if (ship_idx >= 0 &&
                ship_idx < MOX::combat_total_ship_count &&
                MOX::combat_data[ship_idx].owner == owner) {
                MOX::ship_turn_order[i] = -1;
            }
        }

        MOX::side_end_of_turn[owner] = 1;
        MOX::cur_ship = -1;
    }

    static void Advance_After_Ship_Owner_Change(
        int8_t original_owner,
        int16_t active_player_id,
        int16_t* next_ship_flag) {
        if (MOX::combat_data[MOX::cur_ship].owner == original_owner) {
            return;
        }

        const COMBAT_SHIP_ACTION action = ship_initiative == 2
            ? COMBAT_SHIP_ACTION_NONE
            : COMBAT_SHIP_ACTION_DONE;
        *next_ship_flag = Advance_Next_Ship(active_player_id, action);
    }

    void Cycle_Weapon_Bank(int16_t slot, bool received) {
        tactical_control::Close_Decision();
        if (!received && MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination)
            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_TOGGLE_WEAPON, slot, 0, 0);
        auto& active = MOX::combat_data[MOX::cur_ship].weapons[slot].active;
        --active;
        if (active < -1) active = 1;
        if (MOX::combat_data[MOX::cur_ship].owner == tactical_events::Current_Viewer()) tactical_view::Unit_Changed(MOX::cur_ship);
        if (!received) MOX::last_ship_status = -1;
        if (!received && MOX::net_flag != 0) CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
    }

    void Apply_Tactical_Operation(const tactical_control::s_operation* operation,
        int16_t active_player_id,
        int16_t* next_ship_flag,
        bool received) {
        tactical_control::Close_Decision();
        const int16_t actor = MOX::cur_ship;
        const int8_t owner = MOX::combat_data[actor].owner;
        if (operation->kind == tactical_control::FIRE_SELECTION) {
            for (unsigned i = 0; i < 8; ++i)
                if (operation->bank_mask & (1u << i))
                    while (MOX::combat_data[actor].weapons[i].active != operation->bank_states[i]) Cycle_Weapon_Bank(int16_t(i), received);
            return;
        }
        int16_t message = 0, arg1 = 0, arg2 = 0;
        switch (operation->kind) {
            case tactical_control::SELECT:
                message = TACTICAL_NET_MSG_SELECT_SHIP;
                arg1 = operation->target;
                break;
            case tactical_control::MOVE:
                message = TACTICAL_NET_MSG_MOVE_SHIP;
                arg1 = operation->x;
                arg2 = operation->y;
                break;
            case tactical_control::FACE:
                message = TACTICAL_NET_MSG_TURN_TO_FACE;
                arg1 = operation->x;
                arg2 = operation->y;
                break;
            case tactical_control::FIRE:
                message = TACTICAL_NET_MSG_FIRE_WEAPON;
                arg1 = operation->target;
                arg2 = operation->missile_target ? 1 : 0;
                break;
            case tactical_control::DONE: message = TACTICAL_NET_MSG_SHIP_DONE; break;
            default: return;
        }
        if (!received && MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination)
            Send_Tactical_Combat_Message(MOX::net_destination, message, arg1, arg2, 0);
        switch (operation->kind) {
            case tactical_control::SELECT:
                if (MOX::combat_init == 0) Set_Cur_Ship_To(operation->target);
                break;
            case tactical_control::MOVE: {
                const int16_t cost = CMBTMOV1::Move_Cost(actor, operation->x, operation->y);
                if (MOX::combat_data[actor].movement_left < 0) MOX::combat_data[actor].movement_left = 0;
                const int16_t used = CMBTFIR2::Used_Teleporter(actor);
                const int16_t actual = CMBTMOV1::Move_Ship(actor, operation->x, operation->y, cost);
                if (used == CMBTFIR2::Used_Teleporter(actor)) {
                    MOX::combat_data[actor].movement_left -= actual;
                    if (MOX::combat_data[actor].movement_left < 0) MOX::combat_data[actor].movement_left = 0;
                }
                Assign_Combat_Grids(actor);
                CMBTDRW1::Refresh_Combat_Screen_Full();
                Advance_After_Ship_Owner_Change(owner, active_player_id, next_ship_flag);
                break;
            }
            case tactical_control::FACE:
                CMBTMOV1::Turn_To_Face(actor, operation->x, operation->y);
                Advance_After_Ship_Owner_Change(owner, active_player_id, next_ship_flag);
                break;
            case tactical_control::FIRE:
                CMBTFIRE::Fire_Ship(actor, MOX::combat_data[actor].x * 20, MOX::combat_data[actor].y * 20, operation->target, operation->missile_target ? MOX::missile[operation->target].x : MOX::combat_data[operation->target].x * 20, operation->missile_target ? MOX::missile[operation->target].y : MOX::combat_data[operation->target].y * 20, operation->missile_target ? 1 : 0);
                Add_Tactical_Combat_Fields();
                Assign_Combat_Grids(MOX::cur_ship);
                for (int16_t i = 0; i < 8; ++i)
                    if (MOX::combat_data[MOX::cur_ship].weapons[i].active == 0) MOX::combat_data[MOX::cur_ship].weapons[i].active = 1;
                break;
            case tactical_control::DONE:
                if (!received) CMBTFIR2::Set_Combat_Ship_Done(actor);
                MOX::ship_turn_order[MOX::cur_ptr] = -1;
                *next_ship_flag = Next_Ship(active_player_id);
                break;
            default: break;
        }
        tactical_view::Unit_Changed(actor);
        if (!received && MOX::net_flag != 0) CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
        if (!received && operation->kind == tactical_control::DONE) fields::Set_Input_Delay(3);
    }

    bool Consume_Tactical_Operation(int16_t active_player_id, int16_t* next_ship_flag) {
        tactical_control::s_operation operation;
        if (!tactical_control::Take(&operation)) return false;
        const auto result = tactical_control::Validate(&operation);
        if (result == tactical_control::APPLIED) Apply_Tactical_Operation(&operation, active_player_id, next_ship_flag);
        if (*next_ship_flag != 0) tactical_events::Select_Unit(-1);
        tactical_events::Service_Reads(true);
        tactical_control::Complete(result);
        return true;
    }

    void __cdecl Do_Combat_Turn(int16_t active_player_id, int16_t time_warp_only) {
        struct s_decision_scope {
            ~s_decision_scope() {
                tactical_control::Close_Decision();
            }
        } decision_scope;
        time_warp_phase_active = time_warp_only != 0;
        MOX::player_mode[MOX::combat_attacker_player] = CMBTAI::Get_Player_Mode(MOX::combat_attacker_player);
        MOX::player_mode[MOX::combat_defender_player] = CMBTAI::Get_Player_Mode(MOX::combat_defender_player);

        for (int16_t i = 0; i < 15; ++i) {
            MOX::side_end_of_turn[i] = 0;
        }
        Reset_Combat_Side_Waits();

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Pass(i, time_warp_only != 0)) {
                    if (MOX::combat_init != 0 || MOX::combat_data[i].owner == active_player_id) {
                        Init_Ship_For_Start_Of_Turn(i);
                        tactical_view::Unit_Changed(i);
                    }
                }
            }
        }

        Process_Retreating_Ships(active_player_id, time_warp_only);
        RUSS::Mox_Update();

        int16_t order_idx = 0;
        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Turn_Order(
                        i, time_warp_only != 0)) {
                    if (MOX::combat_init != 0 || MOX::combat_data[i].owner == active_player_id) {
                        MOX::ship_turn_order[order_idx] = i;
                        ++order_idx;
                    }
                }
            }
        }

        if (order_idx == 0) {
            time_warp_phase_active = false;
            return;
        }

        if (order_idx != 0) {
            if (MOX::combat_init != 0) {
                qsort(MOX::ship_turn_order, order_idx, sizeof(MOX::ship_turn_order[0]), qsort_ship_initiative);
            }
            MOX::ship_turn_order[order_idx++] = -10;
            while (order_idx < MAX_COMBAT_SHIPS) {
                MOX::ship_turn_order[order_idx++] = -1;
            }
            Reset_Strict_Combat_Sequence();

            RUSS::Mox_Update();
            COMBAT1::start_of_turn = 1;
            MOX::cur_ptr = -1;
            int16_t next_ship_flag = Advance_Next_Ship(
                active_player_id, COMBAT_SHIP_ACTION_NONE);

            if (next_ship_flag == 1) {
                time_warp_phase_active = false;
                return;
            }

            if (next_ship_flag != 1) {
                fields::Assign_Auto_Function(CMBTDRW1::Draw_Main_Combat_Screen, 2);
                MOX::skan_flag = 0;
                MOX::board_flag = 0;
                RUSS::Mox_Update();

                if (MOX::combat_total_ship_count > 0) {
                    for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                        if (COMBINIT::Is_Combat_Satellite(&MOX::combat_data[i]) &&
                            MOX::combat_data[i].unit_status == 0) {
                            if (CMBTMOV1::Ship_Is_In_Stasis(i) == 0) {
                                CMBTMOV1::Rotate_Ship(i, (MOX::combat_data[i].facing_dir + 2) & 0xf, (MOX::combat_data[i].x - MOX::cur_combat_x) * 20, (MOX::combat_data[i].y - MOX::cur_combat_y) * 20);
                            }
                        }
                    }
                }

                COMBAT1::start_of_turn = 0;

                while (next_ship_flag == 0 &&
                    MOX::ship_turn_order[MOX::cur_ptr] >= 0) {
                    tactical_events::Select_Unit(MOX::cur_ship);
                    tactical_events::Service_Reads(true);
                    timer::Mark_Time();
                    MOX::FULL_DRAW_SCREEN_FLAG = 0;
                    int16_t m_left = MOX::combat_data[MOX::cur_ship].movement_left;

                    if (Ship_Controlled_By_Computer(MOX::cur_ship) != 0 &&
                        (MOX::net_flag == 0 || MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM)) {
                        tactical_control::Close_Decision();
                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_AUTO_TURN, 0, 0, 0);
                        }
                        CMBTAI::Do_Auto_Ship_Turn();
                        tactical_view::Unit_Changed(MOX::cur_ship);
                        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                            Add_Tactical_Combat_Fields();
                            Assign_Combat_Grids(MOX::cur_ship);
                            CMBTDRW1::Refresh_Combat_Screen_Full();
                        }
                        MOX::ship_turn_order[MOX::cur_ptr] = -1;
                        next_ship_flag = Advance_Next_Ship(
                            active_player_id, COMBAT_SHIP_ACTION_AUTOMATIC);
                        if (MOX::net_flag != 0) {
                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                        }
                        tactical_events::Service_Reads(true);
                        timer::Release_Time(1);
                        continue;
                    } else {
                        MOX::cur_ship = MOX::ship_turn_order[MOX::cur_ptr];
                        if (CMBTMOV1::Ship_Is_In_Stasis(MOX::cur_ship) == 0) {
                            if (MOX::combat_init != 0 || MOX::combat_data[MOX::cur_ship].owner == active_player_id) {
                                if (MOX::net_flag == 0 || MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM) {
                                    if (fields::input_delay <= 0)
                                        tactical_control::Open_Decision(MOX::cur_ship);
                                    else
                                        tactical_control::Close_Decision();
                                    tactical_events::Service_Reads(true);
                                    if (fields::input_delay <= 0 && Consume_Tactical_Operation(active_player_id, &next_ship_flag)) {
                                        timer::Release_Time(1);
                                        continue;
                                    }
                                    int16_t input_val = 0;
                                    if (CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship) != 0) {
                                        input_val = MOX::done_button;
                                    } else {
                                        input_val = Get_Tactical_Input();
                                    }
                                    if (input_val != 0 && input_val != MOX::map_field && input_val != MOX::center_button)
                                        tactical_control::Close_Decision();

                                    for (int16_t m_idx = 0; m_idx < MAX_COMBAT_FLYERS; ++m_idx) {
                                        if (MOX::missile_field[m_idx] == input_val) {
                                            if (CMBTMIS::Missile_Owner(m_idx) != MOX::combat_data[MOX::cur_ship].owner) {
                                                tactical_control::s_operation operation{tactical_control::FIRE, MOX::cur_ship, m_idx};
                                                operation.missile_target = true;
                                                Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag);
                                            }
                                            input_val = 0;
                                            break;
                                        }
                                    }

                                    if (input_val == MOX::msl_impact_warning_button) {
                                        MOX::msl_impact_warning_flag = 1 - MOX::msl_impact_warning_flag;
                                        MOX::saved_msl_warning = MOX::msl_impact_warning_flag;
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Mox_Send_Message(MOX::player[MOX::net_destination].network_player_id, TACTICAL_NET_PACKET_MISSILE_WARNING, &MOX::msl_impact_warning_flag, sizeof(MOX::msl_impact_warning_flag));
                                        }
                                        input_val = 0;
                                    }

                                    if (Consume_Combat_Side_Wait_Input(&input_val)) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Combat_Side_Wait_Message(MOX::net_destination);
                                        }
                                        next_ship_flag = Finish_Next_Ship(
                                            Select_After_Combat_Side_Wait());
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                    }

                                    if (input_val == MOX::speedx2_button) {
                                        MOX::speedx2_flag = 1 - MOX::speedx2_flag;
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::grid_button) {
                                        MOX::grid_flag = 1 - MOX::grid_flag;
                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::draw_legal_boxes_button) {
                                        MOX::draw_legal_boxes_flag = 1 - MOX::draw_legal_boxes_flag;
                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::shields_button) {
                                        MOX::shields_flag = 1 - MOX::shields_flag;
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::super_fast_button) {
                                        MOX::super_fast_flag = 1;
                                        if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                                            MOX::auto_combat_flag[0] = 1 - MOX::auto_combat_flag[0];
                                            MOX::auto_combat = MOX::auto_combat_flag[0];
                                        } else {
                                            MOX::auto_combat_flag[1] = 1 - MOX::auto_combat_flag[1];
                                            MOX::auto_combat = MOX::auto_combat_flag[1];
                                        }
                                        CMBTDRW1::Draw_Super_Fast_Screen();
                                    }

                                    if (input_val == MOX::self_destruct_button) {
                                        char buf[80];
                                        if (MOX::cur_ship == 0) {
                                            KEN::Ken_Get_Text_Message(56, buf, sizeof(buf));
                                            palette::Refresh_Palette();
                                            GENDRAW::Message_Box(buf);
                                        } else {
                                            KEN::Ken_Get_Text_Message(57, buf, sizeof(buf));
                                            palette::Refresh_Palette();
                                            if (GENDRAW::Do_Confirmation_Box(buf) != 0) {
                                                MOX::order_info_flag = 1 - MOX::order_info_flag;
                                                CMBTDRW1::Move_Options_Background();
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_SELF_DESTRUCT, 0, 0, 0);
                                                }
                                                MOX::combat_data[MOX::cur_ship].drive_current_hits = 0;
                                                int8_t saved_ftl_type = MOX::combat_data[MOX::cur_ship].ftl_type;
                                                MOX::combat_data[MOX::cur_ship].ftl_type = 1;
                                                CMBTFIRE::Destroy_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship));
                                                MOX::combat_data[MOX::cur_ship].ftl_type = saved_ftl_type;
                                                MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                next_ship_flag = Next_Ship(active_player_id);
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                            }
                                        }
                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::msl_warning_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_CYCLE_MISSILE_WARNING, 0, 0, 0);
                                        }
                                        MOX::missile_warning = (MOX::missile_warning + 1) % 3;
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        Add_Tactical_Combat_Fields();
                                        Assign_Combat_Grids(MOX::cur_ship);
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::auto_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_TOGGLE_AUTO_COMBAT, 0, 0, 0);
                                        }
                                        if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                                            MOX::auto_combat_flag[0] = 1 - MOX::auto_combat_flag[0];
                                            MOX::auto_combat = MOX::auto_combat_flag[0];
                                        } else {
                                            MOX::auto_combat_flag[1] = 1 - MOX::auto_combat_flag[1];
                                            MOX::auto_combat = MOX::auto_combat_flag[1];
                                        }
                                        if (MOX::auto_combat == 0) {
                                            MOX::super_fast_flag = 0;
                                        }
                                        fields::Clear_Fields();
                                        MOX::auto_button = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::weapons_button) {
                                        MOX::last_ship_status = -1;
                                        Set_Combat_Help_Fields();
                                    }

                                    if (input_val == MOX::specials_button) {
                                        MOX::last_ship_status = -1;
                                        Set_Combat_Help_Fields();
                                    }

                                    if (input_val == MOX::wait_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_WAIT, 0, 0, 0);
                                        }
                                        next_ship_flag = Advance_Next_Ship(
                                            active_player_id,
                                            COMBAT_SHIP_ACTION_WAIT);
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    for (int16_t w_idx = 0; w_idx < 8; ++w_idx) {
                                        if (MOX::weapon_button[w_idx] == input_val) {
                                            if (MOX::ship_status_display == 0) {
                                                Cycle_Weapon_Bank(w_idx);
                                            }
                                            input_val = 0;
                                            break;
                                        }
                                    }

                                    if (input_val == MOX::cmbt_info_button) {
                                        MOX::order_info_flag = 1 - MOX::order_info_flag;
                                        CMBTDRW1::Move_Options_Background();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::scan_button) {
                                        MOX::board_flag = 0;
                                        Assign_Combat_Grids(MOX::cur_ship);
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::flee_button) {
                                        if (MOX::combat_data[MOX::cur_ship].is_retreating == 0) {
                                            int16_t msg_id = 0;
                                            if (Enemy_Dissipator_Exists(MOX::combat_data[MOX::cur_ship].owner) == 0) {
                                                if (MOX::cur_ship == 0 ||
                                                    COMBINIT::Is_Combat_Satellite(
                                                        &MOX::combat_data[MOX::cur_ship])) {
                                                    msg_id = 59;
                                                } else if (MOX::combat_data[MOX::cur_ship].current_speed < 1) {
                                                    msg_id = 79;
                                                }
                                            } else {
                                                msg_id = 58;
                                            }
                                            if (msg_id != 0) {
                                                char buf[80];
                                                KEN::Ken_Get_Text_Message(msg_id, buf, sizeof(buf));
                                                palette::Refresh_Palette();
                                                GENDRAW::Message_Box(buf);
                                                CMBTDRW1::Refresh_Combat_Screen_Full();
                                            } else {
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_RETREAT, 0, 0, 0);
                                                }
                                                MOX::combat_data[MOX::cur_ship].is_retreating = 1;
                                                MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                next_ship_flag = Next_Ship(active_player_id);
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                            }
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::board_button) {
                                        MOX::skan_flag = 0;
                                        if (CMBTFIR2::Already_Boarded(MOX::cur_ship) != 0) {
                                            char buf[80];
                                            KEN::Ken_Get_Text_Message(60, buf, sizeof(buf));
                                            palette::Refresh_Palette();
                                            GENDRAW::Message_Box(buf);
                                            CMBTDRW1::Refresh_Combat_Screen_Full();
                                            MOX::board_flag = 0;
                                        } else {
                                            Add_Tactical_Combat_Fields();
                                            Assign_Combat_Grids(MOX::cur_ship);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::map_field) {
                                        COMBINIT::Snap_Center_Combat_Screen((MOX::map_x * 2) / 3, (MOX::map_y * 2 * MOX::combat_grid_height) / (3 * ORIGINAL_COMBAT_GRID_HEIGHT));
                                        input_val = 0;
                                    }

                                    int16_t ptr_x = mouse::Pointer_X();
                                    if ((ptr_x < 5 && MOX::cur_combat_x > 0) ||
                                        (mouse::Pointer_X() > 635 && MOX::cur_combat_x < MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH) ||
                                        (mouse::Pointer_Y() < 5 && MOX::cur_combat_y > 0) ||
                                        (mouse::Pointer_Y() > 475 && MOX::cur_combat_y < MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT)) {
                                        COMBINIT::Scroll_Combat_Screen();
                                    }

                                    if (input_val == MOX::center_button) {
                                        COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[MOX::cur_ship].x, MOX::combat_data[MOX::cur_ship].y);
                                    }

                                    if ((int16_t)(input_val + MOX::space_field) == 0) {
                                        int16_t face_x = MOX::cur_combat_x + MOX::xpos / 2;
                                        int16_t face_y = MOX::cur_combat_y + MOX::ypos / 2;
                                        const tactical_control::s_operation operation{tactical_control::FACE, MOX::cur_ship, -1, face_x, face_y};
                                        Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag);
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::space_field) {
                                        int16_t tx = MOX::cur_combat_x + MOX::xpos / 2;
                                        int16_t ty = MOX::cur_combat_y + MOX::ypos / 2;
                                        int16_t img_num = mouse::Get_Pointer_Image_Number();

                                        if (img_num >= 4 && img_num <= 11 && MOX::combat_data[MOX::cur_ship].size_class != 0) {
                                            if (MOX::combat_data[MOX::cur_ship].size_class <= 2) {
                                                tx = MOX::cur_combat_x - 1 + (MOX::xpos + 1) / 2;
                                                ty = MOX::cur_combat_y - 1 + (MOX::ypos + 1) / 2;
                                            } else {
                                                tx = MOX::cur_combat_x - 1 + MOX::xpos / 2;
                                                ty = MOX::cur_combat_y - 1 + MOX::ypos / 2;
                                            }
                                        }

                                        int16_t tgt_idx = -1;
                                        if (tx >= 0 && tx < MOX::combat_grid_width && ty >= 0 && ty < MOX::combat_grid_height) {
                                            tgt_idx = MOX::combat_grid[tx][ty];
                                        }
                                        if (MOX::board_flag + MOX::skan_flag <= 0) {
                                            if (tgt_idx == 2000) {
                                                const tactical_control::s_operation operation{tactical_control::MOVE, MOX::cur_ship, -1, tx, ty};
                                                Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag);
                                            } else if (tgt_idx >= 0 && tgt_idx < MOX::combat_total_ship_count && MOX::combat_data[tgt_idx].unit_status == 0) {
                                                if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_data[tgt_idx].owner) {
                                                    if (MOX::combat_init == 0) {
                                                        const tactical_control::s_operation operation{tactical_control::SELECT, MOX::cur_ship, tgt_idx};
                                                        Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag);
                                                    }
                                                } else {
                                                    if (CMBTMOV1::Ship_Is_In_Stasis(tgt_idx) != 0) {
                                                        char buf[80];
                                                        KEN::Ken_Get_Text_Message(61, buf, sizeof(buf));
                                                        palette::Set_Palette_Gradient(144, 32, 0, 0, 0, 0, 0, 0);
                                                        palette::Refresh_Palette();
                                                        int16_t confirmed;
                                                        {
                                                            tactical_events::s_local_attention attention;
                                                            confirmed = GENDRAW::Do_Confirmation_Box(buf);
                                                        }
                                                        if (confirmed != 0) {
                                                            if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                                Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_REMOVE_STASIS, tgt_idx, 0, 0);
                                                            }
                                                            CMBTFIRE::Remove_Stasis_Beam(tgt_idx);
                                                            if (MOX::net_flag != 0) {
                                                                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                            }
                                                        }
                                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                                    } else if (MOX::combat_data[tgt_idx].special_status_flag == 4) {
                                                        char buf[80];
                                                        KEN::Ken_Get_Text_Message(80, buf, sizeof(buf));
                                                        palette::Refresh_Palette();
                                                        GENDRAW::Message_Box(buf);
                                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                                    } else {
                                                        const tactical_control::s_operation operation{tactical_control::FIRE, MOX::cur_ship, tgt_idx};
                                                        Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag);
                                                    }
                                                }
                                            }
                                        } else if (MOX::skan_flag != 0 && tgt_idx >= 0 && tgt_idx < MOX::combat_total_ship_count && MOX::combat_data[tgt_idx].unit_status == 0) {
                                            CMBTDRW1::Display_Combat_View_Ship(tgt_idx);
                                        } else if (MOX::board_flag != 0 && tgt_idx >= 0 && tgt_idx < MOX::combat_total_ship_count && MOX::combat_data[tgt_idx].unit_status == 0) {
                                            Initiate_Boarding_Action(MOX::cur_ship, tgt_idx);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == end_of_turn_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_END_TURN, MOX::cur_combat_x, MOX::cur_combat_y, 0);
                                        }
                                        if (MOX::combat_init == 0) {
                                            next_ship_flag = 1;
                                        } else {
                                            End_Side_Turn();
                                            next_ship_flag = Next_Ship(active_player_id);
                                        }
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::done_button) {
                                        const tactical_control::s_operation operation{tactical_control::DONE, MOX::cur_ship};
                                        Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag);
                                    }

                                    if (next_ship_flag == 0) {
                                        if (MOX::FULL_DRAW_SCREEN_FLAG == 0 && MOX::combat_data[MOX::cur_ship].movement_left == m_left) {
                                            video::Set_Page_Off();
                                            CMBTDRW1::Draw_Main_Combat_Screen();
                                            RUSS::Mox_Sync_Update();
                                        } else {
                                            Add_Tactical_Combat_Fields();
                                            Assign_Combat_Grids(MOX::cur_ship);
                                            CMBTDRW1::Refresh_Combat_Screen_Full();
                                        }
                                    }

                                } else {
                                    tactical_control::Close_Decision();
                                    int16_t net_processed = 0;
                                    while (net_processed == 0) {
                                        if (MOX::tactical_net_choice == 0) {
                                            timer::Mark_Time();
                                            if (MOX::net_flag != 0) {
                                                if (Get_Tactical_Input() == MOX::auto_button) {
                                                    CMBTAI::Toggle_Auto_Combat_Flags();
                                                }
                                            }
                                            video::Set_Page_Off();
                                            CMBTDRW1::Draw_Main_Combat_Screen();
                                            RUSS::Mox_Sync_Update();
                                            Get_Tactical_Input();
                                            timer::Release_Time(1);
                                        } else {
                                            net_processed = 1;
                                            COMBINIT::Snap_Center_Combat_Screen(MOX::new_message.viewport_x + 16, MOX::new_message.viewport_y + 9);
                                            int16_t f10 = MOX::new_message.arg1;
                                            int16_t f12 = MOX::new_message.arg2;
                                            int16_t f14 = MOX::new_message.arg3;
                                            int16_t f8 = MOX::new_message.message_code;
                                            game_random::Set_Random_Seed(MOX::new_message.random_seed);
                                            struct_::Clear_Structure(&MOX::new_message, sizeof(MOX::new_message));

                                            NETMOX::Net_Debug_Log(
                                                "tactical action consume local_player=%d action=%d args=%d,%d,%d turn=%d cur_ship=%d cur_ptr=%d",
                                                (int)MOX::PLAYER_NUM,
                                                (int)f8,
                                                f10,
                                                f12,
                                                f14,
                                                (int)MOX::combat_turn,
                                                MOX::cur_ship,
                                                (int)MOX::cur_ptr);
                                            switch (f8) {
                                                case TACTICAL_NET_MSG_SELF_DESTRUCT:
                                                    MOX::combat_data[MOX::cur_ship].drive_current_hits = 0;
                                                    {
                                                        int8_t saved_ftl_type = MOX::combat_data[MOX::cur_ship].ftl_type;
                                                        MOX::combat_data[MOX::cur_ship].ftl_type = 1;
                                                        CMBTFIRE::Destroy_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship));
                                                        MOX::combat_data[MOX::cur_ship].ftl_type = saved_ftl_type;
                                                    }
                                                    MOX::tactical_net_choice = TACTICAL_NET_MSG_SELF_DESTRUCT;
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Next_Ship(active_player_id);
                                                    break;
                                                case TACTICAL_NET_MSG_AUTO_TURN:
                                                    CMBTAI::Do_Auto_Ship_Turn();
                                                    tactical_view::Unit_Changed(MOX::cur_ship);
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Advance_Next_Ship(
                                                        active_player_id,
                                                        COMBAT_SHIP_ACTION_AUTOMATIC);
                                                    break;
                                                case TACTICAL_NET_MSG_SHIP_DONE: {
                                                    const tactical_control::s_operation operation{tactical_control::DONE, MOX::cur_ship};
                                                    Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag, true);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_MOVE_SHIP: {
                                                    const tactical_control::s_operation operation{tactical_control::MOVE, MOX::cur_ship, -1, f10, f12};
                                                    Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag, true);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_FIRE_WEAPON: {
                                                    tactical_control::s_operation operation{tactical_control::FIRE, MOX::cur_ship, f10};
                                                    operation.missile_target = f12 != 0;
                                                    Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag, true);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_CYCLE_MISSILE_WARNING:
                                                    MOX::missile_warning = (MOX::missile_warning + 1) % 3;
                                                    Add_Tactical_Combat_Fields();
                                                    Assign_Combat_Grids(MOX::cur_ship);
                                                    break;
                                                case TACTICAL_NET_MSG_TOGGLE_AUTO_COMBAT:
                                                    fields::Clear_Fields();
                                                    MOX::auto_button = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
                                                    if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                                                        MOX::auto_combat_flag[0] = 1 - MOX::auto_combat_flag[0];
                                                        MOX::auto_combat = MOX::auto_combat_flag[0];
                                                    } else {
                                                        MOX::auto_combat_flag[1] = 1 - MOX::auto_combat_flag[1];
                                                        MOX::auto_combat = MOX::auto_combat_flag[1];
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_WAIT:
                                                    next_ship_flag = Advance_Next_Ship(
                                                        active_player_id,
                                                        COMBAT_SHIP_ACTION_WAIT);
                                                    break;
                                                case TACTICAL_NET_MSG_SIDE_WAIT:
                                                    next_ship_flag = Finish_Next_Ship(
                                                        Select_After_Combat_Side_Wait());
                                                    break;
                                                case TACTICAL_NET_MSG_TOGGLE_WEAPON:
                                                    Cycle_Weapon_Bank(f10, true);
                                                    break;
                                                case TACTICAL_NET_MSG_RETREAT:
                                                    MOX::combat_data[MOX::cur_ship].is_retreating = 1;
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Next_Ship(active_player_id);
                                                    break;
                                                case TACTICAL_NET_MSG_TURN_TO_FACE: {
                                                    const tactical_control::s_operation operation{tactical_control::FACE, MOX::cur_ship, -1, f10, f12};
                                                    Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag, true);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_REMOVE_STASIS:
                                                    CMBTFIRE::Remove_Stasis_Beam(f10);
                                                    break;
                                                case TACTICAL_NET_MSG_SELECT_SHIP: {
                                                    const tactical_control::s_operation operation{tactical_control::SELECT, MOX::cur_ship, f10};
                                                    Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag, true);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_BOARDING_ACTION:
                                                    if (f14 == 1) {
                                                        if (MOX::combat_data[MOX::cur_ship].special_status_flag == 1) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 2);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 2;
                                                            Recalculate_Combat_Ship(MOX::cur_ship);
                                                        } else if (MOX::combat_data[MOX::cur_ship].special_status_flag == 4) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 5);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 6;
                                                        }
                                                        CMBTFIR2::Set_Boarded_Flag(MOX::cur_ship);
                                                        MOX::combat_data[MOX::cur_ship].marine_count -= (int8_t)f12;
                                                        CMBTFIR2::Resolve_Raid(MOX::cur_ship, f10, &f12);
                                                        MOX::combat_data[MOX::cur_ship].marine_count += (int8_t)f12;
                                                    } else {
                                                        if (MOX::combat_data[MOX::cur_ship].special_status_flag == 1) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 2);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 2;
                                                            Recalculate_Combat_Ship(MOX::cur_ship);
                                                        } else if (MOX::combat_data[MOX::cur_ship].special_status_flag == 4) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 5);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 6;
                                                        }
                                                        CMBTFIR2::Set_Boarded_Flag(MOX::cur_ship);
                                                        MOX::combat_data[MOX::cur_ship].marine_count -= (int8_t)f12;
                                                        CMBTFIR2::Resolve_Capture(MOX::cur_ship, f10, f12, MOX::combat_data[f10].x * 20, MOX::combat_data[f10].y * 20);
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_END_TURN:
                                                    if (MOX::combat_init == 0) {
                                                        next_ship_flag = 1;
                                                    } else {
                                                        End_Side_Turn();
                                                        next_ship_flag = Next_Ship(active_player_id);
                                                    }
                                                    break;
                                            }
                                            MOX::tactical_net_choice = 0;
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                    }
                                }
                            }
                        } else {
                            MOX::ship_turn_order[MOX::cur_ptr] = -1;
                            next_ship_flag = Next_Ship(active_player_id);
                        }
                    }

                    if (next_ship_flag == 0) {
                        if (MOX::FULL_DRAW_SCREEN_FLAG == 0 && MOX::combat_data[MOX::cur_ship].movement_left == m_left) {
                            video::Set_Page_Off();
                            CMBTDRW1::Draw_Main_Combat_Screen();
                            RUSS::Mox_Sync_Update();
                        } else {
                            Add_Tactical_Combat_Fields();
                            Assign_Combat_Grids(MOX::cur_ship);
                            CMBTDRW1::Refresh_Combat_Screen_Full();
                        }
                    }
                    if (next_ship_flag != 0) tactical_events::Select_Unit(-1);
                    tactical_events::Service_Reads(true);
                    timer::Release_Time(1);
                }
            }
        }

        if (MOX::combat_init == 0 && MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Current_Pass(i)) {
                    if (absorber_keep_charge == 0 &&
                        MOX::combat_data[i].owner == active_player_id) {
                        MOX::combat_data[i].reflected_damage_pool = 0;
                    }
                }
            }
        }

        Seeking_Missiles(active_player_id, time_warp_only != 0);

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Current_Pass(i)) {
                    if (MOX::combat_init != 0 ||
                        MOX::combat_data[i].owner == active_player_id) {
                        for (int16_t j = 0; j < 8; ++j) {
                            if (MOX::combat_data[i].weapons[j].status == 2) {
                                MOX::combat_data[i].weapons[j].status = 1;
                            }
                        }
                        Update_Ordinary_Cloak(i);
                        const int8_t rephase_state =
                            rephase_cloak_next_turn != 0 ? 6 : 7;
                        if (MOX::combat_data[i].special_status_flag == rephase_state &&
                            MOX::combat_data[i].special_status_timer > 0 &&
                            COMBINIT::Does_Combat_Ship_Have_Special(
                                i, SPECIAL_PHASING_CLOAK) != 0) {
                            CMBTSPEC::Draw_Cloak(i, 4);
                            tactical_view::Unit_Changed(i);
                            tactical_events::Retire_Unit(i);
                            MOX::combat_data[i].special_status_flag = 4;

                            const uint16_t tractor_target_idx = MOX::combat_data[i].special_cooldown;
                            MOX::combat_data[i].special_cooldown = COMBAT_SHIP_NONE;
                            if (tractor_target_idx < MOX::combat_total_ship_count) {
                                Recalculate_Combat_Ship(tractor_target_idx);
                            }

                            for (int16_t tractor_ship_idx = 0;
                                 tractor_ship_idx < MOX::combat_total_ship_count;
                                 ++tractor_ship_idx) {
                                if (MOX::combat_data[tractor_ship_idx].special_cooldown == i) {
                                    MOX::combat_data[tractor_ship_idx].special_cooldown = COMBAT_SHIP_NONE;
                                }
                            }

                            Recalculate_Combat_Ship(i);
                            CMBTDRW1::Refresh_Combat_Screen_Full();

                            for (int16_t m = 0; m < MAX_COMBAT_FLYERS; ++m) {
                                if (MOX::missile[m].type > 0 && MOX::missile[m].target_unit_idx == i && MOX::missile[m].source_unit_idx != i) {
                                    CMBTMIS::Retarget_Missile(m);
                                }
                            }
                        }
                        MOX::combat_data[i].ai_range_tactic = (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(i);
                    }
                }
            }
        }

        if (MOX::net_flag != 0) {
            if (MOX::PLAYER_NUM != active_player_id) {
                MOX::msl_impact_warning_flag = MOX::saved_msl_warn;
            }
            if (MOX::net_flag != 0) {
                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }
        }
        time_warp_phase_active = false;
    }

    void Update_Ordinary_Cloak(int16_t ship_idx) {
        const int8_t recloak_state = recloak_next_turn != 0 ? 2 : 3;
        if (MOX::combat_data[ship_idx].special_status_flag != recloak_state) {
            return;
        }

        const bool has_ordinary_cloak =
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_CLOAKING_DEVICE) != 0 ||
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_PHASING_CLOAK) != 0 ||
            (CMBTFIR2::combat_bonus.stealth_field_beam_defense != 0 &&
                COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_STEALTH_FIELD) != 0);
        if (!has_ordinary_cloak) {
            return;
        }

        CMBTSPEC::Draw_Cloak(ship_idx, 1);
        MOX::combat_data[ship_idx].special_status_flag = 1;
    }

    void __cdecl Update_Phasing_Cloak_Timers(bool time_warp_only) {
        if (MOX::combat_total_ship_count <= 0) {
            return;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (Combat_Ship_Is_Functional(i) == 0) {
                continue;
            }
            if (time_warp_only &&
                COMBINIT::Does_Combat_Ship_Have_Special(
                    i, SPECIAL_TIME_WARP_FACILITATOR) == 0) {
                continue;
            }

            s_combat_data* ship = &MOX::combat_data[i];
            if (ship->special_status_flag != 4 ||
                COMBINIT::Does_Combat_Ship_Have_Special(
                    i, SPECIAL_PHASING_CLOAK) == 0) {
                continue;
            }

            if (ship->special_status_timer > 0) {
                ship->special_status_timer--;
            }

            if (ship->special_status_timer == 0) {
                CMBTSPEC::Draw_Cloak(i, 5);
                CMBTSPEC::Draw_Cloak(i, 1);
                ship->special_status_flag = 1;
            }
        }
    }

    int16_t __cdecl Calc_Current_Speed(int16_t ship_idx) {
        int16_t speed = 0;

        if ((MOX::combat_data[ship_idx].ship_idx < 0 &&
                MOX::combat_data[ship_idx].ftl_type == DRIVE_NO_DRIVE) ||
            CMBTMOV1::Ship_In_Black_Hole(ship_idx)) {
            speed = 0;
        } else {
            int combat_speed = (uint8_t)MOX::combat_data[ship_idx].combat_speed_base;

            if (struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_flags, SPECIAL_AUGMENTED_ENGINES) == 1 &&
                struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_damage_flags, SPECIAL_AUGMENTED_ENGINES) == 0) {
                combat_speed += DESIGN::augmented_engines_speed_bonus;
            }

            int owner = MOX::combat_data[ship_idx].owner;

            if (MOX::combat_data[ship_idx].ftl_type != DRIVE_NO_DRIVE) {
                int current_hits = MOX::combat_data[ship_idx].drive_current_hits;
                int max_hits = MOX::combat_data[ship_idx].drive_max_hits;

                if (owner <= 9) {
                    int half_max = max_hits / 2;
                    int range = max_hits - half_max;
                    speed = (int16_t)(((current_hits - half_max) * combat_speed) / range);
                } else {
                    speed = (int16_t)((current_hits * combat_speed) / max_hits);
                }
            }

            if (speed < 0) {
                speed = 0;
            }

            if (owner < MAX_PLAYERS && MOX::player[owner].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                speed += td_combat_speed_bonus;
            }

            if (speed > 0) {
                uint32_t tractor_beams = 0;
                for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                    const int16_t tractor_target_idx = MOX::combat_data[i].special_cooldown == COMBAT_SHIP_NONE
                        ? -1
                        : MOX::combat_data[i].special_cooldown;
                    if (tractor_target_idx == ship_idx &&
                        Combat_Ship_Is_Functional(i)) {
                        for (int j = 0; j < 8; ++j) {
                            if (MOX::combat_data[i].weapons[j].weapon_id == WEAPON_TRACTOR_BEAM) {
                                tractor_beams += (uint8_t)MOX::combat_data[i].weapons[j].count;
                            }
                        }
                    }
                }

                const int8_t size_class = MOX::combat_data[ship_idx].size_class;
                int16_t tractors_to_stop = size_class + 1;
                if (size_class >= 0 && size_class < SHIP_SIZE_COUNT) {
                    tractors_to_stop = COMBAT1::tractors_to_stop[(uint8_t)size_class];
                }

                if (tractors_to_stop <= 0 ||
                    tractor_beams >= (uint32_t)tractors_to_stop) {
                    speed = 0;
                } else {
                    speed -= (int16_t)(((uint32_t)speed * tractor_beams) /
                        (uint32_t)tractors_to_stop);
                    if (speed < 1) {
                        speed = 1;
                    }
                }
            }
        }

        return speed;
    }

    void __cdecl Init_Ship_For_Start_Of_Turn(int16_t ship_idx) {
        int16_t is_captured_now = CMBTFIR2::Being_Captured(ship_idx);
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        ship->combat_status_flags = 0;
        if (is_captured_now != 0) {
            CMBTFIR2::Set_Capturing_Flag(ship_idx);
        }
        ship->field42_0x49 = 0;

        if (ship->is_captured == 1) {
            ship->movement_left = 0;
            ship->special_status_flag = 2;
            for (int i = 0; i < 8; ++i) {
                ship->weapons[i].shots_left = 0;
                ship->weapons[i].status = 0;
            }
            ship->ai_range_tactic =
                (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(ship_idx);
            return;
        }

        int16_t stasis_count = Get_Number_Of_Ships_In_Stasis(ship_idx);
        int16_t speed = Calc_Current_Speed(ship_idx);
        ship->current_speed = speed;
        ship->movement_left = speed;
        movement_reserve[ship_idx] = Maximum_Combat_Speed(ship_idx) - speed;

        if (ship->special_status_flag < 4 &&
            ship->special_status_timer != 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_PHASING_CLOAK) != 0) {
            ship->special_status_flag = 6;
        }
        if (ship->special_status_flag == 2) {
            ship->special_status_flag = 3;
        }
        if (ship->special_status_flag == 6) {
            ship->special_status_flag = 7;
        }

        for (int i = 0; i < 8; ++i) {
            s_combat_weapon_slot* slot = &ship->weapons[i];
            uint8_t count = slot->count;

            if (slot->weapon_id == WEAPON_STASIS_FIELD) {
                if ((int16_t)count < stasis_count) {
                    slot->shots_left = 0;
                    stasis_count -= (int16_t)count;
                } else {
                    slot->shots_left = (uint8_t)((int16_t)count - stasis_count);
                    stasis_count = 0;
                }
            } else {
                slot->shots_left = count;
            }

            if (slot->active != -1) {
                slot->active = 1;
            }

            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_HYPER_X_CAPACITORS) != 0) {
                if (TECHDATA::weapons[slot->weapon_id].type == WEAPON_TYPE_BEAM && slot->status != 1) {
                    slot->status = 2;
                }
            }

            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_FAST_MISSILE_RACKS) != 0) {
                if (TECHDATA::weapons[slot->weapon_id].type == WEAPON_TYPE_MISSILE && slot->status != 1) {
                    slot->status = 2;
                }
            }

            if (slot->weapon_id > 0 && TECHDATA::weapons[slot->weapon_id].type == WEAPON_TYPE_TORPEDO) {
                if (slot->ammo > 0) {
                    slot->ammo--;
                }
            }
        }

        if (continuous_bhg != 0) {
            int16_t active_target_count = 0;
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].black_hole_source_idx == ship_idx) {
                    active_target_count++;
                }
            }

            for (int i = 0; i < 8 && active_target_count > 0; ++i) {
                s_combat_weapon_slot* slot = &ship->weapons[i];
                if (slot->weapon_id != WEAPON_BLACK_HOLE_GENERATOR) {
                    continue;
                }

                int16_t committed_shots = slot->shots_left;
                if (committed_shots > active_target_count) {
                    committed_shots = active_target_count;
                }
                slot->shots_left -= (uint8_t)committed_shots;
                active_target_count -= committed_shots;
            }
        }
        ship->ai_range_tactic =
            (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(ship_idx);
    }

    int __cdecl qsort_ship_initiative(const void* p1, const void* p2) {
        const int16_t* ship1_ptr = (const int16_t*)p1;
        const int16_t* ship2_ptr = (const int16_t*)p2;
        return Compare_Combat_Ships(*ship1_ptr, *ship2_ptr);
    }

    static bool Should_End_Combat() {
        int16_t side_counts[2] = {};

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            const s_combat_data* unit = &MOX::combat_data[i];
            if (unit->unit_status != 5 && unit->is_captured == 0) {
                side_counts[unit->owner == MOX::combat_attacker_player]++;
            }
        }

        const bool attacker_is_human =
            MOX::combat_attacker_player >= 0 &&
            MOX::combat_attacker_player < MAX_PLAYERS &&
            MOX::combat_attacker_player < MOX::NUM_PLAYERS &&
            MOX::player[MOX::combat_attacker_player].objectives == PLAYER_OBJECTIVE_HUMAN;

        if (MOX::cmbt_colony_id != -1 &&
            CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0 &&
            (!attacker_is_human || MOX::auto_combat_flag[0] == 1) &&
            side_counts[0] > 0) {
            side_counts[0]--;
        }

        if (side_counts[0] > 0 && side_counts[1] > 0) {
            return false;
        }

        const int16_t surviving_player =
            side_counts[0] == 0
            ? MOX::combat_attacker_player
            : MOX::combat_defender_player;

        if (surviving_player < 0 ||
            surviving_player >= MAX_PLAYERS ||
            surviving_player >= MOX::NUM_PLAYERS ||
            MOX::player[surviving_player].objectives != PLAYER_OBJECTIVE_HUMAN) {
            return true;
        }

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            const s_missile* missile = &MOX::missile[i];
            if (missile->type > 0 &&
                missile->quantity != 0 &&
                missile->owner != surviving_player) {
                return false;
            }
        }

        return true;
    }

    int16_t __cdecl Check_For_Winner() {
        int16_t attacker_count = 0;
        int16_t defender_count = 0;
        int16_t winner = 0;

        if (MOX::combat_data[0].unit_status != 5) {
            if (CMBTFIRE::Does_Combat_Planet_Have_Defenses()) {
                defender_count = 1;
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].unit_status != 5 && MOX::combat_data[i].is_captured == 0) {
                if (MOX::combat_data[i].owner == MOX::combat_attacker_player) {
                    attacker_count++;
                } else {
                    defender_count++;
                }
            }
        }

        if (MOX::combat_turn > max_combat_turns) {
            winner = -1;
            if (defender_count <= 0) {
                winner = 666;
            }
        }

        if (attacker_count <= 0) {
            winner = -1;
            if (defender_count <= 0) {
                winner = 666;
            }
        } else if (defender_count <= 0) {
            winner = 1;
        }

        if (winner != 0 && MOX::net_flag != 0) {
            NETMOX::Net_Debug_Log(
                "tactical winner local_player=%d winner=%d attackers=%d defenders=%d turn=%d cur_ship=%d cur_ptr=%d choice=%d barrier_counter=%u",
                (int)MOX::PLAYER_NUM,
                (int)winner,
                (int)attacker_count,
                (int)defender_count,
                (int)MOX::combat_turn,
                MOX::cur_ship,
                (int)MOX::cur_ptr,
                (int)MOX::tactical_net_choice,
                (unsigned int)MOX::combat_done_waiting);
        }

        return winner;
    }

    void Reset_Strict_Combat_Sequence() {
        strict_wait_stage = false;
        memset(strict_ship_waited, 0, sizeof(strict_ship_waited));
        memcpy(strict_initial_order, MOX::ship_turn_order, sizeof(strict_initial_order));
    }

    void Reset_Combat_Side_Waits() {
        memset(side_waiting, 0, sizeof(side_waiting));
    }

    bool Combat_Side_Wait_Is_Available() {
        if (MOX::combat_init == 0 || MOX::cur_ship < 0 ||
            MOX::cur_ship >= MOX::combat_total_ship_count ||
            (ship_initiative == 2 && strict_wait_stage)) {
            return false;
        }
        const int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
        return owner == MOX::PLAYER_NUM || MOX::game_type == 1 ||
            MOX::take_control_flag != 0;
    }

    int16_t Add_Combat_Side_Wait_Field() {
        side_wait_button = Combat_Side_Wait_Is_Available()
            ? fields::Add_Hot_Key("h")
            : -100;
        return side_wait_button;
    }

    bool Consume_Combat_Side_Wait_Input(int16_t* input_value) {
        if (input_value == nullptr || side_wait_button == -100 ||
            *input_value != side_wait_button) {
            return false;
        }
        *input_value = 0;
        return true;
    }

    int16_t Select_After_Combat_Side_Wait() {
        if (MOX::combat_init == 0) {
            return 0;
        }
        if (ship_initiative == 2) {
            return Select_Next_Strict_Ship(COMBAT_SHIP_ACTION_SIDE_WAIT);
        }
        return Select_Next_Classic_Ship(COMBAT_SHIP_ACTION_SIDE_WAIT);
    }

    static bool Record_Strict_Wait(int16_t ship_idx) {
        if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
            return false;
        }

        s_combat_data* ship = &MOX::combat_data[ship_idx];
        int32_t cost = wait_movement_cost;
        if (cost > ship->movement_left) {
            cost = ship->movement_left;
        }
        if (cost > 0) {
            ship->movement_left -= (int16_t)cost;
        }
        strict_ship_waited[ship_idx] = true;
        return true;
    }

    static void Apply_Strict_Wait(int16_t ship_idx) {
        if (!Record_Strict_Wait(ship_idx)) {
            return;
        }
        s_combat_data* ship = &MOX::combat_data[ship_idx];
        struct_::Clear_Bit_Field(&ship->combat_status_flags, 2);
    }

    static void Apply_Strict_Side_Wait(int8_t owner) {
        if (strict_wait_stage || owner < 0 || owner >= 15) {
            return;
        }
        side_waiting[owner] = true;
        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                break;
            }
            if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count ||
                MOX::combat_data[ship_idx].owner != owner) {
                continue;
            }
            if (!strict_ship_waited[ship_idx]) {
                Record_Strict_Wait(ship_idx);
            }
            MOX::ship_turn_order[i] = -1;
        }
    }

    static void Add_Strict_Initial_Ship(int16_t ship_idx) {
        int16_t empty_idx = -1;
        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            if (strict_initial_order[i] == ship_idx) {
                return;
            }
            if (strict_initial_order[i] == -1) {
                empty_idx = i;
            } else if (strict_initial_order[i] == -10) {
                if (empty_idx >= 0) {
                    strict_initial_order[empty_idx] = ship_idx;
                } else if (i + 1 < MAX_COMBAT_SHIPS) {
                    strict_initial_order[i] = ship_idx;
                    strict_initial_order[i + 1] = -10;
                }
                return;
            }
        }
    }

    static void Synchronize_Strict_Captured_Ships() {
        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                return;
            }
            if (ship_idx >= 0) {
                Add_Strict_Initial_Ship(ship_idx);
            }
        }
    }

    static int16_t Select_Strict_Stage_Ship() {
        int16_t best_idx = -1;
        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                break;
            }
            if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
                continue;
            }
            if (ship_idx == 0 &&
                CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0) {
                continue;
            }

            const int8_t owner = MOX::combat_data[ship_idx].owner;
            if (owner < 0 || owner >= 15) {
                continue;
            }
            if (MOX::side_end_of_turn[owner] != 0) {
                MOX::ship_turn_order[i] = -1;
                CMBTFIR2::Set_Combat_Ship_Done(ship_idx);
                continue;
            }
            if (Combat_Ship_Is_Functional(ship_idx) == 0) {
                continue;
            }

            if (best_idx < 0) {
                best_idx = i;
                continue;
            }

            const int comparison = Compare_Combat_Ships(
                ship_idx, MOX::ship_turn_order[best_idx]);
            if ((!strict_wait_stage && comparison < 0) ||
                (strict_wait_stage && comparison >= 0)) {
                best_idx = i;
            }
        }

        if (best_idx < 0) {
            return 1;
        }
        MOX::cur_ptr = best_idx;
        MOX::cur_ship = MOX::ship_turn_order[best_idx];
        tactical_events::Select_Unit(MOX::cur_ship);
        return 0;
    }

    static void Begin_Strict_Wait_Stage() {
        Synchronize_Strict_Captured_Ships();
        strict_wait_stage = true;
        memcpy(MOX::ship_turn_order, strict_initial_order, sizeof(strict_initial_order));

        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                break;
            }
            if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
                continue;
            }
            if (ship_idx == 0 &&
                CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0) {
                MOX::ship_turn_order[i] = -1;
                continue;
            }

            s_combat_data* ship = &MOX::combat_data[ship_idx];
            if (!Is_Human_Combat_Owner(ship->owner) &&
                ship->unit_status != 5 &&
                !strict_ship_waited[ship_idx]) {
                Apply_Strict_Wait(ship_idx);
            }

            if ((!strict_ship_waited[ship_idx] &&
                    CMBTMOV1::Ship_Is_In_Stasis(ship_idx) == 0) ||
                ship->unit_status != 0 ||
                ship->is_retreating != 0 ||
                CMBTFIR2::Combat_Ship_Done(ship_idx) != 0) {
                MOX::ship_turn_order[i] = -1;
            }
        }
    }

    int16_t Select_Next_Strict_Ship(COMBAT_SHIP_ACTION action) {
        MOX::missile_warning = 0;

        if (Should_End_Combat()) {
            MOX::cur_ship = -1;
            MOX::cur_ptr = -1;
            return 1;
        }

        if (action == COMBAT_SHIP_ACTION_SIDE_WAIT && MOX::cur_ship >= 0) {
            Apply_Strict_Side_Wait(MOX::combat_data[MOX::cur_ship].owner);
        }

        const bool should_wait =
            !strict_wait_stage &&
            action == COMBAT_SHIP_ACTION_WAIT;
        if (MOX::cur_ptr >= 0 && MOX::cur_ptr < MAX_COMBAT_SHIPS) {
            if (should_wait) {
                Apply_Strict_Wait(MOX::cur_ship);
            }
            if (action != COMBAT_SHIP_ACTION_NONE &&
                action != COMBAT_SHIP_ACTION_SIDE_WAIT) {
                MOX::ship_turn_order[MOX::cur_ptr] = -1;
            }
        }

        if (Select_Strict_Stage_Ship() == 0) {
            return 0;
        }
        if (strict_wait_stage) {
            MOX::ship_turn_order[0] = -10;
            MOX::cur_ship = -1;
            MOX::cur_ptr = -1;
            return 1;
        }

        Begin_Strict_Wait_Stage();
        if (Select_Strict_Stage_Ship() == 0) {
            return 0;
        }

        MOX::ship_turn_order[0] = -10;
        MOX::cur_ship = -1;
        MOX::cur_ptr = -1;
        return 1;
    }

    static int16_t Finish_Next_Ship(int16_t result) {
        tactical_events::Select_Unit(result == 0 ? MOX::cur_ship : -1);
        if (result == 0) {
            if ((MOX::combat_init != 0 && center_on_next_ship) ||
                (CMBTDRW1::Ship_Completely_On_Screen(MOX::cur_ship) == 0 &&
                    COMBAT1::start_of_turn != 0)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[MOX::cur_ship].x, MOX::combat_data[MOX::cur_ship].y);
            }

            COMBAT1::Add_Tactical_Combat_Fields();
            COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
            CMBTDRW1::Refresh_Combat_Screen_Full();

            int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
            if (owner == MOX::combat_attacker_player) {
                MOX::auto_combat = MOX::auto_combat_flag[0];
            } else {
                MOX::auto_combat = MOX::auto_combat_flag[1];
            }

            if (MOX::net_flag != 0) {
                if (owner == MOX::combat_attacker_player) {
                    MOX::net_destination = MOX::combat_defender_player;
                } else {
                    MOX::net_destination = MOX::combat_attacker_player;
                }

                if (owner == MOX::PLAYER_NUM) {
                    MOX::msl_impact_warning_flag = MOX::saved_msl_warning;
                    NETMOX::Mox_Send_Message(MOX::player[MOX::net_destination].network_player_id, TACTICAL_NET_PACKET_MISSILE_WARNING, &MOX::msl_impact_warning_flag, sizeof(MOX::msl_impact_warning_flag));
                }
                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }
        }

        if (MOX::super_fast_flag != 0) {
            CMBTDRW1::Draw_Super_Fast_Screen();
        }
        return result;
    }

    static bool Classic_Ship_Is_Selectable(int16_t ship_idx) {
        if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
            return false;
        }
        if (ship_idx == 0 && CMBTFIRE::Does_Combat_Planet_Have_Defenses() == 0) {
            return false;
        }
        const int8_t owner = MOX::combat_data[ship_idx].owner;
        return owner >= 0 && owner < 15 && MOX::side_end_of_turn[owner] == 0 &&
            Combat_Ship_Is_Functional(ship_idx) != 0 &&
            MOX::combat_data[ship_idx].is_retreating == 0 &&
            CMBTFIR2::Combat_Ship_Done(ship_idx) == 0;
    }

    static bool Side_Has_Selectable_Classic_Ship(int16_t owner) {
        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                break;
            }
            if (Classic_Ship_Is_Selectable(ship_idx) &&
                MOX::combat_data[ship_idx].owner == owner) {
                return true;
            }
        }
        return false;
    }

    int16_t Select_Next_Classic_Ship(COMBAT_SHIP_ACTION action) {
        MOX::missile_warning = 0;

        int16_t saved_cur_ship = MOX::cur_ship;
        int16_t saved_cur_ptr = MOX::cur_ptr;

        if (Should_End_Combat()) {
            return 1;
        }

        if (action == COMBAT_SHIP_ACTION_SIDE_WAIT) {
            if (MOX::combat_init == 0 || MOX::cur_ship < 0) {
                return 0;
            }
            const int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
            if (owner >= 0 && owner < 15) {
                side_waiting[owner] = true;
            }
        }

        if (action == COMBAT_SHIP_ACTION_DONE && saved_cur_ptr >= 0 &&
            saved_cur_ptr < MAX_COMBAT_SHIPS &&
            MOX::ship_turn_order[saved_cur_ptr] >= 0) {
            MOX::ship_turn_order[saved_cur_ptr] = -1;
        }

        int16_t order_count = 0;
        while (order_count < MAX_COMBAT_SHIPS &&
            MOX::ship_turn_order[order_count] != -10) {
            order_count++;
        }
        if (order_count == 0) {
            return 1;
        }

        const bool attacker_present =
            Side_Has_Selectable_Classic_Ship(MOX::combat_attacker_player);
        const bool defender_present =
            Side_Has_Selectable_Classic_Ship(MOX::combat_defender_player);
        bool attacker_waiting = MOX::combat_attacker_player >= 0 &&
            MOX::combat_attacker_player < 15 &&
            side_waiting[MOX::combat_attacker_player];
        bool defender_waiting = MOX::combat_defender_player >= 0 &&
            MOX::combat_defender_player < 15 &&
            side_waiting[MOX::combat_defender_player];
        if (attacker_waiting && defender_waiting && attacker_present &&
            defender_present) {
            attacker_waiting = false;
        } else {
            if (!attacker_present) {
                defender_waiting = false;
            }
            if (!defender_present) {
                attacker_waiting = false;
            }
        }

        int16_t start_ptr = saved_cur_ptr;
        if (action != COMBAT_SHIP_ACTION_SIDE_WAIT) {
            start_ptr++;
        }
        if (start_ptr < 0 || start_ptr >= order_count) {
            start_ptr = 0;
        }

        for (int16_t step = 0; step < order_count; ++step) {
            const int16_t current_ptr =
                (int16_t)((start_ptr + step) % order_count);
            const int16_t ship_idx = MOX::ship_turn_order[current_ptr];
            if (!Classic_Ship_Is_Selectable(ship_idx)) {
                continue;
            }

            const int8_t owner = MOX::combat_data[ship_idx].owner;
            const bool owner_waiting = owner == MOX::combat_attacker_player
                ? attacker_waiting
                : defender_waiting;
            if (owner_waiting) {
                continue;
            }
            MOX::cur_ptr = current_ptr;
            MOX::cur_ship = ship_idx;
            tactical_events::Select_Unit(ship_idx);
            return 0;
        }

        MOX::cur_ship = -1;
        MOX::cur_ptr = -1;
        return 1;
    }

    static int16_t Advance_Next_Ship(
        int16_t active_player_id, COMBAT_SHIP_ACTION action) {
        (void)active_player_id;
        if (ship_initiative == 2) {
            return Finish_Next_Ship(Select_Next_Strict_Ship(action));
        }
        return Finish_Next_Ship(Select_Next_Classic_Ship(action));
    }

    int16_t __cdecl Next_Ship(int16_t unused_player_idx) {
        return Advance_Next_Ship(
            unused_player_idx, COMBAT_SHIP_ACTION_DONE);
    }

    bool Validate_Retreating_Ship(int16_t ship_idx) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];
        int16_t is_dead = CMBTFIRE::Is_Combat_Ship_Dead(ship_idx);
        int16_t speed = 0;
        if (is_dead == 0) {
            speed = Calc_Current_Speed(ship_idx);
        }

        if (is_dead != 0 || speed <= 0 || ship->is_retreating != 1) {
            if (is_dead != 0 || speed <= 0) {
                ship->movement_left = 0;
            }
            ship->is_retreating = 0;
            return false;
        }
        return true;
    }

    void __cdecl Retreat_Ship(int16_t ship_idx) {
        if (!Validate_Retreating_Ship(ship_idx)) {
            return;
        }

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            if (CMBTDRW1::Ship_On_Screen(ship_idx) == 0) {
                COMBINIT::Snap_Center_Combat_Screen(ship->x, ship->y);
            }
            CMBTDRW1::Draw_Retreating_Ship(ship_idx);
            if (!Validate_Retreating_Ship(ship_idx)) {
                return;
            }
        }

        tactical_view::Unit_Changed(ship_idx, true);
        tactical_events::Retire_Unit(ship_idx);
        ship->unit_status = 5;
        ship->is_retreating = 2;
        CMBTFIRE::Retarget_Missiles_Aimed_At(ship_idx);
        COMBAT1::Remove_Continuing_Effect_Specials(ship_idx);
    }

    int16_t __cdecl Get_Number_Of_Ships_In_Stasis(int16_t source_ship_idx) {
        int16_t count = 0;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            const int16_t stasis_source_idx = MOX::combat_data[i].stasis_source_idx == COMBAT_SHIP_NONE
                ? -1
                : MOX::combat_data[i].stasis_source_idx;
            if (source_ship_idx >= 0 && stasis_source_idx == source_ship_idx) {
                count++;
            }
        }

        return count;
    }

    void __cdecl Add_Missile_Fields() {
        MOX::cur_missile_count = 0;

        graphics::Set_Window(0, 0, 0x27F, 0x15E);
        video::Clip_On();

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* missile = &MOX::missile[i];

            if (missile->type > 0 && CMBTMIS::On_Combat_Screen((missile->x - 8) / 20, (missile->y - 8) / 20)) {
                int16_t local_x = missile->x - (MOX::cur_combat_x * 20);
                int16_t local_y = missile->y - (MOX::cur_combat_y * 20);

                MOX::missile_field[i] = fields::Add_Clipped_Hidden_Field(
                    local_x - 16,
                    local_y - 16,
                    local_x - 8,
                    local_y - 8,
                    "",
                    0);

                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x1 = local_x - 16;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x2 = local_x - 8;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y1 = local_y - 16;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y2 = local_y - 8;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].frame = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].icon = 12;

                MOX::cur_missile_count++;
            } else {
                MOX::missile_field[i] = -1000;

                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x1 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x2 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y1 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y2 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].frame = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].icon = 0;
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();
    }

    void __cdecl Remove_Continuing_Effect_Specials(int16_t source_idx) {
        if (source_idx >= 0 && source_idx < MOX::combat_total_ship_count) {
            MOX::combat_data[source_idx].special_cooldown = COMBAT_SHIP_NONE;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            // Check if this ship is being held in stasis by the source ship
            if (MOX::combat_data[i].stasis_source_idx == source_idx) {
                CMBTFIRE::Remove_Stasis_Beam(i);
            }

            // Check if this ship is affected by a black hole created by the source ship
            if (MOX::combat_data[i].black_hole_source_idx == source_idx) {
                MOX::combat_data[i].black_hole_flag = 0;
                MOX::combat_data[i].black_hole_source_idx = COMBAT_SHIP_NONE;
            }

            // Check if special cooldown (or effect tracking) matches the source ship
            if (MOX::combat_data[i].special_cooldown == source_idx) {
                MOX::combat_data[i].special_cooldown = COMBAT_SHIP_NONE;
            }
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            Recalculate_Combat_Ship(i);
        }
        if (interactive_combat_stats != 0) {
            CMBTFIR2::Set_Combat_Ordnance_Values();
        }
    }

    void __cdecl Assign_Mouse_Images() {
        if (MOX::game_type != 1 && MOX::combat_data[MOX::cur_ship].owner != MOX::PLAYER_NUM) {
            MOX::cmbt_mouse_list_clock.icon = MOX::combat_clock_frame;
            mouse::Set_Mouse_List(&MOX::cmbt_mouse_list_clock, 1);
            return;
        }

        if (MOX::order_info_flag != 1) {
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            return;
        }

        int grid_idx = 1;
        int16_t x_val = 0;

        for (int i = 0; i < COMBAT_GRID_VIEW_WIDTH; ++i) {
            int16_t y_val = 0;
            for (int j = 0; j < COMBAT_GRID_VIEW_HEIGHT; ++j) {
                MOX::combat_mouse_grid[grid_idx].x1 = x_val;
                MOX::combat_mouse_grid[grid_idx].x2 = x_val + 0x13;
                MOX::combat_mouse_grid[grid_idx].y1 = y_val;

                if (j == COMBAT_GRID_VIEW_MAX_Y) {
                    MOX::combat_mouse_grid[grid_idx].y2 = y_val + 0x16;
                } else {
                    MOX::combat_mouse_grid[grid_idx].y2 = y_val + 0x13;
                }

                grid_idx++;
                y_val += 0x14;
            }
            x_val += 0x14;
        }

        int16_t size_info = 0;
        if (MOX::combat_data[MOX::cur_ship].size_class > 0) {
            if (MOX::combat_data[MOX::cur_ship].size_class <= 2) {
                size_info = 10;
            } else {
                size_info = 20;
            }
        }

        for (int grid_x = COMBAT_GRID_VIEW_MAX_X; grid_x >= 0; --grid_x) {
            for (int grid_y = COMBAT_GRID_VIEW_MAX_Y; grid_y >= 0; --grid_y) {
                int idx = grid_x * COMBAT_GRID_VIEW_HEIGHT + grid_y + 1;
                int16_t target_ship = -1;
                int16_t world_x = grid_x + MOX::cur_combat_x;
                int16_t world_y = grid_y + MOX::cur_combat_y;

                if (world_x >= 0 && world_x < MOX::combat_grid_width && world_y >= 0 && world_y < MOX::combat_grid_height) {
                    target_ship = MOX::combat_grid[world_x][world_y];
                }

                if (MOX::skan_flag == 1) {
                    if (target_ship < 0 || target_ship >= MOX::combat_total_ship_count) {
                        MOX::combat_mouse_grid[idx].icon = 14;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    } else {
                        MOX::combat_mouse_grid[idx].icon = 13;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    }
                } else if (MOX::board_flag == 1) {
                    if (target_ship < 0 || target_ship >= MOX::combat_total_ship_count ||
                        MOX::combat_data[target_ship].owner == MOX::combat_data[MOX::cur_ship].owner) {
                        MOX::combat_mouse_grid[idx].icon = 2;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    } else {
                        MOX::combat_mouse_grid[idx].icon = 3;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    }
                } else {
                    if (target_ship < 0) {
                        MOX::combat_mouse_grid[idx].icon = 2;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    } else if (target_ship >= MOX::combat_total_ship_count) {
                        if (target_ship == 2000) {
                            int16_t facing = CMBTMOV1::Get_Facing(
                                MOX::combat_data[MOX::cur_ship].x,
                                MOX::combat_data[MOX::cur_ship].y,
                                MOX::cur_combat_x + grid_x,
                                MOX::cur_combat_y + grid_y);
                            MOX::combat_mouse_grid[idx].icon = (facing / 2) + 4;
                            MOX::combat_mouse_grid[idx].frame = 12;
                        }
                    } else {
                        if (MOX::combat_data[target_ship].owner == MOX::combat_data[MOX::cur_ship].owner) {
                            if (target_ship == MOX::cur_ship) {
                                MOX::combat_mouse_grid[idx].icon = 2;
                                MOX::combat_mouse_grid[idx].frame = 12;
                            } else {
                                MOX::combat_mouse_grid[idx].icon = 1;
                                MOX::combat_mouse_grid[idx].frame = 0;
                            }
                        } else {
                            if (CMBTMOV1::Ship_Is_In_Stasis(target_ship) == 0) {
                                MOX::combat_mouse_grid[idx].icon = 12;
                                MOX::combat_mouse_grid[idx].frame = 12;
                            } else {
                                MOX::combat_mouse_grid[idx].icon = 1;
                                MOX::combat_mouse_grid[idx].frame = 0;
                            }
                        }
                    }
                }
            }
        }

        if (size_info > 0) {
            int idx_base = 558;
            int16_t x_val_base = size_info + 620;

            for (int grid_x = COMBAT_GRID_VIEW_MAX_X; grid_x >= 0; --grid_x) {
                int16_t y_end = size_info + 340;

                for (int grid_y = COMBAT_GRID_VIEW_MAX_Y; grid_y >= 0; --grid_y) {
                    int idx = idx_base + grid_y + 1;
                    int16_t icon = MOX::combat_mouse_grid[idx].icon;

                    if (icon >= 4 && icon <= 11) {
                        if (size_info == 20) {
                            if (grid_x < COMBAT_GRID_VIEW_MAX_X && grid_y < COMBAT_GRID_VIEW_MAX_Y) {
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].icon = icon;
                            }
                        } else if (size_info == 10) {
                            int16_t icon19 = MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].icon;
                            if (icon19 < 4 || icon19 > 11) {
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].x1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].x2 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].y1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].y2 = 0;
                            }

                            int16_t icon1 = MOX::combat_mouse_grid[idx + 1].icon;
                            if (icon1 < 4 || icon1 > 11) {
                                MOX::combat_mouse_grid[idx + 1].x1 = 0;
                                MOX::combat_mouse_grid[idx + 1].x2 = 0;
                                MOX::combat_mouse_grid[idx + 1].y1 = 0;
                                MOX::combat_mouse_grid[idx + 1].y2 = 0;
                            }

                            int16_t icon18 = MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].icon;
                            if (icon18 < 4 || icon18 > 11) {
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].x1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].x2 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].y1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].y2 = 0;
                            }
                        }

                        MOX::combat_mouse_grid[idx].x1 = x_val_base;
                        MOX::combat_mouse_grid[idx].x2 = x_val_base + 0x13;
                        MOX::combat_mouse_grid[idx].y1 = y_end;
                        MOX::combat_mouse_grid[idx].y2 = y_end + 0x13;
                    }
                    y_end -= 20;
                }
                x_val_base -= 20;
                idx_base -= COMBAT_GRID_VIEW_HEIGHT;
            }
        }

        mouse::Set_Mouse_List(MOX::combat_mouse_grid, COMBAT_MOUSE_COUNT);
    }

    void __cdecl Set_Cur_Ship_To(int16_t ship_idx) {
        if (ship_idx == 0) {
            if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() <= 0) {
                return;
            }
        }

        int16_t cur_ptr_idx = -1;
        int16_t total_ships = MOX::combat_total_ship_count;

        for (int16_t i = 0; i < total_ships; ++i) {
            int16_t order_val = MOX::ship_turn_order[i];
            if (order_val == -10) {
                break;
            }
            if (ship_idx == order_val) {
                cur_ptr_idx = i;
            }
        }

        if (cur_ptr_idx == -1) {
            for (int16_t i = 0; i < total_ships; ++i) {
                if (MOX::ship_turn_order[i] == -1) {
                    cur_ptr_idx = i;
                    MOX::ship_turn_order[i] = ship_idx;
                    break;
                }
            }
        }

        MOX::cur_ptr = cur_ptr_idx;
        MOX::cur_ship = ship_idx;
        tactical_events::Select_Unit(ship_idx);

        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    int16_t __cdecl Enemy_Dissipator_Exists(int16_t player_idx) {
        int16_t exists = 0;
        int16_t total_ships = MOX::combat_total_ship_count;

        for (int16_t i = 0; i < total_ships; ++i) {
            if (!CMBTMOV1::Ship_Is_In_Stasis(i) &&
                COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_WARP_DISSIPATOR) != 0) {
                s_combat_data* ship = &MOX::combat_data[i];

                if (ship->is_captured == 0 && ship->unit_status == 0 && ship->owner != player_idx) {
                    exists = 1;
                }
            }
        }

        return exists;
    }

    void __cdecl Assign_Combat_Grids(int16_t active_ship_idx) {
        s_combat_data* ship;
        int16_t dim;
        int16_t i;
        int16_t j;

        for (i = 0; i < MOX::combat_grid_width; ++i) {
            for (j = 0; j < MOX::combat_grid_height; ++j) {
                MOX::combat_grid[i][j] = -1;
            }
        }

        if (MOX::order_info_flag == 1) {
            for (int16_t ship_idx = 0; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
                ship = &MOX::combat_data[ship_idx];

                if (ship->unit_status == 0) {
                    MOX::combat_grid[ship->x][ship->y] = ship_idx;

                    if (ship_idx != active_ship_idx || MOX::skan_flag == 1 || MOX::board_flag == 1) {
                        if (ship_idx == 0) {
                            if (MOX::cmbt_colony_id == -1) {
                                dim = 6;
                            } else {
                                int8_t planet_size = MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
                                switch (planet_size) {
                                    case PLANET_SIZE_TINY: dim = 3; break;
                                    case PLANET_SIZE_SMALL: dim = 4; break;
                                    case PLANET_SIZE_MEDIUM: dim = 6; break;
                                    case PLANET_SIZE_LARGE: dim = 7; break;
                                    case PLANET_SIZE_HUGE: dim = 8; break;
                                    default: dim = 6; break;
                                }
                            }

                            for (i = 0; i < dim; ++i) {
                                for (j = 0; j < dim; ++j) {
                                    MOX::combat_grid[ship->x + i][ship->y + j] = 0;
                                }
                            }
                        } else {
                            dim = (ship->size_class + 1) / 2;
                            if (dim == 1) {
                                for (i = 0; i < 2; ++i) {
                                    for (j = 0; j < 2; ++j) {
                                        MOX::combat_grid[ship->x + i][ship->y + j] = ship_idx;
                                    }
                                }
                            } else if (dim > 1 && dim < 4) {
                                for (i = 0; i < 3; ++i) {
                                    for (j = 0; j < 3; ++j) {
                                        MOX::combat_grid[ship->x + i][ship->y + j] = ship_idx;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (MOX::cur_ship == active_ship_idx) {
            COMBAT1::Set_Legal_Moves(active_ship_idx, MOX::combat_data[active_ship_idx].movement_left, 1);
            COMBAT1::Assign_Mouse_Images();
        }
    }

    void __cdecl Initiate_Boarding_Action(int16_t attacker_idx, int16_t defender_idx) {
        bool can_board = true;
        int16_t range = CMBTAI::Range_To_Ship_Squares(attacker_idx, defender_idx);
        int16_t has_transporters = COMBINIT::Does_Combat_Ship_Have_Special(attacker_idx, SPECIAL_TRANSPORTERS);
        int16_t max_range = has_transporters == 1
            ? BOARDING::board_range.transporters
            : BOARDING::board_range.normal;
        s_combat_data* target = &MOX::combat_data[defender_idx];
        char msg_buffer[80];
        int msg_idx = 0;

        const BOARDING::e_boarding_filter boarding_filter =
            BOARDING::Filter_Boarding(attacker_idx, defender_idx);
        if (boarding_filter != BOARDING::e_boarding_filter::allowed) {
            can_board = false;
            const char* warning = nullptr;
            if (boarding_filter ==
                BOARDING::e_boarding_filter::transporter_blocked_by_damper) {
                warning = "Damper Field prevents transporter boarding.";
            } else if (boarding_filter ==
                BOARDING::e_boarding_filter::target_in_black_hole) {
                warning = "Ships trapped in a black hole may not be boarded.";
            } else if (boarding_filter ==
                BOARDING::e_boarding_filter::attacker_in_black_hole) {
                warning = "Ships trapped in a black hole may not board.";
            } else if (boarding_filter ==
                BOARDING::e_boarding_filter::target_phased_out) {
                warning = "Phased out ships may not be boarded.";
            }
            if (warning != nullptr) {
                palette::Refresh_Palette();
                GENDRAW::Warning_Box(warning);
                CMBTDRW1::Refresh_Combat_Screen_Full();
            }
        } else if (target->owner > 8) {
            can_board = false;
            msg_idx = 0x3E; // Monsters cannot be boarded
        } else if (defender_idx == 0) {
            can_board = false;
            msg_idx = 0x3F; // Planets cannot be boarded
        } else if (MOX::combat_data[MOX::cur_ship].owner == target->owner) {
            can_board = false;
            msg_idx = (target->is_captured == 0) ? 0x40 : 0x41; // Cannot board own ship
        } else if (range > max_range) {
            can_board = false;
            msg_idx = 0x42; // Out of range
        } else if (has_transporters != 1 && COMBAT1::Calc_Current_Speed(defender_idx) > 0) {
            can_board = false;
            msg_idx = 0x43; // Ship moving too fast
        } else if (MOX::combat_data[MOX::cur_ship].marine_count == 0) {
            can_board = false;
            msg_idx = 0x44; // No marines
        } else if (has_transporters == 1) {
            if (range > BOARDING::board_range.normal ||
                MOX::combat_data[defender_idx].current_speed > 0) {
                if (BOARDING::boarding.damper_survival_divisor == 0 &&
                    COMBINIT::Does_Combat_Ship_Have_Special(
                        defender_idx, SPECIAL_DAMPER_FIELD) != 0) {
                    can_board = false;
                    palette::Refresh_Palette();
                    GENDRAW::Warning_Box(
                        "Damper Field prevents transporter boarding.");
                    CMBTDRW1::Refresh_Combat_Screen_Full();
                } else {
                    int16_t bearing = CMBTDRW1::Relative_Bearing(defender_idx, attacker_idx);
                    int16_t shield_idx = CMBTFIRE::Facing_Shield(bearing);
                    if (target->shield_arc_current[shield_idx] > 0) {
                        can_board = false;
                        KEN::Ken_Get_Text_Message(0x45, msg_buffer, sizeof(msg_buffer));
                        palette::Refresh_Palette();
                        GENDRAW::Warning_Box(msg_buffer);
                        CMBTDRW1::Refresh_Combat_Screen_Full();
                    }
                    if (COMBINIT::Does_Combat_Ship_Have_Special(defender_idx, SPECIAL_HARD_SHIELDS) != 0) {
                        can_board = false;
                        msg_idx = 0x46;
                    }
                }
            }
        }

        if (!can_board) {
            if (msg_idx != 0) {
                KEN::Ken_Get_Text_Message((int16_t)msg_idx, msg_buffer, sizeof(msg_buffer));
                palette::Refresh_Palette();
                GENDRAW::Warning_Box(msg_buffer);
                CMBTDRW1::Refresh_Combat_Screen_Full();
            }
        } else {
            int16_t marines_sent;
            int16_t result = CMBTFIR2::Init_Boarding_Action(MOX::cur_ship, &marines_sent);
            if (result != 0) {
                MOX::combat_data[MOX::cur_ship].marine_count -= (int8_t)marines_sent;

                if (has_transporters == 1 &&
                    (range > BOARDING::board_range.normal ||
                        MOX::combat_data[defender_idx].current_speed > 0)) {
                    if (COMBINIT::Does_Combat_Ship_Have_Special(defender_idx, SPECIAL_DAMPER_FIELD) != 0) {
                        marines_sent = marines_sent / BOARDING::boarding.damper_survival_divisor;
                    }
                }

                if (MOX::net_flag != 0 && (result == 1 || result == 2)) {
                    int16_t msg_player = MOX::combat_attacker_player;
                    if (MOX::combat_data[attacker_idx].owner == MOX::combat_attacker_player) {
                        msg_player = MOX::combat_defender_player;
                    }
                    if (MOX::PLAYER_NUM != msg_player) {
                        COMBAT1::Send_Tactical_Combat_Message(msg_player, TACTICAL_NET_MSG_BOARDING_ACTION, defender_idx, marines_sent, result);
                    }
                }

                if (result == 1) { // Raid
                    if (MOX::combat_data[MOX::cur_ship].special_status_flag == 1) {
                        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                            CMBTSPEC::Draw_Cloak(MOX::cur_ship, 2);
                        }
                        MOX::combat_data[MOX::cur_ship].special_status_flag = 2;
                        Recalculate_Combat_Ship(MOX::cur_ship);
                    } else if (MOX::combat_data[MOX::cur_ship].special_status_flag == 4) {
                        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                            CMBTSPEC::Draw_Cloak(MOX::cur_ship, 5);
                        }
                        MOX::combat_data[MOX::cur_ship].special_status_flag = 6;
                    }
                    CMBTFIR2::Set_Boarded_Flag(MOX::cur_ship);
                    CMBTFIR2::Resolve_Raid(MOX::cur_ship, defender_idx, &marines_sent);
                    MOX::combat_data[MOX::cur_ship].marine_count += (int8_t)marines_sent;
                } else if (result == 2) { // Capture
                    if (MOX::combat_data[MOX::cur_ship].special_status_flag == 1) {
                        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                            CMBTSPEC::Draw_Cloak(MOX::cur_ship, 2);
                        }
                        MOX::combat_data[MOX::cur_ship].special_status_flag = 2;
                        Recalculate_Combat_Ship(MOX::cur_ship);
                    } else if (MOX::combat_data[MOX::cur_ship].special_status_flag == 4) {
                        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                            CMBTSPEC::Draw_Cloak(MOX::cur_ship, 5);
                        }
                        MOX::combat_data[MOX::cur_ship].special_status_flag = 6;
                    }
                    CMBTFIR2::Set_Boarded_Flag(MOX::cur_ship);
                    CMBTFIR2::Resolve_Capture(MOX::cur_ship, defender_idx, marines_sent, target->x * 20, target->y * 20);
                }

                MOX::board_flag = 0;
                if (MOX::net_flag != 0) {
                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                }
            }
        }
    }

    void __cdecl Seeking_Missiles(int16_t player_idx, bool time_warp_only) {
        int16_t resolved_missile_idx[MAX_COMBAT_FLYERS];
        int16_t resolved_target_idx[MAX_COMBAT_FLYERS];
        int16_t is_finished;
        int16_t resolve_count;
        int16_t target_x, target_y;

        if (CMBTMIS::smarter_fighter_retargeting && !time_warp_only) {
            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                const s_missile& fighter = MOX::missile[i];
                if (fighter.type <= WEAPON_NO_WEAPONS || fighter.quantity <= 0 ||
                    fighter.fighter_count <= 0 ||
                    (MOX::combat_init == 0 && fighter.owner == player_idx) ||
                    TECHDATA::weapons[fighter.type].type != WEAPON_TYPE_FIGHTER ||
                    (!fighter.target_is_missile &&
                        fighter.target_unit_idx == fighter.source_unit_idx)) {
                    continue;
                }
                CMBTMIS::Reconsider_Fighter_Target(i);
            }
        }

        for (int fighter_pass = 0; fighter_pass < 2; ++fighter_pass) {
            const bool fighters_chasing_missiles = fighter_pass != 0;
            do {
                is_finished = 1;
                resolve_count = 0;
                int16_t screen_updated = 0;

                for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                    s_missile* missile = &MOX::missile[i];
                    int16_t target_idx = missile->target_unit_idx;
                    const bool target_is_missile = missile->target_is_missile != 0;

                    if (missile->type <= 0 || missile->travel_dist <= 0 ||
                        (target_is_missile && !fighters_chasing_missiles)) {
                        continue;
                    }

                    if (time_warp_only) {
                        if (missile->fighter_count == -1) {
                            continue;
                        }
                        if (TECHDATA::weapons[missile->type].type != WEAPON_TYPE_FIGHTER &&
                            CMBTMIS::missile_initiative != 0) {
                            continue;
                        }
                    } else if (MOX::combat_init != 0) {
                        if (missile->fighter_count == -1) {
                            continue;
                        }
                    } else if ((target_is_missile ? MOX::missile[target_idx].owner
                                                  : MOX::combat_data[target_idx].owner) != player_idx) {
                        if (player_idx != MOX::combat_defender_player || missile->source_unit_idx != target_idx ||
                            MOX::combat_data[missile->source_unit_idx].owner == player_idx) {
                            continue;
                        }
                    }

                    if (!target_is_missile) {
                        COMBINIT::Ship_Center_Offsets(target_idx, &target_x, &target_y);
                        target_x += MOX::combat_data[target_idx].x * 20;
                        target_y += MOX::combat_data[target_idx].y * 20;
                    } else {
                        target_x = MOX::missile[target_idx].x;
                        target_y = MOX::missile[target_idx].y;
                    }

                    uint8_t resolve_flag = CMBTMIS::Move_Missile(i, target_x, target_y, missile->speed);

                    if (CMBTMIS::Missile_On_Combat_Screen(i)) {
                        screen_updated = 1;
                    }

                    if (missile->x != target_x || missile->y != target_y) {
                        is_finished = 0;
                    }

                    if (resolve_flag == 1) {
                        if (TECHDATA::weapons[missile->type].type != WEAPON_TYPE_FIGHTER || missile->is_active == 1) {
                            resolved_missile_idx[resolve_count] = i;
                            resolved_target_idx[resolve_count] = target_idx;
                            resolve_count++;
                        }
                    }
                }

                if (screen_updated != 0) {
                    video::Set_Page_Off();
                    CMBTDRW1::Draw_Main_Combat_Screen();
                    RUSS::Mox_Sync_Update();
                }

                for (int16_t j = 0; j < resolve_count; ++j) {
                    int16_t m_idx = resolved_missile_idx[j];
                    int16_t t_idx = resolved_target_idx[j];
                    s_missile* missile = &MOX::missile[m_idx];

                    if (missile->target_unit_idx == t_idx) {
                        if (missile->target_is_missile != 0) {
                            target_x = MOX::missile[t_idx].x;
                            target_y = MOX::missile[t_idx].y;
                            CMBTMIS::Resolve_Missile(
                                m_idx, target_x, target_y, true);
                            continue;
                        }

                        s_combat_data* target_ship = &MOX::combat_data[t_idx];
                        if (target_ship->unit_status == 0) {
                            int16_t owner = CMBTMIS::Missile_Owner(m_idx);
                            if (owner != target_ship->owner || missile->source_unit_idx == t_idx) {
                                COMBINIT::Ship_Center_Offsets(t_idx, &target_x, &target_y);
                                target_x += target_ship->x * 20;
                                target_y += target_ship->y * 20;
                                MOX::cur_ship = t_idx;
                                CMBTMIS::Resolve_Missile(
                                    m_idx, target_x, target_y, true);
                            } else {
                                CMBTMIS::Kill_Missile(m_idx);
                            }
                        } else {
                            if (CMBTMIS::smarter_fighter_retargeting &&
                                TECHDATA::weapons[missile->type].type == WEAPON_TYPE_FIGHTER) {
                                CMBTMIS::Retarget_Missile(m_idx);
                            } else {
                                CMBTMIS::Kill_Missile(m_idx);
                            }
                        }
                    }
                }
            } while (is_finished == 0);
        }

        for (int16_t k = 0; k < MAX_COMBAT_FLYERS; ++k) {
            s_missile* missile = &MOX::missile[k];
            int16_t target_idx = missile->target_unit_idx;

            if (missile->type < 1 || TECHDATA::weapons[missile->type].type == WEAPON_TYPE_FIGHTER ||
                missile->fighter_count == -1 || missile->target_is_missile != 0) {
                if (missile->fighter_count == -1) {
                    missile->fighter_count = 0;
                }
            } else {
                bool should_increment = false;
                if (MOX::combat_init == 0) {
                    if (MOX::combat_data[target_idx].owner != player_idx) {
                        if (player_idx == MOX::combat_defender_player && missile->source_unit_idx == target_idx &&
                            MOX::combat_data[missile->source_unit_idx].owner != player_idx) {
                            should_increment = true;
                        }
                    } else {
                        should_increment = true;
                    }
                } else if (!time_warp_only) {
                    should_increment = true;
                }

                if (should_increment) {
                    missile->fighter_count++;
                }
            }
        }
    }

    int16_t __cdecl Ship_Initiative_Value(int16_t ship_idx) {
        int16_t initiative = -5;

        if (ship_idx != -1) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];
            initiative = (ship->tac_val_attack / 10) + ship->current_speed;
        }

        return initiative;
    }

    void __cdecl Set_Legal_Moves(int16_t ship_idx, int16_t range, int16_t check_cost) {
        int16_t footprint;
        int16_t ship_x;
        int16_t ship_y;
        int16_t x_pixel;
        int16_t y_pixel;

        const bool unused_sub_space_teleporter =
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_SUB_SPACE_TELEPORTER) != 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_PHASE_SHIFTER) == 0 &&
            CMBTFIR2::Used_Teleporter(ship_idx) == 0;
        if (unused_sub_space_teleporter) {
            range = CMBTMOV1::sub_space_teleporter_range;
        }

        s_combat_data* ship = &MOX::combat_data[ship_idx];
        footprint = (ship->size_class + 1) / 2;
        if (footprint > 2) {
            footprint = 2;
        }

        ship_x = ship->x;
        ship_y = ship->y;
        x_pixel = 0;

        for (int16_t x = 0; x < MOX::combat_grid_width; ++x) {
            y_pixel = 0;
            for (int16_t y = 0; y < MOX::combat_grid_height; ++y) {
                int16_t* grid_cell = &MOX::combat_grid[x][y];

                if (*grid_cell == -1) {
                    int16_t dx = ship_x - x;
                    if (dx < 0) dx = -dx;

                    int16_t dy = ship_y - y;
                    if (dy < 0) dy = -dy;

                    if (dx <= range && dy <= range) {
                        *grid_cell = 2000;

                        if (footprint == 1) {
                            for (int16_t f_dx = 0; f_dx < 2; ++f_dx) {
                                for (int16_t f_dy = 0; f_dy < 2; ++f_dy) {
                                    if ((x + f_dx >= MOX::combat_grid_width) || (y + f_dy >= MOX::combat_grid_height)) {
                                        *grid_cell = -1;
                                    } else {
                                        int16_t cell_val = MOX::combat_grid[x + f_dx][y + f_dy];
                                        if (cell_val != -1 && cell_val != 2000 && cell_val != ship_idx) {
                                            *grid_cell = -1;
                                        }
                                    }
                                }
                            }
                        } else if (footprint == 2) {
                            for (int16_t f_dx = 0; f_dx <= 2; ++f_dx) {
                                for (int16_t f_dy = 0; f_dy <= 2; ++f_dy) {
                                    if ((x + f_dx >= MOX::combat_grid_width) || (y + f_dy >= MOX::combat_grid_height)) {
                                        *grid_cell = -1;
                                    } else {
                                        int16_t cell_val = MOX::combat_grid[x + f_dx][y + f_dy];
                                        if (cell_val != -1 && cell_val != 2000 && cell_val != ship_idx) {
                                            *grid_cell = -1;
                                        }
                                    }
                                }
                            }
                        }

                        if ((MOX::combat_grid_width - footprint < x) || (MOX::combat_grid_height - footprint < y)) {
                            *grid_cell = -1;
                        }

                        if (*grid_cell == 2000 && check_cost == 1) {
                            int16_t cost = CMBTMOV1::Move_Cost(ship_idx, x, y);
                            if (ship->movement_left < cost) {
                                *grid_cell = -1;
                            }

                            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_SUB_SPACE_TELEPORTER)) {
                                if (CMBTFIR2::Used_Teleporter(ship_idx) == 0 &&
                                    CMBTFIRE::Special_Is_Turned_On(
                                        ship_idx, SPECIAL_SUB_SPACE_TELEPORTER) == 1) {
                                    const bool in_teleporter_range =
                                        CMBTMOV1::Teleporter_Destination_In_Range(
                                            ship_idx, x_pixel, y_pixel);
                                    if (CMBTMOV1::round_teleporter != 0 &&
                                        !in_teleporter_range) {
                                        *grid_cell = -1;
                                    } else if (*grid_cell == -1 &&
                                        in_teleporter_range) {
                                        *grid_cell = 2000;
                                    }
                                }
                            }
                        }
                    }
                }
                y_pixel += 20;
            }
            x_pixel += 20;
        }
    }

    void __cdecl Set_Combat_Help_Fields() {
        int16_t entry_idx = 0;

        if (MOX::order_info_flag == 1) {
            MOX::help_entry_list[0].help_id = 0x269;
            MOX::help_entry_list[0].x1 = 0x124;
            MOX::help_entry_list[0].y1 = 0x1be;
            MOX::help_entry_list[0].x2 = 0x176;
            MOX::help_entry_list[0].y2 = 0x1d8;

            MOX::help_entry_list[1].help_id = 0x26f;
            MOX::help_entry_list[1].x1 = 0x10f;
            MOX::help_entry_list[1].y1 = 0x16a;
            MOX::help_entry_list[1].x2 = 0x14a;
            MOX::help_entry_list[1].y2 = 0x182;

            MOX::help_entry_list[2].help_id = 0x270;
            MOX::help_entry_list[2].x1 = 0x14c;
            MOX::help_entry_list[2].y1 = 0x16a;
            MOX::help_entry_list[2].x2 = 0x18a;
            MOX::help_entry_list[2].y2 = 0x182;

            MOX::help_entry_list[3].help_id = 0x271;
            MOX::help_entry_list[3].x1 = 0x10f;
            MOX::help_entry_list[3].y1 = 0x185;
            MOX::help_entry_list[3].x2 = 0x14a;
            MOX::help_entry_list[3].y2 = 0x19e;

            MOX::help_entry_list[4].help_id = 0x272;
            MOX::help_entry_list[4].x1 = 0x14d;
            MOX::help_entry_list[4].y1 = 0x185;
            MOX::help_entry_list[4].x2 = 0x18a;
            MOX::help_entry_list[4].y2 = 0x19e;

            MOX::help_entry_list[5].help_id = 0x273;
            MOX::help_entry_list[5].x1 = 0x10f;
            MOX::help_entry_list[5].y1 = 0x1a2;
            MOX::help_entry_list[5].x2 = 0x14a;
            MOX::help_entry_list[5].y2 = 0x1b9;

            MOX::help_entry_list[6].help_id = 0x274;
            MOX::help_entry_list[6].x1 = 0x14d;
            MOX::help_entry_list[6].y1 = 0x1a2;
            MOX::help_entry_list[6].x2 = 0x18a;
            MOX::help_entry_list[6].y2 = 0x1b9;
            entry_idx = 7;
        } else {
            MOX::help_entry_list[0].help_id = 0x2b7;
            MOX::help_entry_list[0].x1 = 0x11a;
            MOX::help_entry_list[0].y1 = 0x15e;
            MOX::help_entry_list[0].x2 = 0x180;
            MOX::help_entry_list[0].y2 = 0x173;

            MOX::help_entry_list[1].help_id = 0x2b8;
            MOX::help_entry_list[1].x1 = 0x108;
            MOX::help_entry_list[1].y1 = 0x17f;
            MOX::help_entry_list[1].x2 = 0x197;
            MOX::help_entry_list[1].y2 = 0x18c;

            MOX::help_entry_list[2].help_id = 0x2b9;
            MOX::help_entry_list[2].x1 = 0x108;
            MOX::help_entry_list[2].y1 = 0x18f;
            MOX::help_entry_list[2].x2 = 0x197;
            MOX::help_entry_list[2].y2 = 0x19c;

            MOX::help_entry_list[3].help_id = 0x2ba;
            MOX::help_entry_list[3].x1 = 0x108;
            MOX::help_entry_list[3].y1 = 0x19f;
            MOX::help_entry_list[3].x2 = 0x197;
            MOX::help_entry_list[3].y2 = 0x1ac;

            MOX::help_entry_list[4].help_id = 0x2bb;
            MOX::help_entry_list[4].x1 = 0x108;
            MOX::help_entry_list[4].y1 = 0x1af;
            MOX::help_entry_list[4].x2 = 0x197;
            MOX::help_entry_list[4].y2 = 0x1bc;

            MOX::help_entry_list[5].help_id = 0x2bc;
            MOX::help_entry_list[5].x1 = 0x108;
            MOX::help_entry_list[5].y1 = 0x1bf;
            MOX::help_entry_list[5].x2 = 0x197;
            MOX::help_entry_list[5].y2 = 0x1cc;
            entry_idx = 6;
        }

        MOX::help_entry_list[entry_idx].help_id = 0x26a;
        MOX::help_entry_list[entry_idx].x1 = 0x0a;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x6a;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x1f6;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x272;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        MOX::help_entry_list[entry_idx].help_id = 0x26d;
        if (MOX::missile_warning != 1) {
            MOX::help_entry_list[entry_idx].help_id = 0x26c;
        }
        entry_idx++;

        MOX::help_entry_list[entry_idx].help_id = 0x26e;
        MOX::help_entry_list[entry_idx].x1 = 0x18f;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x1ef;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        entry_idx++;

        const bool has_current_ship = MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count;
        if (has_current_ship && MOX::ship_status_display == 0) {
            for (int j = 0; j < 8; j++) {
                uint16_t tech_app_id = TECHDATA::weapons[MOX::combat_data[MOX::cur_ship].weapons[j].weapon_id].tech_app_id;
                if (tech_app_id != 0) {
                    MOX::help_entry_list[entry_idx + j].help_id = tech_app_id;
                } else {
                    MOX::help_entry_list[entry_idx + j].help_id = -1;
                }
            }
        } else if (has_current_ship) {
            int special_count = 0;
            for (int k = 0; k < 8; k++) {
                MOX::help_entry_list[entry_idx + k].help_id = -1;
            }
            for (int j = 0; j < 39 && special_count < 8; j++) {
                if (struct_::Test_Bit_Field(MOX::combat_data[MOX::cur_ship].special_device_flags, j) == 1) {
                    MOX::help_entry_list[entry_idx + special_count].help_id = TECHDATA::specials[j].tech_app_id;
                    special_count++;
                }
            }
        } else {
            for (int j = 0; j < 8; j++) {
                MOX::help_entry_list[entry_idx + j].help_id = -1;
            }
        }

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x17b;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x185;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x186;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x190;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x191;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x19b;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x19c;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1a6;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1a7;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1b1;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1b2;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1bc;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1bd;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1c7;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1c8;
        (void)(MOX::ship_status_display == 0);
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1d3;
        entry_idx++;

        MOX::help_entry_list[entry_idx].help_id = 0x275;
        MOX::help_entry_list[entry_idx].x1 = 0x71;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x109;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        entry_idx++;

        fields::Set_Help_List(MOX::help_entry_list, entry_idx);
    }

    bool __cdecl Weapon_Is_On(int16_t ship_idx, int16_t weapon_idx) {
        return MOX::combat_data[ship_idx].weapons[weapon_idx].active == 1;
    }
} // namespace COMBAT1
