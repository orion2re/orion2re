#include "pch.h"
#include "tactical_control.h"

namespace CMBTDRW1 {
    enum class Combat_View_Subject {
        Strategic_Ship,
        Strategic_Lander,
        Strategic_Combat_Ship,
        Tactical_Ship,
        Tactical_Planet,
        Monster,
    };

    static Combat_View_Subject combat_view_subject = Combat_View_Subject::Strategic_Ship;

    static Combat_View_Subject Tactical_Combat_View_Subject(int16_t combat_idx) {
        if (MOX::combat_data[combat_idx].owner >= NONPLAYER_SHIP_TYPE_AMOEBA) {
            return Combat_View_Subject::Monster;
        }

        // Tactical combat reserves index 0 for the colony or planet object.
        if (combat_idx == 0) {
            return Combat_View_Subject::Tactical_Planet;
        }

        return Combat_View_Subject::Tactical_Ship;
    }

    const char s_combat_lbx[11] = "combat.lbx";
    uint8_t green[16];
    MOX2::action_func_t k_save;

    int16_t g_max;
    int16_t g_min;
    char* g_msg;
    int16_t g_var;

    s_animation_header* board_capture_button_seg;
    s_animation_header* board_raid_button_seg;
    s_animation_header* board_left_button_seg;
    s_animation_header* board_right_button_seg;

    static void Draw_Animation_Speed() {
        if (!timer::Animation_Speed_Change_Is_Visible()) {
            return;
        }

        char text[32];
        snprintf(
            text,
            sizeof(text),
            "Animation speed %+d",
            timer::Get_Animation_Speedup());
        fonts::Set_Font_Style(3, &MOX::combat_green);
        fonts::Print_Right(630, 8, text);
    }

    void __cdecl Draw_Shield_Arcs() {
        if (MOX::cur_ship == 0) {
            return;
        }

        const char* labels[4];
        if (MOX::settings.language == 4) {
            labels[0] = "A";
            labels[1] = "D";
            labels[2] = "I";
            labels[3] = "S";
        } else {
            labels[0] = "F";
            labels[1] = "R";
            labels[2] = "B";
            labels[3] = "L";
        }

        int16_t ship_center_and_size[7];
        ship_center_and_size[1] = 15;
        ship_center_and_size[2] = 22;
        ship_center_and_size[3] = 25;
        ship_center_and_size[4] = 30;
        ship_center_and_size[5] = 33;
        ship_center_and_size[6] = 33;

        int16_t cy;
        COMBINIT::Ship_Center_Offsets(MOX::cur_ship, ship_center_and_size, &cy);

        int16_t cx = ship_center_and_size[0];
        cx += MOX::combat_data[MOX::cur_ship].x * 20;
        cy += MOX::combat_data[MOX::cur_ship].y * 20;

        int8_t facing = MOX::combat_data[MOX::cur_ship].facing_dir;

        int16_t x2[4];
        int16_t y2[4];
        int16_t dx[4];
        int16_t dy[4];

        int16_t sz = ship_center_and_size[MOX::combat_data[MOX::cur_ship].size_class + 1];

        int16_t idx0 = (facing / 4) % 4;
        int16_t idx1 = (idx0 + 1) % 4;
        int16_t idx2 = (idx0 + 2) % 4;
        int16_t idx3 = (idx0 + 3) % 4;

        switch (facing) {
            case 0:
            case 4:
            case 8:
            case 12: {
                x2[0] = 2000;
                y2[0] = 2000;
                x2[1] = -2000;
                y2[1] = -2000;
                x2[2] = 2000;
                y2[2] = -2000;
                x2[3] = -2000;
                y2[3] = 2000;

                dx[idx0] = sz;
                dy[idx0] = 0;
                dx[idx1] = 0;
                dy[idx1] = sz;
                dx[idx2] = -sz;
                dy[idx2] = 0;
                dx[idx3] = 0;
                dy[idx3] = -sz;
                break;
            }
            case 1:
            case 5:
            case 9:
            case 13: {
                x2[0] = 3827;
                y2[0] = -9239;
                x2[1] = -9239;
                y2[1] = -3827;
                x2[2] = -3827;
                y2[2] = 9239;
                x2[3] = 9239;
                y2[3] = 3827;

                int16_t v23 = (sz * 23) / 25;
                int16_t v19 = (sz * 19) / 50;

                dx[idx0] = v23;
                dy[idx0] = -v19;
                dx[idx1] = v19;
                dy[idx1] = v23;
                dx[idx2] = -v23;
                dy[idx2] = v19;
                dx[idx3] = -v19;
                dy[idx3] = -v23;
                break;
            }
            case 2:
            case 6:
            case 10:
            case 14: {
                x2[0] = 0;
                y2[0] = 2000;
                x2[1] = -2000;
                y2[1] = 0;
                x2[2] = 0;
                y2[2] = -2000;
                x2[3] = 2000;
                y2[3] = 0;

                dx[idx0] = sz;
                dy[idx0] = -sz;
                dx[idx1] = sz;
                dy[idx1] = sz;
                dx[idx2] = -sz;
                dy[idx2] = sz;
                dx[idx3] = -sz;
                dy[idx3] = -sz;
                break;
            }
            case 3:
            case 7:
            case 11:
            case 15: {
                x2[0] = 9239;
                y2[0] = -3827;
                x2[1] = -3827;
                y2[1] = -9239;
                x2[2] = -9239;
                y2[2] = 3827;
                x2[3] = 3827;
                y2[3] = 9239;

                int16_t v23 = (sz * 23) / 25;
                int16_t v19 = (sz * 19) / 50;

                dx[idx0] = v19;
                dy[idx0] = -v23;
                dx[idx1] = v23;
                dy[idx1] = v19;
                dx[idx2] = -v19;
                dy[idx2] = v23;
                dx[idx3] = -v23;
                dy[idx3] = -v19;
                break;
            }
            default:
                break;
        }

        fonts::Set_Font_Style(0, &MOX::combat_green);

        for (int16_t i = 0; i < 4; ++i) {
            dx[i] -= 2;
            dy[i] -= 3;

            int16_t x1 = cx - MOX::cur_combat_x * 20;
            int16_t y1 = cy - MOX::cur_combat_y * 20;
            int16_t x2_coord = x2[i] - MOX::cur_combat_x * 20 + cx;
            int16_t y2_coord = y2[i] - MOX::cur_combat_y * 20 + cy;

            line::Line(x1, y1, x2_coord, y2_coord, 0x54);

            fonts::Print(x1 + dx[i], y1 + dy[i], labels[i]);
        }
    }

    void __cdecl Detailed_View_Ship(int16_t ship_idx) {
        char ship_name[16];
        bool is_editable = (ship_idx != -1);

        EVANHELP::Save_Help_List();
        MOX2::Get_Auto_Function(&k_save);
        video::Set_Page_Back();
        MOX::ship_frame--;
        MOX2::Execute_Auto_Function(&k_save);
        fields::Draw_Visible_Fields_Back();
        video::Copy_Back_To_Off();
        RUSS::Mox_Sync_Update();
        fields::Assign_Auto_Function(Combat_Draw_View_Ship, 1);
        allocate::Push_Block(MOX::screen_seg);

        if (is_editable) {
            if (MOX::settings.strategic_combat_flag == 1) {
                combat_view_subject = Combat_View_Subject::Strategic_Combat_Ship;
            } else if (MOX::ship[ship_idx].d.ship_type != SHIP_TYPE_COMBAT) {
                combat_view_subject = Combat_View_Subject::Strategic_Lander;
            } else {
                combat_view_subject = Combat_View_Subject::Strategic_Ship;
            }
            MOX::ship_data_id = -1;
            COMBINIT::Load_Display_Combat_Ship(ship_idx);

            // BUG: Potential buffer overflow if name is longer than 15 characters

            strlcpy(ship_name, MOX::global_combat_data.name, sizeof(ship_name));
            INITSHIP::Get_Ship_Combat_Bonuses(
                ship_idx,
                &MOX::global_combat_data.tac_val_defense,
                &MOX::global_combat_data.tac_val_evade,
                &MOX::global_combat_data.tac_val_attack);

            uint32_t bitmap_size = bitmap::Get_Bitmap_Size(98, 98);
            MOX::draw_ship_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);
            MOX::combat_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);

            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("Combat.lbx", 0xB, MOX::global_cache_seg);
            remap::Calculate_Picture_Remap_Colors(anim);

