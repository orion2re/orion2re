#include "pch.h"
#include "game_events.h"

namespace COLGCBT {

    s_help_box colony_ground_combat_screen_help[3] = {
        {541, 1, 40, 260, 185},
        {542, 378, 40, 639, 185},
        {540, 0, 0, 639, 479},
    };

    s_mouse_list mouse_list_blank = {0, 0, 0, 0, 639, 479};

    uint8_t N_DEF_UNIT_ROWS = 3;
    uint8_t N_ATK_UNIT_ROWS = 3;
    uint8_t title_changed_flag;
    int16_t old_ctr = -1;
    int16_t g_ctr;
    s_invasion_side_info* g_attack_ptr;
    s_animation_header* explosion_anim;
    uint16_t display_fast;
    s_animation_header* heart_anim;
    uint16_t start_combat;
    s_animation_header* frame_anim;
    s_invasion_side_info* g_defend_ptr;
    int16_t fields[2];
    int16_t top_line;
    s_animation_header* anims[40];

    uint8_t player_color_array[MAX_PLAYERS][8] = {
        {146, 236, 237, 238, 239, 150, 151, 152},
        {218, 219, 220, 212, 222, 223, 224, 225},
        {192, 193, 194, 195, 196, 197, 198, 199},
        {89, 93, 97, 99, 101, 103, 105, 108},
        {112, 114, 115, 116, 117, 118, 119, 135},
        {210, 211, 212, 213, 214, 215, 216, 217},
        {121, 122, 123, 124, 124, 125, 126, 127},
        {183, 184, 185, 186, 187, 188, 189, 189},
    };

    uint8_t short_player_color_array[MAX_PLAYERS][4] = {
        {236, 237, 238, 239},
        {219, 221, 223, 225},
        {232, 232, 234, 235},
        {86, 97, 103, 107},
        {114, 116, 118, 119},
        {211, 213, 214, 216},
        {121, 123, 125, 127},
        {184, 185, 186, 187},
    };

    s_ground_unit g_units[2][40];

    void __cdecl Calc_Top_Line() {
        if (COLONY::colony_bldgs[0].buildings[0] > 0) {
            COLONY::colony_bldgs[0].buildings[0] = 0;
            top_line = 1;
            return;
        }

        if (COLONY::colony_bldgs[1].buildings[0] > 0 ||
            COLONY::colony_bldgs[0].buildings[1] > 0 ||
            COLONY::colony_bldgs[1].buildings[1] > 0) {
            top_line = 1;
            return;
        }

        if (COLONY::colony_bldgs[2].buildings[0] > 0 ||
            COLONY::colony_bldgs[0].buildings[2] > 0 ||
            COLONY::colony_bldgs[2].buildings[1] > 0 ||
            COLONY::colony_bldgs[1].buildings[2] > 0 ||
            COLONY::colony_bldgs[2].buildings[2] > 0) {
            top_line = 2;
            return;
        }

        top_line = 3;
    }

    void __cdecl Replace_Colgcbt_Color_With_Player_Colors(s_animation_header* anim, int16_t player_idx) {
        uint8_t color = (uint8_t)MOX::player[player_idx].color;

        if (color != 2) {
            replace::Replace_Color_Range(anim, player_color_array[color], 0xC0, 8);
            replace::Replace_Color_Range(anim, short_player_color_array[color], 0xE8, 4);
        }
    }

    void __cdecl Load_Combat_Anims() {
        explosion_anim = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLONY_LBX, 7, MOX::screen_seg);

