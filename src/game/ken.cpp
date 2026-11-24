#include "pch.h"

namespace KEN {
    uint8_t personalities[14][10] = {
        {PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST},
        {PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC},
        {PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE},
        {PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE},
        {PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST},
        {PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST},
        {PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE},
        {PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC},
        {PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE},
        {PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST},
        {PLAYER_PERSONALITY_RUTHLESS, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_ERRATIC},
        {PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_AGGRESSIVE, PLAYER_PERSONALITY_ERRATIC},
        {PLAYER_PERSONALITY_ERRATIC, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_HONORABLE, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST, PLAYER_PERSONALITY_PACIFIST},
        {PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC, PLAYER_PERSONALITY_XENOPHOBIC},
    };

    void __cdecl Ken_Init() {
    }

    uint16_t __cdecl Get_Weighted_Choice(uint16_t* weights, int16_t count) {
        uint32_t sum;

        while (true) {
            sum = 0;

            if (count <= 0) {
                break;
            }

            for (int32_t i = 0; i < count; ++i) {
                sum += weights[i];
                if (sum >= 0x200) {
                    break;
                }
            }

            if (sum >= 0x200) {
                for (int32_t j = 0; j < count; ++j) {
                    weights[j] >>= 1;
                }
                continue;
            }

            break;
        }

        if (sum == 0) {
            return 0;
        }

        int32_t rem = game_random::Random(sum) - weights[0];
        uint16_t idx = 0;

        if (rem > 0) {
            while (idx < count - 1) {
                idx++;
                rem -= weights[idx];
                if (rem <= 0) {
                    break;
                }
            }
        }

        return idx;
    }