            NEWGAME::color_ken.colors[0] = remap::Get_Remap_Palette_Color(0x84);
            NEWGAME::color_ken.colors[1] = remap::Get_Remap_Palette_Color(0x8C);
            NEWGAME::color_ken.colors[2] = remap::Get_Remap_Palette_Color(0x8A);
            NEWGAME::color_ken.colors[3] = remap::Get_Remap_Palette_Color(0x8C);
            NEWGAME::color_ken.colors[4] = remap::Get_Remap_Palette_Color(0x8C);
            NEWGAME::color_ken.colors[5] = remap::Get_Remap_Palette_Color(0x8C);
            NEWGAME::color_ken.colors[6] = remap::Get_Remap_Palette_Color(0x8C);
            NEWGAME::color_ken.colors[7] = remap::Get_Remap_Palette_Color(0x8C);
        }

        if (MOX::global_combat_data.officer_idx == -1) {
            int16_t p_idx = (int16_t)MOX::global_combat_data.owner;
            if (p_idx >= MAX_PLAYERS) p_idx = 0;

            uint8_t race = (uint8_t)MOX::player[p_idx].race;
            MOX::detail_fleet_icon_seg = (s_animation_header*)farload::Far_Reload_Next("flticons.lbx", race, MOX::screen_seg);
        } else {
            int16_t mode = is_editable ? 0 : 2;
            MOX::detail_fleet_icon_seg = (s_animation_header*)OFFICER::Load_Officer_Picture(0, MOX::global_combat_data.officer_idx, MOX::screen_seg, 0, mode);
        }

        fields::Mark_Fields();

        if (is_editable) {
            fonts::Set_Font_Style(4, &NEWGAME::color_ken);
            fields::Add_Continuous_String_Input_Field(0x106, 0x32, 0x67, 0xC, ship_name, 0xF, 0, 0, 0, 0);
            fields::Add_Hidden_Field(0x10C, 0x32, 0x17F, 0x40, "", 0);
        }

        int16_t exit_field_id = fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "", 0);
        bool exit_triggered = false;
        do {
            int16_t input = fields::Get_Input();
            if (input == exit_field_id) {
                exit_triggered = true;
            }
            video::Set_Page_Off();
            Combat_Draw_View_Ship();
            RUSS::Mox_Sync_Update();
        } while (!exit_triggered);

        if (is_editable) {
            strings::Trim(ship_name);
            if (ship_name[0] != '\0') {
                strlcpy(MOX::ship[ship_idx].d.name, ship_name, sizeof(MOX::ship[ship_idx].d.name));
            }
        }

        fields::Release_Fields();
        EVANHELP::Restore_Help_List();
        MOX2::Put_Auto_Function(&k_save);
        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Draw_Legal_Boxes() {
        if (MOX::cur_ship == 0) {
            return;
        }

        s_combat_data* ship = &MOX::combat_data[MOX::cur_ship];
        bool can_move = false;

        if (ship->movement_left > 0) {
            can_move = true;
        } else {
            if (COMBINIT::Does_Combat_Ship_Have_Special(MOX::cur_ship, SPECIAL_SUB_SPACE_TELEPORTER) ||
                COMBINIT::Does_Combat_Ship_Have_Special(MOX::cur_ship, SPECIAL_PHASE_SHIFTER)) {
                if (CMBTFIR2::Used_Teleporter(MOX::cur_ship) == 0) {
                    can_move = true;
                }
            }
        }

        if (can_move) {
            int16_t box_size;
            switch (ship->size_class) {
                case 0:
                    box_size = 20; /* 0x14 */
                    break;
                case 1:
                case 2:
                    box_size = 40; /* 0x28 */
                    break;
                case 3:
                case 4:
                case 5:
                    box_size = 60; /* 0x3C */
                    break;
                default:
                    break;
            }

            uint8_t color = (MOX::in_nebula_flag == 1) ? 0 : 0x81 /* 129 */;

            for (int16_t x = MOX::cur_combat_x; x < MOX::cur_combat_x + COMBAT_GRID_VIEW_WIDTH; ++x) {
                for (int16_t y = MOX::cur_combat_y; y < MOX::cur_combat_y + COMBAT_GRID_VIEW_HEIGHT; ++y) {
                    if (MOX::combat_grid[x][y] == 2000 /* 0x7D0 */) {
                        int16_t x1 = (x - MOX::cur_combat_x) * 20;
                        int16_t y1 = (y - MOX::cur_combat_y) * 20;
                        graphics::Fill(x1, y1, x1 + box_size, y1 + box_size, color);
                    }
                }
            }
        }
    }

    int16_t __cdecl Relative_Bearing(int16_t ship_a_idx, int16_t ship_b_idx) {
        int16_t x1, y1;
        int16_t x2, y2;
        int16_t bearing;
        int8_t facing;

        // Get coordinates for ship A
        COMBINIT::Ship_Center_Offsets(ship_a_idx, &x1, &y1);
        x1 += MOX::combat_data[ship_a_idx].x * 20;
        y1 += MOX::combat_data[ship_a_idx].y * 20;

        // Get coordinates for ship B
        COMBINIT::Ship_Center_Offsets(ship_b_idx, &x2, &y2);
        x2 += MOX::combat_data[ship_b_idx].x * 20;
        y2 += MOX::combat_data[ship_b_idx].y * 20;

        // Calculate angle between ships. Y-axis is inverted in screen space calculation here.
        bearing = angle::Get_Angle(x2 - x1, y1 - y2);

        facing = MOX::combat_data[ship_a_idx].facing_dir;

        // Subtract ship A's heading from the absolute bearing.
        // Heading is in 16 discrete directions (0-15), where each unit is 22.5 degrees.
        // 22.5 * facing = 22 * facing + facing / 2
        bearing = bearing - (facing / 2) - (facing * 22);

        // Normalize angle to [0, 359]
        if (bearing < 0) {
            bearing += 360;
        }

        return bearing;
    }

    int16_t __cdecl Relative_Bearing_XY(int16_t ship_idx, int16_t target_x, int16_t target_y) {
        int16_t off_x;
        int16_t off_y;
        int16_t result_bearing;
        s_combat_data* ship;

        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        ship = &MOX::combat_data[ship_idx];

        off_x += (int16_t)ship->x * 20;
        off_y += (int16_t)ship->y * 20;

        result_bearing = angle::Get_Angle((int32_t)(target_x - off_x), (int32_t)(off_y - target_y));

        result_bearing -= (int16_t)(ship->facing_dir / 2);
        result_bearing += (int16_t)(ship->facing_dir * -22);

        if (result_bearing < 0) {
            result_bearing += 360;
        }

        return result_bearing;
    }

    void __cdecl Draw_Internals_Display(int16_t x_off, int16_t y_off) {
        char msg_buffer[80];
        char itoa_buffer[80];

        if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) return;

        if (MOX::cur_ship != 0) {
            s_combat_data* ship = &MOX::combat_data[MOX::cur_ship];

            fonts::Set_Font_Style_Shadow_Down(2, &MOX::combat_green);
            fonts::Set_Outline_Color(0, 0);
            KEN::Ken_Get_Text_Message(36, msg_buffer, 80);
            fonts::Print_Centered(445, 362, msg_buffer);

            fonts::Set_Font_Style_Shadow_Down(1, &MOX::combat_green);
            fonts::Set_Outline_Color(0, 0);

            int16_t current_y = y_off + 4;
            if (ship->ftl_type == 0) {
                KEN::Ken_Get_Text_Message(37, msg_buffer, 80);
                fonts::Print(x_off + 4, current_y, msg_buffer);
            } else {
                KEN::Ken_Get_Text_Message(14, msg_buffer, 80);
                fonts::Print(x_off + 4, current_y, msg_buffer);
                snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", ship->drive_current_hits);
                fonts::Print_Right(x_off + 85, current_y, itoa_buffer);
            }

            current_y += 10;
            if (ship->shield_type == 0) {
                KEN::Ken_Get_Text_Message(38, msg_buffer, 80);
                fonts::Print(x_off + 4, current_y, msg_buffer);
            } else {
                KEN::Ken_Get_Text_Message(39, msg_buffer, 80);
                fonts::Print(x_off + 4, current_y, msg_buffer);
                snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", ship->shield_current_hits);
                fonts::Print_Right(x_off + 85, current_y, itoa_buffer);
            }

            current_y += 10;
            KEN::Ken_Get_Text_Message(21, msg_buffer, 80);
            fonts::Print(x_off + 4, current_y, msg_buffer);
            snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", ship->computer_current_hits);
            fonts::Print_Right(x_off + 85, current_y, itoa_buffer);

            current_y += 10;
            KEN::Ken_Get_Text_Message(16, msg_buffer, 80);
            fonts::Print(x_off + 4, current_y, msg_buffer);
            snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", ship->structure_max - ship->structure_damage);
            fonts::Print_Right(x_off + 85, current_y, itoa_buffer);

            current_y += 10;
            KEN::Ken_Get_Text_Message(18, msg_buffer, 80);
            fonts::Print(x_off + 4, current_y, msg_buffer);
            snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", ship->armor_remaining);
            fonts::Print_Right(x_off + 85, current_y, itoa_buffer);

            current_y = y_off + 64;
            KEN::Ken_Get_Text_Message(40, msg_buffer, 80);
            fonts::Print(x_off + 4, current_y, msg_buffer);
            fonts::Print_Integer_Right(x_off + 85, current_y, ship->current_speed);

            current_y = y_off + 75;
            KEN::Ken_Get_Text_Message(90, msg_buffer, 80);
            fonts::Print(x_off + 4, current_y, msg_buffer);
            fonts::Print_Integer_Right(x_off + 85, current_y, ship->movement_left);
        }
    }

    void __cdecl Full_Draw_Combat_Screen() {
        char local_ship_name[32];

        if (MOX::apply_damage_to_planet != 0 && MOX::super_fast_flag == 0) {
            if (MOX::in_nebula_flag == 0) {
                graphics::Fill(0, 0, 639, 479, 0);
            }
            RUSS::Mox_Update();

            graphics::Interlaced_Fill(15, 362, 102, 374, 0, 199);
            graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
            graphics::Interlaced_Fill(401, 379, 488, 467, 0, 199);
            RUSS::Mox_Update();

            if (MOX::cur_ship > -1) {
                Draw_Internals_Display(401, 379);
            }
            RUSS::Mox_Update();

            MOX::combat_background_seg = (s_animation_header*)buffer::Buffer_Reload(s_combat_lbx, 0, MOX::global_cache_seg);
            RUSS::Mox_Update();
            animate::Draw(0, 351, MOX::combat_background_seg);

            bool is_auto_combat = false;
            if (MOX::PLAYER_NUM == MOX::combat_attacker_player) {
                if (MOX::auto_combat_flag[0] != 0) is_auto_combat = true;
            } else {
                if (MOX::auto_combat_flag[1] != 0) is_auto_combat = true;
            }

            if (is_auto_combat) {
                animate::Set_Animation_Frame((s_animation_header*)MOX::auto_button_seg, 1);
                animate::Draw(273, 364, (s_animation_header*)MOX::auto_button_seg);
            }
            RUSS::Mox_Update();

            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();

            int16_t star_idx = (MOX::in_nebula_flag == 1) ? 35 : 46;
            MOX::combat_far_star = (s_animation_header*)buffer::Buffer_Reload(s_combat_lbx, star_idx, MOX::global_cache_seg);
            RUSS::Mox_Update();

            animate::Draw(MOX::combat_far_x, MOX::combat_far_y, MOX::combat_far_star);
            animate::Draw(MOX::combat_far_x - 640, MOX::combat_far_y, MOX::combat_far_star);
            RUSS::Mox_Update();
            animate::Draw(MOX::combat_far_x - 640, MOX::combat_far_y - 480, MOX::combat_far_star);
            animate::Draw(MOX::combat_far_x, MOX::combat_far_y - 480, MOX::combat_far_star);
            RUSS::Mox_Update();

            if (MOX::draw_legal_boxes_flag == 1 && MOX::cur_ship > -1) {
                int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
                if (owner < MAX_PLAYERS && MOX::player[owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Draw_Legal_Boxes();
                }
            }
            RUSS::Mox_Update();

            MOX::combat_mid_star = (s_animation_header*)buffer::Buffer_Reload(s_combat_lbx, 47, MOX::global_cache_seg);
            RUSS::Mox_Update();
            animate::Draw(MOX::combat_mid_x, MOX::combat_mid_y, MOX::combat_mid_star);
            animate::Draw(MOX::combat_mid_x - 640, MOX::combat_mid_y, MOX::combat_mid_star);
            RUSS::Mox_Update();
            animate::Draw(MOX::combat_mid_x - 640, MOX::combat_mid_y - 480, MOX::combat_mid_star);
            animate::Draw(MOX::combat_mid_x, MOX::combat_mid_y - 480, MOX::combat_mid_star);
            RUSS::Mox_Update();

            MOX::combat_near_star = (s_animation_header*)buffer::Buffer_Reload(s_combat_lbx, 48, MOX::global_cache_seg);
            RUSS::Mox_Update();
            animate::Draw(MOX::combat_near_x, MOX::combat_near_y, MOX::combat_near_star);
            animate::Draw(MOX::combat_near_x - 640, MOX::combat_near_y, MOX::combat_near_star);
            RUSS::Mox_Update();
            animate::Draw(MOX::combat_near_x - 640, MOX::combat_near_y - 480, MOX::combat_near_star);
            animate::Draw(MOX::combat_near_x, MOX::combat_near_y - 480, MOX::combat_near_star);
            RUSS::Mox_Update();

            video::Clip_Off();
            graphics::Reset_Window();

            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].unit_status == 0 && i != MOX::dont_draw_ship) {
                    int16_t cooldown = MOX::combat_data[i].special_cooldown == COMBAT_SHIP_NONE
                        ? int16_t{-1}
                        : MOX::combat_data[i].special_cooldown;
                    if (cooldown != -1 && cooldown != MOX::dont_draw_ship) {
                        CMBTSPEC::Draw_Continuing_Tractor(i);
                    }
                }
            }

            if (MOX::cur_ship != -1) {
                fonts::Set_Font_Style_Shadow_Down(2, &MOX::combat_green);
                fonts::Set_Outline_Color(0, 0);
                strlcpy(local_ship_name, MOX::combat_data[MOX::cur_ship].name, sizeof(local_ship_name));
                BILL::Squeeze_Print_Centered(59, 362, local_ship_name, 88);
                Draw_Ship_Status(14, 376, MOX::cur_ship);
                Draw_Grey_Damage_Bars(14, 376, MOX::cur_ship);
                Draw_Crew_Level_Stars(MOX::cur_ship, 15, 379);
            }
            RUSS::Mox_Update();

            if (MOX::grid_flag == 1) {
                for (int16_t x = 0; x < 640; x += 20) {
                    line::Line(x, 0, x, 350, 4);
                }
                for (int16_t y = 0; y < 360; y += 20) {
                    line::Line(0, y, 640, y, 4);
                }
            }
            RUSS::Mox_Update();

            Draw_Reduced_Combat_Map();
            MOX::last_ship_status = -2;
        }
    }

    void __cdecl Draw_Special_Status_Display() {
        s_colors color_active;
        s_colors color_inactive;
        s_colors color_damaged;
        char msg_buffer[80];
        int16_t visible_special_count = 0;
        int16_t y_pos = 0x17c;

        if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) return;

        color_active = {{0x50, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53}};
        color_inactive = {{0xDF, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC}};
        color_damaged = {{0x48, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A}};

        graphics::Interlaced_Fill(0x72, 0x17a, 0x107, 0x1d2, 0, 0xc7 /* 199 */);

        const int16_t special_count =
            COMBINIT::amoeba_regeneration != 0 ? SPECIAL_COUNT : SPECIAL_REGENERATION;
        for (int16_t bit = 0; bit < special_count; ++bit) {
            s_combat_data* ship = &MOX::combat_data[MOX::cur_ship];
            if (struct_::Test_Bit_Field(ship->special_device_flags, bit)) {
                s_colors* current_color;

                if (struct_::Test_Bit_Field(ship->special_device_damage_flags, bit)) {
                    current_color = &color_damaged;
                } else {
                    if (CMBTFIRE::Special_Is_On(MOX::cur_ship, visible_special_count)) {
                        current_color = &color_active;
                    } else {
                        current_color = &color_inactive;
                    }
                }

                visible_special_count++;

                fonts::Set_Font_Style_Shadow_Down(1, current_color);
                fonts::Set_Outline_Color(0, 0);
                fonts::Set_Font_Spacing(1);
                fonts::Print(0x7b, y_pos, TECHDATA::specials[bit].name);

                y_pos += 11;
            }
        }

        if (visible_special_count == 0) {
            fonts::Set_Font_Style_Shadow_Down(2, &MOX::combat_green);
            fonts::Set_Outline_Color(0, 0);
            KEN::Ken_Get_Text_Message(7, msg_buffer, 0x50);
            fonts::Print_Centered(187 /* 0xbb */, 418 /* 0x1a2 */, msg_buffer);
        }
    }

    void __cdecl Draw_Main_Combat_Screen() {
        MOX::combat_clock_frame++;
        if (MOX::combat_clock_frame > 18) {
            MOX::combat_clock_frame = 15;
        }

        uint8_t page_flag = video::Get_Current_Video_Page_Flag();
        MOX::auto_cmbt_button_value = MOX::auto_combat;

        if (MOX::apply_damage_to_planet == 0) return;
        if (MOX::super_fast_flag != 0) return;

        int16_t ptr_x = mouse::Pointer_X();
        int16_t ptr_y = mouse::Pointer_Y();
        int16_t ptr_img = mouse::Get_Pointer_Image_Number();
        int16_t input_idx = fields::Scan_Input();
        int16_t hovered_missile = -1000;

        if (input_idx > 0) {
            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                if (MOX::missile_field[i] == input_idx &&
                    MOX::missile[i].type > 0 &&
                    CMBTMIS::On_Combat_Screen((MOX::missile[i].x - 8) / 20, (MOX::missile[i].y - 8) / 20)) {
                    hovered_missile = i;
                    break;
                }
            }
        }

        if (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count) {
            int16_t new_last_ship_status = MOX::last_ship_status;

            if (ptr_x / 20 < COMBAT_GRID_VIEW_WIDTH && ptr_y / 20 < COMBAT_GRID_VIEW_HEIGHT) {
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();

                int16_t size_class = MOX::combat_data[MOX::cur_ship].size_class;
                int16_t ship_draw_size;
                if (size_class == 0) {
                    ship_draw_size = 0;
                } else if (size_class > 2) {
                    ship_draw_size = 20;
                } else {
                    ship_draw_size = 10;
                }

                int16_t grid_x = (ptr_x - ship_draw_size + 12) / 20;
                int16_t grid_y = (ptr_y - ship_draw_size + 12) / 20;
                if (grid_x < 0) {
                    grid_x = 0;
                } else if (grid_x > COMBAT_GRID_VIEW_MAX_X) {
                    grid_x = COMBAT_GRID_VIEW_MAX_X;
                }

                if (grid_y < 0) {
                    grid_y = 0;
                } else if (grid_y > COMBAT_GRID_VIEW_MAX_Y) {
                    grid_y = COMBAT_GRID_VIEW_MAX_Y;
                }

                if (MOX::combat_grid[grid_x + MOX::cur_combat_x][grid_y + MOX::cur_combat_y] == 2000 &&
                    MOX::skan_flag == 0 &&
                    (MOX::game_type == 1 || MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM) &&
                    MOX::order_info_flag == 1) {
                    int16_t box_size;
                    switch (size_class) {
                        case 0:
                            box_size = 20;
                            break;
                        case 1:
                        case 2:
                            box_size = 40;
                            break;
                        case 3:
                        case 4:
                        case 5:
                            box_size = 60;
                            break;
                        default:
                            goto skip_box;
                    }

                    graphics::Box(grid_x * 20, grid_y * 20, box_size, box_size, 1, 177 /* 0xb1 */);
                }

            skip_box:
                video::Clip_Off();
                graphics::Reset_Window();

                int16_t ptr_offset = mouse::Get_Pointer_Offset();
                int16_t ptr_offset_y = mouse::Get_Pointer_Offset();
                int16_t hov_grid_x = (ptr_x + ptr_offset) / 20;
                int16_t hov_grid_y = (ptr_y + ptr_offset_y) / 20;
                if (hov_grid_x < 0) {
                    hov_grid_x = 0;
                } else if (hov_grid_x > COMBAT_GRID_VIEW_MAX_X) {
                    hov_grid_x = COMBAT_GRID_VIEW_MAX_X;
                }

                if (hov_grid_y < 0) {
                    hov_grid_y = 0;
                } else if (hov_grid_y > COMBAT_GRID_VIEW_MAX_Y) {
                    hov_grid_y = COMBAT_GRID_VIEW_MAX_Y;
                }

                int16_t hov_ship = MOX::combat_grid[hov_grid_x + MOX::cur_combat_x][hov_grid_y + MOX::cur_combat_y];

                if (MOX::ship_status_display != 0) {
                    Draw_Special_Status_Display();
                } else {
                    if (hovered_missile > -1) {
                        Draw_Weapon_Status_Display(-1, hovered_missile);
                        graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
                        fonts::Set_Font_Style_Shadow_Down(1, &MOX::combat_green);
                        fonts::Set_Outline_Color(0, 0);

                        char* weapon_name = TECHDATA::weapons[MOX::missile[hovered_missile].type].name;
                        BILL::Squeeze_Print_Centered(445, 365, weapon_name, 89);

                        CMBTMIS::Draw_Missile_Status(401, 379, hovered_missile);
                        new_last_ship_status = -1;
                    } else {
                        if ((ptr_img == 12 || ptr_img == 1) && hov_ship != -1 && hov_ship < MOX::combat_total_ship_count) {
                            Draw_Weapon_Status_Display(hov_ship, -1);
                            if (hov_ship != MOX::last_ship_status) {
                                if (MOX::combat_data[hov_ship].unit_status == 0) {
                                    video::Set_Page_Back();
                                    fonts::Set_Font_Style_Shadow_Down(2, &MOX::combat_green);
                                    fonts::Set_Outline_Color(0, 0);
                                    graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);

                                    char ship_name[32];
                                    strlcpy(ship_name, MOX::combat_data[hov_ship].name, sizeof(ship_name));
                                    BILL::Squeeze_Print_Centered(445, 362, ship_name, 89);

                                    Draw_Ship_Status(400, 376, hov_ship);
                                    Draw_Grey_Damage_Bars(400, 376, hov_ship);
                                    Draw_Crew_Level_Stars(hov_ship, 401, 379);

                                    if (page_flag == 1) {
                                        video::Set_Page_Off();
                                    }
                                    new_last_ship_status = hov_ship;
                                } else {
                                    video::Set_Page_Back();
                                    graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
                                    graphics::Interlaced_Fill(401, 379, 488, 467, 0, 199);
                                    Draw_Internals_Display(401, 379);

                                    if (page_flag == 1) {
                                        video::Set_Page_Off();
                                    }
                                    new_last_ship_status = 666; /* 0x29A */
                                }
                            }
                        } else {
                            if (MOX::last_ship_status != 666 /* 0x29A */) {
                                video::Set_Page_Back();
                                Draw_Weapon_Status_Display(-1, -1);
                                graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
                                graphics::Interlaced_Fill(401, 379, 488, 467, 0, 199);
                                Draw_Internals_Display(401, 379);

                                if (page_flag == 1) {
                                    video::Set_Page_Off();
                                }
                                new_last_ship_status = 666; /* 0x29A */
                            }
                        }
                    }
                }
            } else {
                if (MOX::last_ship_status != 666 /* 0x29A */) {
                    video::Set_Page_Back();
                    if (MOX::ship_status_display == 0) {
                        Draw_Weapon_Status_Display(-1, -1);
                    } else {
                        Draw_Special_Status_Display();
                    }
                    graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
                    graphics::Interlaced_Fill(401, 379, 488, 467, 0, 199);
                    Draw_Internals_Display(401, 379);

                    if (page_flag == 1) {
                        video::Set_Page_Off();
                    }
                    new_last_ship_status = 666; /* 0x29A */
                }
            }

            MOX::last_ship_status = new_last_ship_status;

            if (MOX::shields_flag == 1) {
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                Draw_Shield_Arcs();
                video::Clip_Off();
                graphics::Reset_Window();
            }
        }

        graphics::Set_Window(0, 0, 639, 350);
        video::Clip_On();

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].unit_status == 0 && MOX::dont_draw_ship != i) {
                    if (Ship_On_Screen(i)) {
                        Draw_Ship(i, (MOX::combat_data[i].x - MOX::cur_combat_x) * 20, (MOX::combat_data[i].y - MOX::cur_combat_y) * 20);
                    }
                }
            }
        }

        if (MOX::cur_ship > 0 && MOX::cur_ship < MOX::combat_total_ship_count && MOX::dont_draw_ship != MOX::cur_ship) {
            if (MOX::combat_data[MOX::cur_ship].unit_status == 0) {
                if (COMBAT1::Ship_Controlled_By_Computer(MOX::cur_ship) == 0) {
                    if (MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM || MOX::game_type == 1) {
                        int16_t frame = (MOX::ship_frame / 2) % 4;
                        int16_t draw_x;
                        int16_t draw_y;
                        s_animation_header* cursor_anim;

                        switch (MOX::combat_data[MOX::cur_ship].size_class) {
                            case 0:
                                cursor_anim = MOX::ship_cursor[0];
                                draw_x = (MOX::combat_data[MOX::cur_ship].x - MOX::cur_combat_x) * 20 - 3;
                                draw_y = (MOX::combat_data[MOX::cur_ship].y - MOX::cur_combat_y) * 20 - 3;
                                break;
                            case 1:
                            case 2:
                                cursor_anim = MOX::ship_cursor[1];
                                draw_x = (MOX::combat_data[MOX::cur_ship].x - MOX::cur_combat_x) * 20 - 5;
                                draw_y = (MOX::combat_data[MOX::cur_ship].y - MOX::cur_combat_y) * 20 - 4;
                                break;
                            case 3:
                            case 4:
                            case 5:
                                cursor_anim = MOX::ship_cursor[2];
                                draw_x = (MOX::combat_data[MOX::cur_ship].x - MOX::cur_combat_x) * 20 - 8;
                                draw_y = (MOX::combat_data[MOX::cur_ship].y - MOX::cur_combat_y) * 20 - 8;
                                break;
                            default:
                                goto skip_cursor;
                        }

                        animate::Set_Animation_Frame(cursor_anim, frame);
                        animate::Draw(draw_x, draw_y, cursor_anim);
                    }
                }
            }
        }

    skip_cursor:

        CMBTMIS::Draw_Missiles();
        video::Clip_Off();
        graphics::Reset_Window();

        fonts::Set_Font_Style_Shadow_Down(2, &MOX::combat_green);
        fonts::Set_Outline_Color(0, 0);

        if (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count && MOX::missile_warning > 0 &&
            MOX::combat_data[MOX::cur_ship].owner < MAX_PLAYERS &&
            MOX::player[MOX::combat_data[MOX::cur_ship].owner].objectives == PLAYER_OBJECTIVE_HUMAN) {
            if (MOX::missile_warning == 1) {
                int16_t ms_frame = animate::Get_Current_Frame((s_animation_header*)MOX::msl_warning_on_seg);
                animate::Reset_Animation_Frame((s_animation_header*)MOX::msl_warning_on_seg);
                if (ms_frame > 0) {
                    for (int16_t i = 0; i < ms_frame; ++i) {
                        animate::Draw(500, 363, (s_animation_header*)MOX::msl_warning_on_seg);
                    }
                }
                animate::Draw(500, 363, (s_animation_header*)MOX::msl_warning_on_seg);
                animate::Draw(529, 407, (s_animation_header*)MOX::warning_missile_seg);
            } else {
                Draw_Reduced_Combat_Map();
                Draw_Reduced_Map_Box();
            }
        } else {
            Draw_Current_Ship_Blip();
            Draw_Reduced_Map_Box();
        }

        MOX::ship_frame++;
        if (MOX::ship_frame == 32000) {
            MOX::ship_frame = 0;
        }

        if (MOX::order_info_flag == 0) {
            Draw_Fake_Combat_Buttons();
            MOX::combat_options_background_seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 9, MOX::global_cache_seg);
            animate::Draw(248, 298, MOX::combat_options_background_seg);
            Draw_Option_Panel_Lights();
        }

        if (MOX::auto_combat == 1) {
            Draw_Fake_Combat_Buttons();
            Draw_Animation_Speed();
        }
    }

    void __cdecl Draw_Grey_Damage_Bars(int16_t x, int16_t y, int16_t ship_idx) {
        s_animation_header* seg;

        seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x2B, MOX::global_cache_seg);
        animate::Draw(x - 1, y - 1, seg);

        seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x2C, MOX::global_cache_seg);
        animate::Draw(x, y + 0x4E, seg);

        if (ship_idx == 0) {
            seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
            animate::Draw(x + 1, y + 0x4F, seg);

            if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() != 0) {
                int16_t struct_dmg = MOX::combat_data[ship_idx].structure_damage;

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x58, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                int16_t armor_idx = INITSHIP::Best_Armor(MOX::combat_data[0].owner);
                int32_t denom = CMBTFIRE::Ground_Defense_Structure(
                    TECHDATA::armor[armor_idx].ships_bonus);
                int16_t bar_len = ((struct_dmg * 88) / denom) / 2;

                graphics::Set_Window(0, 0, x + bar_len + 1, 0x1DF);
                video::Clip_On();

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                graphics::Set_Window(x - bar_len + 0x58, 0, 0x27F, 0x1DF);

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                video::Clip_Off();
                graphics::Reset_Window();
            } else {
                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);
            }
            return;
        }

        uint8_t previous_owner = MOX::combat_data[ship_idx].previous_owner;

        if (previous_owner <= 9) {
            int16_t hull_size;

            if (COMBINIT::Is_Combat_Satellite(&MOX::combat_data[ship_idx])) {
                hull_size = COMBINIT::Combat_Satellite_Hull_Class(
                    &MOX::combat_data[ship_idx]);
            } else {
                hull_size = MOX::combat_data[ship_idx].size_class;
            }

            int16_t hull_armor = TECHDATA::hull_data[hull_size].armor_hp;
            int16_t armor_percent = TECHDATA::armor[MOX::combat_data[ship_idx].armor_type].ships_bonus;

            int32_t armor_total = hull_armor + (armor_percent * hull_armor) / 100;

            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_HEAVY_ARMOR) != 0) {
                armor_total *= 3;
            }

            int16_t armor_rem = MOX::combat_data[ship_idx].armor_remaining;
            int32_t armor_dmg = armor_total - armor_rem;

            if (armor_dmg != 0) {
                int16_t bar_len = ((armor_dmg * 88) / armor_total) / 2;

                graphics::Set_Window(0, 0, x + bar_len + 1, 0x1DF);
                video::Clip_On();

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x4F, seg);

                graphics::Set_Window(x - bar_len + 0x58, 0, 0x27F, 0x1DF);

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x4F, seg);

                video::Clip_Off();
                graphics::Reset_Window();
            }

            int16_t struct_dmg = MOX::combat_data[ship_idx].structure_damage;

            if (struct_dmg > 0) {
                int16_t bar_len = (struct_dmg * 44) / MOX::combat_data[ship_idx].structure_max;
                if (bar_len < 2) {
                    bar_len = 2;
                }

                graphics::Set_Window(x + 1, 0, x + bar_len + 1, 0x1DF);
                video::Clip_On();

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                video::Clip_Off();
                graphics::Reset_Window();
            }

            int32_t max_space = CMBTFIR2::Max_Total_Internal_Space(ship_idx);
            int32_t total_space = CMBTFIR2::Total_Internal_Space(ship_idx);
            int32_t space_dmg = max_space - total_space;

            if (space_dmg != 0) {
                int16_t bar_len = (space_dmg * 44) / max_space;
                if (bar_len < 2) {
                    bar_len = 2;
                }

                graphics::Set_Window(x - bar_len + 0x58, 0, x + 0x58, 0x1DF);
                video::Clip_On();

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                video::Clip_Off();
                graphics::Reset_Window();
            }
        } else {
            seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x59, MOX::global_cache_seg);
            animate::Draw(x + 1, y + 0x4F, seg);

            seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x58, MOX::global_cache_seg);
            animate::Draw(x + 1, y + 0x55, seg);

            int16_t struct_dmg = MOX::combat_data[ship_idx].structure_damage;

            if (struct_dmg > 0) {
                int16_t bar_len = ((struct_dmg * 88) / MOX::combat_data[ship_idx].structure_max) / 2;

                graphics::Set_Window(0, 0, x + bar_len + 1, 0x1DF);
                video::Clip_On();

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                graphics::Set_Window(x - bar_len + 0x58, 0, 0x27F, 0x1DF);

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x55, seg);

                video::Clip_Off();
                graphics::Reset_Window();
            }

            int32_t max_space = CMBTFIR2::Max_Total_Internal_Space(ship_idx);
            int32_t total_space = CMBTFIR2::Total_Internal_Space(ship_idx);
            int32_t space_dmg = max_space - total_space;

            if (space_dmg != 0) {
                int16_t bar_len = ((space_dmg * 88) / max_space) / 2;

                graphics::Set_Window(0, 0, x + bar_len + 1, 0x1DF);
                video::Clip_On();

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x4F, seg);

                graphics::Set_Window(x - bar_len + 0x58, 0, 0x27F, 0x1DF);

                seg = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x57, MOX::global_cache_seg);
                animate::Draw(x + 1, y + 0x4F, seg);

                video::Clip_Off();
                graphics::Reset_Window();
            }
        }
    }

    void __cdecl Refresh_Combat_Screen_Full() {
        COMBAT1::Add_Tactical_Combat_Fields();
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);

        if (MOX::super_fast_flag == 0) {
            video::Set_Page_Back();
            CMBTDRW1::Full_Draw_Combat_Screen();

            if (MOX::ship_status_display == 0) {
                CMBTDRW1::Draw_Weapon_Status_Display(-1, -1);
            } else {
                CMBTDRW1::Draw_Special_Status_Display();
            }

            graphics::Interlaced_Fill(0x191, 0x16a, 0x1e9, 0x176, 0, 199 /* 0xc7 */);
            graphics::Interlaced_Fill(0x191, 0x17b, 0x1e8, 0x1d3, 0, 199 /* 0xc7 */);

            CMBTDRW1::Draw_Internals_Display(0x191, 0x17b);
            MOX::last_ship_status = 666 /* 0x29a */;

            video::Copy_Back_To_Off();
            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();
            RUSS::Mox_Sync_Update();
        }
    }

    static int16_t Reduced_Map_Y(int16_t grid_y) {
        return (grid_y * 3 * ORIGINAL_COMBAT_GRID_HEIGHT) / (2 * MOX::combat_grid_height) + 364;
    }

    void __cdecl Draw_Current_Ship_Blip() {
        if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) return;

        int16_t blip_x = (MOX::combat_data[MOX::cur_ship].x * 3) / 2 + 500;
        int16_t blip_y = Reduced_Map_Y(MOX::combat_data[MOX::cur_ship].y);

        int16_t blip_frame_arr[4] = {0, 2, 5, 2};
        int16_t blip_frame = blip_frame_arr[(MOX::ship_frame / 2) % 4];

        if (MOX::cur_ship == 0) {
            int8_t p_size = MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
            if (p_size == 0) {
                Draw_Blip4(blip_x, blip_y, 0x60, blip_frame);
            } else if (p_size > 0 && p_size < PLANET_SIZE_COUNT) {
                allocate::Push_Block(MOX::screen_seg);
                s_animation_header* p_bmp = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 400);
                bitmap::Draw_Picture_To_Bitmap(p_bmp, MOX::combat_map_planet_seg);

                for (int16_t i = 0x60; i < 0x70; ++i) {
                    int16_t shade = i - blip_frame;
                    if (shade < 0x60) shade = 0x60;
                    replace::Replace_Color(p_bmp, i, (int8_t)shade);
                }

                animate::Draw(blip_x + p_size - 7, blip_y + p_size - 7, p_bmp);
                allocate::Pop_Block(MOX::screen_seg);
            }
        } else {
            int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
            int16_t blip_color = (owner == (int8_t)MOX::combat_attacker_player) ? 0x20 : 0x60;

            switch (MOX::combat_data[MOX::cur_ship].size_class) {
                case 0:
                    Draw_Blip2(blip_x, blip_y, blip_color, blip_frame);
                    break;
                case 1:
                case 2:
                    Draw_Blip3(blip_x, blip_y, blip_color, blip_frame);
                    break;
                case 3:
                    Draw_Blip4(blip_x, blip_y, blip_color, blip_frame);
                    break;
                case 4:
                case 5:
                    Draw_Blip5(blip_x, blip_y, blip_color, blip_frame);
                    break;
            }
        }
    }

    void __cdecl Draw_Reduced_Combat_Map() {
        int16_t x_pos, y_pos;
        int8_t planet_size;

        if (MOX::cmbt_colony_id != -1) {
            y_pos = Reduced_Map_Y(MOX::combat_data[0].y);
            x_pos = (MOX::combat_data[0].x * 3) / 2 + 500 /* 0x1f4 */;

            planet_size = MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;

            if (planet_size == 0) {
                Draw_Blip4(x_pos, y_pos, 0x60, 0);
            } else if (planet_size >= 1 && planet_size < PLANET_SIZE_COUNT) {
                animate::Draw(x_pos + planet_size - 7, y_pos + planet_size - 7, MOX::combat_map_planet_seg);
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].unit_status == 0) {
                uint8_t color = (MOX::combat_data[i].owner == MOX::combat_attacker_player) ? 0x20 : 0x60;

                y_pos = Reduced_Map_Y(MOX::combat_data[i].y);
                x_pos = (MOX::combat_data[i].x * 3) / 2 + 500 /* 0x1f4 */;

                switch (MOX::combat_data[i].size_class) {
                    case 0:
                        Draw_Blip2(x_pos, y_pos, color, 0);
                        break;
                    case 1:
                    case 2:
                        Draw_Blip3(x_pos, y_pos, color, 0);
                        break;
                    case 3:
                        Draw_Blip4(x_pos, y_pos, color, 0);
                        break;
                    case 4:
                    case 5:
                        Draw_Blip5(x_pos, y_pos, color, 0);
                        break;
                    default:
                        break;
                }
            }
        }

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (MOX::missile[i].type > 0) {
                y_pos = Reduced_Map_Y(MOX::missile[i].y / 20);
                x_pos = ((MOX::missile[i].x / 20) * 3) / 2 + 500 /* 0x1f4 */;
                graphics::Dot(x_pos, y_pos, 0x1c);
            }
        }
    }

    void __cdecl Draw_Crew_Level_Stars(int16_t ship_idx, int16_t x, int16_t y) {
        int16_t current_x;
        int16_t counter;
        char buffer[80];

        if (ship_idx != 0) {
            counter = 0;
            current_x = x;
            if (MOX::combat_data[ship_idx].crew_quality > CREW_QUALITY_GREEN) {
                do {
                    counter++;
                    animate::Draw(current_x, y, (s_animation_header*)MOX::level_symbol_seg);
                    current_x += 11;
                } while (counter < MOX::combat_data[ship_idx].crew_quality);
            }
        }

        fonts::Set_Font_Style(2, &MOX::combat_green);

        if (MOX::combat_data[ship_idx].officer_idx > -1) {
            KEN::Ken_Get_Text_Message(86, buffer, 80);
            fonts::Print_Right(x + 87, y + 2, buffer);
        }
    }

    void __cdecl Draw_Reduced_Map_Box() {
        if (MOX::combat_grid_height != ORIGINAL_COMBAT_GRID_HEIGHT) {
            const int16_t left = 499 + MOX::cur_combat_x * 3 / 2;
            const int16_t top = 363 + MOX::cur_combat_y * 102 / MOX::combat_grid_height;
            const int16_t bottom = 364 + (MOX::cur_combat_y + COMBAT_GRID_VIEW_HEIGHT) * 102 / MOX::combat_grid_height;
            line::Line(left, top, left + 50, top, 138);
            line::Line(left, bottom, left + 50, bottom, 138);
            line::Line(left, top, left, bottom, 138);
            line::Line(left + 50, top, left + 50, bottom, 138);
            return;
        }
        int16_t base_y = (MOX::cur_combat_y * 3) / 2;
        int16_t base_x = (MOX::cur_combat_x * 3) / 2;

        // Top-left corner
        line::Line(base_x + 499 /* 0x1f3 */, base_y + 363 /* 0x16b */, base_x + 506 /* 0x1fa */, base_y + 363 /* 0x16b */, 138 /* 0x8a */);
        line::Line(base_x + 499 /* 0x1f3 */, base_y + 363 /* 0x16b */, base_x + 499 /* 0x1f3 */, base_y + 370 /* 0x172 */, 138 /* 0x8a */);

        // Bottom-left corner
        line::Line(base_x + 499 /* 0x1f3 */, base_y + 392 /* 0x188 */, base_x + 506 /* 0x1fa */, base_y + 392 /* 0x188 */, 138 /* 0x8a */);
        line::Line(base_x + 499 /* 0x1f3 */, base_y + 392 /* 0x188 */, base_x + 499 /* 0x1f3 */, base_y + 385 /* 0x181 */, 138 /* 0x8a */);

        // Top-right corner
        line::Line(base_x + 549 /* 0x225 */, base_y + 363 /* 0x16b */, base_x + 542 /* 0x21e */, base_y + 363 /* 0x16b */, 138 /* 0x8a */);
        line::Line(base_x + 549 /* 0x225 */, base_y + 363 /* 0x16b */, base_x + 549 /* 0x225 */, base_y + 370 /* 0x172 */, 138 /* 0x8a */);

        // Bottom-right corner
        line::Line(base_x + 549 /* 0x225 */, base_y + 392 /* 0x188 */, base_x + 542 /* 0x21e */, base_y + 392 /* 0x188 */, 138 /* 0x8a */);
        line::Line(base_x + 549 /* 0x225 */, base_y + 392 /* 0x188 */, base_x + 549 /* 0x225 */, base_y + 385 /* 0x181 */, 138 /* 0x8a */);
    }

    void __cdecl Draw_Fake_Combat_Buttons() {
        animate::Set_Animation_Frame((s_animation_header*)MOX::weapons_button_seg, 1 - MOX::ship_status_display);
        animate::Draw(0x72, 0x169, (s_animation_header*)MOX::weapons_button_seg);

        animate::Set_Animation_Frame((s_animation_header*)MOX::specials_button_seg, MOX::ship_status_display);
        animate::Draw(0xbb, 0x169, (s_animation_header*)MOX::specials_button_seg);

        if (MOX::order_info_flag == 1) {
            animate::Set_Animation_Frame((s_animation_header*)MOX::auto_button_seg, MOX::auto_combat);
            animate::Draw(0x111, 0x16c, (s_animation_header*)MOX::auto_button_seg);

            animate::Set_Animation_Frame((s_animation_header*)MOX::scan_button_seg, MOX::skan_flag);
            animate::Draw(0x151, 0x16c, (s_animation_header*)MOX::scan_button_seg);

            animate::Reset_Animation_Frame((s_animation_header*)MOX::board_button_seg);
            animate::Draw(0x111, 0x188, (s_animation_header*)MOX::board_button_seg);

            animate::Reset_Animation_Frame((s_animation_header*)MOX::flee_button_seg);
            animate::Draw(0x151, 0x188, (s_animation_header*)MOX::flee_button_seg);

            animate::Reset_Animation_Frame((s_animation_header*)MOX::wait_button_seg);
            animate::Draw(0x111, 0x1a4, (s_animation_header*)MOX::wait_button_seg);

            animate::Reset_Animation_Frame((s_animation_header*)MOX::cmbt_done_button_seg);
            animate::Draw(0x151, 0x1a4, (s_animation_header*)MOX::cmbt_done_button_seg);

            animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_info_button_seg, 0);
            animate::Draw(0x12a, 0x1c0, (s_animation_header*)MOX::cmbt_info_button_seg);
        } else {
            CMBTDRW1::Draw_Option_Panel_Lights();

            animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_info_button_seg, 0);
            animate::Draw(0x12a, 0x12d, (s_animation_header*)MOX::cmbt_info_button_seg);

            animate::Set_Animation_Frame((s_animation_header*)MOX::self_destruct_seg, 0);
            animate::Draw(0x11c, 0x15c, (s_animation_header*)MOX::self_destruct_seg);
        }
    }

    void __cdecl Draw_Option_Panel_Lights() {
        animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, MOX::msl_impact_warning_flag);
        animate::Draw(0x10B, 0x182, (s_animation_header*)MOX::cmbt_option_seg);

        animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, MOX::speedx2_flag);
        animate::Draw(0x10B, 0x192, (s_animation_header*)MOX::cmbt_option_seg);

        animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, MOX::draw_legal_boxes_flag);
        animate::Draw(0x10B, 0x1A2, (s_animation_header*)MOX::cmbt_option_seg);

        animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, MOX::shields_flag);
        animate::Draw(0x10B, 0x1B2, (s_animation_header*)MOX::cmbt_option_seg);

        animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, MOX::grid_flag);
        animate::Draw(0x10B, 0x1C2, (s_animation_header*)MOX::cmbt_option_seg);
    }

    /**
     * Draws a 2x2 pixel "blip" with varying intensities.
     * 
     * @param x Screen X coordinate
     * @param y Screen Y coordinate
     * @param intensity_a First component of intensity/color
     * @param intensity_b Second component of intensity/color
     */
    void __cdecl Draw_Blip2(int16_t x, int16_t y, int16_t intensity_a, int16_t intensity_b) {
        int16_t base_intensity = intensity_a + intensity_b;

        // Top-left pixel
        graphics::Dot(x, y, (uint8_t)(base_intensity + 9));

        // Top-right pixel
        graphics::Dot(x + 1, y, (uint8_t)(base_intensity + 6));

        // Bottom-left pixel
        graphics::Dot(x, y + 1, (uint8_t)(base_intensity + 6));

        // Bottom-right pixel
        graphics::Dot(x + 1, y + 1, (uint8_t)(base_intensity + 4));
    }

    /**
     * Draws a 3x3 blip on the screen using multiple dots with varying color offsets.
     * The color of each dot is relative to the sum of p3 and p4.
     */
    void __cdecl Draw_Blip3(int16_t x, int16_t y, int16_t base_color, int16_t color_offset) {
        int16_t color_base = base_color + color_offset;

        // Row 0
        graphics::Dot(x, y, (uint8_t)(color_base + 9));
        graphics::Dot(x + 1, y, (uint8_t)(color_base + 6));
        graphics::Dot(x + 2, y, (uint8_t)(color_base + 4));

        // Row 1
        graphics::Dot(x, y + 1, (uint8_t)(color_base + 6));
        graphics::Dot(x + 1, y + 1, (uint8_t)(color_base + 6));
        graphics::Dot(x + 2, y + 1, (uint8_t)(color_base + 4));

        // Row 2
        graphics::Dot(x, y + 2, (uint8_t)(color_base + 4));
        graphics::Dot(x + 1, y + 2, (uint8_t)(color_base + 4));
        graphics::Dot(x + 2, y + 2, (uint8_t)(color_base + 1));
    }

    void __cdecl Draw_Blip4(int16_t x, int16_t y, int16_t base_color, int16_t color_offset) {
        int16_t color_sum = base_color + color_offset;

        graphics::Dot(x, y, (uint8_t)(color_sum + 1));
        graphics::Dot(x + 1, y, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 2, y, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 3, y, (uint8_t)(color_sum + 1));

        graphics::Dot(x, y + 1, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 1, y + 1, (uint8_t)(color_sum + 9));
        graphics::Dot(x + 2, y + 1, (uint8_t)(color_sum + 6));
        graphics::Dot(x + 3, y + 1, (uint8_t)(color_sum + 4));

        graphics::Dot(x, y + 2, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 1, y + 2, (uint8_t)(color_sum + 6));
        graphics::Dot(x + 2, y + 2, (uint8_t)(color_sum + 6));
        graphics::Dot(x + 3, y + 2, (uint8_t)(color_sum + 4));

        graphics::Dot(x, y + 3, (uint8_t)(color_sum + 1));
        graphics::Dot(x + 1, y + 3, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 2, y + 3, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 3, y + 3, (uint8_t)(color_sum + 1));
    }

    /**
     * Draws a 5x5 sprite blip on the screen using a calculated color base.
     * The color of each pixel is determined by the sum of two color parameters and a local offset.
     */
    void __cdecl Draw_Blip5(int16_t x, int16_t y, int16_t color_base_a, int16_t color_base_b) {
        uint8_t base = (uint8_t)(color_base_a + color_base_b);

        // Row 0
        graphics::Dot(x, y, base + 2);
        graphics::Dot(x + 1, y, base + 4);
        graphics::Dot(x + 2, y, base + 4);
        graphics::Dot(x + 3, y, base + 4);
        graphics::Dot(x + 4, y, base + 1);

        // Row 1
        graphics::Dot(x, y + 1, base + 4);
        graphics::Dot(x + 1, y + 1, base + 9);
        graphics::Dot(x + 2, y + 1, base + 9);
        graphics::Dot(x + 3, y + 1, base + 6);
        graphics::Dot(x + 4, y + 1, base + 4);

        // Row 2
        graphics::Dot(x, y + 2, base + 4);
        graphics::Dot(x + 1, y + 2, base + 9);
        graphics::Dot(x + 2, y + 2, base + 6);
        graphics::Dot(x + 3, y + 2, base + 6);
        graphics::Dot(x + 4, y + 2, base + 4);

        // Row 3
        graphics::Dot(x, y + 3, base + 4);
        graphics::Dot(x + 1, y + 3, base + 6);
        graphics::Dot(x + 2, y + 3, base + 6);
        graphics::Dot(x + 3, y + 3, base + 6);
        graphics::Dot(x + 4, y + 3, base + 4);

        // Row 4
        graphics::Dot(x, y + 4, base + 1);
        graphics::Dot(x + 1, y + 4, base + 4);
        graphics::Dot(x + 2, y + 4, base + 4);
        graphics::Dot(x + 3, y + 4, base + 4);
        graphics::Dot(x + 4, y + 4, base + 1);
    }

    void __cdecl Draw_Super_Fast_Screen() {
        MOX::super_fast_flag = 0;
        video::Set_Page_Off();
        CMBTDRW1::Full_Draw_Combat_Screen();

        if (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count) {
            if (MOX::ship_status_display == 0) {
                CMBTDRW1::Draw_Weapon_Status_Display(-1, -1);
            } else {
                CMBTDRW1::Draw_Special_Status_Display();
            }
        }

        graphics::Interlaced_Fill(0x191, 0x16a, 0x1e9, 0x176, 0x0, 199 /* 0xc7 */);
        graphics::Interlaced_Fill(0x191, 0x17b, 0x1e8, 0x1d3, 0x0, 199 /* 0xc7 */);

        if (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count) {
            CMBTDRW1::Draw_Internals_Display(0x191, 0x17b);
        }
        MOX::last_ship_status = 666 /* 0x29a */;

        CMBTDRW1::Draw_Main_Combat_Screen();
        fields::Draw_Visible_Fields();
        video::Copy_Off_To_Back();
        RUSS::Mox_Sync_Update();

        MOX::super_fast_flag = 1;
    }

    void __cdecl Move_Options_Background() {
        int16_t target_y;
        int16_t current_y;
        int16_t step;

        if (MOX::order_info_flag == 0) {
            MOX::order_info_flag = 1;
            current_y = 445;
            target_y = 298;
            step = -7;
            video::Set_Page_Back();
            Full_Draw_Combat_Screen();
            Draw_Main_Combat_Screen();
            Draw_Fake_Combat_Buttons();
            video::Copy_Back_To_Off();
            RUSS::Mox_Sync_Update();
            MOX::order_info_flag = 0;
        } else {
            current_y = 298;
            target_y = 445;
            step = 7;
            video::Set_Page_Back();
            Full_Draw_Combat_Screen();
            Draw_Main_Combat_Screen();
            Draw_Fake_Combat_Buttons();
            video::Copy_Back_To_Off();
            video::Set_Page_Off();
            MOX::combat_options_background_seg = (s_animation_header*)buffer::Buffer_Reload(s_combat_lbx, 9, MOX::global_cache_seg);
            animate::Draw(248, 298, MOX::combat_options_background_seg);
            RUSS::Mox_Sync_Update();
        }

        fields::Disable_Draw_All_Fields();

        if (current_y != target_y) {
            do {
                current_y += step;

                if (step < 0) {
                    if (current_y < target_y) {
                        current_y = target_y;
                    }
                } else if (current_y > target_y) {
                    current_y = target_y;
                }

                video::Set_Page_Off();
                animate::Set_Animation_Frame((s_animation_header*)MOX::weapons_button_seg, 1 - MOX::ship_status_display);
                animate::Draw(114, 361, (s_animation_header*)MOX::weapons_button_seg);
                animate::Set_Animation_Frame((s_animation_header*)MOX::specials_button_seg, MOX::ship_status_display);
                animate::Draw(187, 361, (s_animation_header*)MOX::specials_button_seg);

                graphics::Set_Window(0, 0, 639, 479);
                video::Clip_On();

                MOX::combat_options_background_seg = (s_animation_header*)buffer::Buffer_Reload(s_combat_lbx, 9, MOX::global_cache_seg);
                animate::Draw(248, current_y, MOX::combat_options_background_seg);

                animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, (int32_t)MOX::msl_impact_warning_flag);
                animate::Draw(267, current_y + 88, (s_animation_header*)MOX::cmbt_option_seg);

                animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, (int32_t)MOX::speedx2_flag);
                animate::Draw(267, current_y + 104, (s_animation_header*)MOX::cmbt_option_seg);

                animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, (int32_t)MOX::draw_legal_boxes_flag);
                animate::Draw(267, current_y + 120, (s_animation_header*)MOX::cmbt_option_seg);

                animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, (int32_t)MOX::shields_flag);
                animate::Draw(267, current_y + 136, (s_animation_header*)MOX::cmbt_option_seg);

                animate::Set_Animation_Frame((s_animation_header*)MOX::cmbt_option_seg, (int32_t)MOX::grid_flag);
                animate::Draw(267, current_y + 152, (s_animation_header*)MOX::cmbt_option_seg);

                video::Clip_Off();
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();

            } while (current_y != target_y);
        }

        COMBAT1::Add_Tactical_Combat_Fields();
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
        fields::Enable_Draw_All_Fields();
        Refresh_Combat_Screen_Full();
    }

    void __cdecl Display_Combat_View_Ship(int16_t ship_idx) {
        struct_::Copy_Structure(&MOX::global_combat_data,
            &MOX::combat_data[ship_idx],
            sizeof(MOX::global_combat_data));
        MOX::ship_data_id = ship_idx;
        combat_view_subject = Tactical_Combat_View_Subject(ship_idx);

        video::Set_Page_Off();
        MOX::last_ship_status = 1;
        CMBTDRW1::Full_Draw_Combat_Screen();

        if (MOX::ship_status_display == 0) {
            CMBTDRW1::Draw_Weapon_Status_Display(-1, -1);
        } else {
            CMBTDRW1::Draw_Special_Status_Display();
        }

        CMBTDRW1::Draw_Main_Combat_Screen();
        video::Copy_Off_To_Back();
        RUSS::Mox_Sync_Update();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        memset(MOX::combat_grid, 0xff, sizeof(MOX::combat_grid));

        CMBTDRW1::Detailed_View_Ship(-1);
        COMBAT1::Add_Tactical_Combat_Fields();
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
        CMBTDRW1::Refresh_Combat_Screen_Full();
        fields::Assign_Auto_Function(CMBTDRW1::Draw_Main_Combat_Screen, 1);
    }

    int16_t __cdecl Board_Popup(int16_t min_val, int16_t max_val, char* msg, int16_t* out_val) {
        tactical_control::Close_Decision();
        int16_t result = 1;
        int16_t done = 0;

        g_min = min_val;
        g_max = max_val;
        g_msg = msg;
        g_var = max_val;

        allocate::Push_Block(MOX::screen_seg);

        board_left_button_seg = (s_animation_header*)farload::Far_Reload_Next("combat.LBX", 6, MOX::screen_seg);
        board_right_button_seg = (s_animation_header*)farload::Far_Reload_Next("combat.LBX", 40, MOX::screen_seg);
        board_raid_button_seg = (s_animation_header*)farload::Far_Reload_Next("combat.LBX", 39, MOX::screen_seg);
        board_capture_button_seg = (s_animation_header*)farload::Far_Reload_Next("combat.LBX", 41, MOX::screen_seg);

        video::Set_Page_Back();
        Full_Draw_Combat_Screen();
        Draw_Main_Combat_Screen();
        fields::Draw_Visible_Fields_Back();
        video::Copy_Back_To_Off();
        RUSS::Mox_Sync_Update();
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Board_Popup, 1);
        fields::Clear_Fields();

        fields::Add_Scroll_Field(0xbb, 0xf9, min_val, max_val + 1, min_val, max_val, 0xcf, 0xd, (short*)&g_var, "", 0);

        int16_t btn_left = fields::Add_Button_Field(0xa4, 0xfd, "", board_left_button_seg, "", 0);
        int16_t btn_right = fields::Add_Button_Field(0x196, 0xfd, "", board_right_button_seg, "", 0);
        int16_t btn_raid = fields::Add_Button_Field(0xa3, 0x111, "", board_raid_button_seg, "", 0);
        int16_t btn_capture = fields::Add_Button_Field(0x159, 0x111, "", board_capture_button_seg, "", 0);
        int16_t btn_cancel = fields::Add_Hot_Key("\x1B");

        video::Set_Page_Off();
        Draw_Board_Popup();
        RUSS::Mox_Sync_Update();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        while (!done) {
            int16_t input = fields::Get_Input();
            fields::Scan_Input();

            if (input == btn_capture) {
                done = 1;
                result = 2;
            }

            if (input == btn_raid) {
                done = 1;
                result = 1;
            }

            if (input == btn_right && g_var < max_val) {
                g_var++;
            }

            if (input == btn_left && g_var > 0) {
                g_var--;
            }

            if (input == btn_cancel) {
                result = 0;
                done = 1;
            }

            if (!done) {
                video::Set_Page_Off();
                Draw_Board_Popup();
                RUSS::Mox_Sync_Update();
            }
        }

        allocate::Pop_Block(MOX::screen_seg);
        fields::Clear_Fields();
        fields::Assign_Auto_Function(Draw_Main_Combat_Screen, 1);
        Refresh_Combat_Screen_Full();
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);

        *out_val = g_var;

        return result;
    }

    int16_t __cdecl Ship_Completely_On_Screen(int16_t ship_idx) {
        int16_t idx = ship_idx;
        int16_t x = (int16_t)MOX::combat_data[idx].x;
        int16_t y = (int16_t)MOX::combat_data[idx].y;

        int16_t result = (int16_t)(CMBTMIS::On_Combat_Screen(x, y) & 0x00FF);

        if (result == 0) {
            return 0;
        }

        if (ship_idx == 0) {
            int16_t planet_size;
            if (MOX::combat_defender_player == 8) {
                planet_size = 2;
            } else {
                int16_t colony_idx = MOX::cmbt_colony_id;
                int16_t planet_idx = MOX::colony[colony_idx].planet;
                planet_size = (int16_t)MOX::planet[planet_idx].size;
            }

            int16_t dx = 0, dy = 0;
            bool do_planet_check = true;

            switch (planet_size) {
                case 0:
                    dx = 2;
                    dy = 2;
                    break;
                case 1:
                    dx = 3;
                    dy = 3;
                    break;
                case 2:
                    dx = 5;
                    dy = 5;
                    break;
                case 3:
                    dx = 6;
                    dy = 6;
                    break;
                case 4:
                    dx = 7;
                    dy = 7;
                    break;
                default:
                    do_planet_check = false;
                    break;
            }

            if (do_planet_check) {
                result &= (int16_t)(CMBTMIS::On_Combat_Screen(x + dx, y + dy) & 0x00FF);
            }
        }

        int8_t size_class = MOX::combat_data[idx].size_class;
        switch (size_class) {
            case 1:
            case 2:
                result &= (int16_t)(CMBTMIS::On_Combat_Screen(x + 1, y + 1) & 0x00FF);
                break;
            case 3:
            case 4:
            case 5:
                result = (int16_t)(CMBTMIS::On_Combat_Screen(x + 2, y + 2) & 0x00FF);
                break;
            default:
                break;
        }

        return result;
    }

    int16_t __cdecl Ship_On_Screen(int16_t ship_idx) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];
        int16_t x = ship->x;
        int16_t y = ship->y;

        int16_t combined_result = (int16_t)CMBTMIS::On_Combat_Screen(x, y);

        if (ship_idx == 0) {
            int16_t planet_size = 2;

            if (MOX::combat_defender_player != 8) {
                int16_t colony_id = MOX::cmbt_colony_id;
                int16_t planet_idx = MOX::colony[colony_id].planet;
                planet_size = MOX::planet[planet_idx].size;
            }

            switch (planet_size) {
                case 0:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 2, y + 2);
                    break;
                case 1:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 3, y + 3);
                    break;
                case 2:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 5, y + 5);
                    break;
                case 3:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 6, y + 6);
                    break;
                case 4:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 7, y + 7);
                    break;
                default:
                    break;
            }
        } else {
            int8_t size_class = ship->size_class;

            switch (size_class) {
                case 1:
                case 2:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 1, y + 1);
                    break;
                case 3:
                case 4:
                case 5:
                    combined_result |= (int16_t)CMBTMIS::On_Combat_Screen(x + 2, y + 2);
                    break;
                default:
                    break;
            }
        }

        return combined_result;
    }

    void __cdecl Scroll_Draw_Combat_Screen() {
        graphics::Set_Window(0, 0, 639, 350);
        video::Clip_On();

        if (MOX::in_nebula_flag == 0) {
            graphics::Fill(0, 0, 639, 350, 0);
        }

        int index = (MOX::in_nebula_flag == 1) ? 35 /* 0x23 */ : 46 /* 0x2e */;
        MOX::combat_far_star = (s_animation_header*)buffer::Buffer_Reload(MOX::s_combat_lbx, index, MOX::global_cache_seg);

        animate::Draw(MOX::combat_far_x, MOX::combat_far_y, MOX::combat_far_star);
        animate::Draw(MOX::combat_far_x - 640, MOX::combat_far_y, MOX::combat_far_star);
        animate::Draw(MOX::combat_far_x - 640, MOX::combat_far_y - 480, MOX::combat_far_star);
        animate::Draw(MOX::combat_far_x, MOX::combat_far_y - 480, MOX::combat_far_star);

        MOX::combat_mid_star = (s_animation_header*)buffer::Buffer_Reload(MOX::s_combat_lbx, 47 /* 0x2f */, MOX::global_cache_seg);
        animate::Draw(MOX::combat_mid_x, MOX::combat_mid_y, MOX::combat_mid_star);
        animate::Draw(MOX::combat_mid_x - 640, MOX::combat_mid_y, MOX::combat_mid_star);
        animate::Draw(MOX::combat_mid_x - 640, MOX::combat_mid_y - 480, MOX::combat_mid_star);
        animate::Draw(MOX::combat_mid_x, MOX::combat_mid_y - 480, MOX::combat_mid_star);

        MOX::combat_near_star = (s_animation_header*)buffer::Buffer_Reload(MOX::s_combat_lbx, 48 /* 0x30 */, MOX::global_cache_seg);
        animate::Draw(MOX::combat_near_x, MOX::combat_near_y, MOX::combat_near_star);
        animate::Draw(MOX::combat_near_x - 640, MOX::combat_near_y, MOX::combat_near_star);
        animate::Draw(MOX::combat_near_x - 640, MOX::combat_near_y - 480, MOX::combat_near_star);
        animate::Draw(MOX::combat_near_x, MOX::combat_near_y - 480, MOX::combat_near_star);

        video::Clip_Off();
        graphics::Reset_Window();

        if (MOX::grid_flag == 1) {
            for (int16_t x = 0; x < 640; x += 20) {
                line::Line(x, 0, x, 350, 4);
            }
            for (int16_t y = 0; y < 360; y += 20) {
                line::Line(0, y, 640, y, 4);
            }
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].unit_status == 0) {
                if (MOX::dont_draw_ship != i) {
                    int16_t cooldown = MOX::combat_data[i].special_cooldown == COMBAT_SHIP_NONE
                        ? int16_t{-1}
                        : MOX::combat_data[i].special_cooldown;
                    if (cooldown != -1 && cooldown != MOX::dont_draw_ship) {
                        CMBTSPEC::Draw_Continuing_Tractor(i);
                    }
                }
            }
        }

        graphics::Set_Window(0, 0, 639, 350);
        video::Clip_On();

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].unit_status == 0) {
                int16_t sx = (MOX::combat_data[i].x - MOX::cur_combat_x) * 20;
                int16_t sy = (MOX::combat_data[i].y - MOX::cur_combat_y) * 20;
                Draw_Ship(i, sx, sy);
            }
        }

        CMBTMIS::Draw_Missiles();
        video::Clip_Off();
        graphics::Reset_Window();

        if (MOX::missile_warning == 1) {
            int16_t frames = animate::Get_Current_Frame(MOX::msl_warning_on_seg);
            animate::Reset_Animation_Frame(MOX::msl_warning_on_seg);
            while (frames > 0) {
                animate::Draw(500, 363, MOX::msl_warning_on_seg);
                frames--;
            }
            animate::Draw(500, 363, MOX::msl_warning_on_seg);
            animate::Draw(529, 407, MOX::warning_missile_seg);
        } else {
            Draw_Reduced_Combat_Map();
            Draw_Reduced_Map_Box();
        }

        if (MOX::last_ship_status != 666 /* 0x29a */) {
            if (MOX::ship_status_display == 0) {
                Draw_Weapon_Status_Display(-1, -1);
            } else {
                Draw_Special_Status_Display();
            }
            graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
            graphics::Interlaced_Fill(401, 379, 488, 467, 0, 199);
            Draw_Internals_Display(401, 379);
        }

        if (MOX::order_info_flag == 0) {
            Draw_Fake_Combat_Buttons();
            MOX::combat_options_background_seg = (s_animation_header*)buffer::Buffer_Reload(MOX::s_combat_lbx, 9, MOX::global_cache_seg);
            animate::Draw(248, 298, MOX::combat_options_background_seg);
            Draw_Option_Panel_Lights();
        }
    }

    uint8_t __cdecl Get_Weapon_Arc(int16_t bearing) {
        uint8_t arc_flags = WEAPON_FIRING_ARC_NONE;

        // Front arc bit (approx. +/- 60 degrees)
        if (bearing <= 60 || bearing >= 300) {
            arc_flags = WEAPON_FIRING_ARC_FORWARD;
        }

        // Side-extended front arc bit (approx. +/- 120 degrees)
        if (bearing <= 120 || bearing >= 240) {
            arc_flags += WEAPON_FIRING_ARC_FORWARD_EXTENDED;
        }

        // Rear-extended back arc bit (approx. 60 to 300 degrees)
        if (bearing >= 60 && bearing <= 300) {
            arc_flags += WEAPON_FIRING_ARC_BACK_EXTENDED;
        }

        // Back arc bit (approx. 120 to 240 degrees)
        if (bearing >= 120 && bearing <= 240) {
            arc_flags += WEAPON_FIRING_ARC_REAR;
        }

        return arc_flags;
    }

    void __cdecl Get_Draw_Ship_Offsets(int16_t ship_idx, int16_t* out_x, int16_t* out_y) {
        *out_x = 0;
        *out_y = 0;

        if (ship_idx == 0) {
            if (MOX::cmbt_colony_id == -1) {
                *out_x = 7;
                *out_y = 7;
                return;
            }

            static const int16_t planet_offsets[PLANET_SIZE_COUNT][2] = {
                {2, 2},
                {0, 1},
                {7, 7},
                {4, 4},
                {6, 6},
            };

            int16_t planet_size =
                MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
            if (planet_size < PLANET_SIZE_TINY ||
                planet_size >= PLANET_SIZE_COUNT) {
                return;
            }

            *out_x = planet_offsets[planet_size][0];
            *out_y = planet_offsets[planet_size][1];
            return;
        }

        int8_t facing_dir = MOX::combat_data[ship_idx].facing_dir;

        switch ((MOX::combat_data[ship_idx].size_class + 1) / 2) {
            case 0:
                *out_x = -16;
                *out_y = -16;
                break;
            case 1:
                *out_x = -6;
                *out_y = -6;
                break;
            case 2:
            case 3:
                *out_x = 4;
                *out_y = 4;
                break;
            default:
                break;
        }

        int facing_quadrant = (facing_dir - 1) / 4;
        if (facing_quadrant <= 0) {
            facing_quadrant = 0;
        }

        if (facing_quadrant == 1) {
            *out_x -= 2;
        } else if (facing_quadrant == 2) {
            *out_x -= 2;
            *out_y -= 1;
        } else if (facing_quadrant == 3) {
            *out_y -= 1;
        }

        *out_x -= 2;
        *out_y -= 3;
    }

    void __cdecl Draw_Ship_To_Bitmap(int16_t ship_idx, int16_t layer, int16_t plasma_mode, int16_t plasma_progress) {
        int16_t facing_dir = MOX::combat_data[ship_idx].facing_dir;
        s_animation_header* anim = MOX::combat_data[ship_idx].ship_image_seg;
        uint8_t previous_owner = MOX::combat_data[ship_idx].previous_owner;

        if (anim == nullptr || MOX::combat_data[ship_idx].owner != previous_owner) {
            uint8_t attacker_palette_remap[32];
            uint8_t defender_palette_remap[32];

            for (int16_t i = 0; i < 32; ++i) {
                attacker_palette_remap[i] = i + 0x20;
                defender_palette_remap[i] = i + 0x60;
            }

            char lbx_name[12];
            int32_t picture_num;

            if (previous_owner < MAX_PLAYERS && MOX::combat_data[ship_idx].picture_num != 0x2C) {
                strlcpy(lbx_name, "cmbtshp.lbx", sizeof(lbx_name));
                uint8_t color = MOX::player[previous_owner].color;
                picture_num = color * 45 + MOX::combat_data[ship_idx].picture_num;
            } else {
                strlcpy(lbx_name, "monster.lbx", sizeof(lbx_name));
                if (MOX::combat_data[ship_idx].picture_num == 0x2C) {
                    picture_num = 25;
                } else {
                    picture_num = MOX::combat_data[ship_idx].picture_num;
                }
            }

            anim = (s_animation_header*)buffer::Buffer_Reload(lbx_name, picture_num, MOX::global_cache_seg);

            if (MOX::combat_data[ship_idx].owner == MOX::combat_attacker_player) {
                replace::Replace_Color_Range(anim, attacker_palette_remap, 0x60, 0x20);
            } else {
                replace::Replace_Color_Range(anim, defender_palette_remap, 0x20, 0x20);
            }
        }

        int32_t dir_group = (facing_dir - 1) / 4;
        if (dir_group <= 0) {
            dir_group = 0;
        }

        switch (dir_group) {
            case 0:
                animate::Set_Animation_Frame(anim, facing_dir * 4);
                bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, anim);
                break;
            case 1:
                animate::Set_Animation_Frame(anim, (8 - facing_dir) * 4);
                bitmap::Draw_Picture_To_Bitmap(MOX::ship_bitmap, anim);
                bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 0);
                break;
            case 2:
                animate::Set_Animation_Frame(anim, (facing_dir - 8) * 4);
                bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, anim);
                bitmap::Flip_Bitmap(MOX::ship_bitmap, MOX::draw_ship_bitmap, 0);
                bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 3);
                break;
            case 3:
                animate::Set_Animation_Frame(anim, (16 - facing_dir) * 4);
                bitmap::Draw_Picture_To_Bitmap(MOX::ship_bitmap, anim);
                bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 3);
                break;
            default:
                break;
        }

        if (plasma_mode != 0) {
            Plasma_Darken(MOX::draw_ship_bitmap, plasma_progress, plasma_mode);
        }
    }

    /**
     * Calculates the screen drawing coordinates for the center of a ship in combat.
     * It combines the world-to-screen coordinate transformation (multiplied by 20)
     * with local ship-specific centering offsets.
     */
    void __cdecl Draw_Ship_Status(int16_t x, int16_t y, int16_t ship_idx) {
        if (x < 0x14) {
            graphics::Fill(0xf, 0x17b, 0x66, 0x1c5, 0);
            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x2d, MOX::global_cache_seg);
            animate::Draw(0xf, 0x17b, anim);
        } else if (x > 0x17c) {
            graphics::Fill(0x191, 0x17b, 0x1e8, 0x1d3, 0);
            s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 0x2d, MOX::global_cache_seg);
            animate::Draw(0x191, 0x17b, anim);
        }

        if (ship_idx == 0) {
            animate::Draw(x + 0x10, y + 0xf, MOX::combat_planet_seg2);
            s_combat_data* planet_combat = &MOX::combat_data[0];

            if (planet_combat->owner != 8) {
                char combat_text_segment[80];
                char colony_status_text[80];
                char value_text[12];

                fonts::Set_Font_Style(0, &MOX::combat_green);
                KEN::Ken_Get_Text_Message(0, combat_text_segment, 0x50);
                strlcpy(colony_status_text, combat_text_segment, sizeof(colony_status_text));

                if (MOX::cmbt_colony_id == -1) {
                    snprintf(value_text, sizeof(value_text), "%i", 0x23);
                } else {
                    snprintf(value_text, sizeof(value_text), "%i", MOX::colony[MOX::cmbt_colony_id].n_pops);
                }

                strlcat(colony_status_text, value_text, sizeof(colony_status_text));
                strlcat(colony_status_text, "  ", sizeof(colony_status_text));
                if (MOX::cmbt_colony_id != -1) {
                    if (MOX::colony[MOX::cmbt_colony_id].military[1] > 0) {
                        KEN::Ken_Get_Text_Message(1, combat_text_segment, 0x50);
                        strlcat(colony_status_text, combat_text_segment, sizeof(colony_status_text));
                        snprintf(value_text, sizeof(value_text), "%i", MOX::colony[MOX::cmbt_colony_id].military[1]);
                        strlcat(colony_status_text, value_text, sizeof(colony_status_text));
                        strlcat(colony_status_text, "  ", sizeof(colony_status_text));
                    }
                    if (MOX::colony[MOX::cmbt_colony_id].military[0] > 0) {
                        KEN::Ken_Get_Text_Message(2, combat_text_segment, 0x50);
                        strlcat(colony_status_text, combat_text_segment, sizeof(colony_status_text));
                        snprintf(value_text, sizeof(value_text), "%i", MOX::colony[MOX::cmbt_colony_id].military[0]);
                        strlcat(colony_status_text, value_text, sizeof(colony_status_text));
                        strlcat(colony_status_text, "  ", sizeof(colony_status_text));
                    }
                }

                fonts::Print(x + 2, y + 0x48, colony_status_text);
                colony_status_text[0] = '\0';

                if (MOX::cmbt_colony_id != -1) {
                    int16_t current_y = y + 4;
                    if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_MISSILE_BASE] != 0) {
                        fonts::Print(x + 2, current_y, TECHDATA::buildings[BUILDING_MISSILE_BASE].name);
                        current_y += 7;
                    }
                    if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_GROUND_BATTERIES] != 0) {
                        fonts::Print(x + 2, current_y, TECHDATA::buildings[BUILDING_GROUND_BATTERIES].name);
                        current_y += 7;
                    }
                    if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_FIGHTER_GARRISON] != 0) {
                        fonts::Print(x + 2, current_y, TECHDATA::buildings[BUILDING_FIGHTER_GARRISON].name);
                        current_y += 7;
                    }
                    if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_STELLAR_CONVERTER] != 0) {
                        fonts::Print(x + 2, current_y, TECHDATA::buildings[BUILDING_STELLAR_CONVERTER].name);
                    }
                }
            }
        } else {
            Draw_Ship_To_Bitmap(ship_idx, 0, 0, 0);
            bitmap::Create_Blank_Bitmap(0x62, 0x62, MOX::combat_bitmap);

            int16_t idx = ship_idx;
            s_combat_data* ship = &MOX::combat_data[idx];
            int8_t facing = MOX::combat_data[idx].facing_dir;
            int8_t shield_type = MOX::combat_data[idx].shield_type;
            uint16_t shield_current_hits = MOX::combat_data[idx].shield_current_hits;
            int16_t current_speed = MOX::combat_data[idx].current_speed;
            int32_t shield_quadrant = (int32_t)facing / 4;
            // Sprite placement groups exact cardinal facings with the preceding quadrant.
            int32_t facing_quadrant = ((int32_t)facing - 1) / 4;
            if (facing_quadrant < 0) {
                facing_quadrant = 0;
            }
            int32_t facing_mod = (int32_t)facing % 4;

            int16_t x_offset = (facing_quadrant == 1 || facing_quadrant == 2) ? 14 : 16;
            bitmap::Add_Picture_To_Bitmap(x_offset, 11, MOX::draw_ship_bitmap, MOX::combat_bitmap);

            if (shield_type > 0 && shield_current_hits > 0) {
                bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);

                int shield_val = (ship->shield_arc_max + 19) / 10;
                if (shield_val > 10) {
                    shield_val = 10;
                }

                if (shield_val - 1 <= 9) {
                    switch (shield_val - 1) {
                        case 0:
                        case 1:
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x51, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x52, 0);
                            for (int i = 0; i < 4; i++) {
                                if (ship->shield_arc_current[i] < 0x0B) {
                                    bitmap::Copy_Bitmap_To_Bitmap(MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                    int mask_idx = (i - shield_quadrant + 4) % 4 + facing_mod * 4;
                                    bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::shield_mask_bitmap[mask_idx], 1);
                                    replace::Replace_Color(MOX::draw_ship_bitmap, 0x52, 1);
                                    if (ship->shield_arc_current[i] < 1) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x51, 1);
                                    }
                                    bitmap::Add_Picture_To_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                }
                            }
                            break;
                        case 2:
                        case 3:
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x50, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x52, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x54, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x56, 0);
                            for (int i = 0; i < 4; i++) {
                                if (ship->shield_arc_current[i] < 0x1F) {
                                    bitmap::Copy_Bitmap_To_Bitmap(MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                    int mask_idx = (i - shield_quadrant + 4) % 4 + facing_mod * 4;
                                    bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::shield_mask_bitmap[mask_idx], 1);
                                    replace::Replace_Color(MOX::draw_ship_bitmap, 0x56, 1);
                                    if (ship->shield_arc_current[i] < 0x16) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x54, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x0C) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x52, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 1) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x50, 1);
                                    }
                                    bitmap::Add_Picture_To_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                }
                            }
                            break;
                        case 4:
                        case 5:
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x50, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x51, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x53, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x54, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x55, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x56, 0);
                            for (int i = 0; i < 4; i++) {
                                if (ship->shield_arc_current[i] < 0x33) {
                                    bitmap::Copy_Bitmap_To_Bitmap(MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                    int mask_idx = (i - shield_quadrant + 4) % 4 + facing_mod * 4;
                                    bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::shield_mask_bitmap[mask_idx], 1);
                                    replace::Replace_Color(MOX::draw_ship_bitmap, 0x56, 1);
                                    if (ship->shield_arc_current[i] < 0x2A) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x55, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x20) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x54, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x16) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x53, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x0C) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x51, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 1) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x50, 1);
                                    }
                                    bitmap::Add_Picture_To_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                }
                            }
                            break;
                        case 6:
                        case 7:
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x50, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x51, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x52, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x53, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x54, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x55, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x56, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x57, 0);
                            for (int i = 0; i < 4; i++) {
                                if (ship->shield_arc_current[i] < 0x47) {
                                    bitmap::Copy_Bitmap_To_Bitmap(MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                    int mask_idx = (i - shield_quadrant + 4) % 4 + facing_mod * 4;
                                    bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::shield_mask_bitmap[mask_idx], 1);
                                    replace::Replace_Color(MOX::draw_ship_bitmap, 0x57, 1);
                                    if (ship->shield_arc_current[i] < 0x3E) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x56, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x34) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x55, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x2A) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x54, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x20) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x53, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x16) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x52, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x0C) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x51, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 1) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x50, 1);
                                    }
                                    bitmap::Add_Picture_To_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                }
                            }
                            break;
                        case 8:
                        case 9:
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x50, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x51, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x52, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x53, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x53, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x54, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x54, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x55, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x56, 0);
                            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 0x57, 0);
                            for (int i = 0; i < 4; i++) {
                                if (ship->shield_arc_current[i] < 0x5B) {
                                    bitmap::Copy_Bitmap_To_Bitmap(MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                    int mask_idx = (i - shield_quadrant + 4) % 4 + facing_mod * 4;
                                    bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::shield_mask_bitmap[mask_idx], 1);
                                    replace::Replace_Color(MOX::draw_ship_bitmap, 0x57, 1);
                                    if (ship->shield_arc_current[i] < 0x52) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x56, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x48) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x55, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x3E) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x54, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x2A) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x53, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x16) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x52, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 0x0C) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x51, 1);
                                    }
                                    if (ship->shield_arc_current[i] < 1) {
                                        replace::Replace_Color(MOX::draw_ship_bitmap, 0x50, 1);
                                    }
                                    bitmap::Add_Picture_To_Bitmap(0, 0, MOX::draw_ship_bitmap, MOX::combat_bitmap);
                                }
                            }
                            break;
                    }
                }

                bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::combat_bitmap, (s_animation_header*)MOX::shield_div_bitmap[facing_mod], 0);
                Draw_Ship_To_Bitmap(ship_idx, 1, 0, 0);

                int32_t dir2 = ((int32_t)facing - 1) / 4;
                if (dir2 < 0) {
                    dir2 = 0;
                }

                int16_t dx2 = (dir2 == 1 || dir2 == 2) ? 14 : 16;
                bitmap::Add_Picture_To_Bitmap(dx2, 11, MOX::draw_ship_bitmap, MOX::combat_bitmap);
            }

            replace::Replace_Color(MOX::combat_bitmap, 1, 0);
            replace::Replace_Color(MOX::combat_bitmap, 2, 0);
            animate::Draw(x, y, MOX::combat_bitmap);

            if (ship->current_speed <= 0) {
                struct s_colors shadow_color;
                shadow_color.colors[0] = 0x48;
                for (int i = 1; i < 8; ++i) {
                    shadow_color.colors[i] = 0x4A;
                }
                fonts::Set_Font_Style_Shadow_Down(1, &shadow_color);

                char disabled_ship_text[80];
                KEN::Ken_Get_Text_Message(0x47, disabled_ship_text, 0x50);
                fonts::Print_Centered(x + 45, y + 70, disabled_ship_text);
            }
        }
    }

    void __cdecl Get_Ship_Center_Draw_XY(int16_t combat_idx, int16_t* x_out, int16_t* y_out) {
        int16_t offset_x;
        int16_t offset_y;

        // Retrieve ship-specific centering offsets based on ship image dimensions
        COMBINIT::Ship_Center_Offsets(combat_idx, &offset_x, &offset_y);

        // Calculate screen X coordinate: (world_x - viewport_x) * grid_size + local_offset
        // 0x14 is the grid cell size (20 pixels)
        *x_out = (int16_t)(MOX::combat_data[combat_idx].x - MOX::cur_combat_x) * 20 + offset_x;

        // Calculate screen Y coordinate: (world_y - viewport_y) * grid_size + local_offset
        *y_out = (int16_t)(MOX::combat_data[combat_idx].y - MOX::cur_combat_y) * 20 + offset_y;
    }

    void __cdecl Draw_Retreating_Ship(int16_t ship_idx) {
        int8_t owner = MOX::combat_data[ship_idx].owner;
        int16_t facing = (MOX::combat_attacker_player == owner) ? 8 : 0;
        int16_t x_offset = (MOX::combat_attacker_player == owner) ? 28 : 31;

        int16_t move_step_fast = x_offset / 3;
        int16_t move_step_slow = x_offset / 4 + 1;
        int16_t retreat_offset_x = 0;
        int16_t frame_step = 1;

        COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[ship_idx].x, MOX::combat_data[ship_idx].y);

        int16_t ship_center_x;
        int16_t ship_center_y;
        Get_Ship_Center_Draw_XY(ship_idx, &ship_center_x, &ship_center_y);

        int16_t base_x = MOX::combat_data[ship_idx].x * 20;
        int16_t base_y = MOX::combat_data[ship_idx].y * 20;

        if (MOX::combat_data[ship_idx].facing_dir != facing) {
            CMBTMOV1::Rotate_Ship(ship_idx, facing, MOX::cur_combat_x * -20 + base_x, MOX::cur_combat_y * -20 + base_y);
        }

        if (CMBTFIRE::Is_Combat_Ship_Dead(ship_idx) != 0) {
            return;
        }

        s_animation_header* effect_seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 42, MOX::global_cache_seg);
        int16_t effect_width = animate::Get_Width(effect_seg);
        int16_t effect_height = animate::Get_Height(effect_seg);
        int16_t frame_count = animate::Get_Animation_Frame_Count(effect_seg);

        int16_t ship_actual_width;
        int16_t ship_actual_height;
        Get_Ship_Actual_Size(ship_idx, &ship_actual_width, &ship_actual_height);

        MOX::dont_draw_ship = ship_idx;

        Get_Ship_Center_Draw_XY(ship_idx, &ship_center_x, &ship_center_y);

        video::Set_Page_Off();

        int16_t draw_x;
        if (MOX::combat_attacker_player == owner) {
            draw_x = ship_center_x - (ship_actual_width / 2) - x_offset - effect_width;
        } else {
            draw_x = ship_center_x + (ship_actual_width / 2) + x_offset;
        }

        int16_t draw_y = ship_center_y - (effect_height / 2);

        if (draw_x < 4) {
            draw_x = 4;
        }
        if (draw_x + effect_width > 0x27F) {
            draw_x = 0x276 - effect_width;
        }

        if (draw_y < 4) {
            draw_y = 4;
        }
        if (draw_y + effect_height > 0x15E) {
            draw_y = 0x15A - effect_height;
        }

        int16_t window_status = 0;
        MISC::Save_Window_Status(&window_status);

        int16_t x1, y1, x2, y2;
        if (MOX::combat_attacker_player == owner) {
            x1 = draw_x + 0x21;
            y1 = draw_y + 0x35;
            x2 = ship_center_x + ship_actual_width;
            y2 = ship_center_y + ship_actual_height;
        } else {
            x1 = ship_center_x - ship_actual_width;
            y1 = draw_y + 0x35;
            x2 = draw_x + 0x1E;
            y2 = draw_y + effect_height;
        }

        int32_t sound_id = ERICNET::Mox_Play_Sound(52 /* 0x34 */);
        sound::Pitch_Bend_Sound(sound_id, (2 - MOX::combat_data[ship_idx].size_class) * 2500);

        if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
            frame_step = 3;
        }

        if (frame_count > 0) {
            for (int16_t frame = 0; frame < frame_count; frame += frame_step) {
                int16_t step = (frame < 5) ? move_step_fast : move_step_slow;
                step *= frame_step;

                timer::Mark_Time();
                Draw_Main_Combat_Screen();
                video::Clip_On();
                graphics::Set_Window(0, 0, 0x27F, 0x15E);

                effect_seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 42, MOX::global_cache_seg);
                animate::Set_Animation_Frame(effect_seg, frame);
                animate::Draw(draw_x, draw_y, effect_seg);

                graphics::Set_Window(x1, y1, x2, y2);
                Draw_Ship(ship_idx, MOX::cur_combat_x * -20 + retreat_offset_x + base_x, MOX::cur_combat_y * -20 + base_y);

                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);

                if (MOX::combat_attacker_player == owner) {
                    retreat_offset_x -= step;
                } else {
                    retreat_offset_x += step;
                }
            }
        }

        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_Ship(int16_t ship_idx, int16_t draw_x, int16_t draw_y) {
        int16_t frame_lookup[4] = {1, 2, 3, 2};
        const int16_t ship_frame_stride = 4;
        int16_t frame_idx;
        uint8_t previous_owner = MOX::combat_data[ship_idx].previous_owner;

        if (previous_owner <= 9) {
            frame_idx = frame_lookup[((MOX::ship_frame / 2) + ship_idx) % 4];
        } else {
            frame_idx = (MOX::ship_frame / 2) % 4;
        }

        int16_t x_offset, y_offset;
        Get_Draw_Ship_Offsets(ship_idx, &x_offset, &y_offset);

        if (ship_idx == 0) {
            animate::Draw((MOX::combat_data[0].x - MOX::cur_combat_x) * 20 + x_offset,
                (MOX::combat_data[0].y - MOX::cur_combat_y) * 20 + y_offset,
                MOX::combat_planet_seg);
            COMBINIT::Ship_Center_Offsets(0, &draw_x, &draw_y);
            return;
        }

        int8_t facing_dir = MOX::combat_data[ship_idx].facing_dir;
        s_animation_header* anim = MOX::combat_data[ship_idx].ship_image_seg;
        int8_t owner = MOX::combat_data[ship_idx].owner;

        if (anim == nullptr || previous_owner != owner) {
            uint8_t attacker_palette_remap[32];
            uint8_t defender_palette_remap[32];
            for (int i = 0; i < 32; ++i) {
                attacker_palette_remap[i] = i + 32;
                defender_palette_remap[i] = i + 96;
            }

            char lbx_name[12];
            int16_t picture_num;
            if (previous_owner < MAX_PLAYERS && MOX::combat_data[ship_idx].picture_num != 44 /* 0x2c */) {
                strlcpy(lbx_name, "cmbtshp.lbx", sizeof(lbx_name));
                int16_t color = MOX::player[previous_owner].color;
                picture_num = color * 45 + MOX::combat_data[ship_idx].picture_num;
            } else {
                strlcpy(lbx_name, "monster.lbx", sizeof(lbx_name));
                if (MOX::combat_data[ship_idx].picture_num == 44 /* 0x2c */) {
                    picture_num = 25; /* 0x19 */
                } else {
                    picture_num = MOX::combat_data[ship_idx].picture_num;
                }
            }

            anim = (s_animation_header*)buffer::Buffer_Reload(lbx_name, picture_num, MOX::global_cache_seg);

            if (owner == MOX::combat_attacker_player) {
                replace::Replace_Color_Range(anim, attacker_palette_remap, 96 /* 0x60 */, 32 /* 0x20 */);
            } else {
                replace::Replace_Color_Range(anim, defender_palette_remap, 32 /* 0x20 */, 32 /* 0x20 */);
            }
        }

        int quadrant = (facing_dir - 1) / 4;
        if (quadrant <= 0) {
            quadrant = 0;
        }

        switch (quadrant) {
            case 0: {
                int16_t done = CMBTFIR2::Combat_Ship_Done(ship_idx);
                int16_t draw_frame;
                if (done == 0 && MOX::combat_data[ship_idx].is_captured == 0) {
                    draw_frame = ship_frame_stride * facing_dir + frame_idx;
                } else {
                    draw_frame = ship_frame_stride * facing_dir;
                }
                animate::Set_Animation_Frame(anim, draw_frame);
                bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, anim);
                break;
            }
            case 1: {
                int16_t done = CMBTFIR2::Combat_Ship_Done(ship_idx);
                int16_t draw_frame;
                if (done == 0 && MOX::combat_data[ship_idx].is_captured == 0) {
                    draw_frame = ship_frame_stride * (8 - facing_dir) + frame_idx;
                } else {
                    draw_frame = ship_frame_stride * (8 - facing_dir);
                }
                animate::Set_Animation_Frame(anim, draw_frame);
                bitmap::Draw_Picture_To_Bitmap(MOX::ship_bitmap, anim);
                bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 0);
                break;
            }
            case 2: {
                int16_t done = CMBTFIR2::Combat_Ship_Done(ship_idx);
                int16_t draw_frame;
                if (done == 0 && MOX::combat_data[ship_idx].is_captured == 0) {
                    draw_frame = ship_frame_stride * (facing_dir - 8) + frame_idx;
                } else {
                    draw_frame = ship_frame_stride * (facing_dir - 8);
                }
                animate::Set_Animation_Frame(anim, draw_frame);
                bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, anim);
                bitmap::Flip_Bitmap(MOX::ship_bitmap, MOX::draw_ship_bitmap, 0);
                bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 3);
                break;
            }
            case 3: {
                int16_t done = CMBTFIR2::Combat_Ship_Done(ship_idx);
                int16_t draw_frame;
                if (done == 0 && MOX::combat_data[ship_idx].is_captured == 0) {
                    draw_frame = ship_frame_stride * (16 - facing_dir) + frame_idx;
                } else {
                    draw_frame = ship_frame_stride * (16 - facing_dir);
                }
                animate::Set_Animation_Frame(anim, draw_frame);
                bitmap::Draw_Picture_To_Bitmap(MOX::ship_bitmap, anim);
                bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 3);
                break;
            }
        }

        uint8_t status_flag = MOX::combat_data[ship_idx].special_status_flag;
        if (status_flag == 4) {
            if (MOX::game_type == 1 ||
                (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count &&
                    MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM)) {
                bitmap::Outline_Bitmap(MOX::ship_bitmap, MOX::draw_ship_bitmap, 5, 0);
                bitmap::Mask_Out_Bitmap_With_Bitmap(0, 0, MOX::ship_bitmap, MOX::draw_ship_bitmap, 0);
                animate::Draw(x_offset + draw_x, y_offset + draw_y, MOX::ship_bitmap);
            }
            return;
        }

        if (status_flag == 1 || status_flag == 5) {
            int val = (MOX::ship_frame + ship_idx * 2) % 16;
            Plasma_Darken(MOX::draw_ship_bitmap, val, 3);
        }

        if (MOX::combat_data[ship_idx].reflected_damage_pool > 0 &&
            !CMBTMOV1::Ship_Is_In_Stasis(ship_idx) &&
            !CMBTMOV1::Ship_In_Black_Hole(ship_idx) &&
            MOX::combat_data[ship_idx].plasma_web_damage == 0) {
            CMBTFIRE::Draw_Energy_Absorber(ship_idx, draw_x, draw_y);
        }

        animate::Draw(x_offset + draw_x, y_offset + draw_y, MOX::draw_ship_bitmap);

        if (CMBTMOV1::Ship_Is_In_Stasis(ship_idx)) {
            CMBTSPEC::Draw_Stasis_Ball(ship_idx, 100, 0, 0);
            return;
        }

        if (MOX::combat_data[ship_idx].plasma_web_damage > 0) {
            CMBTSPEC::Draw_Plasma_Web(ship_idx, draw_x, draw_y);
            return;
        }

        if (CMBTMOV1::Ship_In_Black_Hole(ship_idx)) {
            CMBTFIRE::Draw_BHG(ship_idx, 0, 0);
        }
    }

    void __cdecl Get_Ship_Actual_Size(int16_t ship_id, int16_t* width, int16_t* height) {
        int16_t x_offset;
        int16_t y_offset;

        CMBTDRW1::Draw_Ship_To_Bitmap(ship_id, 0, 0, 0);

        bitmap::Get_Bitmap_Actual_Size(
            MOX::draw_ship_bitmap,
            &x_offset,
            &y_offset,
            width,
            height);
    }

    void __cdecl Create_Ship_Darken_Map() {
        for (int i = 0; i < 0x20; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - j * 2);
                MOX::ship_darken_map[i][j] = val;
                if (val > 0x20) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }

        for (int i = 0x20; i < 0x30; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - j);
                MOX::ship_darken_map[i][j] = val;
                if (val < 0x20) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }

        for (int i = 0x30; i < 0x40; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - j);
                MOX::ship_darken_map[i][j] = val;
                if (val < 0x30) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }

        for (int i = 0x40; i < 0x48; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - (j / 2));
                MOX::ship_darken_map[i][j] = val;
                if (val < 0x40) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }

        for (int i = 0x48; i < 0x50; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - (j / 2));
                MOX::ship_darken_map[i][j] = val;
                if (val < 0x48) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }

        for (int i = 0x60; i < 0x70; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - j);
                MOX::ship_darken_map[i][j] = val;
                if (val < 0x60) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }

        for (int i = 0x70; i < 0x80; ++i) {
            for (int j = 0; j < 0x10; ++j) {
                uint8_t val = (uint8_t)(i - j);
                MOX::ship_darken_map[i][j] = val;
                if (val < 0x70) {
                    MOX::ship_darken_map[i][j] = 0;
                }
            }
        }
    }

    void __cdecl Draw_Board_Popup() {
        fonts::Set_Font_Style(2, &MOX::combat_green);

        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("combat.LBX", 36, MOX::global_cache_seg);
        animate::Draw(150, 100, anim);

        BILL::_Print_Paragraph(165, 153, 261, g_msg, 2);

        fonts::Print_Integer_Centered(182, 231, g_min);
        fonts::Print_Integer_Centered(296, 231, g_var);
        fonts::Print_Integer_Centered(408, 231, g_max);

        int16_t offset = (int16_t)((((int32_t)g_var - (int32_t)g_min) * 216) / ((int32_t)g_max - (int32_t)g_min) + 36);

        graphics::Fill(186, 253, offset + 149, 263, 193);

        offset += 150;

        line::Line(offset, 253, offset, 263, 0);
    }

    void __cdecl Draw_Weapon_Status_Display(int16_t target_ship_idx, int16_t target_missile_idx) {
        s_colors color_b0 = {{0x53, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56}};
        s_colors color_b8 = {{0x50, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53}};
        s_colors color_a8 = {{0xdf, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc}};
        s_colors color_6c = {{0x48, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a}};

        char weapon_label[52];
        char ammo_text[20];
        char arc_text[80];

        graphics::Interlaced_Fill(114, 378, 263, 466, 0, 199);

        if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) return;

        short y_pos = 380; // 0x17c

        for (int weapon_slot_idx = 0; weapon_slot_idx < 8; ++weapon_slot_idx) {
            int16_t w_id = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].weapon_id;

            if (w_id != 0) {
                uint8_t arc = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].arc;
                short is_ai = COMBAT1::Ship_Controlled_By_Computer(MOX::cur_ship);
                short display_color_mode = 0;

                if (is_ai == 0) {
                    if (COMBAT1::Weapon_Is_On(MOX::cur_ship, weapon_slot_idx) == 0) {
                        display_color_mode = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].active == 0 ? 2 : 3;
                    } else if (target_missile_idx < 0) {
                        if (target_ship_idx != -1) {
                            short rel_bearing = Relative_Bearing(MOX::cur_ship, target_ship_idx);
                            uint8_t required_arc = Get_Weapon_Arc(rel_bearing);

                            if ((arc & required_arc) != 0 &&
                                CMBTFIRE::Weapon_In_Range(weapon_slot_idx, MOX::cur_ship, target_ship_idx, 0, 0) != 0 &&
                                CMBTFIRE::Target_Is_Legal_For_Weapon(
                                    MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].weapon_id,
                                    MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].specials,
                                    target_ship_idx,
                                    0) != 0) {
                                display_color_mode = 1;
                            } else {
                                display_color_mode = 0;
                            }
                        } else {
                            display_color_mode = 0;
                        }
                    } else {
                        short rel_bearing = Relative_Bearing_XY(MOX::cur_ship, MOX::missile[target_missile_idx].x, MOX::missile[target_missile_idx].y);
                        uint8_t required_arc = Get_Weapon_Arc(rel_bearing);

                        if ((arc & required_arc) == 0 ||
                            CMBTFIRE::Weapon_In_Range(weapon_slot_idx, MOX::cur_ship, target_missile_idx, 1, 0) == 0 ||
                            CMBTFIRE::Target_Is_Legal_For_Weapon(
                                MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].weapon_id,
                                MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].specials,
                                target_missile_idx,
                                1) == 0) {
                            display_color_mode = 0;
                        } else {
                            display_color_mode = 1;
                        }
                    }
                } else {
                    display_color_mode = 0;
                }

                uint8_t weapon_type_class = TECHDATA::weapons[w_id].type;
                int16_t ammo = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].ammo;
                uint8_t shots_left = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].shots_left;
                uint8_t status = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].status;

                if (display_color_mode != 3 &&
                    (((weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_BOMB || weapon_type_class == WEAPON_TYPE_FIGHTER) && ammo == 0) ||
                        (weapon_type_class == WEAPON_TYPE_TORPEDO && ammo > 0) ||
                        (shots_left == 0 && status != 1))) {
                    display_color_mode = 2;
                }

                s_colors* font_color;
                if (display_color_mode == 0) {
                    font_color = &color_b8;
                } else if (display_color_mode == 1) {
                    font_color = &color_b0;
                } else if (display_color_mode == 2) {
                    font_color = &color_a8;
                } else {
                    font_color = &color_6c;
                }

                fonts::Set_Font_Style_Shadow_Down(1, font_color);
                fonts::Set_Outline_Color(0, 0);
                fonts::Set_Font_Spacing(1);

                int32_t count_to_print = shots_left;
                if (count_to_print == 0 && status == 1) {
                    count_to_print = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].count;
                }

                fonts::Print_Integer_Right(127, y_pos, count_to_print);

                uint16_t specials = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].specials;

                if ((specials & 2) == 0) {
                    if ((specials & 4) == 0) {
                        weapon_label[0] = '\0';
                    } else {
                        strlcpy(weapon_label, TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].name_plural, sizeof(weapon_label));
                        strlcat(weapon_label, " ", sizeof(weapon_label));
                    }
                } else {
                    strlcpy(weapon_label, TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].name_plural, sizeof(weapon_label));
                    strlcat(weapon_label, " ", sizeof(weapon_label));
                }

                if (MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].shots_left == 1) {
                    strlcat(weapon_label, TECHDATA::weapons[w_id].name, sizeof(weapon_label));
                } else {
                    strlcat(weapon_label, TECHDATA::weapons[w_id].name_plural, sizeof(weapon_label));
                }

                if (display_color_mode == 0) {
                    fonts::Set_Font_Style_Shadow_Down(1, &color_b8);
                } else if (display_color_mode == 1) {
                    fonts::Set_Font_Style_Shadow_Down(1, &color_b0);
                } else if (display_color_mode == 2) {
                    fonts::Set_Font_Style_Shadow_Down(1, &color_a8);
                }

                if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_BOMB || weapon_type_class == WEAPON_TYPE_FIGHTER || w_id == WEAPON_ANTI_MISSILE_ROCKET) {
                    strlcat(weapon_label, " x", sizeof(weapon_label));
                    snprintf(ammo_text, sizeof(ammo_text), "%i", ammo);
                    strlcat(weapon_label, ammo_text, sizeof(weapon_label));
                } else if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                    if (ammo > 0) {
                        strlcat(weapon_label, " ", sizeof(weapon_label));
                        snprintf(ammo_text, sizeof(ammo_text), "%i", ammo);
                        strlcat(weapon_label, ammo_text, sizeof(weapon_label));
                        KEN::Ken_Get_Text_Message(85, arc_text, 80);
                        strlcat(weapon_label, arc_text, sizeof(weapon_label));
                    }
                } else if (weapon_type_class == WEAPON_TYPE_BEAM) {
                    strlcat(weapon_label, " (", sizeof(weapon_label));
                    switch (arc) {
                        case WEAPON_FIRING_ARC_FORWARD:
                            KEN::Ken_Get_Text_Message(3, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_FORWARD_EXTENDED:
                            KEN::Ken_Get_Text_Message(4, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_BACK_EXTENDED:
                            KEN::Ken_Get_Text_Message(6, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_REAR:
                            KEN::Ken_Get_Text_Message(5, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_ALL_SECTORS:
                            strlcat(weapon_label, "360", sizeof(weapon_label));
                            break;
                    }
                    strlcat(weapon_label, ")", sizeof(weapon_label));
                }

                fonts::Set_Outline_Color(0, 0);
                fonts::Set_Font_Spacing(1);
                fonts::Print(131, y_pos, weapon_label);
            }
            y_pos += 11;
        }
    }

    void __cdecl Plasma_Darken(s_animation_header* bitmap, int16_t progress, int16_t mode) {
        int frame = progress % 16;
        animate::Set_Animation_Frame(MOX::darken_plasma_map, frame);

        uint8_t* darken_data = (uint8_t*)MOX::darken_plasma_map + Animation_Frame_Offset_(MOX::darken_plasma_map, MOX::darken_plasma_map->current_frame);

        uint8_t* bitmap_data = (uint8_t*)bitmap + Animation_Frame_Offset_(bitmap, bitmap->current_frame);

        int16_t darkness = 0;
        switch (mode) {
            case 0:
                darkness = 0;
                break;
            case 1:
                darkness = progress / 4;
                if (darkness > 6) darkness = 6;
                break;
            case 2:
                darkness = 6 - (progress / 4);
                if (darkness < 0) darkness = 0;
                break;
            case 3:
                darkness = 6;
                break;
            case 4:
                darkness = progress / 3;
                if (darkness > 20) darkness = 20;
                break;
            case 5:
                darkness = (progress / -3) + 12;
                if (darkness < 0) darkness = 0;
                break;
            default:
                darkness = 0;
                break;
        }

        if (darkness == 0) return;

        int16_t darken_w = animate::Get_Width(MOX::darken_plasma_map);
        int16_t darken_h = animate::Get_Height(MOX::darken_plasma_map);
        int16_t bitmap_w = animate::Get_Width(bitmap);
        int16_t bitmap_h = animate::Get_Height(bitmap);

        if (bitmap_h <= 0) return;

        int32_t dst_offset = 0;
        int32_t src_offset = 0;

        for (int16_t y = 0; y < bitmap_h; ++y) {
            if (y < darken_h) {
                if (bitmap_w > 0) {
                    for (int16_t x = 0; x < bitmap_w; ++x) {
                        if (x < darken_w) {
                            uint8_t darken_pixel = darken_data[src_offset + x];
                            uint32_t val;
                            if (darkness < 5) {
                                val = static_cast<uint32_t>((darken_pixel * darkness) >> 2);
                            } else {
                                val = static_cast<uint32_t>(darken_pixel + darkness - 4);
                            }
                            if (val > 15) {
                                val = 15;
                            }
                            uint8_t bitmap_pixel = bitmap_data[dst_offset + x];
                            bitmap_data[dst_offset + x] = MOX::ship_darken_map[bitmap_pixel][val];
                        }
                    }
                }
            }
            dst_offset += bitmap_w;
            src_offset += darken_w;
        }
    }

    void __cdecl Draw_Bombs(int16_t attacker_idx, int16_t target_idx, int16_t weapon_id, int16_t damage) {
        bool is_bio = (TECHDATA::weapons[weapon_id].natural_mods & WEAPON_NATURAL_FLAG_BIOLOGICAL) != 0;
        bool has_transporter = COMBINIT::Does_Combat_Ship_Have_Special(attacker_idx, SPECIAL_TRANSPORTERS) != 0;
        int16_t speed = (MOX::speedx2_flag != 0 || MOX::net_flag != 0) ? 12 : 6;

        if (Ship_Completely_On_Screen(attacker_idx) == 0) {
            COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[attacker_idx].x, MOX::combat_data[attacker_idx].y);
        }

        int16_t target_x, target_y;
        Get_Ship_Center_Draw_XY(target_idx, &target_x, &target_y);

        int16_t start_x, start_y;
        if (has_transporter) {
            int16_t mid_x = animate::Get_Width(MOX::combat_planet_seg) / 2;
            int16_t mid_y = animate::Get_Height(MOX::combat_planet_seg) / 2;
            Get_Ship_Center_Draw_XY(attacker_idx, &start_x, &start_y);

            int16_t range = special::Range(start_x, start_y, target_x, target_y);
            int32_t limit_sq = (int32_t)mid_x * mid_x + (int32_t)mid_y * mid_y + 3600;

            if ((int32_t)range * range > limit_sq) {
                do {
                    special::Absolute_Interpolate_Line(&start_x, &start_y, start_x, start_y, target_x, target_y, 20);
                    range = special::Range(start_x, start_y, target_x, target_y);
                } while ((int32_t)range * range > limit_sq);
            }
            CMBTSPEC::Draw_Transporter_Bomb_Beam(attacker_idx, start_x, start_y);
        } else {
            Get_Ship_Center_Draw_XY(attacker_idx, &start_x, &start_y);
        }

        int16_t facing = CMBTMOV1::Get_Facing(start_x, start_y, target_x, target_y);
        int32_t projectile_entry = facing + (is_bio ? 160 /* 0xA0 */ : 144 /* 0x90 */);

        video::Set_Page_Back();
        Full_Draw_Combat_Screen();

        int16_t curr_x = start_x;
        int16_t curr_y = start_y;
        while (curr_x != target_x || curr_y != target_y) {
            timer::Mark_Time();
            video::Set_Page_Off();
            Draw_Main_Combat_Screen();
            graphics::Set_Window(0, 0, 639 /* 0x27F */, 350 /* 0x15E */);
            video::Clip_On();

            special::Absolute_Interpolate_Line(&curr_x, &curr_y, start_x, start_y, target_x, target_y, speed);

            s_animation_header* projectile_anim = (s_animation_header*)buffer::Buffer_Reload("cmbtmisl.lbx", projectile_entry, MOX::global_cache_seg);
            animate::Draw(curr_x - 17, curr_y - 17, projectile_anim);

            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }

        if (is_bio) {
            BEAMS::Add_Damage_Indicator_For_Biological(target_x, target_y, 1, damage);
        } else {
            BEAMS::Add_Damage_Indicator_For_Missile(target_x, target_y, 1, damage);
        }

        int32_t sound_handle = ERICNET::Mox_Play_Sound(24 /* 0x18 */);
        s_animation_header* explosion_anim = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 7, MOX::global_cache_seg);
        int16_t frame_count = animate::Get_Animation_Frame_Count(explosion_anim);

        for (int16_t i = 0; i < frame_count; ++i) {
            timer::Mark_Time();
            video::Set_Page_Off();
            Draw_Main_Combat_Screen();
            graphics::Set_Window(0, 0, 639 /* 0x27F */, 350 /* 0x15E */);
            video::Clip_On();

            explosion_anim = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 7, MOX::global_cache_seg);
            animate::Set_Animation_Frame(explosion_anim, i);
            animate::Draw(target_x - 12, target_y - 12, explosion_anim);

            BEAMS::Draw_Damage_Message_Queue();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }

        BEAMS::Draw_Damage_Message_Queue_Until_Done();
        sound::Fadeout_Sound(sound_handle, 10);
    }

    static void Build_Signed_Bonus_String(char* out_text, size_t out_text_size, int32_t value) {
        if (value > 0) {
            snprintf(out_text, out_text_size, "+%d", value);
        } else {
            snprintf(out_text, out_text_size, "%d", value);
        }
    }

    static void Copy_Crew_Quality_Without_Parenthetical(char* out_text, size_t out_text_size, int32_t crew_quality) {
        strlcpy(out_text, CMBTDRW1::Crew_Quality_String(crew_quality), out_text_size);
        char* paren = strchr(out_text, '(');
        if (paren != nullptr) {
            *paren = '\0';
        }
    }

    static int16_t Weakest_Shield_Arc() {
        int16_t weakest = MOX::global_combat_data.shield_arc_current[0];
        for (int32_t arc = 1; arc < 4; ++arc) {
            if (MOX::global_combat_data.shield_arc_current[arc] < weakest) {
                weakest = MOX::global_combat_data.shield_arc_current[arc];
            }
        }
        return weakest;
    }

    static void Build_Shield_Strength_Text(
        char* out_text,
        size_t out_text_size,
        int32_t current,
        int32_t maximum) {
        const char* format = HAROLD::H_Message(HESTR_059_D_STRENGTH);
        const char* placeholder = strstr(format, "%d");
        if (placeholder == nullptr) {
            snprintf(out_text, out_text_size, "%d of %d", current, maximum);
            return;
        }

        snprintf(
            out_text,
            out_text_size,
            "%.*s%d of %d%s",
            (int32_t)(placeholder - format),
            format,
            current,
            maximum,
            placeholder + 2);
    }

    void __cdecl Combat_Draw_View_Ship(void) {
        char message_text[0x50];
        char line_text[0x100];
        char quantity_text[0x100];
        char arc_text[0x50];
        uint8_t grayscale_table[0x20];
        s_colors heading_colors{};
        s_colors text_colors{};
        s_colors damaged_colors{};
        uint8_t panel_color;
        uint32_t panel_fill_color;
        int32_t player_idx;
        int16_t view_x;
        int16_t view_y;
        int16_t row_y;
        int16_t left_panel_x;
        int16_t right_panel_x;
        int16_t left_panel_right_x;
        int16_t right_panel_right_x;
        int16_t left_value_x;
        int16_t right_value_x;
        bool use_remap_view;
        bool is_planet;
        bool uses_standard_layout;

        MOX::scan_ship_seg =
            buffer::Buffer_Reload("combat.lbx", 5, MOX::global_cache_seg);

        view_x = static_cast<int16_t>(0x149 - animate::Get_Width((s_animation_header*)MOX::scan_ship_seg) / 2);
        view_y = static_cast<int16_t>(0x0F9 - animate::Get_Height((s_animation_header*)MOX::scan_ship_seg) / 2);

        const Combat_View_Subject subject = combat_view_subject;
        is_planet = subject == Combat_View_Subject::Tactical_Planet;
        uses_standard_layout = subject != Combat_View_Subject::Monster;
        use_remap_view =
            subject == Combat_View_Subject::Strategic_Ship ||
            subject == Combat_View_Subject::Strategic_Lander ||
            subject == Combat_View_Subject::Strategic_Combat_Ship;
        const bool show_drive =
            (subject == Combat_View_Subject::Strategic_Ship ||
                subject == Combat_View_Subject::Tactical_Ship) &&
            MOX::global_combat_data.ship_idx >= 0 &&
            MOX::global_combat_data.ftl_type > DRIVE_NO_DRIVE;
        const bool show_subsystem_hits =
            subject == Combat_View_Subject::Strategic_Ship ||
            subject == Combat_View_Subject::Tactical_Ship;

        if (!use_remap_view) {
            for (int32_t i = 0; i < 8; ++i) {
                CMBTDRW1::green[i] = static_cast<uint8_t>(0x80 + i * 2);
            }

            damaged_colors.colors[0] = 0x48;
            for (int32_t i = 1; i < 8; ++i) {
                damaged_colors.colors[i] = 0x4A;
            }

            panel_color = 0xC7;

            graphics::Tri_Interlaced_Fill(
                view_x,
                view_y,
                view_x + 0x145,
                view_y + 0x183,
                CMBTDRW1::green[1],
                CMBTDRW1::green[1],
                0);

            graphics::Interlaced_Fill(
                view_x + 0x0A,
                view_y + 0x1A,
                view_x + 0x5F,
                view_y + 0x69,
                0,
                panel_color);

            if (MOX::global_combat_data.owner < MAX_PLAYERS) {
                graphics::Interlaced_Fill(
                    view_x + 0xEC,
                    view_y + 0x1A,
                    view_x + 0x134,
                    view_y + 0x69,
                    0,
                    panel_color);
            }

            heading_colors.colors[0] = CMBTDRW1::green[2];
            heading_colors.colors[1] = CMBTDRW1::green[6];
            heading_colors.colors[2] = CMBTDRW1::green[5];
            heading_colors.colors[3] = CMBTDRW1::green[6];
            heading_colors.colors[4] = CMBTDRW1::green[6];
            heading_colors.colors[5] = CMBTDRW1::green[6];
            heading_colors.colors[6] = CMBTDRW1::green[6];
            heading_colors.colors[7] = CMBTDRW1::green[6];

            text_colors.colors[0] = CMBTDRW1::green[0];
            text_colors.colors[1] = CMBTDRW1::green[4];
            text_colors.colors[2] = CMBTDRW1::green[2];
            text_colors.colors[3] = CMBTDRW1::green[4];
            text_colors.colors[4] = CMBTDRW1::green[4];
            text_colors.colors[5] = CMBTDRW1::green[4];
            text_colors.colors[6] = CMBTDRW1::green[4];
            text_colors.colors[7] = CMBTDRW1::green[4];

            CMBTDRW1::Draw_Ship_Status((int16_t)(view_x + 0x0A), (int16_t)(view_y + 0x18), MOX::ship_data_id);

            player_idx = MOX::global_combat_data.owner;
            if (player_idx < 9) {
                if (MOX::global_combat_data.officer_idx == -1) {
                    if (player_idx < MAX_PLAYERS) {
                        animate::Draw(view_x + 0xF6, view_y + 0x27, (s_animation_header*)MOX::detail_fleet_icon_seg);
                    }
                } else {
                    for (int32_t i = 0; i < 0x20; ++i) {
                        grayscale_table[i] = static_cast<uint8_t>(i);
                    }

                    allocate::Push_Block(MOX::screen_seg);
                    s_animation_header* gray_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x2710);
                    bitmap::Draw_Picture_To_Bitmap(gray_bitmap, (s_animation_header*)MOX::detail_fleet_icon_seg);
                    bitmap::Gray_Scale_Bitmap(gray_bitmap, 0, 0x20, grayscale_table, 0);
                    animate::Draw(view_x + 0xEB, view_y + 0x19, gray_bitmap);
                    allocate::Pop_Block(MOX::screen_seg);
                }
            }
        } else {
            for (int32_t i = 0; i < 8; ++i) {
                CMBTDRW1::green[i] = remap::Get_Remap_Palette_Color(0x80 + i * 2);
            }

            panel_color = remap::Get_Remap_Palette_Color(0xC7);
            damaged_colors.colors[0] = remap::Get_Remap_Palette_Color(0x48);
            for (int32_t i = 1; i < 8; ++i) {
                damaged_colors.colors[i] = remap::Get_Remap_Palette_Color(0x4A);
            }

            graphics::Tri_Interlaced_Fill(
                view_x,
                view_y,
                view_x + 0x145,
                view_y + 0x183,
                CMBTDRW1::green[2],
                CMBTDRW1::green[2],
                0);

            graphics::Interlaced_Fill(
                view_x + 0x0A,
                view_y + 0x1A,
                view_x + 0x5F,
                view_y + 0x69,
                0,
                panel_color);

            if (MOX::global_combat_data.owner < MAX_PLAYERS) {
                graphics::Interlaced_Fill(
                    view_x + 0xEC,
                    view_y + 0x1A,
                    view_x + 0x134,
                    view_y + 0x69,
                    0,
                    panel_color);
            }

            heading_colors.colors[0] = CMBTDRW1::green[2];
            heading_colors.colors[1] = CMBTDRW1::green[6];
            heading_colors.colors[2] = CMBTDRW1::green[5];
            heading_colors.colors[3] = CMBTDRW1::green[6];
            heading_colors.colors[4] = CMBTDRW1::green[6];
            heading_colors.colors[5] = CMBTDRW1::green[6];
            heading_colors.colors[6] = CMBTDRW1::green[6];
            heading_colors.colors[7] = CMBTDRW1::green[6];

            text_colors.colors[0] = CMBTDRW1::green[0];
            text_colors.colors[1] = CMBTDRW1::green[4];
            text_colors.colors[2] = CMBTDRW1::green[2];
            text_colors.colors[3] = CMBTDRW1::green[4];
            text_colors.colors[4] = CMBTDRW1::green[4];
            text_colors.colors[5] = CMBTDRW1::green[4];
            text_colors.colors[6] = CMBTDRW1::green[4];
            text_colors.colors[7] = CMBTDRW1::green[4];

            CMBTDRW1::Remap_Draw_Ship_Status(static_cast<int16_t>(view_x + 0x0A), static_cast<int16_t>(view_y + 0x18));

            if (MOX::global_combat_data.owner < MAX_PLAYERS) {
                if (MOX::global_combat_data.officer_idx == -1) {
                    animate::Remap_Draw(view_x + 0xF6, view_y + 0x27, (s_animation_header*)MOX::detail_fleet_icon_seg);
                } else {
                    animate::Draw(view_x + 0xEC, view_y + 0x16, (s_animation_header*)MOX::detail_fleet_icon_seg);
                }
            }
        }

        fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);

        KEN::Ken_Get_Text_Message(8, message_text, 0x50);
        row_y = static_cast<int16_t>(view_y + 0x1A);
        fonts::Print_Centered(view_x + 0xA7, row_y, message_text);

        panel_fill_color = static_cast<uint8_t>(panel_color);

        graphics::Interlaced_Fill(
            view_x + 0x6A,
            row_y + 0x0C,
            view_x + 0xDB,
            row_y + 0x30,
            0,
            panel_fill_color);

        fonts::Set_Font_Style_Shadow_Down(1, &text_colors);

        row_y = static_cast<int16_t>(row_y + 0x0F);
        KEN::Ken_Get_Text_Message(9, message_text, 0x50);
        fonts::Print(view_x + 0x6F, row_y, message_text);

        Build_Signed_Bonus_String(line_text, sizeof(line_text), MOX::global_combat_data.tac_val_attack);
        fonts::Print_Right(view_x + 0xD6, row_y, line_text);

        row_y = static_cast<int16_t>(row_y + 0x0C);
        KEN::Ken_Get_Text_Message(10, message_text, 0x50);
        fonts::Print(view_x + 0x6F, row_y, message_text);

        Build_Signed_Bonus_String(line_text, sizeof(line_text), MOX::global_combat_data.tac_val_defense);
        fonts::Print_Right(view_x + 0xD6, row_y, line_text);

        row_y = static_cast<int16_t>(row_y + 0x0C);
        KEN::Ken_Get_Text_Message(11, message_text, 0x50);
        fonts::Print(view_x + 0x6F, row_y, message_text);

        snprintf(line_text, sizeof(line_text), "%d%s", static_cast<int32_t>(MOX::global_combat_data.tac_val_evade), "%");
        fonts::Print_Right(view_x + 0xD6, row_y, line_text);

        if (MOX::global_combat_data.owner <= 9) {
            fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);
            Copy_Crew_Quality_Without_Parenthetical(
                line_text,
                sizeof(line_text),
                static_cast<int32_t>(MOX::global_combat_data.crew_quality));
            fonts::Print_Centered(view_x + 0xA6, row_y + 0x12, line_text);
        }

        row_y = static_cast<int16_t>(row_y + 0x20);
        if (!is_planet && MOX::global_combat_data.marine_count != 0xFF) {
            KEN::Ken_Get_Text_Message(12, message_text, 0x50);
            fonts::Print(view_x + 0x6F, row_y, message_text);
            fonts::Print_Integer_Right(
                view_x + 0xD6,
                row_y,
                static_cast<uint8_t>(MOX::global_combat_data.marine_count));
        }

        left_panel_x = static_cast<int16_t>(view_x + 0x05);
        right_panel_x = static_cast<int16_t>(view_x + 0xA5);
        left_panel_right_x = static_cast<int16_t>(left_panel_x + 0x9B);
        right_panel_right_x = static_cast<int16_t>(right_panel_x + 0x9B);
        left_value_x = static_cast<int16_t>(left_panel_x + 0x03);
        right_value_x = static_cast<int16_t>(right_panel_x + 0x03);

        row_y = static_cast<int16_t>(view_y + 0x76);

        if (MOX::global_combat_data.is_captured == 1) {
            fonts::Set_Font_Style_Shadow_Down(4, &damaged_colors);
            fonts::Set_Outline_Color(0, 1);

            KEN::Ken_Get_Text_Message(13, message_text, 0x50);
            fonts::Print(left_panel_x, view_y + 0x03, message_text);
            fonts::Print_Right(view_x + 0x143, view_y + 0x03, message_text);
        }

        fonts::Set_Font_Style_Shadow_Down(4, &heading_colors);
        fonts::Set_Outline_Color(0, 1);
        graphics::Interlaced_Fill(0x10C, 0x31, 0x17F, 0x41, 0, panel_fill_color);

        if (!use_remap_view) {
            fonts::Print_Centered(0x145, 0x32, MOX::global_combat_data.name);
        }

        fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);

        KEN::Ken_Get_Text_Message(
            uses_standard_layout ? 14 : 0x57,
            message_text,
            0x50);
        fonts::Print(left_panel_x, row_y, message_text);

        if (show_drive) {
            fonts::Set_Font_Style_Shadow_Down(1, &text_colors);
            snprintf(
                line_text,
                sizeof(line_text),
                "%s%d%s%d%s",
                "(",
                static_cast<int32_t>(MOX::global_combat_data.drive_current_hits),
                "/",
                static_cast<int32_t>(MOX::global_combat_data.drive_max_hits),
                ")");
            fonts::Print_Right(left_panel_right_x, row_y + 0x03, line_text);
        }

        fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);

        if (uses_standard_layout) {
            fonts::Print(right_panel_x, row_y, TECHDATA::armor[MOX::global_combat_data.armor_type].name);
        } else {
            KEN::Ken_Get_Text_Message(0x58, message_text, 0x50);
            fonts::Print(right_panel_x, row_y, message_text);
        }

        row_y = static_cast<int16_t>(row_y + 0x0E);

        graphics::Interlaced_Fill(
            left_panel_x,
            row_y - 0x02,
            left_panel_right_x,
            row_y + (uses_standard_layout ? 0x15 : 0x08),
            0,
            panel_fill_color);

        graphics::Interlaced_Fill(
            right_panel_x,
            row_y - 0x02,
            right_panel_right_x,
            row_y + (uses_standard_layout ? 0x15 : 0x08),
            0,
            panel_fill_color);

        fonts::Set_Font_Style_Shadow_Down(1, &text_colors);

        if (uses_standard_layout) {
            KEN::Ken_Get_Text_Message(15, message_text, 0x50);
            int16_t ftl_speed = 0;
            if (show_drive) {
                ftl_speed =
                    TECHDATA::drives[MOX::global_combat_data.ftl_type].warp_speed;
                if (MOX::global_combat_data.owner >= 0 &&
                    MOX::global_combat_data.owner < MAX_PLAYERS) {
                    ftl_speed = INITSHIP::Get_FTL_Speed(
                        MOX::global_combat_data.owner,
                        MOX::global_combat_data.ftl_type);
                }
            }
            snprintf(
                line_text,
                sizeof(line_text),
                "%s%d%s",
                "^ ",
                static_cast<int32_t>(ftl_speed),
                message_text);
            fonts::Print(left_value_x, row_y, line_text);
        }

        if (!is_planet) {
            KEN::Ken_Get_Text_Message(0x59, message_text, 0x50);
            snprintf(
                line_text,
                sizeof(line_text),
                message_text,
                static_cast<int32_t>(MOX::global_combat_data.structure_max) -
                    static_cast<int32_t>(MOX::global_combat_data.structure_damage),
                static_cast<int32_t>(MOX::global_combat_data.structure_max));
        } else {
            const int16_t best_armor = INITSHIP::Best_Armor(MOX::combat_data->owner);
            snprintf(
                line_text,
                sizeof(line_text),
                "%s%d%s%s",
                "^ ",
                static_cast<int16_t>(CMBTFIRE::Ground_Defense_Structure(
                    TECHDATA::armor[best_armor].ships_bonus)),
                " ",
                [&]() -> const char* {
                    KEN::Ken_Get_Text_Message(16, message_text, 0x50);
                    return message_text;
                }());
        }

        fonts::Print(right_value_x, row_y, line_text);

        if (uses_standard_layout) {
            row_y = static_cast<int16_t>(row_y + 0x0C);
        }

        KEN::Ken_Get_Text_Message(17, message_text, 0x50);
        snprintf(
            line_text,
            sizeof(line_text),
            "%s%d%s",
            "^ ",
            static_cast<int32_t>(MOX::global_combat_data.current_speed),
            message_text);
        fonts::Print(left_value_x, row_y, line_text);

        if (uses_standard_layout) {
            if (is_planet) {
                KEN::Ken_Get_Text_Message(0x14, line_text, 0x50);
                fonts::Print(
                    right_value_x + fonts::Get_String_Width("^ "),
                    row_y,
                    line_text);
            } else {
                int32_t armor_total = 0;
                if (subject != Combat_View_Subject::Strategic_Combat_Ship) {
                    s_combat_data* ship = &MOX::global_combat_data;
                    const int16_t hull_index =
                        COMBINIT::Is_Combat_Satellite(ship)
                        ? COMBINIT::Combat_Satellite_Hull_Class(ship)
                        : ship->size_class;
                    armor_total = TECHDATA::hull_data[hull_index].armor_hp;
                    armor_total +=
                        (static_cast<int32_t>(
                             TECHDATA::armor[ship->armor_type].ships_bonus) *
                            armor_total) /
                        100;

                    if (struct_::Test_Bit_Field(
                            ship->special_device_flags,
                            SPECIAL_HEAVY_ARMOR) != 0) {
                        armor_total *= 3;
                    }
                }

                KEN::Ken_Get_Text_Message(0x5B, message_text, 0x50);
                snprintf(
                    line_text,
                    sizeof(line_text),
                    message_text,
                    static_cast<int32_t>(MOX::global_combat_data.armor_remaining),
                    armor_total);
                fonts::Print(right_value_x, row_y, line_text);
            }
        } else {
            row_y = static_cast<int16_t>(row_y + 0x0C);
        }

        row_y = static_cast<int16_t>(row_y + 0x10);

        if (uses_standard_layout) {
            fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);
            KEN::Ken_Get_Text_Message(0x15, message_text, 0x50);
            fonts::Print(left_panel_x, row_y, message_text);

            fonts::Set_Font_Style_Shadow_Down(1, &text_colors);
            if (show_subsystem_hits) {
                snprintf(
                    line_text,
                    sizeof(line_text),
                    "%s%d%s%d%s",
                    "(",
                    static_cast<int32_t>(MOX::global_combat_data.computer_current_hits),
                    "/",
                    static_cast<int32_t>(MOX::global_combat_data.computer_max_hits),
                    ")");
                fonts::Print_Right(left_panel_right_x, row_y + 0x03, line_text);
            }

            fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);
            if (MOX::global_combat_data.shield_type >= 10) {
                fonts::Print(
                    right_panel_x,
                    row_y,
                    TECHDATA::buildings[MOX::global_combat_data.shield_type].name);
            } else {
                fonts::Print(
                    right_panel_x,
                    row_y,
                    TECHDATA::shields[MOX::global_combat_data.shield_type].name);

                if (show_subsystem_hits &&
                    MOX::global_combat_data.shield_type > 0) {
                    fonts::Set_Font_Style_Shadow_Down(1, &text_colors);
                    snprintf(
                        line_text,
                        sizeof(line_text),
                        "%s%d%s%d%s",
                        "(",
                        static_cast<int32_t>(MOX::global_combat_data.shield_current_hits),
                        "/",
                        static_cast<int32_t>(MOX::global_combat_data.shield_max_hits),
                        ")");
                    fonts::Print_Right(right_panel_right_x, row_y + 0x03, line_text);
                }
            }

            row_y = static_cast<int16_t>(row_y + 0x0C);

            graphics::Interlaced_Fill(
                left_panel_x,
                row_y,
                left_panel_right_x,
                row_y + 0x0D,
                0,
                panel_fill_color);

            graphics::Interlaced_Fill(
                right_panel_x,
                row_y,
                right_panel_right_x,
                row_y + 0x0D,
                0,
                panel_fill_color);

            row_y = static_cast<int16_t>(row_y + 0x03);

            fonts::Set_Font_Style_Shadow_Down(1, &text_colors);
            if (MOX::global_combat_data.computer_current_hits > 0) {
                KEN::Ken_Get_Text_Message(0x16, message_text, 0x50);
                snprintf(
                    line_text,
                    sizeof(line_text),
                    message_text,
                    static_cast<int32_t>(
                        TECHDATA::computers[MOX::global_combat_data.computer_type].bonus));
                fonts::Print(left_value_x, row_y, line_text);
            } else {
                KEN::Ken_Get_Text_Message(0x17, message_text, 0x50);
                fonts::Print(left_value_x, row_y, message_text);
            }

            if (show_subsystem_hits) {
                int32_t maximum = MOX::global_combat_data.shield_arc_max;
                if (subject == Combat_View_Subject::Strategic_Ship) {
                    maximum = TECHDATA::Shield_Strength(
                        MOX::global_combat_data.size_class,
                        MOX::global_combat_data.shield_type);
                    if (struct_::Test_Bit_Field(
                            MOX::global_combat_data.special_device_flags,
                            SPECIAL_MULTI_PHASED_SHIELDS) != 0 &&
                        struct_::Test_Bit_Field(
                            MOX::global_combat_data.special_device_damage_flags,
                            SPECIAL_MULTI_PHASED_SHIELDS) == 0) {
                        maximum = TECHDATA::Apply_Multi_Phased_Shields_Bonus(maximum);
                    }
                }

                Build_Shield_Strength_Text(
                    line_text,
                    sizeof(line_text),
                    Weakest_Shield_Arc(),
                    maximum);
            } else {
                KEN::Ken_Get_Text_Message(0x18, message_text, 0x50);
                int32_t shield_strength = MOX::global_combat_data.shield_arc_max;
                if (is_planet) {
                    if (MOX::global_combat_data.owner == NONPLAYER_SHIP_TYPE_ANTARAN) {
                        shield_strength = 0;
                    } else if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() != 0 &&
                        COMBINIT::Does_Combat_Ship_Have_Special(
                            0, SPECIAL_DAMPER_FIELD) == 0) {
                        const int16_t shield_type =
                            INITSHIP::Best_Ship_Shield(MOX::global_combat_data.owner);
                        shield_strength += TECHDATA::shields[shield_type].shield_class;
                    }
                }
                snprintf(
                    line_text,
                    sizeof(line_text),
                    message_text,
                    shield_strength);
            }
            fonts::Print(right_value_x, row_y, line_text);

            row_y = static_cast<int16_t>(row_y + 0x12);
        }

        fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);
        KEN::Ken_Get_Text_Message(0x19, message_text, 0x50);
        fonts::Print(left_panel_x, row_y, message_text);

        KEN::Ken_Get_Text_Message(0x1A, message_text, 0x50);
        fonts::Print(left_panel_x + 0x96, row_y, message_text);

        KEN::Ken_Get_Text_Message(0x1B, message_text, 0x50);
        fonts::Print(left_panel_x + 0xC8, row_y, message_text);

        graphics::Interlaced_Fill(
            left_panel_x,
            row_y + 0x0D,
            right_panel_right_x,
            row_y + 0x66,
            0,
            panel_fill_color);

        row_y = static_cast<int16_t>(row_y + 0x10);

        for (int32_t weapon_slot = 0; weapon_slot < 8; ++weapon_slot) {
            const s_combat_weapon_slot& slot = MOX::global_combat_data.weapons[weapon_slot];

            if (slot.weapon_id != 0) {
                const int16_t weapon_id = slot.weapon_id;
                const int16_t weapon_arc = slot.arc;

                fonts::Set_Font_Style_Shadow_Down(1, &text_colors);
                fonts::Set_Font_Spacing(1);

                const char* weapon_name =
                    (slot.count == 1)
                    ? TECHDATA::weapons[weapon_id].name
                    : TECHDATA::weapons[weapon_id].name_plural;

                snprintf(quantity_text, sizeof(quantity_text), "%d%s%s", static_cast<int32_t>(slot.count), " ", weapon_name);

                strlcpy(arc_text, " (", sizeof(arc_text));
                switch (weapon_arc) {
                    case WEAPON_FIRING_ARC_FORWARD:
                        KEN::Ken_Get_Text_Message(3, message_text, 0x50);
                        strlcat(arc_text, message_text, sizeof(arc_text));
                        break;
                    case WEAPON_FIRING_ARC_FORWARD_EXTENDED:
                        KEN::Ken_Get_Text_Message(4, message_text, 0x50);
                        strlcat(arc_text, message_text, sizeof(arc_text));
                        break;
                    case WEAPON_FIRING_ARC_BACK_EXTENDED:
                        KEN::Ken_Get_Text_Message(6, message_text, 0x50);
                        strlcat(arc_text, message_text, sizeof(arc_text));
                        break;
                    case WEAPON_FIRING_ARC_REAR:
                        KEN::Ken_Get_Text_Message(5, message_text, 0x50);
                        strlcat(arc_text, message_text, sizeof(arc_text));
                        break;
                    case WEAPON_FIRING_ARC_ALL_SECTORS:
                        strlcat(arc_text, "360", sizeof(arc_text));
                        break;
                    default:
                        break;
                }
                strlcat(arc_text, ")", sizeof(arc_text));

                if (weapon_arc != WEAPON_FIRING_ARC_FORWARD) {
                    strlcat(quantity_text, arc_text, sizeof(quantity_text));
                }

                fonts::Print(left_value_x, row_y, quantity_text);

                switch (TECHDATA::weapons[weapon_id].type) {
                    case 0:
                        fonts::Print(left_panel_x + 0x99, row_y, "-");
                        break;

                    case 1:
                    case 3:
                    case 4:
                        fonts::Print_Integer_Right(left_panel_x + 0xA1, row_y, slot.ammo);
                        break;

                    case 2:
                        if (TECHDATA::weapons[weapon_id].ammo > 0) {
                            const int16_t ammo_box_x = static_cast<int16_t>(left_panel_x + 0x99);

                            if (slot.ammo == 0) {
                                KEN::Ken_Get_Text_Message(0x1C, message_text, 0x50);
                                fonts::Print(ammo_box_x, row_y, message_text);
                            } else {
                                int16_t box_x = ammo_box_x;
                                for (int16_t ammo_index = 0;
                                     ammo_index < TECHDATA::weapons[weapon_id].ammo;
                                     ++ammo_index, box_x = static_cast<int16_t>(box_x + 0x09)) {
                                    if (ammo_index < slot.ammo) {
                                        graphics::Box(box_x, row_y, 7, 7, 1, 0x49);
                                        graphics::Box(box_x + 1, row_y + 1, 5, 5, 1, 0x4B);
                                        graphics::Box(box_x + 2, row_y + 2, 3, 3, 1, 0x4B);
                                        graphics::Box(box_x + 3, row_y + 3, 1, 1, 1, 0x4B);
                                    } else {
                                        graphics::Box(box_x, row_y, 7, 7, 1, 0x51);
                                        graphics::Box(box_x + 1, row_y + 1, 5, 5, 1, 0x54);
                                        graphics::Box(box_x + 2, row_y + 2, 3, 3, 1, 0x54);
                                        graphics::Box(box_x + 3, row_y + 3, 1, 1, 1, 0x54);
                                    }
                                }
                            }
                        }
                        break;

                    default:
                        break;
                }

                if (slot.specials == 0) {
                    KEN::Ken_Get_Text_Message(0x1D, message_text, 0x50);
                    fonts::Print(left_panel_x + 0xCB, row_y, message_text);
                } else {
                    uint16_t mod_bits = slot.specials;
                    line_text[0] = '\0';

                    for (int32_t mod_index = 0; mod_index < WEAPON_MOD_COUNT; ++mod_index) {
                        if ((mod_bits & 1) != 0) {
                            if (line_text[0] != '\0') {
                                strlcat(line_text, ", ", sizeof(line_text));
                            }
                            strlcat(line_text, TECHDATA::weapon_modifications[mod_index].name_plural, sizeof(line_text));
                        }
                        mod_bits >>= 1;
                    }

                    fonts::Print(left_panel_x + 0xCB, row_y, line_text);
                }
            }

            row_y = static_cast<int16_t>(row_y + 0x0B);
        }

        fonts::Set_Font_Style_Shadow_Down(2, &heading_colors);
        row_y = static_cast<int16_t>(row_y + 0x07);

        KEN::Ken_Get_Text_Message(0x1E, message_text, 0x50);
        fonts::Print(left_panel_x, row_y, message_text);

        row_y = static_cast<int16_t>(row_y + 0x10);

        graphics::Interlaced_Fill(
            left_panel_x,
            row_y - 0x03,
            right_panel_right_x,
            row_y + 0x2B,
            0,
            panel_fill_color);

        int16_t special_row_y = row_y;
        int16_t displayed_specials = 0;

        const int32_t special_count =
            COMBINIT::amoeba_regeneration != 0 ? SPECIAL_COUNT : SPECIAL_REGENERATION;
        for (int32_t special_index = 0; special_index < special_count; ++special_index) {
            if (struct_::Test_Bit_Field(MOX::global_combat_data.special_device_flags, special_index) != 0) {
                const s_colors* active_colors =
                    (struct_::Test_Bit_Field(MOX::global_combat_data.special_device_damage_flags, special_index) == 0)
                    ? &text_colors
                    : &damaged_colors;

                fonts::Set_Font_Style_Shadow_Down(1, active_colors);
                fonts::Set_Font_Spacing(1);

                if (((displayed_specials & 1) == 0) && (displayed_specials != 0)) {
                    special_row_y = static_cast<int16_t>(special_row_y + 0x0B);
                }

                if ((displayed_specials & 1) == 0) {
                    fonts::Print(left_value_x, special_row_y, TECHDATA::specials[special_index].name);
                } else {
                    fonts::Print(right_value_x, special_row_y, TECHDATA::specials[special_index].name);
                }

                ++displayed_specials;
            }
        }

        MOX::scan_ship_seg =
            buffer::Buffer_Reload("combat.lbx", 5, MOX::global_cache_seg);

        animate::Draw(view_x - 0x09, view_y - 0x09, (s_animation_header*)MOX::scan_ship_seg);
    }

    char* __cdecl Crew_Quality_String(int16_t crew_quality) {
        char format_text[80];

        MOX::temp_string[0] = '\0';

        switch (crew_quality) {
            case CREW_QUALITY_GREEN:
                KEN::Ken_Get_Text_Message(0x1f, format_text, 0x50);
                snprintf(MOX::temp_string, sizeof(MOX::temp_string), format_text, 0);
                return MOX::temp_string;

            case CREW_QUALITY_REGULAR:
                KEN::Ken_Get_Text_Message(0x20, format_text, 0x50);
                snprintf(MOX::temp_string, sizeof(MOX::temp_string), format_text, 100);
                return MOX::temp_string;

            case CREW_QUALITY_VETERAN:
                KEN::Ken_Get_Text_Message(0x21, format_text, 0x50);
                snprintf(MOX::temp_string, sizeof(MOX::temp_string), format_text, 200);
                return MOX::temp_string;

            case CREW_QUALITY_ELITE:
                KEN::Ken_Get_Text_Message(0x22, format_text, 0x50);
                snprintf(MOX::temp_string, sizeof(MOX::temp_string), format_text, 300);
                return MOX::temp_string;

            case CREW_QUALITY_ULTRA_ELITE:
                KEN::Ken_Get_Text_Message(0x23, format_text, 0x50);
                snprintf(MOX::temp_string, sizeof(MOX::temp_string), format_text, 300);
                break;

            default:
                break;
        }

        return MOX::temp_string;
    }

    void __cdecl Remap_Draw_Ship_Status(int16_t x, int16_t y) {
        s_animation_header* ship_picture = KEN::Get_Ship_Id_Picture_Seg(MOX::global_combat_data.ship_idx);

        bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, ship_picture);
        bitmap::Create_Blank_Bitmap(0x62, 0x62, MOX::combat_bitmap);
        bitmap::Add_Picture_To_Bitmap(0x10, 0x0B, MOX::draw_ship_bitmap, MOX::combat_bitmap);

        if (MOX::global_combat_data.shield_type > 0) {
            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
            bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);

            switch (MOX::global_combat_data.shield_type) {
                case 1:
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[2]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[4]), 0);
                    break;

                case 2:
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[1]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[2]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[3]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[5]), 0);
                    break;

                case 3:
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 2, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[0]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[1]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[3]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[4]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[5]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[6]), 0);
                    break;

                case 4:
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, 1, 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[0]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[1]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[2]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[3]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[4]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[5]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[6]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[7]), 0);
                    break;

                case 5:
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[0]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[1]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[2]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[2]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[3]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[4]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[4]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[5]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[6]), 0);
                    bitmap::Outline_Bitmap(MOX::combat_bitmap, MOX::combat_bitmap, static_cast<uint16_t>(CMBTDRW1::green[7]), 0);
                    break;

                default:
                    break;
            }

            replace::Replace_Color(MOX::combat_bitmap, 1, 0);
            replace::Replace_Color(MOX::combat_bitmap, 2, 0);
        }

        animate::Draw(x, y, MOX::combat_bitmap);
    }

} // namespace CMBTDRW1
