#include "pch.h"

#include "game/boarding.h"
#include "game/techdata.h"

namespace {
    s_combat_data combat_data[2];
    s_ship_data ships[2];
    s_player players[2];
    s_leader_data leaders[1];
    int16_t killed_officer;
    int16_t pooled_officer;
    int16_t raid_choice_idx;
    int16_t fixed_raid_choice;
    int16_t combat_shield_max;
    int16_t boarding_attacker_bonus;
    int16_t boarding_defender_bonus;
    uint32_t capture_random_value;
    int16_t paragraph_values[8];
    std::string last_raid_message;
    bool last_result_was_timed;
    int boarding_init_count;
    int warning_count;
    char last_warning[80];
    int network_message_count;
    int16_t network_marines;
    int16_t network_action;
    int network_wait_count;
    char weapon_names[8][1024] = {
        "Weapon zero with a deliberately long raid report name",
        "Weapon one with a deliberately long raid report name",
        "Weapon two with a deliberately long raid report name",
        "Weapon three with a deliberately long raid report name",
        "Weapon four with a deliberately long raid report name",
        "Weapon five with a deliberately long raid report name",
        "Weapon six with a deliberately long raid report name",
        "Weapon seven with a deliberately long raid report name"
    };
    char weapon_plural_names[8][1024] = {
        "Weapon zeroes with a deliberately long raid report name",
        "Weapon ones with a deliberately long raid report name",
        "Weapon twos with a deliberately long raid report name",
        "Weapon threes with a deliberately long raid report name",
        "Weapon fours with a deliberately long raid report name",
        "Weapon fives with a deliberately long raid report name",
        "Weapon sixes with a deliberately long raid report name",
        "Weapon sevens with a deliberately long raid report name"
    };
    char special_names[8][1024] = {
        "Special device zero with a deliberately long raid report name",
        "Special device one with a deliberately long raid report name",
        "Special device two with a deliberately long raid report name",
        "Special device three with a deliberately long raid report name",
        "Special device four with a deliberately long raid report name",
        "Special device five with a deliberately long raid report name",
        "Special device six with a deliberately long raid report name",
        "Special device seven final marker"
    };
}

