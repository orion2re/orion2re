#include "pch.h"

#include "combat1_turn_order.h"

namespace COMBAT1 {
    bool strict_wait_stage;
    int16_t side_wait_button;
    static bool strict_ship_waited[MAX_COMBAT_SHIPS];
    static int16_t strict_initial_order[MAX_COMBAT_SHIPS];
    static bool side_waiting[15];

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

    int16_t __cdecl Ship_Controlled_By_Computer(int16_t ship_idx) {
        int16_t is_computer = 0;
        int8_t owner = MOX::combat_data[ship_idx].owner;

        if (((owner >= MAX_PLAYERS) || (MOX::player[owner].objectives != PLAYER_OBJECTIVE_HUMAN) || (MOX::auto_combat == 1)) &&
            (MOX::take_control_flag == 0)) {
            is_computer = 1;
        }

        return is_computer;
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
            return 0;
        }

        MOX::cur_ship = -1;
        MOX::cur_ptr = -1;
        return 1;
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
} // namespace COMBAT1