    void __cdecl Load_Player_Ship_Palette(int16_t player_idx) {
        s_animation_header* palette_anim; // BUG: uninitialized pointer used for player_idx > 14

        if (player_idx < MAX_PLAYERS) {
            palette_anim = (s_animation_header*)buffer::Buffer_Reload(
                "ships.lbx",
                (uint8_t)game::current_state.players[player_idx].color * 50 + 49,
                MOX::global_cache_seg);
        } else {
            switch (player_idx) {
                case MAX_PLAYERS:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 413, MOX::global_cache_seg);
                    break;
                case 9:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 419, MOX::global_cache_seg);
                    break;
                case 10:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 417, MOX::global_cache_seg);
                    break;
                case 11:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 416, MOX::global_cache_seg);
                    break;
                case 12:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 414, MOX::global_cache_seg);
                    break;
                case 13:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 415, MOX::global_cache_seg);
                    break;
                case 14:
                    palette_anim = (s_animation_header*)buffer::Buffer_Reload("ships.lbx", 418, MOX::global_cache_seg);
                    break;
            }
        }

        animate::Draw_Palette(palette_anim);
        palette::Refresh_Palette();
        MOX::reloaded_ship_palette = 1;
    }

    void __cdecl Ken_Init2() {
    }

    void __cdecl Ken_Get_Text_Message(int32_t msg_index, char* buffer, int16_t message_arg) {
        if (msg_index >= 0x50) {
            JIM::Get_Text_Message("KENTEXT1.LBX", (int16_t)(msg_index - 0x50), buffer, message_arg);
        } else {
            JIM::Get_Text_Message("KENTEXT.LBX", (int16_t)msg_index, buffer, message_arg);
        }
    }

    void __cdecl Init_NPC_Personalities_Objectives_Themes() {
        if (game::current_state.player_count <= 0) {
            return;
        }

        for (int i = 0; i < game::current_state.player_count; i++) {
            int difficulty_val = game_random::Random(10) - MOX::settings.game_difficulty + 1;
            if (difficulty_val < 0) {
                difficulty_val = 0;
            } else if (difficulty_val > 9) {
                difficulty_val = 9;
            }

            game::current_state.players[i].personality = personalities[game::current_state.players[i].race][difficulty_val];

            int obj_w[6];
            int des_w[4];
            int spec_w[7];
            const int32_t objective_weight_count = (int32_t)(sizeof(obj_w) / sizeof(obj_w[0]));
            const int32_t design_weight_count = (int32_t)(sizeof(des_w) / sizeof(des_w[0]));
            const int32_t special_weight_count = (int32_t)(sizeof(spec_w) / sizeof(spec_w[0]));

            obj_w[0] = 1;
            obj_w[2] = 1;
            obj_w[5] = 1;
            des_w[AI_SHIP_DESIGN_THEME_CARRIER] = 1;

            obj_w[1] = 2;
            obj_w[3] = 2;
            obj_w[4] = 2;
            des_w[AI_SHIP_DESIGN_THEME_BALANCED] = 2;
            des_w[AI_SHIP_DESIGN_THEME_MISSILE] = 2;
            spec_w[AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE] = 2;
            spec_w[AI_SHIP_SPECIAL_THEME_CAPTURE] = 2;
            spec_w[AI_SHIP_SPECIAL_THEME_MISSILE_DEFENSE] = 2;

            des_w[AI_SHIP_DESIGN_THEME_SPECIAL] = 1;
            spec_w[AI_SHIP_SPECIAL_THEME_ARMOR] = 2;
            spec_w[AI_SHIP_SPECIAL_THEME_BIO_WEAPON] = 1;
            spec_w[AI_SHIP_SPECIAL_THEME_CLOAKING] = 1;
            spec_w[AI_SHIP_SPECIAL_THEME_SHIELD] = 3;

            int8_t* traits = game::current_state.players[i].traits;

            if (traits[TRAIT_POP_GROWTH_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_BONUS].value) {
                obj_w[5] = 11;
                obj_w[2] = 11;
            }
            if (traits[TRAIT_POP_GROWTH_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[5] += 100;
                obj_w[2] += 100;
            }
            if (traits[TRAIT_FOOD_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_FOOD][raceopt::PRIMARY_CHOICE_BONUS].value) {
                obj_w[5] += 3;
                obj_w[4] = 5;
                obj_w[3] = 5;
            }
            if (traits[TRAIT_FOOD_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_FOOD][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[5] += 100;
                obj_w[4] += 100;
                obj_w[3] += 100;
            }
            if (traits[TRAIT_PRODUCTION_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_PRODUCTION][raceopt::PRIMARY_CHOICE_BONUS].value) {
                obj_w[5] += 3;
                obj_w[4] += 3;
                obj_w[3] += 3;
            }
            if (traits[TRAIT_PRODUCTION_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_PRODUCTION][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[5] += 10;
                obj_w[4] += 10;
                obj_w[3] += 10;
            }
            if (traits[TRAIT_RESEARCH_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_RESEARCH][raceopt::PRIMARY_CHOICE_BONUS].value) {
                obj_w[4] += 3;
                obj_w[3] += 10;
            }
            if (traits[TRAIT_RESEARCH_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_RESEARCH][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[4] += 10;
                obj_w[3] += 100;
            }
            if (traits[TRAIT_SHIP_DEFENSE] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_SHIP_DEFENSE][raceopt::PRIMARY_CHOICE_BONUS].value) {
                obj_w[1] = 12;
                spec_w[AI_SHIP_SPECIAL_THEME_MISSILE_DEFENSE] = 102;
                des_w[AI_SHIP_DESIGN_THEME_CARRIER] = 11;
            }
            if (traits[TRAIT_SHIP_DEFENSE] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_SHIP_DEFENSE][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[1] += 100;
                spec_w[AI_SHIP_SPECIAL_THEME_MISSILE_DEFENSE] += 1000;
                des_w[AI_SHIP_DESIGN_THEME_CARRIER] += 100;
            }
            if (traits[TRAIT_SHIP_ATTACK] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_SHIP_ATTACK][raceopt::PRIMARY_CHOICE_BONUS].value) {
                spec_w[AI_SHIP_SPECIAL_THEME_CLOAKING] = 4;
                des_w[AI_SHIP_DESIGN_THEME_BALANCED] = 102;
                obj_w[1] += 10;
            }
            if (traits[TRAIT_SHIP_ATTACK] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_SHIP_ATTACK][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[1] += 100;
                spec_w[AI_SHIP_SPECIAL_THEME_CLOAKING] += 10;
                des_w[AI_SHIP_DESIGN_THEME_BALANCED] += 1000;
            }
            if (traits[TRAIT_GROUND_COMBAT_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROUND_COMBAT][raceopt::PRIMARY_CHOICE_BONUS].value) {
                spec_w[AI_SHIP_SPECIAL_THEME_CAPTURE] = 102;
                obj_w[2] += 10;
            }
            if (traits[TRAIT_GROUND_COMBAT_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROUND_COMBAT][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[2] += 100;
                spec_w[AI_SHIP_SPECIAL_THEME_CAPTURE] += 1000;
            }
            if (traits[TRAIT_SPYING_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_SPYING][raceopt::PRIMARY_CHOICE_BONUS].value) {
                obj_w[0] = 4;
            }
            if (traits[TRAIT_SPYING_BONUS] == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_SPYING][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                obj_w[0] += 10;
            }
            if (traits[TRAIT_LOW_G_WORLD] == 1) {
                obj_w[4] += 10;
                obj_w[5] += 3;
                obj_w[3] += 3;
            }
            if (traits[TRAIT_HEAVY_G_WORLD] == 1) {
                obj_w[2] += 10;
                spec_w[AI_SHIP_SPECIAL_THEME_CAPTURE] += 1000;
            }
            if (traits[TRAIT_AQUATIC] == 1) {
                obj_w[4] += 3;
                obj_w[5] += 3;
                obj_w[3] += 3;
            }
            if (traits[TRAIT_SUBTERRANEAN] == 1) {
                obj_w[4] += 10;
                obj_w[5] += 3;
                obj_w[3] += 3;
            }
            if (traits[TRAIT_LARGE_HOME_WORLD] == 1) {
                obj_w[4] += 3;
                obj_w[5] += 10;
            }
            if (traits[TRAIT_RICH_HOME_WORLD] == 1) {
                obj_w[4] += 10;
            }
            if (traits[TRAIT_CYBERNETIC] == 1) {
                obj_w[4] += 3;
                des_w[AI_SHIP_DESIGN_THEME_BALANCED] += 10;
                spec_w[AI_SHIP_SPECIAL_THEME_ARMOR] = 1002;
            }
            if (traits[TRAIT_EATS_MINERALS] == 1) {
                obj_w[4] += 3;
                obj_w[2] += 10;
                obj_w[3] += 3;
            }
            if (traits[TRAIT_CHARISMATIC] == 1) {
                obj_w[0] += 100;
            }
            if (traits[TRAIT_CREATIVE] == 1) {
                obj_w[3] += 1000;
            }
            if (traits[TRAIT_ENVIRONMENT_IMMUNE] == 1) {
                obj_w[4] += 100;
            }
            if (traits[TRAIT_FANTASTIC_TRADERS] == 1) {
                obj_w[0] += 100;
            }
            if (traits[TRAIT_STEALTHY_SHIPS] == 1) {
                obj_w[1] += 100;
            }
            if (traits[TRAIT_WARLORD] == 1) {
                obj_w[2] += 10;
            }
            if (traits[TRAIT_TRANS_DIMENSIONAL] == 1) {
                obj_w[2] += 10;
                obj_w[1] += 13;
                spec_w[AI_SHIP_SPECIAL_THEME_MISSILE_DEFENSE] += 100;
            }

            if (MOX::settings.game_difficulty == GAME_DIFFICULTY_HARD) {
                obj_w[4] += 3;
                obj_w[2] += 3;
                obj_w[3] += 3;
            }
            if (MOX::settings.game_difficulty == GAME_DIFFICULTY_IMPOSSIBLE) {
                obj_w[4] += 10;
                obj_w[2] += 10;
                obj_w[3] += 10;
            }
            if (game::current_state.players[i].personality == PLAYER_PERSONALITY_XENOPHOBIC) {
                spec_w[AI_SHIP_SPECIAL_THEME_BIO_WEAPON] = 4;
            }

            // Calculate objectives
            int total_obj = 0;
            for (int32_t j = 0; j < objective_weight_count; ++j) {
                total_obj += obj_w[j];
            }
            while (total_obj > 1000) {
                total_obj = 0;
                for (int32_t j = 0; j < objective_weight_count; ++j) {
                    obj_w[j] /= 2;
                    total_obj += obj_w[j];
                }
            }
            int rand_obj = game_random::Random(total_obj);
            int selected_obj = 0;
            rand_obj -= obj_w[0];
            if (rand_obj > 0) {
                for (int32_t j = 1; j < objective_weight_count; ++j) {
                    selected_obj++;
                    rand_obj -= obj_w[j];
                    if (rand_obj <= 0) break;
                }
            }

            uint8_t prev_obj = game::current_state.players[i].objectives;
            game::current_state.players[i].objectives = selected_obj;
            if (prev_obj == 100) {
                game::current_state.players[i].objectives = 100;
            }

            // Calculate ship design theme
            int total_des = 0;
            for (int32_t j = AI_SHIP_DESIGN_THEME_BALANCED; j < design_weight_count; ++j) {
                total_des += des_w[j];
            }
            while (total_des > 1000) {
                total_des = 0;
                for (int32_t j = AI_SHIP_DESIGN_THEME_BALANCED; j < design_weight_count; ++j) {
                    des_w[j] /= 2;
                    total_des += des_w[j];
                }
            }
            int rand_des = game_random::Random(total_des);
            int selected_des = AI_SHIP_DESIGN_THEME_BALANCED;
            rand_des -= des_w[AI_SHIP_DESIGN_THEME_BALANCED];
            if (rand_des > 0) {
                for (int32_t j = AI_SHIP_DESIGN_THEME_CARRIER; j < design_weight_count; ++j) {
                    selected_des++;
                    rand_des -= des_w[j];
                    if (rand_des <= 0) break;
                }
            }
            game::current_state.players[i].ship_design_theme = selected_des;

            // Calculate ship special theme
            int total_spec = 0;
            for (int32_t j = AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE; j < special_weight_count; ++j) {
                total_spec += spec_w[j];
            }
            while (total_spec > 1000) {
                total_spec = 0;
                for (int32_t j = AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE; j < special_weight_count; ++j) {
                    spec_w[j] /= 2;
                    total_spec += spec_w[j];
                }
            }
            int rand_spec = game_random::Random(total_spec);
            int selected_spec = AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE;
            rand_spec -= spec_w[AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE];
            if (rand_spec > 0) {
                for (int32_t j = AI_SHIP_SPECIAL_THEME_CAPTURE; j < special_weight_count; ++j) {
                    selected_spec++;
                    rand_spec -= spec_w[j];
                    if (rand_spec <= 0) break;
                }
            }
            game::current_state.players[i].ship_special_theme = selected_spec;
        }
    }

    s_animation_header* __cdecl Get_Ship_Picture_Seg(int16_t player_idx, int16_t ship_type) {
        return Do_Get_Ship_Picture_Seg(player_idx, ship_type);
    }

    void __cdecl Get_Derelict_Applications(int16_t player_idx, uint16_t* app_array, int16_t* app_count) {
        int16_t best_shield_idx = SHIELD_NO_SHIELD;
        int16_t max_weapon_idx = WEAPON_LASER_CANNON;
        int16_t min_diff = 200;
        int16_t best_weapon_idx = WEAPON_NO_WEAPONS;

        *app_count = 0;

        if (game::current_state.player_count > 0) {
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                int16_t shield_idx = INITSHIP::Best_Ship_Shield(factions::From_Legacy(i));
                if (best_shield_idx < shield_idx) {
                    best_shield_idx = shield_idx;
                }

                int16_t weapon_idx = INITSHIP::Best_Beam_Weapon(i);
                if (TECHDATA::weapons[max_weapon_idx].max_damage < TECHDATA::weapons[weapon_idx].max_damage) {
                    max_weapon_idx = weapon_idx;
                }
            }
        }

        uint16_t shield_app_id = TECHDATA::shields[best_shield_idx].tech_app_id;

        if (TECHDATA::technology_applications[TECHDATA::weapons[max_weapon_idx].tech_app_id].tech_field_id == TECH_FIELD_XENON_TECHNOLOGY) {
            max_weapon_idx = WEAPON_DISRUPTER;
        }

        for (int16_t i = WEAPON_MASS_DRIVER; i < 40; ++i) {
            if (TECHDATA::weapons[i].type == WEAPON_TYPE_BEAM &&
                TECHDATA::technology_applications[TECHDATA::weapons[i].tech_app_id].tech_field_id != TECH_FIELD_XENON_TECHNOLOGY) {
                int16_t diff = TECHDATA::weapons[i].max_damage - TECHDATA::weapons[max_weapon_idx].max_damage;

                if (diff >= 0 && diff <= min_diff && diff <= 50) {
                    if (game::current_state.players[player_idx].tech_applications[TECHDATA::weapons[i].tech_app_id] != 3) {
                        best_weapon_idx = i;
                        if (i == max_weapon_idx) {
                            min_diff = 1;
                        } else {
                            min_diff = diff;
                        }
                    }
                }
            }
        }

        if (min_diff < 200) {
            app_array[*app_count] = TECHDATA::weapons[best_weapon_idx].tech_app_id;
            (*app_count)++;
        }

        if (game::current_state.players[player_idx].tech_applications[shield_app_id] != TECH_RESEARCH_STATUS_RESEARCHED) {
            app_array[*app_count] = shield_app_id;
            (*app_count)++;
        }
    }

    s_animation_header* __cdecl Get_Ship_Id_Picture_Seg(int16_t ship_idx) {
        return Do_Get_Ship_Picture_Seg(
            (int16_t)(int8_t)factions::Legacy_Code(game::current_state.ship_records[ship_idx].d.previous_owner),
            game::current_state.ship_records[ship_idx].d.picture_num);
    }

    s_animation_header* __cdecl Do_Get_Ship_Picture_Seg(int16_t player_idx, int16_t ship_type) {
        int8_t color = MAX_PLAYERS;
        if (player_idx < MAX_PLAYERS) {
            color = (int8_t)game::current_state.players[player_idx].color;
        }

        return (s_animation_header*)buffer::Buffer_Reload(
            "ships.lbx",
            (int16_t)(ship_type + color * 50),
            MOX::global_cache_seg);
    }
} // namespace KEN