        anims[8] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 0, MOX::screen_seg);
        anims[5] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 1, MOX::screen_seg);
        anims[7] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 2, MOX::screen_seg);
        anims[9] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 3, MOX::screen_seg);
        anims[6] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 4, MOX::screen_seg);

        Replace_Colgcbt_Color_With_Player_Colors(anims[8], g_attack_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[5], g_attack_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[7], g_attack_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[9], g_attack_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[6], g_attack_ptr->player_id);

        anims[13] = anims[8];
        anims[10] = anims[5];
        anims[12] = anims[7];
        anims[14] = anims[9];
        anims[11] = anims[6];

        anims[18] = anims[8];
        anims[15] = anims[5];
        anims[17] = anims[7];
        anims[19] = anims[9];
        anims[16] = anims[6];

        anims[28] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 5, MOX::screen_seg);
        anims[25] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 6, MOX::screen_seg);
        anims[27] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 7, MOX::screen_seg);
        anims[29] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 8, MOX::screen_seg);
        anims[26] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 9, MOX::screen_seg);

        Replace_Colgcbt_Color_With_Player_Colors(anims[28], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[25], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[27], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[29], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[26], g_defend_ptr->player_id);

        anims[33] = anims[28];
        anims[30] = anims[25];
        anims[32] = anims[27];
        anims[34] = anims[29];
        anims[31] = anims[26];

        anims[38] = anims[28];
        anims[35] = anims[25];
        anims[37] = anims[27];
        anims[39] = anims[29];
        anims[36] = anims[26];

        if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            anims[23] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 15, MOX::screen_seg);
            anims[20] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 16, MOX::screen_seg);
            anims[22] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 17, MOX::screen_seg);
            anims[24] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 18, MOX::screen_seg);
            anims[21] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 19, MOX::screen_seg);
        } else {
            anims[23] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 10, MOX::screen_seg);
            anims[20] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 11, MOX::screen_seg);
            anims[22] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 12, MOX::screen_seg);
            anims[24] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 13, MOX::screen_seg);
            anims[21] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 14, MOX::screen_seg);
        }

        Replace_Colgcbt_Color_With_Player_Colors(anims[23], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[20], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[22], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[24], g_defend_ptr->player_id);
        Replace_Colgcbt_Color_With_Player_Colors(anims[21], g_defend_ptr->player_id);

        heart_anim = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 20, MOX::screen_seg);
        frame_anim = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLGCBT_LBX, 21, MOX::screen_seg);
    }

    void __cdecl Colony_Combat_Screen(s_invasion_side_info* attacker, s_invasion_side_info* defender, uint32_t seed, int16_t colony_idx) {
        game_events::s_leader_view_scope leader_view(leader_read::GROUND_SUMMARY);
        bool is_exit = false;
        MOX2::action_func_t auto_function_ptr;

        MOX::at_least_one_tactical_combat = 1;
        start_combat = 0;
        display_fast = 0;
        title_changed_flag = 0;

        g_attack_ptr = attacker;
        g_defend_ptr = defender;

        N_ATK_UNIT_ROWS = 3;
        N_DEF_UNIT_ROWS = 3;

        COLONY::Set_Colony_Font();
        COLONY::Load_Colony_Screen_Seg();
        Load_Combat_Anims();

        COLONY::using_colony_screen_palette = 1;
        COLONY::Set_Colony_Globals(colony_idx);
        COLONY::sys_disp_star_handle = COLONY::star_handle;
        COLONY::drawing_display = 0;
        COLONY::valid_colony = 1;

        COLONY::Load_Planet_Screen();
        FLCQUEUE::Free_Flic_Queue_And_Pointers();
        Set_Up_Troops();

        if (COLONY::valid_colony != 0) {
            COLONY::Make_Bldg_Array_For_Colony(COLONY::colony_handle, 1);
        }

        mouse::Set_Mouse_List(&mouse_list_blank, 1);
        ERIC::Fast_Fade_Out();
        COLONY::Update_Colony_Palette();
        palstore::Darken_Palette(100);
        palette::Clear_Palette_Changes(0, 255);
        palette::Refresh_Palette();

        COLONY::Reset_Field_Arrays();
        Add_Global_Fields();
        COLONY::full_draw = 1;

        MOX2::Get_Auto_Function(&auto_function_ptr);
        ::fields::Assign_Auto_Function(Draw_Colony_Combat_Screen, 1);
        Set_Help_List();

        graphics::Fill(0, 0, 639, 479, 0);
        game_events::Ready_Leader_View(leader_read::GROUND_SUMMARY);
        Draw_Colony_Combat_Screen();

        palstore::Darken_Palette(100);
        palette::Clear_Palette_Changes(0, 255);
        RUSS::Mox_Sync_Update();
        ERIC::Fast_Fade_In();

        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        video::Set_Refresh_Stencil();
        ::fields::Clear_Mouse_Buffer();
        ::fields::Set_Input_Delay(5);

        timer::Mark_Time();
        timer::Release_Time(16);

        game_random::Set_Random_Seed(seed);

        do {
            timer::Mark_Time();
            JIM::Play_Streaming_Music(10, -1, 1);
            Add_Global_Fields();
            game_events::Ready_Leader_View(leader_read::GROUND_SUMMARY);
            Draw_Colony_Combat_Screen();
            RUSS::Mox_Sync_Update();

            int16_t input = ::fields::Get_Input();
            ::fields::Scan_Input();

            if (input != 0) {
                if (display_fast == 0 &&
                    (uint8_t)attacker->current_troop_type_idx < UNIT_COUNT &&
                    (uint8_t)defender->current_troop_type_idx < UNIT_COUNT) {
                    display_fast = 1;
                } else {
                    is_exit = true;
                }

                if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                    COLONY::full_draw = 1;
                    MOX::FULL_DRAW_SCREEN_FLAG = 0;
                    Set_Help_List();
                }
            }

            timer::Release_Time(1);
        } while (!is_exit);

        leader_view.Close();
        JIM::Play_Background_Music();
        sound::Stop_All_Sounds();
        COLONY::Reset_Field_Mode();
        ::fields::Clear_Fields();
        ::fields::Deactivate_Help_List();
        ::fields::Deactivate_Auto_Function();

        MOX2::Put_Auto_Function(&auto_function_ptr);
        FLCQUEUE::Allocate_Flic_Queue_And_Pointers();

        MOX::current_screen = MOX::return_screen;
        COLONY::using_colony_screen_palette = 0;
    }

    void __cdecl Set_Help_List() {
        ERICHELP::Eric_Set_Help_List(colony_ground_combat_screen_help, 3);
    }

    void __cdecl Draw_Summary_Box_ATK() {
        int16_t row = 0;

        if (COLONY::full_draw != 0) {
            graphics::Darken_Fill(2, 0x29, 0x103, 0xb8);
            animate::Draw(1, 0x28, frame_anim);
            COLONY::Set_Colony_Font_To_Player(2, (uint8_t)g_attack_ptr->player_id);
            char* str = ESTRINGS::E_Strings(0xbb);
            fonts::Print_Centered(0x82, row * 11 + 50, str);

            row++;
            Print_Best_Summary(g_attack_ptr, &row);
            Print_Race_Summary(g_attack_ptr, &row);

            row++;
            Print_Troop_Totals(1, row * 11 + 50, g_attack_ptr);
        }
    }

    int16_t __cdecl Get_Next_Troop_Type(uint8_t side) {
        for (int16_t i = 0; i < UNIT_COUNT; ++i) {
            if (side == 0) {
                if (N_Troop_Type(i, side) < g_attack_ptr->unit_counts[i]) {
                    return i;
                }
            } else {
                if (N_Troop_Type(i, side) < g_defend_ptr->unit_counts[i]) {
                    return i;
                }
            }
        }
        return -1;
    }

    s_animation_header* __cdecl Action_Anim(uint8_t side, uint8_t state, int8_t type) {
        switch (state) {
            case 0:
                return explosion_anim;
            case 1:
                return anims[(type + side * UNIT_COUNT) * 5];
            case 2:
                return anims[(type + side * UNIT_COUNT) * 5 + 1];
            case 3:
                return anims[(type + side * UNIT_COUNT) * 5 + 2];
            case 4:
                return anims[(type + side * UNIT_COUNT) * 5 + 3];
            case 5:
                return anims[(type + side * UNIT_COUNT) * 5 + 4];
            default:
                return nullptr;
        }
    }

    void __cdecl Resort_Units(uint8_t side_idx) {
        s_ground_unit swapped_unit;
        int16_t passes = 40;

        do {
            for (int16_t i = 0; i < 39; ++i) {
                s_animation_header* anim1 = Action_Anim(
                    g_units[side_idx][i].side,
                    g_units[side_idx][i].state,
                    g_units[side_idx][i].type);

                s_animation_header* anim2 = Action_Anim(
                    g_units[side_idx][i + 1].side,
                    g_units[side_idx][i + 1].state,
                    g_units[side_idx][i + 1].type);

                int16_t height1 = animate::Get_Height(anim1);
                int16_t height2 = animate::Get_Height(anim2);

                if (g_units[side_idx][i + 1].y + height2 < g_units[side_idx][i].y + height1) {
                    struct_::Copy_Structure(&swapped_unit, &g_units[side_idx][i], sizeof(swapped_unit));
                    struct_::Copy_Structure(&g_units[side_idx][i], &g_units[side_idx][i + 1], sizeof(swapped_unit));
                    struct_::Copy_Structure(&g_units[side_idx][i + 1], &swapped_unit, sizeof(swapped_unit));
                }
            }
            --passes;
        } while (passes != 0);
    }

    void __cdecl Draw_Summary_Box_DEF() {
        int16_t line_y_offset = 0;

        if (COLONY::full_draw != 0) {
            graphics::Darken_Fill(0x17B, 0x29, 0x27E, 0xB8);
            animate::Draw(0x17A, 0x28, frame_anim);

            COLONY::Set_Colony_Font_To_Player(2, (uint8_t)g_defend_ptr->player_id);

            char* str = ESTRINGS::E_Strings(0x100);
            fonts::Print_Centered(0x1FC, line_y_offset * 11 + 50, str);

            line_y_offset++;

            Print_Best_Summary(g_defend_ptr, &line_y_offset);
            Print_Race_Summary(g_defend_ptr, &line_y_offset);

            line_y_offset++;

            Print_Troop_Totals(0x17A, line_y_offset * 11 + 50, g_defend_ptr);
        }
    }

    int16_t __cdecl N_Slots_Left(uint8_t side) {
        int16_t count = 0;
        for (int16_t i = 0; i < 40; ++i) {
            if (g_units[side][i].state == 0) {
                count++;
            }
        }
        return count;
    }

    s_ground_unit* __cdecl Get_First_Unit(uint8_t side) {
        for (int16_t i = 0; i < 40; ++i) {
            if (g_units[side][i].state == 0) {
                return &g_units[side][i];
            }
        }
        return nullptr;
    }

    int16_t __cdecl N_Troop_Type(int16_t unit_type, uint8_t side_idx) {
        int16_t count = 0;

        for (int16_t i = 0; i < 40; ++i) {
            if (g_units[side_idx][i].type == unit_type && g_units[side_idx][i].state != 0) {
                count++;
            }
        }

        return count;
    }

    void __cdecl Spatter_Down_Starting_Troops() {
        for (uint8_t side = 0; side < 2; ++side) {
            for (int16_t i = 0; i < 40; ++i) {
                g_units[side][i].x = 0;
                g_units[side][i].y = 0;
                g_units[side][i].state = 0;
            }

            int16_t slots_left = N_Slots_Left(side);
            bool done = (slots_left == 0);

            if (slots_left > 10) {
                slots_left = 10;
            }

            while (!done) {
                Add_Initial_Unit(side);
                slots_left--;

                if (slots_left == 0 || Get_Next_Troop_Type(side) == -1) {
                    done = true;
                }
            }

            Resort_Units(side);
        }
    }

    void __cdecl Set_Up_Troops() {
        memset(&g_units, 0, sizeof(g_units));
        Calc_Top_Line();
        Spatter_Down_Starting_Troops();
    }

    void __cdecl Add_Global_Fields() {
        ::fields::Clear_Fields();

        fields[0] = ::fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, ESTRINGS::E_Strings(0xc), 0);
        fields[1] = ::fields::Add_Hot_Key(ESTRINGS::E_Strings(0x87));
    }

    void __cdecl Draw_Summary_Boxes() {
        Draw_Summary_Box_ATK();
        Draw_Summary_Box_DEF();
    }

    void __cdecl Do_Unit_Turn(s_ground_unit* unit) {
        COLONY::Set_Colony_Font_To(1);

        if (unit->state == 0) {
            return;
        }

        if (unit->side == 0) {
            if (g_attack_ptr->round_killed_unit_type == unit->type && unit->state != 1) {
                unit->state = 1;
                unit->anim_frame = 0;
                unit->is_hit = 0;
                g_attack_ptr->round_killed_unit_type = -1;
                g_attack_ptr->round_hit_unit_type = -1;
                COLONY::full_draw = 1;
            } else if (g_attack_ptr->round_hit_unit_type == unit->type && unit->state != 1 && unit->state != 5) {
                if (game_random::Random(N_Left_Active_To_Process(unit)) <= 1) {
                    unit->state = 5;
                    unit->is_hit = 0;
                    unit->anim_frame = 0;
                    g_attack_ptr->round_hit_unit_type = -1;
                }
            }
        } else {
            if (g_defend_ptr->round_killed_unit_type == unit->type && unit->state != 1) {
                unit->state = 1;
                unit->is_hit = 0;
                unit->anim_frame = 0;
                g_defend_ptr->round_killed_unit_type = -1;
                g_defend_ptr->round_hit_unit_type = -1;
                COLONY::full_draw = 1;
            } else if (g_defend_ptr->round_hit_unit_type == unit->type && unit->state != 1 && unit->state != 5) {
                if (game_random::Random(N_Left_Active_To_Process(unit)) <= 1) {
                    unit->state = 5;
                    unit->is_hit = 0;
                    unit->anim_frame = 0;
                    g_defend_ptr->round_hit_unit_type = -1;
                }
            }
        }

        s_animation_header* seg = Action_Anim(unit->side, unit->state, unit->type);

        if (unit->state != 3 && (unit->state == 0 || unit->target == nullptr)) {
            if (unit->target != nullptr && unit->target->type == -1) {
                unit->target = nullptr;
            }
        } else {
            if (unit->anim_frame == 0 && unit->state == 3 && unit->action_timer >= 10) {
                unit->target = nullptr;
            }

            if (unit->target == nullptr) {
                unit->target = Pick_Target(unit->side == 0);

                if (unit->target == nullptr || game_random::Random(N_Left_Of_Me_To_Process(unit)) <= 2) {
                    unit->target = nullptr;
                    if (game_random::Random(2) == 1) {
                        unit->state = 4;
                    } else {
                        unit->state = 2;
                        unit->anim_frame = 0;
                    }
                    seg = Action_Anim(unit->side, unit->state, unit->type);
                }

                unit->action_timer = 0;
                unit->sound_id = 0;
            }

            if (unit->target != nullptr) {
                if (unit->target->type != -1) {
                    unit->target->is_hit = 1;
                    Draw_Unit_Beam(unit);
                }
            }
        }

        if (unit->anim_frame < animate::Get_Animation_Frame_Count(seg) - 1 && unit->state != 2) {
            if ((abs(g_ctr) % 2) == 0) {
                unit->anim_frame++;
            }

            if (unit->target != nullptr) {
                if (unit->action_timer == 5) {
                    if (unit->target->type == 0 && unit->target->state == 1) {
                        ERICNET::Mox_Play_Sound(0x1E);
                    } else {
                        ERICNET::Mox_Play_Sound(0x1C);
                    }
                }
                if (unit->action_timer == 0) {
                    Play_Unit_Beam_Sound(unit);
                }
                if (unit->action_timer == 10) {
                    sound::Fadeout_Sound(unit->sound_id, 10);
                }

                unit->action_timer++;
                if (unit->action_timer > 11) {
                    unit->target = nullptr;
                }
            }
        } else {
            Pick_Next_Action(unit);
        }

        if (unit->state != 0) {
            animate::Set_Animation_Frame(seg, unit->anim_frame);
            animate::Draw(unit->x, unit->y, seg);
        }

        if (unit->state == 4 && (unit->x < 0x136 || unit->x > 0x14a)) {
            bool modify_x = false;

            if (unit->type >= 1) {
                if (unit->anim_frame % 2 == 0) {
                    modify_x = true;
                } else {
                    if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED) {
                        modify_x = true;
                    }
                }
            } else if (unit->type == 0) {
                if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    if (unit->anim_frame % 2 == 0 && unit->anim_frame > 1) {
                        modify_x = true;
                    }
                } else {
                    modify_x = true;
                }
            } else {
                if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    modify_x = true;
                }
            }

            if (modify_x) {
                unit->x = unit->x - (unit->side * 2) + 1;
            }
        }

        Do_Delta_Y(unit);
    }

    void __cdecl Verify_Legal_Totals() {
        for (uint8_t side = 0; side < 2; ++side) {
            s_invasion_side_info* side_info = side == 0 ? g_attack_ptr : g_defend_ptr;
            for (int16_t type = 0; type < UNIT_COUNT; ++type) {
                int16_t excess = -side_info->unit_counts[type];
                for (const auto& unit : g_units[side]) {
                    if (unit.type == type && unit.state > 1) {
                        ++excess;
                    }
                }

                // Fast-forward can overwrite casualty events between draws.
                // Reconcile living sprites without restarting death animations.
                for (auto& unit : g_units[side]) {
                    if (excess <= 0) {
                        break;
                    }
                    if (unit.type == type && unit.state > 1) {
                        unit.state = 1;
                        unit.anim_frame = 0;
                        unit.is_hit = 0;
                        unit.target = nullptr;
                        --excess;
                        COLONY::full_draw = 1;

                        // This casualty is already reflected in unit_counts.
                        if (side_info->round_killed_unit_type == type) {
                            side_info->round_killed_unit_type = -1;
                            side_info->round_hit_unit_type = -1;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Draw_Summary() {
        bool attackers_defeated = true;

        for (int32_t i = 0; i < UNIT_COUNT; ++i) {
            if (g_attack_ptr->unit_counts[i] > 0) {
                attackers_defeated = false;
            }
        }

        COLONY::Set_Colony_Font_To(4);

        int16_t string_id;
        if (attackers_defeated) {
            string_id = 0x3D;
        } else {
            string_id = 0x3E;
        }

        char* race_name = MOX::player[g_attack_ptr->player_id].race_name;
        char* planet_name = HACCESS::Get_Planet_Name(MOX::colony[COLONY::colony_handle].planet);
        char* format_str = ESTRINGS::E_Strings(string_id);

        char buffer[500];
        snprintf(buffer, sizeof(buffer), format_str, planet_name, race_name);

        fonts::Print_Centered(319, 10, buffer);
    }

    void __cdecl Force_Firing_Pairs() {
        for (uint8_t side = 0; side < 2; ++side) {
            s_ground_unit* target = Find_Unshot_Target(side);
            if (target != nullptr) {
                bool opp_side = (side == 0);
                s_ground_unit* shooter = Find_Lowest_Framed_Unit(opp_side, 3, 1);

                if (shooter == nullptr) {
                    shooter = Find_Lowest_Framed_Unit(opp_side, 2, 0);
                    if (shooter == nullptr) {
                        shooter = Find_Lowest_Framed_Unit(opp_side, 4, 0);
                    }
                }

                if (shooter == nullptr) {
                    FLCQUEUE::Add_Explosion_Anim(target->x, target->y);
                } else {
                    shooter->state = 3;
                    shooter->anim_frame = 0;
                    shooter->target = target;
                    target->is_hit = 1;
                }
            }
        }
    }

    void __cdecl Add_Initial_Unit(uint8_t side) {
        int16_t base_x[2] = {0x32, 0x24e};

        s_ground_unit* unit = Get_First_Unit(side);
        if (unit != nullptr) {
            int8_t type = (int8_t)Get_Next_Troop_Type(side);
            unit->type = type;

            if (type != -1) {
                unit->state = 2;
                unit->x = (int16_t)(base_x[side] + game_random::Random(0x32) - 0x14);

                if (game_random::Random(0x55) + 0x168 >= 0x1ae) {
                    unit->y = 0x1ae;
                } else {
                    unit->y = (int16_t)(game_random::Random(0x55) + 0x168);
                }

                unit->side = side;
                unit->y_offset = (int16_t)game_random::Random(3) - 2;
                unit->anim_frame = 0;
            } else {
                unit->x = base_x[side];
            }
        }
    }

    void __cdecl Add_Unit(uint8_t side) {
        int16_t start_x[2] = {-10, 640};

        s_ground_unit* unit = Get_First_Unit(side);
        if (unit != nullptr) {
            unit->type = (int8_t)Get_Next_Troop_Type(side);

            if (unit->type != -1) {
                unit->state = 2;
                unit->x = start_x[side];

                s_animation_header* anim = Action_Anim(unit->side, unit->state, unit->type);
                int16_t height = animate::Get_Height(anim);

                if ((480 - height) > (int32_t)(game_random::Random(85) + 360)) {
                    unit->y = (int16_t)(game_random::Random(85) + 360);
                } else {
                    anim = Action_Anim(unit->side, unit->state, unit->type);
                    height = animate::Get_Height(anim);
                    unit->y = 480 - height;
                }

                unit->side = (int8_t)side;
                unit->y_offset = (int16_t)game_random::Random(3) - 2;
                unit->anim_frame = 0;
                unit->target = nullptr;
            } else {
                unit->x = start_x[side];
            }
        }
    }

    void __cdecl Print_Data(char* label, int16_t value, int16_t* row, s_invasion_side_info* side_info) {
        char text_buf[500];

        int16_t x_pos;
        if (g_attack_ptr == side_info) {
            x_pos = 1;
        } else {
            x_pos = 0x17A;
        }

        int16_t y_pos = (*row * 11) + 0x32;

        /* fmtpara \x1A0/\x1A1 switch the inserted label/value runs to left and right justification. */
        snprintf(text_buf, sizeof(text_buf), ESTRINGS::E_Strings(0x4B), "\x1A"
                                                                         "0",
            label,
            "\x1A"
            "1",
            value);

        FMTPARA::Print_Formatted_Paragraph(x_pos + 9, y_pos, 0xF3, text_buf, 0);

        (*row)++;
    }

    void __cdecl Reinforce_Units(uint8_t side) {
        int16_t slots_left = N_Slots_Left(side);
        bool done = (slots_left == 0);

        if (slots_left > 10) {
            slots_left = 10;
        }

        while (!done) {
            Add_Unit(side);
            slots_left--;
            if (slots_left == 0) {
                done = true;
            }
        }
    }

    void __cdecl Draw_Troops() {
        Verify_Legal_Totals();

        for (uint8_t side = 0; side < 2; ++side) {
            for (int16_t unit_idx = 0; unit_idx < 40; ++unit_idx) {
                Do_Unit_Turn(&g_units[side][unit_idx]);
            }
            Force_Firing_Pairs();
            Reinforce_Units(side);
            Resort_Units(side);
        }
    }

    void __cdecl Print_Race_Summary(s_invasion_side_info* side_info, int16_t* y_pos) {
        char officer_name[52];

        if (side_info->combat_modifiers.ground_combat_bonus != 0) {
            Print_Data(ESTRINGS::E_Strings(0x1B5), side_info->combat_modifiers.ground_combat_bonus, y_pos, side_info);
        }

        if (side_info->combat_modifiers.low_g_penalty != 0) {
            Print_Data(ESTRINGS::E_Strings(0x16C), side_info->combat_modifiers.low_g_penalty, y_pos, side_info);
        }

        if (side_info->combat_modifiers.subterranean_bonus != 0) {
            Print_Data(ESTRINGS::E_Strings(0x1F1), (uint8_t)side_info->combat_modifiers.subterranean_bonus, y_pos, side_info);
        }

        if (side_info->combat_modifiers.difficulty_bonus != 0 && capture::Check_Release_Version() != 0) {
            Print_Data(ESTRINGS::E_Strings(0x7B), side_info->combat_modifiers.difficulty_bonus, y_pos, side_info);
        }

        if (side_info->combat_modifiers.officer_idx > 0) {
            leader_read::s_leader leader;
            if (leader_read::Read_Ground(MOX::PLAYER_NUM, side_info, leader_read::GROUND_SUMMARY, &leader)) {
                Print_Data(leader.name, leader.bonus, y_pos, side_info);
                return;
            }
            OFFICER::Officer_Name(officer_name, sizeof(officer_name), side_info->player_id, side_info->combat_modifiers.officer_idx);
            Print_Data(officer_name, side_info->combat_modifiers.officer_bonus, y_pos, side_info);
        }
    }

    void __cdecl Print_Best_Summary(s_invasion_side_info* side_info, int16_t* row) {
        if (side_info->combat_modifiers.antigrav_bonus != 0) {
            Print_Data(TECHDATA::technology_applications[TECH_APP_ANTI_GRAV_HARNESS].name,
                (uint8_t)side_info->combat_modifiers.antigrav_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.best_armor_tech_app_id != 0) {
            uint8_t tech_app_id = side_info->combat_modifiers.best_armor_tech_app_id;

            Print_Data(TECHDATA::technology_applications[tech_app_id].name,
                (uint8_t)side_info->combat_modifiers.troop_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.personal_shield_tech_app_id != 0) {
            uint8_t tech_app_id = side_info->combat_modifiers.personal_shield_tech_app_id;

            Print_Data(TECHDATA::technology_applications[tech_app_id].name,
                (uint8_t)side_info->combat_modifiers.personal_shield_bonus,
                row,
                side_info);
        }

        if (side_info->combat_modifiers.rifle_tech_app_id != 0) {
            uint8_t tech_app_id = side_info->combat_modifiers.rifle_tech_app_id;

            Print_Data(TECHDATA::technology_applications[tech_app_id].name,
                (uint8_t)side_info->combat_modifiers.rifle_bonus,
                row,
                side_info);
        }
    }

    void __cdecl Draw_Colony_Combat_Screen() {
        uint32_t random_seed = game_random::Get_Random_Seed();
        video::Set_Page_Off();

        if (COLONY::full_draw != 0) {
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
            animate::Draw(0, 0, COLONY::C_Anims(1));
            animate::Draw(0, 0, COLONY::C_Anims(0));
        }

        COLONY::Set_Colony_Font_To(1);
        Draw_Summary_Boxes();
        if (COLONY::full_draw != 0) {
            leader_read::s_leader leaders[2];
            int16_t count = 0;
            for (const auto* side : {g_attack_ptr, g_defend_ptr}) {
                if (leader_read::Read_Ground(MOX::PLAYER_NUM, side, leader_read::GROUND_SUMMARY, &leaders[count])) ++count;
            }
            game_events::Publish_Leader_View(leader_read::GROUND_SUMMARY, leaders, count);
        }
        COLONY::Set_Colony_Font_To(1);
        COLONY::Draw_Colony_Bldgs();

        if (COLONY::full_draw != 0) {
            video::Copy_Off_To_Back();
            video::Set_Refresh_Stencil();
            COLONY::full_draw = 0;
        }

        COLONY::Set_Colony_Font_To(3);

        if ((uint8_t)g_attack_ptr->current_troop_type_idx < UNIT_COUNT &&
            (uint8_t)g_defend_ptr->current_troop_type_idx < UNIT_COUNT) {
            char buffer[500];
            char* race_name = MOX::player[g_attack_ptr->player_id].race_name;
            char* planet_name = HACCESS::Get_Planet_Name(MOX::colony[COLONY::colony_handle].planet);
            snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x2F), planet_name, race_name);
            fonts::Print_Centered(0x13F, 10, buffer);

        } else {
            Draw_Summary();
            if (title_changed_flag == 0) {
                title_changed_flag = 1;
                COLONY::full_draw = 1;
            }
        }

        graphics::Set_Window(0, 0, 0x27F, 0x1DF);
        video::Clip_On();
        Draw_Troops();
        FLCQUEUE::Draw_Flic_Queue();
        video::Clip_Off();

        game_random::Set_Random_Seed(random_seed);

        for (;;) {
            g_ctr++;

            if ((uint8_t)g_attack_ptr->current_troop_type_idx < UNIT_COUNT &&
                (uint8_t)g_defend_ptr->current_troop_type_idx < UNIT_COUNT) {
                if (start_combat == 0) {
                    start_combat = 1;
                } else {
                    int16_t ctr = g_ctr;
                    if ((ctr % 6) == 0 || display_fast != 0) {
                        invasion::Ground_Combat_Round(g_attack_ptr, g_defend_ptr);
                    }

                    if (display_fast == 0) {
                        timer::Release_Time(1);
                    }
                }
            } else if (start_combat == 0) {
                start_combat = 1;
            }

            if (display_fast == 0) {
                return;
            }

            if ((uint8_t)g_attack_ptr->current_troop_type_idx >= UNIT_COUNT ||
                (uint8_t)g_defend_ptr->current_troop_type_idx >= UNIT_COUNT) {
                return;
            }
        }
    }

    void __cdecl Print_Troop_Totals(int16_t x, int16_t y, s_invasion_side_info* side_info) {
        int16_t active_unit_count = 0;
        int16_t valid_troop_idx = 0;
        char local_str[500];

        for (int32_t i = 0; i < UNIT_COUNT; i++) {
            if (side_info->unit_counts[i] != 0) {
                active_unit_count++;
            }
        }

        int16_t spacing = 261 / (active_unit_count + 1);
        int16_t cur_x = x + spacing;

        for (int32_t j = 0; j < UNIT_COUNT; j++) {
            if (side_info->unit_counts[j] != 0) {
                uint8_t race = (uint8_t)MOX::player[side_info->player_id].race;
                s_animation_header* anim = COLONY::Player_Troop_Anim(side_info->player_id, j, race);

                COLONY::Set_Colony_Font_To_Player(1, (int8_t)g_attack_ptr->player_id);

                int32_t strength = side_info->unit_strengths[j];
                snprintf(local_str, sizeof(local_str), ESTRINGS::E_Strings(13), strength);
                fonts::Print_Centered(cur_x, 176, local_str);

                uint8_t hp = (uint8_t)side_info->unit_hps[j];
                if (hp <= 1) {
                    snprintf(local_str, sizeof(local_str), ESTRINGS::E_Strings(22), hp);
                } else {
                    snprintf(local_str, sizeof(local_str), ESTRINGS::E_Strings(23), hp);
                }
                fonts::Print_Centered(cur_x, 167, local_str);

                if (MOX::settings.language == 4) {
                    snprintf(local_str, sizeof(local_str), "%s: %d", COLONY::Player_Troop_Name(side_info->player_id, j, race), side_info->unit_counts[j]);
                } else {
                    snprintf(local_str, sizeof(local_str), ESTRINGS::E_Strings(14), side_info->unit_counts[j], COLONY::Player_Troop_Name(side_info->player_id, j, race));
                }
                fonts::Print_Centered(cur_x, 158, local_str);

                ERIC::Draw_Bottom_Centered(x + (valid_troop_idx + 1) * spacing, 157, anim);

                // This call effectively acts as a no-op since the result is never printed
                snprintf(local_str, sizeof(local_str), ESTRINGS::E_Strings(124), side_info->unit_counts[j]);

                valid_troop_idx++;
                cur_x += spacing;
            }
        }

        if (valid_troop_idx != old_ctr) {
            COLONY::full_draw = 1;
            old_ctr = valid_troop_idx;
        }
    }

    int16_t __cdecl N_Left_Active_To_Process(s_ground_unit* start_unit) {
        int16_t count = 0;
        s_ground_unit* current_unit = start_unit;
        int8_t side = start_unit->side;
        s_ground_unit* end_unit = &g_units[side][40];

        // High-level intent is scanning this side's 40-unit block, but keep
        // the pointer-based do-while to match the original assembly exactly.
        do {
            if (current_unit->state != 0 && current_unit->state != 1 && current_unit->type == start_unit->type) {
                count++;
            }
            current_unit++;
        } while (current_unit < end_unit);

        return count;
    }

    int16_t __cdecl N_Left_Of_Me_To_Process(s_ground_unit* unit) {
        int16_t count = 0;
        s_ground_unit* end_ptr = &g_units[unit->side][40];

        for (s_ground_unit* current = unit; current < end_ptr; ++current) {
            if (current->state == unit->state && current->type == unit->type) {
                count++;
            }
        }

        return count;
    }

    s_ground_unit* __cdecl Pick_Target(uint8_t side) {
        for (int16_t i = 0; i < 40; ++i) {
            uint8_t state = g_units[side][i].state;
            if ((state == 5 || state == 1) && g_units[side][i].is_hit == 0) {
                return &g_units[side][i];
            }
        }
        return nullptr;
    }

    s_ground_unit* __cdecl Find_Unshot_Target(uint8_t side) {
        for (int16_t i = 0; i < 40; ++i) {
            if (g_units[side][i].state == 1 && g_units[side][i].is_hit == 0) {
                return &g_units[side][i];
            }
        }
        return nullptr;
    }

    s_ground_unit* __cdecl Find_Lowest_Framed_Unit(uint8_t side_idx, int16_t state, uint8_t require_no_target) {
        s_ground_unit* lowest_unit = nullptr;
        int16_t lowest_frame = 100;

        for (int16_t i = 0; i < 40; ++i) {
            if ((int8_t)g_units[side_idx][i].state == state) {
                if (g_units[side_idx][i].anim_frame <= lowest_frame) {
                    if (require_no_target == 0 || g_units[side_idx][i].target == nullptr) {
                        lowest_frame = g_units[side_idx][i].anim_frame;
                        lowest_unit = &g_units[side_idx][i];
                    }
                }
            }
        }

        return lowest_unit;
    }

    void __cdecl Get_Firepoint_Offsets(s_ground_unit* unit, int16_t* x_offset, int16_t* y_offset) {
        s_animation_header* anim = Action_Anim(unit->side, unit->state, unit->type);
        int16_t width = animate::Get_Width(anim);
        animate::Get_Height(anim);

        if (unit->side == 0) {
            if (unit->type == 0) {
                if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    *x_offset = width - 9;
                    *y_offset = 13;
                } else {
                    *x_offset = width - 10;
                    *y_offset = 20;
                }
            } else if (unit->type >= 1 && unit->type <= 3) {
                *x_offset = 16;
                *y_offset = 19;
            }
        } else if (unit->side == 1) {
            if (unit->type == 0) {
                if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    *x_offset = 9;
                    *y_offset = 13;
                } else {
                    *x_offset = 10;
                    *y_offset = 20;
                }
            } else if (unit->type >= 1 && unit->type <= 3) {
                *x_offset = 16;
                *y_offset = 19;
            }
        }
    }

    void __cdecl Draw_Unit_Beam(s_ground_unit* unit) {
        int16_t fire_off_x;
        int16_t fire_off_y;
        Get_Firepoint_Offsets(unit, &fire_off_x, &fire_off_y);

        int16_t tgt_off_x;
        int16_t tgt_off_y;
        Get_Target_Offsets(unit->target, &tgt_off_x, &tgt_off_y);

        int16_t x1 = unit->x + fire_off_x;
        int16_t y1 = unit->y + fire_off_y;
        int16_t x2 = x1;
        int16_t y2 = y1;

        int16_t target_x = unit->target->x + tgt_off_x;
        int16_t target_y = unit->target->y + tgt_off_y;

        int32_t abs_dy = abs(y1 - target_y);
        int32_t abs_dx = abs(x1 - target_x);

        int16_t max_dist = (abs_dy > abs_dx) ? abs_dy : abs_dx;

        int16_t beam_len = Get_CGCBT_Beam_Len(unit->side);

        int16_t act_dist = unit->action_timer * (max_dist / 5);

        int16_t len1 = act_dist;
        if (max_dist < act_dist) {
            len1 = max_dist;
        }

        if (len1 < 0) {
            len1 = 0;
        }

        BEAMS::Rob_Interpolate_Line(&x1, &y1, target_x, target_y, len1);

        int16_t len2 = beam_len + act_dist;
        if (max_dist < len2) {
            len2 = max_dist;
        }

        BEAMS::Rob_Interpolate_Line(&x2, &y2, target_x, target_y, len2);

        Draw_CGCBT_Beam(x1, y1, x2, y2, unit->side);
    }

    void __cdecl Play_Unit_Beam_Sound(s_ground_unit* unit) {
        int16_t default_sound_id = 0;
        s_invasion_side_info* side_info = nullptr;
        int16_t sound_id;

        if (unit->side == 0) {
            side_info = g_attack_ptr;
        } else if (unit->side == 1) {
            side_info = g_defend_ptr;
        }

        if (side_info != nullptr) {
            switch (side_info->combat_modifiers.rifle_tech_app_id) {
                case TECH_APP_FUSION_RIFLE:
                    sound_id = 9;
                    break;
                case TECH_APP_LASER_RIFLE:
                    sound_id = 1;
                    break;
                case TECH_APP_PHASOR_RIFLE:
                    sound_id = 19;
                    break;
                case TECH_APP_PLASMA_RIFLE:
                    sound_id = 13;
                    break;
                case TECH_APP_NO_TECH:
                case TECH_APP_PULSE_RIFLE:
                    sound_id = 12;
                    break;
                default:
                    sound_id = default_sound_id;
                    break;
            }
        } else {
            sound_id = default_sound_id;
        }

        unit->sound_id = ERICNET::Mox_Play_Sound(sound_id);
    }

    void __cdecl Do_Delta_Y(s_ground_unit* unit) {
        s_animation_header* seg = Action_Anim(unit->side, unit->state, unit->type);

        // These function calls do not use their return values, but are kept to preserve possible side effects
        animate::Get_Width(seg);
        animate::Get_Animation_Frame_Count(seg);

        int16_t y_step_frames[11] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
        constexpr int y_step_frame_count = sizeof(y_step_frames) / sizeof(y_step_frames[0]);
        bool uses_y_step = unit->anim_frame >= 0 && unit->anim_frame < y_step_frame_count &&
            y_step_frames[unit->anim_frame] == 1;

        if (unit->state == 4) {
            if ((unit->type == 0 && MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED && unit->anim_frame % 3 == 2) ||
                uses_y_step) {
                unit->y += unit->y_offset;
            }
        }
    }

    void __cdecl Pick_Next_Action(s_ground_unit* unit) {
        unit->anim_frame = 0;

        switch (unit->state) {
            case 0:
                if (unit->type != -1) {
                    unit->type = -1;
                }
                break;

            case 1:
                unit->state = 0;
                sound::Fadeout_Sound(unit->sound_handle, 10);
                COLONY::full_draw = 1;
                break;

            case 2:
                if ((uint8_t)g_attack_ptr->current_troop_type_idx < UNIT_COUNT &&
                    (uint8_t)g_defend_ptr->current_troop_type_idx < UNIT_COUNT) {
                    unit->state = 3;
                } else {
                    unit->state = 4;
                }
                break;

            case 3:
                if ((uint8_t)g_attack_ptr->current_troop_type_idx < UNIT_COUNT &&
                    (uint8_t)g_defend_ptr->current_troop_type_idx < UNIT_COUNT) {
                    uint32_t rnd = game_random::Random(4);
                    if (rnd == 1) {
                        unit->state = 4;
                    } else if (rnd == 2) {
                        unit->state = 3;
                        unit->x += (int16_t)(game_random::Random(3) - 2);
                    } else {
                        unit->state = 4;
                        if (rnd != 3) {
                            unit->y_offset = (int16_t)(game_random::Random(3) - 2);
                        }
                    }
                } else {
                    unit->state = 2;
                }
                break;

            case 4: {
                s_animation_header* anim = Action_Anim(unit->side, unit->state, unit->type);
                animate::Get_Width(anim);

                sound::Fadeout_Sound(unit->sound_handle, 10);
                unit->sound_handle = 0;

                if ((uint8_t)g_attack_ptr->current_troop_type_idx < UNIT_COUNT &&
                    (uint8_t)g_defend_ptr->current_troop_type_idx < UNIT_COUNT) {
                    if (game_random::Random(2) == 1) {
                        unit->state = 3;
                        break;
                    }
                }

                if (game_random::Random(2) == 1) {
                    unit->state = 2;
                } else {
                    unit->state = 4;
                }
                break;
            }

            case 5:
                unit->state = 2;
                break;

            default:
                break;
        }

        Do_Legal_Move_Check(unit);

        if (unit->state == 4 && unit->type == 0 && unit->sound_handle == 0) {
            unit->sound_handle = ERICNET::Mox_Play_Sound(31);
        }
    }

    void __cdecl Get_Target_Offsets(s_ground_unit* unit, int16_t* out_x, int16_t* out_y) {
        s_animation_header* anim = Action_Anim(unit->side, unit->state, unit->type);
        int16_t width = animate::Get_Width(anim);
        animate::Get_Height(anim);

        if (unit->side == 0) {
            if (unit->type == 0) {
                if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    *out_x = width - 15;
                    *out_y = 14;
                } else {
                    *out_x = width - 22;
                    *out_y = 26;
                }
            } else if (unit->type >= 1 && unit->type <= 3) {
                *out_x = 14;
                *out_y = 18;
            }
        } else if (unit->side == 1) {
            if (unit->type == 0) {
                if (MOX::player[g_defend_ptr->player_id].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    *out_x = 15;
                    *out_y = 14;
                } else if (unit->state == 5) {
                    *out_x = 14;
                    *out_y = 23;
                } else {
                    *out_x = 22;
                    *out_y = 26;
                }
            } else if (unit->type >= 1 && unit->type <= 3) {
                *out_x = 10;
                *out_y = 18;
            }
        }
    }

    int16_t __cdecl Get_CGCBT_Beam_Len(uint8_t side_idx) {
        s_invasion_side_info* side; // binary leaves this stack local indeterminate for side_idx != 0/1

        if (side_idx == 0) {
            side = g_attack_ptr;
        } else if (side_idx == 1) {
            side = g_defend_ptr;
        }

        uint8_t rifle_idx = (uint8_t)(side->combat_modifiers.rifle_tech_app_id - TECH_APP_FUSION_RIFLE);
        if (rifle_idx > 72) {
            return 1;
        }

        switch (rifle_idx) {
            case 0:
                return 70;
            case 28:
                return 100;
            case 55:
                return 20;
            case 65:
                return 70;
            case 72:
                return 1;
            default:
                return 1;
        }
    }

    void __cdecl Draw_CGCBT_Beam(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t side) {
        if (x1 == x2 && y1 == y2) {
            return;
        }

        uint8_t rifle_id = 0;
        if (side == 0) {
            rifle_id = g_attack_ptr->combat_modifiers.rifle_tech_app_id;
        } else if (side == 1) {
            rifle_id = g_defend_ptr->combat_modifiers.rifle_tech_app_id;
        }

        // The original code only meaningfully handles sides 0 and 1. Use a
        // zero rifle id for any other value so we take the default beam setup
        // instead of relying on the original undefined dereference path.

        int16_t p5;
        int16_t p6;
        int16_t p7;
        int16_t p8;
        int16_t p9;
        int16_t p10;
        int16_t p11;
        int16_t p12;
        int16_t p13;
        int16_t p14;

        switch (rifle_id) {
            case TECH_APP_FUSION_RIFLE: /* Fusion Rifle */
                p6 = 11;
                p7 = 40;
                p5 = 183;
                p11 = 0;
                p10 = 189;
                p8 = 4;
                p9 = 186;
                p13 = 0;
                p14 = 0;
                p12 = 0;
                break;
            case TECH_APP_LASER_RIFLE: /* Laser Rifle */
                p5 = 236;
                p8 = 1;
                p9 = 236;
                p11 = 0;
                p6 = 4;
                p7 = 10;
                p10 = 239;
                p13 = 0;
                p14 = 0;
                p12 = 1;
                break;
            case TECH_APP_PHASOR_RIFLE: /* Phasor Rifle */
                p8 = 2;
                p5 = 236;
                p6 = 4;
                p9 = 236;
                p14 = 1;
                p7 = 40;
                p10 = 239;
                p11 = 0;
                p13 = 1;
                p12 = 1;
                break;
            case TECH_APP_PLASMA_RIFLE: /* Plasma Rifle */
                p8 = 2;
                p9 = 236;
                p5 = 236;
                p14 = 1;
                p11 = 1;
                p13 = 0;
                p6 = 4;
                p7 = 70;
                p10 = 239;
                p12 = 0;
                break;
            case TECH_APP_PULSE_RIFLE: /* Pulse Rifle */
                p6 = 11;
                p7 = 10;
                p5 = 95;
                p14 = 1;
                p8 = 2;
                p13 = 0;
                p10 = 104;
                p9 = 96;
                p11 = 1;
                p12 = 0;
                break;
            default:
                p5 = 0;
                p6 = 0;
                p7 = 0;
                p8 = 0;
                p9 = 0;
                p10 = 0;
                p11 = 0;
                p12 = 0;
                p13 = 0;
                p14 = 0;
                break;
        }

        if (x1 == x2 || y1 == y2) {
            return;
        }

        bolt::Bolt_Segment(x1, y1, x2, y2, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
    }

    int16_t __cdecl Top_Line_Y() {
        int16_t y_coords[4];

        y_coords[0] = 0x1CD;
        y_coords[1] = 0x1AE;
        y_coords[2] = 0x194;
        y_coords[3] = 0x17D;

        return y_coords[top_line];
    }

    int16_t __cdecl Top_Line_X(uint8_t index) {
        int16_t coords[2];

        coords[0] = 0x00F2;
        coords[1] = 0x0187;

        return coords[index];
    }

    int8_t __cdecl Move_Is_Legal(s_ground_unit* unit) {
        s_animation_header* anim = Action_Anim(unit->side, unit->state, unit->type);
        int16_t width = animate::Get_Width(anim);
        int16_t height = animate::Get_Height(anim);
        int16_t frame_count = animate::Get_Animation_Frame_Count(anim);

        width += unit->x;
        height += unit->y + ((unit->y_offset * frame_count) / 5);

        if (unit->state == 4 && unit->type == 0) {
            width += (unit->side * -80) + 40;
        }

        if (height < 480) {
            if (Top_Line_Y() < height) {
                if (unit->state != 4) {
                    return 1;
                }
                if (unit->side == 0) {
                    if (Top_Line_X(0) > width) {
                        return 1;
                    }
                } else if (unit->side == 1) {
                    if (Top_Line_X(1) < width) {
                        return 1;
                    }
                }
            }

            // Redundant check present in original assembly layout
            if (height < 480) {
                if (Top_Line_Y() < height) {
                    if (unit->state == 2) {
                        return 0;
                    }
                    unit->state = 2;
                    if (unit->sound_handle != 0) {
                        sound::Fadeout_Sound(unit->sound_handle, 10);
                        unit->sound_handle = 0;
                    }
                    unit->y_offset = game_random::Random(3) - 2;
                    return 1;
                }

                if (unit->side == 0) {
                    if (GEO::Points_On_Same_Side_Of_Line(width, height, 0, 479, 10, 361, 225, 461) > 0) {
                        return 1;
                    }
                } else {
                    if (GEO::Points_On_Same_Side_Of_Line(width, height, 630, 479, 406, 461, 629, 364) > 0) {
                        return 1;
                    }
                }

                unit->y_offset = 1;
                return 1;
            }
        }

        unit->y_offset = -1;
        return 1;
    }

    void __cdecl Do_Legal_Move_Check(s_ground_unit* unit) {
        s_animation_header* anim = Action_Anim(unit->side, unit->state, unit->type);
        int16_t height = animate::Get_Height(anim);
        int16_t frame_count = animate::Get_Animation_Frame_Count(anim);

        int16_t bottom = height + unit->y + (unit->y_offset * frame_count) / 5;

        while (!Move_Is_Legal(unit)) {
            switch (game_random::Random(4)) {
                case 1:
                    unit->y_offset = 0;
                    break;
                case 2:
                    unit->y_offset = 1;
                    break;
                case 3:
                    unit->y_offset = -1;
                    break;
                case 4:
                    unit->state = 2;
                    if (unit->sound_handle != 0) {
                        sound::Fadeout_Sound(unit->sound_handle, 10);
                        unit->sound_handle = 0;
                    }
                    break;
            }

            if (bottom > 478) {
                unit->y_offset = -1;
            }
        }
    }

} // namespace COLGCBT