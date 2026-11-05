#pragma once
#include <cstdint>

namespace COMBAT1 {
    enum e_operation { NONE,
        SELECT,
        MOVE,
        FACE,
        FIRE,
        DONE,
        FIRE_SELECTION,
        BOARD,
        RETREAT,
        WAIT };
    enum e_result : int { APPLIED,
        UNAVAILABLE,
        NO_CONTROL,
        CONFLICT,
        ILLEGAL,
        LOCAL_REQUIRED };
    struct s_operation {
        e_operation kind = NONE;
        int16_t actor = -1, target = -1, x = 0, y = 0;
        bool missile_target = false;
        uint8_t bank_mask = 0;
        int8_t bank_states[8] = {};
        int16_t boarding_mode = 0, marines = 0;
    };
    extern int32_t end_of_turn_button;

    extern uint8_t interactive_combat_stats;
    extern uint8_t td_combat_speed_bonus;
    extern uint8_t continuous_bhg;
    extern uint8_t non_ics_tractor_and_bhg_mode;
    extern uint8_t phasing_cloak_decloak_defense;
    extern uint8_t tractors_to_stop[SHIP_SIZE_COUNT];
    extern uint8_t recloak_next_turn;
    extern uint8_t rephase_cloak_next_turn;
    extern int8_t ship_initiative;
    extern uint16_t wait_movement_cost;
    extern uint8_t max_combat_turns;
    extern uint8_t absorber_keep_charge;
    extern uint8_t disable_scan;
    extern bool center_on_next_ship;

    inline void Handle_Destroyed_Cloak(
        int8_t* cloak_state,
        uint8_t phasing_timeout,
        bool phasing_cloak_destroyed,
        bool has_ordinary_cloak,
        bool has_phasing_cloak) {
        if (phasing_timeout != 0) {
            if (has_ordinary_cloak && phasing_cloak_destroyed) {
                if (*cloak_state == 4) {
                    *cloak_state = 1;
                } else if (*cloak_state == 6) {
                    *cloak_state = 2;
                } else if (*cloak_state == 7) {
                    *cloak_state = 3;
                }
            } else if (phasing_cloak_destroyed) {
                *cloak_state = 6;
            }
        } else if (!has_ordinary_cloak && !has_phasing_cloak) {
            *cloak_state = 2;
        }
    }

    inline int16_t Adjust_Movement_After_Speed_Change(
        int16_t old_speed,
        int16_t new_speed,
        int16_t maximum_speed,
        int16_t movement_left,
        int16_t* movement_reserve) {
        const int16_t speed_delta = new_speed - old_speed;
        int16_t movement_delta;

        if (speed_delta < 0) {
            movement_delta = -movement_left;
            if (movement_delta < speed_delta) {
                movement_delta = speed_delta;
            }
        } else {
            movement_delta = speed_delta;
            if (*movement_reserve - movement_delta < maximum_speed - new_speed) {
                movement_delta = *movement_reserve - (maximum_speed - new_speed);
                if (movement_delta < 0) {
                    movement_delta = 0;
                }
            }
        }

        *movement_reserve -= movement_delta;
        return movement_left + movement_delta;
    }

    inline void Restore_Captured_Ship_To_Turn_Order(
        int16_t* turn_order,
        int16_t turn_order_count,
        int16_t ship_idx) {
        int16_t empty_idx = -1;

        for (int16_t i = 0; i < turn_order_count; ++i) {
            const int16_t order_ship_idx = turn_order[i];
            if (order_ship_idx == ship_idx) {
                return;
            }
            if (order_ship_idx == -1) {
                empty_idx = i;
                continue;
            }
            if (order_ship_idx != -10) {
                continue;
            }

            if (empty_idx >= 0) {
                turn_order[empty_idx] = ship_idx;
            } else if (i + 1 < turn_order_count) {
                turn_order[i] = ship_idx;
                turn_order[i + 1] = -10;
            }
            return;
        }
    }

    // Refreshes speed and, when configured, current-turn movement and combat bonuses.
    // tractor_applied: nonzero when this refresh immediately follows a new tractor link.
    void Recalculate_Combat_Ship(int16_t ship_idx, bool tractor_applied = false);

    // Returns nonzero when the indexed combat ship is active, uncaptured, and not in stasis.
    // ship_idx: combat ship index to test.
    int16_t __cdecl Combat_Ship_Is_Functional(int16_t ship_idx);

    bool Combat_Ship_Participates_In_Pass(int16_t ship_idx, bool time_warp_only);
    bool Combat_Ship_Participates_In_Current_Pass(int16_t ship_idx);
    bool Combat_Ship_Participates_In_Turn_Order(
        int16_t ship_idx, bool time_warp_only);

    // Resets and refreshes per-turn combat state for one ship.
    // ship_idx: combat ship index to initialize.
    void __cdecl Init_Ship_For_Start_Of_Turn(int16_t ship_idx);