namespace TEST_BOARDING {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(ships, 0, sizeof(ships));
        memset(players, 0, sizeof(players));
        memset(leaders, 0, sizeof(leaders));
        memset(MOX::crew_data, 0, sizeof(MOX::crew_data));
        memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
        memset(TECHDATA::specials, 0, sizeof(TECHDATA::specials));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        for (s_combat_data& ship : combat_data) {
            ship.officer_idx = -1;
            ship.ship_idx = -1;
            ship.stasis_source_idx = COMBAT_SHIP_NONE;
            ship.black_hole_source_idx = COMBAT_SHIP_NONE;
        }
        for (s_ship_data& ship : ships) {
            ship.officer_index = -1;
        }
        memset(MOX::ship_turn_order, -1, sizeof(MOX::ship_turn_order));
        MOX::ship_turn_order[0] = -10;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::combat_total_ship_count = 2;
        MOX::auto_combat = 0;
        MOX::auto_combat_flag[0] = 0;
        MOX::auto_combat_flag[1] = 0;
        MOX::super_fast_flag = 0;
        MOX::in_nebula_flag = 0;
        MOX::ken_trace = 0;
        killed_officer = -1;
        pooled_officer = -1;
        raid_choice_idx = 0;
        fixed_raid_choice = -1;
        combat_shield_max = 0;
        boarding_attacker_bonus = 0;
        boarding_defender_bonus = 0;
        capture_random_value = 0;
        memset(paragraph_values, 0, sizeof(paragraph_values));
        last_raid_message.clear();
        last_result_was_timed = false;
        boarding_init_count = 0;
        warning_count = 0;
        last_warning[0] = '\0';
        network_message_count = 0;
        network_marines = -1;
        network_action = -1;
        network_wait_count = 0;
        BOARDING::telepathic_multiple_capture = 0;
        BOARDING::kill_excess_marines_after_capture = 0;
        BOARDING::board_in_black_hole = 1;
        BOARDING::board_range = {3, 12};
        BOARDING::boarding.damper_survival_divisor = 3;
        BOARDING::boarding.auto_close_results = 0;
        BOARDING::boarding.raid_damage_per_marine = 50;
        BOARDING::boarding.raid_items_per_marine = 2;
        CMBTFIR2::combat_bonus = {
            50, 20, 0, 10, 20, 50, 100, 0, 80, 80, 10, 30, 15,
        };
        CMBTFIR2::quantum_detonator.explode_chance = 6;
    }

    int16_t Killed_Officer_() {
        return killed_officer;
    }

    int16_t Pooled_Officer_() {
        return pooled_officer;
    }

    void Set_Combat_Shield_Max_(int16_t value) {
        combat_shield_max = value;
    }

    void Set_Raid_Choice_(int16_t value) {
        fixed_raid_choice = value;
    }

    int Raid_Selection_Count_() {
        return raid_choice_idx;
    }

    void Prepare_Long_Raid_() {
        static const char* item_names[8] = {
            "zero", "one", "two", "three", "four", "five", "six", "seven",
        };
        static const char* plural_item_names[8] = {
            "zeroes", "ones", "twos", "threes", "fours", "fives", "sixes", "sevens",
        };

        combat_data[0].owner = 0;
        combat_data[1].owner = 1;
        strlcpy(combat_data[1].name, "Target", sizeof(combat_data[1].name));
        combat_data[1].drive_current_hits = 100;
        combat_data[1].computer_current_hits = 100;
        combat_data[1].shield_current_hits = 100;

        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[1].objectives = PLAYER_OBJECTIVE_HUMAN;

        for (int i = 0; i < 8; ++i) {
            snprintf(
                weapon_names[i],
                sizeof(weapon_names[i]),
                "Weapon %s with a deliberately long raid report name",
                item_names[i]);
            snprintf(
                weapon_plural_names[i],
                sizeof(weapon_plural_names[i]),
                "Weapon %s with a deliberately long raid report name",
                plural_item_names[i]);
            snprintf(
                special_names[i],
                sizeof(special_names[i]),
                i == 7
                    ? "Special device %s final marker"
                    : "Special device %s with a deliberately long raid report name",
                item_names[i]);
            combat_data[1].weapons[i].weapon_id = (uint16_t)(i + 1);
            combat_data[1].weapons[i].count = 1;
            combat_data[1].weapon_hits[i] = 100;
            combat_data[1].weapon_hits_per_unit[i] = 100;
            combat_data[1].weapon_total_hits[i] = 100;
            combat_data[1].special_hits[i] = i == 0 ? 200 : 10;
            combat_data[1].special_device_hits[i] = combat_data[1].special_hits[i];
            struct_::Set_Bit_Field(combat_data[1].special_device_flags, i + 1);

            TECHDATA::weapons[i + 1].name = weapon_names[i];
            TECHDATA::weapons[i + 1].name_plural = weapon_plural_names[i];
            TECHDATA::specials[i + 1].name = special_names[i];
        }
    }

    void Prepare_Oversized_Raid_() {
        Prepare_Long_Raid_();

        for (int i = 0; i < 8; ++i) {
            memset(weapon_names[i], 'a' + i, sizeof(weapon_names[i]) - 1);
            weapon_names[i][sizeof(weapon_names[i]) - 1] = '\0';
            memset(weapon_plural_names[i], 'k' + i, sizeof(weapon_plural_names[i]) - 1);
            weapon_plural_names[i][sizeof(weapon_plural_names[i]) - 1] = '\0';
            memset(special_names[i], 'A' + i, sizeof(special_names[i]) - 1);
            special_names[i][sizeof(special_names[i]) - 1] = '\0';
        }
        strlcpy(
            &special_names[7][sizeof(special_names[7]) - 40],
            "oversized raid report final marker",
            40);
    }

    const char* Last_Raid_Message_() {
        return last_raid_message.c_str();
    }

    int16_t Boarding_Attacker_Bonus_() {
        return boarding_attacker_bonus;
    }

    int16_t Boarding_Defender_Bonus_() {
        return boarding_defender_bonus;
    }

    int16_t Paragraph_Value_(int16_t index) {
        return paragraph_values[index];
    }

    bool Last_Result_Was_Timed_() {
        return last_result_was_timed;
    }

    void Set_Random_Value_(uint32_t value) {
        capture_random_value = value;
    }

    int Boarding_Init_Count_() {
        return boarding_init_count;
    }

    int Warning_Count_() {
        return warning_count;
    }

    const char* Last_Warning_() {
        return last_warning;
    }

    int Network_Message_Count_() {
        return network_message_count;
    }

    int16_t Network_Marines_() {
        return network_marines;
    }

    int16_t Network_Action_() {
        return network_action;
    }

    int Network_Wait_Count_() {
        return network_wait_count;
    }
}

