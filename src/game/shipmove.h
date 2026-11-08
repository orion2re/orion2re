#pragma once
#include <cstdint>
#include "types.h"

struct s_wormhole_data;

namespace SHIPMOVE {
    extern uint8_t jump_gate_speed_bonus;
    extern uint16_t warp_interdictor_range;
    extern bool jump_gate_pass_nebula;
    extern s_ratio16 phase_shifter_speed_multiplier;

    struct s_ship_communication_range {
        uint8_t tachyon;
        uint8_t sub_space;
    };

    extern s_ship_communication_range ship_communication_range;

    void __cdecl Allocate_New_Ship_Slots();
    void __cdecl Move_All_Ships_Toward_Stars();
    void __cdecl Reset_Needless_Ignore_Combat_Ships_Flags();

    void __cdecl Update_All_Ship_Ranges();

    bool __cdecl Star_In_Normal_Range_Of_Player(s_star_data* star, int16_t player_idx);

    bool __cdecl Star_In_Extended_Range_Of_Player(s_star_data* star, int16_t player_idx);

    bool __cdecl Star_In_Range_Of_Player_Aux1(s_star_data* star, int16_t player_idx, int16_t dist);
    bool __cdecl Star_In_Range_Of_Player_Aux2(s_star_data* star, int16_t player_idx, int16_t dist);
    bool __cdecl Star_In_Range_Of_Player(s_star_data* star, int16_t player_idx, int16_t dist);

    void __cdecl Update_Player_Ship_Range(s_player* player);

    void __cdecl Ships_Try_To_Move_To(int16_t* ship_list, int16_t target_star_idx, s_ship_move_info* move_info);
    void Evaluate_Fleet_Move_From_Star(int16_t* ship_list, int16_t target_star_idx, int16_t origin_star, s_ship_move_info* move_info);

    void __cdecl Make_Ships_Move_To(int16_t* ship_list, int16_t destination_star_idx, s_ship_move_info* move_info);
    bool __cdecl Apply_Player_Movement_Order(int16_t* ship_list, int16_t destination_star_idx, s_ship_move_info* move_info);

    bool __cdecl Confirm_Officer_Change(int16_t leader_idx, int16_t target_ship_idx, int8_t target_eta);
    void __cdecl Recalculate_Moving_Fleet(int16_t ship_idx);
    void __cdecl Recalculate_All_Moving_Fleets();

    int16_t __cdecl Create_Nonplayer_Ship(int8_t owner, s_ship_design* design, int16_t x, int16_t y);

    bool __cdecl Player_Has_Ships_Pestering_Other_Player(int player_idx, uint8_t colony_bit_idx);

    void __cdecl Unmark_Just_Built_AI_Ships();

    int16_t __cdecl Player_N_Turns_From_Star_To_Star(int16_t player_idx, int16_t start_star_idx, int16_t end_star_idx);

    int16_t __cdecl Ship_Destination_Star(s_ship_data* ship);

    int16_t __cdecl Ship_Range(s_ship_data* ship);

    bool __cdecl Can_Order_Ship(s_ship_data* ship);

    bool __cdecl Ship_Is_Moving(const s_ship_data* ship);

    void __cdecl Make_Ship_Arrive_At_Star(s_ship_data* ship, int16_t star_idx);
    void __cdecl Update_Ship_ETAs(int16_t player_id);

    void __cdecl Do_Hyperspace_Beast(int32_t player_idx);

    void __cdecl Do_Wormhole_Event(int32_t player_idx, s_wormhole_data* wormhole);
} // namespace SHIPMOVE