    // Compares two ship indices by tactical initiative for qsort.
    // p1/p2: pointers to int16_t ship indices.
    int __cdecl qsort_ship_initiative(const void* p1, const void* p2);

    // Computes the initiative score used for combat turn ordering.
    // ship_idx: combat ship index to score.
    int16_t __cdecl Ship_Initiative_Value(int16_t ship_idx);

    // Resets transient state used by strict two-stage combat sequencing.
    void Reset_Strict_Combat_Sequence();
    void Reset_Combat_Side_Waits();

    // Selects the next strict-mode ship without updating combat presentation.
    // action: disposition of the previously selected ship.
    int16_t Select_Next_Strict_Ship(COMBAT_SHIP_ACTION action);
    int16_t Select_Next_Classic_Ship(COMBAT_SHIP_ACTION action);
    bool Combat_Side_Wait_Is_Available();
    int16_t Add_Combat_Side_Wait_Field();
    bool Consume_Combat_Side_Wait_Input(int16_t* input_value);
    int16_t Select_After_Combat_Side_Wait();

    extern uint16_t start_of_turn;

    // Advances to the next eligible unit in the combat turn-order list.
    // unused_player_idx: reserved caller context, currently unused.
    int16_t __cdecl Next_Ship(int16_t unused_player_idx);

    int16_t __cdecl Ship_Controlled_By_Computer(int16_t ship_idx);

    // Sends one encoded tactical-combat message to the remote player.
    // player_idx: destination player index; message_code/arg1/arg2/arg3: encoded payload values.
    int32_t __cdecl Send_Tactical_Combat_Message(int16_t player_idx, int16_t message_code, int16_t arg1, int16_t arg2, int16_t arg3);
    int32_t Send_Combat_Side_Wait_Message(int16_t player_idx);

    void __cdecl Add_Tactical_Combat_Fields(void);
    bool Ship_Wait_Is_Available();

    void __cdecl Add_Missile_Fields(void);

    // Rebuilds occupancy and move overlays for the combat grid.
    // active_ship_idx: ship whose movement overlay should be refreshed.
    void __cdecl Assign_Combat_Grids(int16_t active_ship_idx);

    // Marks reachable combat-grid cells for the given ship.
    // ship_idx: combat ship index; range: movement range to test; check_cost: nonzero to enforce move-cost limits.
    void __cdecl Set_Legal_Moves(int16_t ship_idx, int16_t range, int16_t check_cost);

    void __cdecl Assign_Mouse_Images();

    void __cdecl Set_Cur_Ship_To(int16_t ship_idx);

    void __cdecl Set_Combat_Help_Fields();

    int16_t __cdecl Enemy_Dissipator_Exists(int16_t player_idx);

    // Validates and resolves a boarding attempt against the selected target.
    // attacker_idx/defender_idx: combat ship indices.
    void __cdecl Initiate_Boarding_Action(int16_t attacker_idx, int16_t defender_idx);
    void Commit_Boarding_Action(int16_t attacker_idx, int16_t defender_idx, int16_t mode, int16_t marines);
    void Apply_Received_Boarding_Action(int16_t attacker, int16_t target, int16_t marines, int16_t mode);

    // Advances and resolves seeking missiles for the current combat pass.
    // player_idx: player whose pass is being processed.
    void __cdecl Seeking_Missiles(int16_t player_idx, bool time_warp_only);

    // Runs one tactical-combat turn pass.
    // active_player_id: side being processed; time_warp_only: nonzero limits work to the special-gated pass.
    void __cdecl Do_Combat_Turn(int16_t active_player_id, int16_t time_warp_only);
    void Apply_Tactical_Operation(const s_operation* operation,
        int16_t active_player_id,
        int16_t* next_ship_flag,
        bool received = false);
    bool Consume_Tactical_Operation(int16_t active_player_id, int16_t* next_ship_flag);
    void Cycle_Weapon_Bank(int16_t slot, bool received = false);
    // Restores an eligible ordinary cloak after its configured firing delay.
    // ship_idx: combat ship index whose ordinary cloak state should be maintained.
    void Update_Ordinary_Cloak(int16_t ship_idx);
    // Applies end-of-round phasing-cloak timer countdown and reversion for all combat ships.
    void __cdecl Update_Phasing_Cloak_Timers(bool time_warp_only = false);
    int16_t __cdecl Check_For_Winner();
    int16_t __cdecl Calc_Current_Speed(int16_t ship_idx);

    bool Validate_Retreating_Ship(int16_t ship_idx);
    void __cdecl Retreat_Ship(int16_t ship_idx);
    int16_t Check_Human_Retreat(int16_t ship_idx);

    void __cdecl Remove_Continuing_Effect_Specials(int16_t source_idx);

    int16_t __cdecl Get_Number_Of_Ships_In_Stasis(int16_t source_ship_idx);

    bool __cdecl Weapon_Is_On(int16_t ship_idx, int16_t weapon_idx);
} // namespace COMBAT1