namespace MOX {
    s_combat_data* combat_data = ::combat_data;
    s_ship_data* ship = ships;
    s_player* player = players;
    s_leader_data* leaders = ::leaders;
    int16_t ship_turn_order[MAX_COMBAT_SHIPS];
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    int16_t combat_total_ship_count;
    int16_t cur_ship;
    int16_t cur_ptr;
    int16_t combat_turn;
    int16_t cur_combat_x;
    int16_t cur_combat_y;
    int16_t board_flag;
    int16_t net_flag;
    int16_t net_destination;
    int16_t PLAYER_NUM;
    int16_t NUM_PLAYERS;
    int16_t auto_combat;
    int16_t auto_combat_flag[2];
    int16_t super_fast_flag;
    int16_t in_nebula_flag;
    int16_t ordnance_bonus[15];
    uint16_t apply_damage_to_planet;
    uint16_t ken_trace;
    s_crew_data crew_data[CREW_QUALITY_COUNT];
} // namespace MOX

namespace OFFICER {
    void __cdecl Kill_Officer(int16_t officer_idx) {
        killed_officer = officer_idx;
    }

    void __cdecl Move_Officer_To_Pool(int16_t officer_idx) {
        pooled_officer = officer_idx;
    }

    int16_t __cdecl Owned_Officer_Level(int16_t) {
        return 0;
    }
}

namespace COMBINIT {
    int16_t __cdecl Does_Combat_Ship_Have_Special(int16_t ship_idx, int16_t special_id) {
        const uint8_t mask = (uint8_t)(1u << (special_id % 8));
        const s_combat_data& ship = MOX::combat_data[ship_idx];
        return (ship.special_device_flags[special_id / 8] & mask) != 0 &&
               (ship.special_device_damage_flags[special_id / 8] & mask) == 0;
    }

    int16_t __cdecl Calc_Combat_Shield_Max(int16_t) {
        return combat_shield_max;
    }

    void __cdecl Ship_Center_Offsets(int16_t, int16_t* x, int16_t* y) {
        *x = 0;
        *y = 0;
    }
}

namespace CMBTFIRE {
    void __cdecl Destroy_Ship(int16_t, int16_t, int16_t, int16_t) {
    }

    void __cdecl Retarget_Missiles_Aimed_At(int16_t) {
    }

    int16_t __cdecl Is_Combat_Ship_Dead(int16_t) {
        return 0;
    }

    void __cdecl Remove_Stasis_Beam(int16_t) {
    }

    int16_t __cdecl Facing_Shield(int16_t) {
        return 0;
    }

    int16_t __cdecl Does_Combat_Planet_Have_Defenses() {
        return 0;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return capture_random_value;
    }

    uint32_t __cdecl Get_Random_Seed() {
        return 0;
    }
}

namespace invasion {
    void __cdecl Get_Boarding_Info(
        int16_t,
        int16_t,
        int16_t attacker_bonus,
        int16_t,
        int16_t,
        int16_t defender_bonus,
        s_invasion_side_info* side_a,
        s_invasion_side_info* side_b) {
        boarding_attacker_bonus = attacker_bonus;
        boarding_defender_bonus = defender_bonus;
        memset(side_a, 0, sizeof(*side_a));
        memset(side_b, 0, sizeof(*side_b));
        side_a->unit_strengths[1] = 100;
        side_a->combat_modifiers.difficulty_bonus = 2;
        side_b->combat_modifiers.difficulty_bonus = 2;
    }

    void __cdecl Ground_Combat_Round(
        s_invasion_side_info*,
        s_invasion_side_info*) {
    }
}

namespace FMTPARA {
    void __cdecl Set_Paragraph_Sint_Pointer(uint16_t index, void* value) {
        paragraph_values[index] = *(int16_t*)value;
    }
}

namespace RUSS {
    int8_t all_ai_game = 0;
    int16_t __cdecl Get_Weighted_Choice_Short(const uint16_t*, int16_t) {
        static const int16_t choices[20] = {
            2, 3, 4, 5, 6, 7, 8, 9,
            10, 11, 12, 13, 14, 15, 16, 17,
            0, 1, 10, 10
        };
        const int16_t choice = fixed_raid_choice >= 0
            ? fixed_raid_choice
            : choices[raid_choice_idx % 20];
        raid_choice_idx++;
        return choice;
    }
}

