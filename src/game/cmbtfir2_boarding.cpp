#include "pch.h"

namespace CMBTFIR2 {
    void __cdecl Set_Boarded_Flag(int16_t ship_idx) {
        struct_::Set_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 1);
    }

    int16_t __cdecl Already_Boarded(int16_t ship_idx) {
        return (int16_t)struct_::Test_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 1);
    }

    int16_t __cdecl Init_Boarding_Action(int16_t unit_idx, int16_t* boarding_choice_out) {
        char message_buffer[80];
        int16_t boarding_strength;

        KEN::Ken_Get_Text_Message(41 /* 0x29 */, message_buffer, sizeof(message_buffer));

        boarding_strength = (MOX::combat_data[unit_idx].marine_count + 1) / 2;

        return CMBTDRW1::Board_Popup(0, boarding_strength, message_buffer, boarding_choice_out);
    }

    void __cdecl Select_Raid_Internal(int16_t ship_idx, int16_t* raid_type, int16_t* component_idx) {
        uint16_t weights[20];
        int32_t total_space;
        int16_t choice;

        game_random::Random(100);
        *raid_type = -1;

        total_space = CMBTFIR2::Total_Internal_Space(ship_idx);
        memset(weights, 0, sizeof(weights));

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        weights[0] = ship->drive_current_hits == 1 ? 1 : ship->drive_current_hits / 2;
        weights[1] = ship->computer_current_hits;

        for (int i = 0; i < 8; ++i) {
            weights[i + 2] = ship->weapon_hits[i];
        }

        for (int i = 0; i < 8; ++i) {
            weights[i + 10] = ship->special_hits[i];
        }

        weights[18] = ship->shield_current_hits;

        for (int i = 0; i < 19; ++i) {
            if (weights[i] != 0) {
                // The extra point keeps a fully intact final subsystem eligible for a raid.
                weights[i] = 101 - ((int32_t)weights[i] * 100 / total_space);
            }
        }

        choice = RUSS::Get_Weighted_Choice_Short(weights, 19);

        if (choice == 0) {
            *raid_type = 5 /* RAID_DRIVE */;
            return;
        }

        if (choice == 1) {
            *raid_type = 8 /* RAID_COMPUTER */;
            return;
        }

        bool found = false;
        for (int i = 0; i < 8; ++i) {
            if (choice == i + 2) {
                found = true;
                *raid_type = 3 /* RAID_WEAPON */;
                *component_idx = i;
                break;
            }
        }

        if (!found) {
            for (int i = 0; i < 8; ++i) {
                if (choice == i + 10) {
                    found = true;
                    *raid_type = 9 /* RAID_SPECIAL */;
                    *component_idx = i;
                    break;
                }
            }
        }

        if (choice == 18) {
            *raid_type = 2 /* RAID_SHIELD */;
        }
    }

    void __cdecl Clear_Capturing_Flag(int16_t ship_idx) {
        struct_::Clear_Bit_Field(&MOX::combat_data[ship_idx].combat_status_flags, 4);
    }

    int16_t __cdecl Get_Combat_Ship_Special_Id(int16_t ship_idx, int16_t occurrence) {
        int16_t found_special_id = -1;
        bool is_found = false;
        int16_t current_occurrence_count = 0;

        for (int16_t bit_idx = 0; bit_idx < 39; ++bit_idx) {
            if (is_found) {
                break;
            }

            if (struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_flags, bit_idx)) {
                if (current_occurrence_count == occurrence) {
                    is_found = true;
                    found_special_id = bit_idx;
                } else {
                    current_occurrence_count++;
                }
            }
        }

        if (!is_found) {
            found_special_id = -1;
        }

        return found_special_id;
    }

    void __cdecl Capture_Ship(int16_t ship_idx, int16_t x, int16_t y) {
        int16_t new_owner;
        s_combat_data* combat_ship = &MOX::combat_data[ship_idx];

        if (combat_ship->officer_idx > -1) {
            OFFICER::Kill_Officer(combat_ship->officer_idx);
            combat_ship->officer_idx = -1;
        }

        if (combat_ship->ship_idx != -1) {
            int16_t officer_idx = MOX::ship[combat_ship->ship_idx].officer_index;
            if (officer_idx != -1) {
                OFFICER::Move_Officer_To_Pool(officer_idx);
            }
        }

        CMBTFIR2::Clear_Capturing_Flag(ship_idx);

        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_QUANTUM_DETONATOR)) {
            if (game_random::Random(10) < quantum_detonator.explode_chance) {
                CMBTFIRE::Destroy_Ship(ship_idx, x, y, 4);
                return;
            }
        }

        if (combat_ship->owner == MOX::combat_attacker_player) {
            new_owner = MOX::combat_defender_player;
        } else {
            new_owner = MOX::combat_attacker_player;
        }

        combat_ship->owner = (int8_t)new_owner;

        CMBTFIRE::Retarget_Missiles_Aimed_At(ship_idx);
        COMBAT1::Remove_Continuing_Effect_Specials(ship_idx);

        if (new_owner >= MAX_PLAYERS || !MOX::player[new_owner].traits[TRAIT_TELEPATHIC]) {
            combat_ship->is_captured = 1;
        } else if (BOARDING::telepathic_multiple_capture != 0) {
            combat_ship->is_captured = 0;
        }

        combat_ship->is_retreating = 0;
        combat_ship->special_cooldown = COMBAT_SHIP_NONE;

        COMBAT1::Restore_Captured_Ship_To_Turn_Order(
            MOX::ship_turn_order,
            sizeof(MOX::ship_turn_order) / sizeof(MOX::ship_turn_order[0]),
            ship_idx);
        struct_::Clear_Bit_Field(&combat_ship->combat_status_flags, 2);
    }
    void __cdecl Resolve_Raid(int16_t attacker_idx, int16_t target_idx, int16_t* raiders) {
        int16_t total_damage = 0;

        s_combat_data* target = &MOX::combat_data[target_idx];
        s_combat_data* attacker = &MOX::combat_data[attacker_idx];

        int16_t target_owner = target->owner;
        int16_t attacker_owner = attacker->owner;

        if (attacker_owner == target_owner) {
            return;
        }

        int16_t raid_results[19];
        for (int i = 0; i < 19; i++) {
            raid_results[i] = 0;
        }

        int16_t marines = (uint8_t)target->marine_count;
        int16_t target_raiders = *raiders;

        int16_t attacker_commando_bonus = Get_Fleet_Commando_Bonus(attacker_owner);
        int16_t attacker_boarding_bonus = Crew_Boarding_Combat_Bonus(attacker_idx);
        int16_t target_boarding_bonus = Crew_Boarding_Combat_Bonus(target_idx);
        int16_t has_special_1d = COMBINIT::Does_Combat_Ship_Have_Special(target_idx, SPECIAL_SECURITY_STATIONS);
        int16_t target_commando_bonus = Get_Fleet_Commando_Bonus(target_owner);
        int16_t target_security_bonus = Get_Fleet_Security_Bonus(target_owner);

        s_invasion_side_info side1;
        s_invasion_side_info side2;
        ;

        invasion::Get_Boarding_Info(
            attacker_owner,
            *raiders,
            attacker_commando_bonus + attacker_boarding_bonus,
            target_owner,
            marines,
            target_boarding_bonus + has_special_1d * invasion::troop.bonus.security_stations + target_commando_bonus + target_security_bonus,
            &side1,
            &side2);

        int16_t attacker_strength = side1.unit_strengths[1];
        int16_t target_strength = side2.unit_strengths[1];

        int16_t j = 0;
        if (*raiders > 0) {
            do {
                int16_t diff = (int16_t)(attacker_strength + (int16_t)game_random::Random(100));
                diff -= (int16_t)(target_strength + (int16_t)game_random::Random(100));

                if (diff > 20) {
                    int16_t hits = BOARDING::boarding.raid_items_per_marine;
                    if (hits == 0) {
                        hits = 1;
                    }
                    while (hits > 0) {
                        int16_t raid_target = BOARDING::boarding.raid_damage_per_marine;
                        int16_t subsystem;
                        int16_t status = -1;
                        Select_Raid_Internal(target_idx, &status, &subsystem);
                        if (status != -1) {
                            switch (status) {
                                case 2: raid_results[1]++; break;
                                case 3: raid_results[subsystem + 3]++; break;
                                case 5: raid_results[0]++; break;
                                case 8: raid_results[2]++; break;
                                case 9: raid_results[subsystem + 11]++; break;
                            }
                            Apply_Internal_Damage(target_idx, &raid_target, status, subsystem, 0);
                        }
                        hits--;
                    }
                }

                if (diff > 50 && marines > 0) {
                    marines--;
                }
                if (diff < -50) {
                    target_raiders--;
                }
                j++;
            } while (j < *raiders);
        }

        for (int i = 0; i < 19; i++) {
            total_damage += raid_results[i];
        }

        char title_str[80];
        std::string body_str;
        char temp_str[80];
        const bool build_report = MOX::super_fast_flag == 0;
        const auto append = [&](const char* text) {
            if (build_report && text != nullptr) {
                body_str += text;
            }
        };
        const auto append_number = [&](int16_t value) {
            if (build_report) {
                body_str += std::to_string(value);
            }
        };

        KEN::Ken_Get_Text_Message(42, temp_str, sizeof(temp_str));
        strlcpy(title_str, temp_str, sizeof(title_str));
        strlcat(title_str, target->name, sizeof(title_str));

        KEN::Ken_Get_Text_Message(92, temp_str, sizeof(temp_str));
        append(temp_str);

        append_number(attacker_strength);

        append("          ");

        KEN::Ken_Get_Text_Message(93, temp_str, sizeof(temp_str));
        append(temp_str);

        append_number(target_strength);

        append("\n\n");

        if (total_damage <= 0) {
            KEN::Ken_Get_Text_Message(47, temp_str, sizeof(temp_str));
            append(temp_str);
        } else {
            int16_t is_dead = CMBTFIRE::Is_Combat_Ship_Dead(target_idx);
            if (is_dead > 0) {
                CMBTFIRE::Destroy_Ship(target_idx, target->x * 20, target->y * 20, is_dead);
            }

            if (raid_results[0] > 0) {
                KEN::Ken_Get_Text_Message(43, temp_str, sizeof(temp_str));
                append(temp_str);
            }

            if (raid_results[1] > 0) {
                KEN::Ken_Get_Text_Message(44, temp_str, sizeof(temp_str));
                append(temp_str);

                if (target->shield_current_hits < 1) {
                    KEN::Ken_Get_Text_Message(46, temp_str, sizeof(temp_str));
                } else {
                    KEN::Ken_Get_Text_Message(45, temp_str, sizeof(temp_str));
                }
                append(temp_str);
            }

            if (raid_results[2] > 0) {
                KEN::Ken_Get_Text_Message(21, temp_str, sizeof(temp_str));
                append(temp_str);
                append(" ");

                if (target->computer_current_hits < 1) {
                    KEN::Ken_Get_Text_Message(46, temp_str, sizeof(temp_str));
                } else {
                    KEN::Ken_Get_Text_Message(45, temp_str, sizeof(temp_str));
                }
                append(temp_str);
            }

            for (int k = 0; k < 8; k++) {
                if (raid_results[k + 3] > 0) {
                    append_number(raid_results[k + 3]);
                    append(" ");

                    if (raid_results[k + 3] < 2) {
                        append(TECHDATA::weapons[target->weapons[k].weapon_id].name);
                    } else {
                        append(TECHDATA::weapons[target->weapons[k].weapon_id].name_plural);
                    }
                    append(" ");

                    KEN::Ken_Get_Text_Message(46, temp_str, sizeof(temp_str));
                    append(temp_str);
                }
            }

            for (int k = 0; k < 8; k++) {
                if (raid_results[k + 11] > 0) {
                    append("1 ");

                    int16_t special_id = Get_Combat_Ship_Special_Id(target_idx, k);
                    append(TECHDATA::specials[special_id].name);
                    append(" ");

                    KEN::Ken_Get_Text_Message(target->special_hits[k] > 0 ? 45 : 46, temp_str, sizeof(temp_str));
                    append(temp_str);
                }
            }
        }

        if (target_raiders < *raiders) {
            int16_t lost = *raiders - target_raiders;
            append(string::s_Newline);
            append_number(lost);
            append(" ");

            if (lost < 2) {
                KEN::Ken_Get_Text_Message(94, temp_str, sizeof(temp_str));
            } else {
                KEN::Ken_Get_Text_Message(95, temp_str, sizeof(temp_str));
            }
            append(temp_str);
        }

        if (marines < (int16_t)(uint8_t)target->marine_count) {
            int16_t lost = (int16_t)(uint8_t)target->marine_count - marines;
            append(string::s_Newline);
            append_number(lost);
            append(" ");

            if (lost < 2) {
                KEN::Ken_Get_Text_Message(96, temp_str, sizeof(temp_str));
            } else {
                KEN::Ken_Get_Text_Message(97, temp_str, sizeof(temp_str));
            }
            append(temp_str);
        }

        int sound_handle = ERICNET::Mox_Play_Sound(67); // 0x43

        int16_t is_auto;
        if (MOX::combat_attacker_player == target_owner) {
            is_auto = MOX::auto_combat_flag[1];
        } else {
            is_auto = MOX::auto_combat_flag[0];
        }

        bool show_timed_box = BOARDING::boarding.auto_close_results != 0;
        if (BOARDING::boarding.auto_close_results == 0 &&
            is_auto == 0 && target_owner < MAX_PLAYERS && MOX::player[target_owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
            show_timed_box = false;
        } else if (BOARDING::boarding.auto_close_results == 0 &&
            MOX::auto_combat == 0 && attacker_owner < MAX_PLAYERS && MOX::player[attacker_owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
            show_timed_box = false;
        }

        if (MOX::super_fast_flag == 0) {
            if (show_timed_box) {
                TEXTBOX::Timed_Text_Box(title_str, body_str.c_str(), 54);
            } else {
                TEXTBOX::Text_Box(title_str, body_str.c_str());
            }
        }

        sound::Fadeout_Sound(sound_handle, 10);

        target->marine_count = (int8_t)marines;
        *raiders = target_raiders;

        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    void __cdecl Resolve_Capture(int16_t attacker_idx, int16_t defender_idx, int16_t attacker_marines, int16_t target_x, int16_t target_y) {
        s_combat_data* attacker = &MOX::combat_data[attacker_idx];
        s_combat_data* defender = &MOX::combat_data[defender_idx];

        if (attacker->owner == defender->owner) {
            return;
        }

        int16_t defender_marines_start = (uint8_t)defender->marine_count;
        int16_t defender_marines_current = defender_marines_start;
        int16_t attacker_marines_start = attacker_marines;

        int16_t attacker_commando_bonus = Get_Fleet_Commando_Bonus((int16_t)attacker->owner);
        int16_t attacker_boarding_bonus = Crew_Boarding_Combat_Bonus(attacker_idx);
        int16_t has_dashboards = COMBINIT::Does_Combat_Ship_Have_Special(defender_idx, SPECIAL_SECURITY_STATIONS);
        int16_t defender_commando_bonus = Get_Fleet_Commando_Bonus((int16_t)defender->owner);
        int16_t defender_boarding_bonus = Crew_Boarding_Combat_Bonus(defender_idx);
        int16_t defender_security_bonus = Get_Fleet_Security_Bonus((int16_t)defender->owner);
        int16_t defender_total_bonus =
            has_dashboards * invasion::troop.bonus.security_stations + defender_commando_bonus + defender_boarding_bonus + defender_security_bonus;

        s_invasion_side_info side1;
        s_invasion_side_info side2;

        invasion::Get_Boarding_Info(
            (int16_t)attacker->owner,
            attacker_marines,
            attacker_commando_bonus + attacker_boarding_bonus,
            (int16_t)defender->owner,
            defender_marines_start,
            defender_total_bonus,
            &side1,
            &side2);

        if (defender_marines_start > 0) {
            while (attacker_marines > 0) {
                invasion::Ground_Combat_Round(&side1, &side2);
                if (side1.round_killed_unit_type > -1) {
                    attacker_marines--;
                }
                if (side2.round_killed_unit_type > -1) {
                    defender_marines_current--;
                }
                if (defender_marines_current <= 0) break;
            }
        }

        int16_t attacker_marines_lost = attacker_marines_start - attacker_marines;
        int16_t defender_marines_lost = defender_marines_start - defender_marines_current;

        char tmp_buf[80];
        char header_buf[80];
        char msg_buf[500];

        KEN::Ken_Get_Text_Message(0x30, tmp_buf, sizeof(tmp_buf));
        strlcpy(header_buf, tmp_buf, sizeof(header_buf));
        strlcat(header_buf, defender->name, sizeof(header_buf));

        FMTPARA::Set_Paragraph_Sint_Pointer(0, &attacker_marines_start);
        FMTPARA::Set_Paragraph_Sint_Pointer(1, &attacker_marines_lost);
        FMTPARA::Set_Paragraph_Sint_Pointer(2, &attacker_marines);
        FMTPARA::Set_Paragraph_Sint_Pointer(3, &defender_marines_start);
        FMTPARA::Set_Paragraph_Sint_Pointer(4, &defender_marines_lost);
        FMTPARA::Set_Paragraph_Sint_Pointer(5, &defender_marines_current);
        FMTPARA::Set_Paragraph_Sint_Pointer(6, &side1.unit_strengths[1]);
        FMTPARA::Set_Paragraph_Sint_Pointer(7, &side2.unit_strengths[1]);

        // fmtpara 0x1A + '2' switches the paragraph to centered alignment.
        strlcpy(msg_buf, "\0322", sizeof(msg_buf));
        KEN::Ken_Get_Text_Message(0x5C, tmp_buf, sizeof(tmp_buf));
        strlcat(msg_buf, tmp_buf, sizeof(msg_buf));
        // fmtpara 0x18 + 'B' inserts paragraph variable 6, and \aX175. moves the text cursor horizontally.
        strlcat(msg_buf, "\030B6\aX175.", sizeof(msg_buf));
        KEN::Ken_Get_Text_Message(0x5D, tmp_buf, sizeof(tmp_buf));
        strlcat(msg_buf, tmp_buf, sizeof(msg_buf));
        KEN::Ken_Get_Text_Message(0x64, tmp_buf, sizeof(tmp_buf));
        // fmtpara 0x18 + 'B' inserts variables 7, 0, 3, and 1, while the alignment codes and
        // \aX cursor moves lay out the boarding-strength summary across centered columns.
        strlcat(msg_buf, "\030B7\n\0321\030B0\aX80.\0321\030B3\aX255.\0320\n\0321\030B1\aX80.\0320\aX82. ", sizeof(msg_buf));
        strlcat(msg_buf, tmp_buf, sizeof(msg_buf));
        // fmtpara right-aligns variable 4, then restores left alignment and advances the cursor.
        strlcat(msg_buf, "\0321\030B4\aX255.\0320\aX257. ", sizeof(msg_buf));
        strlcat(msg_buf, tmp_buf, sizeof(msg_buf));
        // fmtpara cursor moves place divider lines and the trailing summary fields before restoring centered alignment.
        strlcat(msg_buf, "\n\aX48.-------\aX223.-------\n\0321\030B2\aX80.\0321\030B5\aX255.\0320\n\n\0322", sizeof(msg_buf));

        bool is_timed = BOARDING::boarding.auto_close_results != 0;
        int16_t auto_combat_flag = (defender->owner == MOX::combat_attacker_player) ? MOX::auto_combat_flag[0] : MOX::auto_combat_flag[1];

        if (BOARDING::boarding.auto_close_results == 0 &&
            auto_combat_flag == 0 && defender->owner < MAX_PLAYERS && MOX::player[defender->owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
            is_timed = false;
        } else if (BOARDING::boarding.auto_close_results == 0 &&
            MOX::auto_combat == 0 && attacker->owner < MAX_PLAYERS && MOX::player[attacker->owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
            is_timed = false;
        }

        int16_t result_msg_id;
        if (attacker_marines > 0) {
            int16_t captured_marines =
                attacker_marines - defender_marines_current;
            defender->marine_count = (int8_t)BOARDING::Captured_Marine_Count(
                defender_idx, captured_marines);
            Capture_Ship(defender_idx, target_x, target_y);
            result_msg_id = (defender->unit_status == 0) ? 0x32 : 0x31;
        } else {
            defender->marine_count = (int8_t)defender_marines_current;
            result_msg_id = 0x33;
        }

        KEN::Ken_Get_Text_Message(result_msg_id, tmp_buf, sizeof(tmp_buf));
        strlcat(msg_buf, tmp_buf, sizeof(msg_buf));

        int32_t sound_handle = ERICNET::Mox_Play_Sound(0x43);

        if (MOX::super_fast_flag == 0) {
            if (!is_timed) {
                TEXTBOX::Text_Box(header_buf, msg_buf);
            } else {
                TEXTBOX::Timed_Text_Box(header_buf, msg_buf, 54);
            }
        }

        sound::Fadeout_Sound(sound_handle, 10);
        CMBTDRW1::Refresh_Combat_Screen_Full();
    }
} // namespace CMBTFIR2