namespace KEN {
    void __cdecl Ken_Get_Text_Message(int32_t msg_index, char* buffer, int16_t message_arg) {
        const char* message = "";
        switch (msg_index) {
        case 21: message = "Computer "; break;
        case 42: message = "Raid on "; break;
        case 43: message = "Drive damaged\n"; break;
        case 44: message = "Shield "; break;
        case 45: message = "damaged\n"; break;
        case 46: message = "destroyed\n"; break;
        case 47: message = "No systems damaged\n"; break;
        case 92: message = "Attacker strength "; break;
        case 93: message = "Defender strength "; break;
        case 94: message = "attacking marine killed\n"; break;
        case 95: message = "attacking marines killed\n"; break;
        case 96: message = "defending marine killed\n"; break;
        case 97: message = "defending marines killed\n"; break;
        }
        strlcpy(buffer, message, (size_t)message_arg);
    }
}

namespace ERICNET {
    int __cdecl Mox_Play_Sound(int16_t) {
        return 0;
    }
}

namespace sound {
    void __cdecl Fadeout_Sound(int16_t, int8_t) {
    }
}

namespace CMBTDRW1 {
    void Present_Tactical_Warning(const char* text) {
        GENDRAW::Warning_Box(text);
    }
    void __cdecl Refresh_Combat_Screen_Full() {
    }

    int16_t __cdecl Relative_Bearing(int16_t, int16_t) {
        return 0;
    }

    int16_t __cdecl Board_Popup(int16_t, int16_t, char*, int16_t*) {
        boarding_init_count++;
        return 0;
    }
    int16_t Board_Popup(const s_board_popup_input*, int16_t*) {
        boarding_init_count++;
        return 0;
    }
}

namespace CMBTMOV1 {
    bool __cdecl Ship_In_Black_Hole(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].black_hole_source_idx != COMBAT_SHIP_NONE;
    }

    int8_t __cdecl Ship_Is_In_Stasis(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].stasis_source_idx != COMBAT_SHIP_NONE;
    }

    void __cdecl Wait_Until_Net_Opponent_Finished(int16_t) {
        network_wait_count++;
    }
}

namespace CMBTSPEC {
    void __cdecl Draw_Cloak(int16_t, int16_t) {
    }
}

namespace GENDRAW {
    void __cdecl Warning_Box(const char* message) {
        warning_count++;
        snprintf(last_warning, sizeof(last_warning), "%s", message);
    }
}

namespace palette {
    void __cdecl Refresh_Palette() {
    }
}

namespace NETMOX {
    void __cdecl Net_Debug_Log(const char*, ...) {
    }

    int32_t __cdecl Mox_Send_Message(
        int16_t,
        uint16_t message_type,
        void* message,
        uint32_t message_size
    ) {
        if (message_type == TACTICAL_NET_PACKET_ACTION &&
            message_size == sizeof(s_net_tactical_combat_message)) {
            const s_net_tactical_combat_message* combat_message =
                (const s_net_tactical_combat_message*)message;
            network_message_count++;
            network_marines = combat_message->arg2;
            network_action = combat_message->arg3;
        }
        return 0;
    }
}

namespace special {
    int16_t __cdecl Range(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        const int16_t dx = abs(x2 - x1);
        const int16_t dy = abs(y2 - y1);
        return dx > dy ? dx + dy / 2 : dy + dx / 2;
    }
}

namespace CMBTDRW1 {
    void Present_Boarding_Result(const char* title, const char* text, uint16_t ticks, bool) {
        if (MOX::super_fast_flag != 0) return;
        if (ticks)
            TEXTBOX::Timed_Text_Box(title, text, ticks);
        else
            TEXTBOX::Text_Box(title, text);
    }
} // namespace CMBTDRW1

namespace TEXTBOX {
    void __cdecl Text_Box(const char*, const char* message) {
        last_result_was_timed = false;
        last_raid_message = message;
    }

    void __cdecl Timed_Text_Box(const char*, const char* message, uint16_t) {
        last_result_was_timed = true;
        last_raid_message = message;
    }
} // namespace TEXTBOX

namespace string {
    const char s_Newline[] = "\n";
}

namespace TECHDATA {
    s_tech_computer_data computers[COMPUTER_COUNT];
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
}
