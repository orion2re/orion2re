#include "pch.h"
#include "tactical_view.h"

#include <cstddef>

namespace CMBTSPEC {
    namespace {
        void Play_Leader_Death_SFX(int16_t ship_idx) {
            if (MOX::combat_data[ship_idx].officer_idx >= 0) {
                ERICNET::Mox_Play_Sound(0x21);
            }
        }

        CMBTFIRE::SPHERICAL_EFFECT Spherical_Effect_From_Type(
            int16_t effect_type) {
            switch (effect_type) {
                case 0:
                    return CMBTFIRE::SPHERICAL_EFFECT_PLASMA_FLUX;
                case 1:
                    return CMBTFIRE::SPHERICAL_EFFECT_PULSAR;
                case 2:
                    return CMBTFIRE::SPHERICAL_EFFECT_SPATIAL_COMPRESSOR;
                default:
                    return CMBTFIRE::SPHERICAL_EFFECT_DRIVE_EXPLOSION;
            }
        }
    } // namespace

    int32_t g_engine_sfx_handle;
    uint8_t leader_dies_with_no;
    uint8_t compressor_per_size_class_damage;

    bool __cdecl Draw_Clipped_Tractor_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_animation_header* bitmap) {
        const int64_t original_dx = (int32_t)x2 - x1;
        const int64_t original_dy = (int32_t)y2 - y1;

        if (line::Clip_Line(&x1, &y1, &x2, &y2) != 0) {
            return false;
        }

        const int64_t clipped_dx = (int32_t)x2 - x1;
        const int64_t clipped_dy = (int32_t)y2 - y1;
        // Clip_Line may swap endpoint pairs to normalize screen coordinates.
        // Restore the caller's emitter-to-target direction after clipping.
        if (original_dx * clipped_dx + original_dy * clipped_dy < 0) {
            const int16_t old_x1 = x1;
            const int16_t old_y1 = y1;
            x1 = x2;
            y1 = y2;
            x2 = old_x1;
            y2 = old_y1;
        }

        bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, bitmap);
        return true;
    }

    bool __cdecl Try_Get_Active_Tractor_Target(int16_t ship_idx, int16_t* target_ship_idx) {
        if (ship_idx < 0 || ship_idx >= MOX::combat_total_ship_count) {
            return false;
        }

        s_combat_data& ship = MOX::combat_data[ship_idx];
        if (ship.unit_status != 0) {
            ship.special_cooldown = COMBAT_SHIP_NONE;
            return false;
        }

        int16_t target_idx = ship.special_cooldown == COMBAT_SHIP_NONE ? -1 : ship.special_cooldown;
        if (target_idx < 0) {
            return false;
        }

        if (target_idx >= MOX::combat_total_ship_count || target_idx == ship_idx ||
            MOX::combat_data[target_idx].unit_status != 0) {
            ship.special_cooldown = COMBAT_SHIP_NONE;
            return false;
        }

        if (target_ship_idx != nullptr) {
            *target_ship_idx = target_idx;
        }

        return true;
    }

    void __cdecl Draw_Cloak(int16_t ship_idx, int16_t cloak_type) {
        int16_t unused_x, unused_y;
        int16_t offset_x, offset_y;
        int16_t rel_x, rel_y;
        int16_t duration;
        int16_t step;
        int32_t sound_handle;

        if (CMBTDRW1::Ship_On_Screen(ship_idx) == 0) {
            return;
        }

        s_combat_data& ship = MOX::combat_data[ship_idx];

        // This call's results are not used but maintained for semantic equivalence.
        CMBTDRW1::Get_Draw_Ship_Offsets(ship.size_class, &unused_x, &unused_y);

        switch (cloak_type) {
            case 1:
            case 2:
                duration = 24;
                break;
            case 4:
                duration = 60;
                break;
            case 5:
                duration = 36;
                break;
            default:
                // The original code reads the stack slot later reused for
                // sound_handle here. Use a safe fallback instead of depending
                // on that stack-alias UB in C++.
                duration = 0;
                break;
        }

        MOX::dont_draw_ship = ship_idx;
        rel_x = (ship.x - MOX::cur_combat_x) * 20;
        rel_y = (ship.y - MOX::cur_combat_y) * 20;

        step = (MOX::speedx2_flag == 1 || MOX::net_flag != 0)
            ? animation.cloak_steps.fast
            : animation.cloak_steps.normal;

        sound_handle = ERICNET::Mox_Play_Sound(10);

        if (duration > 0) {
            for (int16_t progress = 0; progress < duration; progress += step) {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                CMBTDRW1::Draw_Ship_To_Bitmap(ship_idx, 0, cloak_type, progress);
                CMBTDRW1::Get_Draw_Ship_Offsets(ship_idx, &offset_x, &offset_y);
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                animate::Draw(offset_x + rel_x, offset_y + rel_y, MOX::draw_ship_bitmap);
                video::Clip_Off();
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }
        }

        MOX::dont_draw_ship = -1;
        sound::Fadeout_Sound(sound_handle, 10);
    }

    void __cdecl Start_Ship_Engine_SFX() {
        if (CMBTSPEC::g_engine_sfx_handle == 0) {
            CMBTSPEC::g_engine_sfx_handle = ERICNET::Mox_Play_Sound(0x1f);
            sound::Loop_Sound(CMBTSPEC::g_engine_sfx_handle, -1);
        }
    }

    void __cdecl End_Ship_Engine_SFX() {
        sound::Fadeout_Sound(CMBTSPEC::g_engine_sfx_handle, 10);
        CMBTSPEC::g_engine_sfx_handle = 0;
    }

    void __cdecl Draw_Continuing_Tractor(int16_t ship_idx) {
        int16_t x1, y1, x2, y2;
        int16_t target_idx;

        if (!Try_Get_Active_Tractor_Target(ship_idx, &target_idx)) {
            return;
        }

        COMBINIT::Ship_Center_Offsets(ship_idx, &x1, &y1);
        x1 += (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20;
        y1 += (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20;

        COMBINIT::Ship_Center_Offsets(target_idx, &x2, &y2);
        x2 += (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20;
        y2 += (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20;

        animate::Set_Animation_Frame((s_animation_header*)MOX::small_tractor_seg, 6);
        palette::Restore_Glass_Remap_Block(MOX::tractor_remap_colors);
        allocate::Push_Block(MOX::screen_seg);

        int32_t remaining_space = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        s_animation_header* temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, remaining_space - 100);

        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bitmap, MOX::small_tractor_seg);
        bitmap::Set_Picture_Glassed((s_animation_header*)temp_bitmap);

        graphics::Set_Window(0, 0, 639, 350);
        video::Clip_On();
        Draw_Clipped_Tractor_Line(x1, y1, x2, y2, temp_bitmap);
        video::Clip_Off();

        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Draw_Moving_Tractor(int16_t ship_idx, int16_t center_x, int16_t center_y) {
        int16_t target_ship_idx;
        int16_t x1, y1, x2, y2;

        if (!Try_Get_Active_Tractor_Target(ship_idx, &target_ship_idx)) {
            return;
        }

        COMBINIT::Ship_Center_Offsets(ship_idx, &x1, &y1);
        if (ship_idx == MOX::cur_ship) {
            x1 += center_x - (MOX::cur_combat_x * 20);
            y1 += center_y - (MOX::cur_combat_y * 20);
        } else {
            x1 += (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20;
            y1 += (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20;
        }

        COMBINIT::Ship_Center_Offsets(target_ship_idx, &x2, &y2);
        if (target_ship_idx == MOX::cur_ship) {
            x2 += center_x - (MOX::cur_combat_x * 20);
            y2 += center_y - (MOX::cur_combat_y * 20);
        } else {
            x2 += (MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20;
            y2 += (MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20;
        }

        animate::Set_Animation_Frame((s_animation_header*)MOX::small_tractor_seg, (MOX::ship_frame % 3) + 4);
        palette::Restore_Glass_Remap_Block(MOX::tractor_remap_colors);
        allocate::Push_Block(MOX::screen_seg);

        int32_t remaining = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        s_animation_header* temp_bmp = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, remaining - 100);

        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bmp, MOX::small_tractor_seg);
        bitmap::Set_Picture_Glassed((s_animation_header*)temp_bmp);
        Draw_Clipped_Tractor_Line(x1, y1, x2, y2, temp_bmp);

        allocate::Pop_Block(MOX::screen_seg);
        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
    }

    void __cdecl Lightning_Field(int16_t ship_idx, int16_t missile_idx, int16_t target_x, int16_t target_y) {
        int16_t off_x, off_y;
        int16_t target_x_rel_center, target_y_rel_center;
        int16_t missile_draw_x, missile_draw_y;
        int16_t draw_x, draw_y;
        int16_t old_dont_draw;
        int32_t sound_handle;
        s_animation_header* anim;

        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);
        target_x_rel_center = target_x - off_x;
        target_y_rel_center = target_y - off_y;

        if (CMBTDRW1::Ship_Completely_On_Screen(ship_idx) == 0) {
            COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[ship_idx].x, MOX::combat_data[ship_idx].y);
        }

        missile_draw_x = MOX::missile[missile_idx].x - MOX::cur_combat_x * 20;
        missile_draw_y = MOX::missile[missile_idx].y - MOX::cur_combat_y * 20;

        target_x_rel_center -= MOX::cur_combat_x * 20;
        target_y_rel_center -= MOX::cur_combat_y * 20;

        int16_t param_3_rel = target_x - MOX::cur_combat_x * 20;
        int16_t param_4_rel = target_y - MOX::cur_combat_y * 20;

        draw_x = (target_x - MOX::cur_combat_x * 20) - 36;
        draw_y = (target_y - MOX::cur_combat_y * 20) - 36;

        int16_t range = special::Range(param_3_rel, param_4_rel, missile_draw_x, missile_draw_y);
        if (range > 16) {
            special::Interpolate_Line(&param_3_rel, &param_4_rel, missile_draw_x, missile_draw_y, range - 16);
        }

        old_dont_draw = MOX::dont_draw_ship;
        MOX::dont_draw_ship = ship_idx;
        sound_handle = ERICNET::Mox_Play_Sound(4);

        for (int i = 15; i > 0; --i) {
            timer::Mark_Time();
            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();
            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();
            CMBTDRW1::Draw_Ship(ship_idx, target_x_rel_center, target_y_rel_center);
            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0, MOX::global_cache_seg);
            animate::Draw(draw_x, draw_y, anim);
            video::Clip_Off();
            graphics::Reset_Window();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }

        MOX::dont_draw_ship = old_dont_draw;
        sound::Fadeout_Sound(sound_handle, 10);
    }

    void __cdecl Draw_Stasis_Ball(int16_t ship_idx, int16_t scale_percent, int16_t off_x, int16_t off_y) {
        int16_t center_x;
        int16_t center_y;
        int16_t actual_w;
        int16_t actual_h;
        int16_t min_x;
        int16_t min_y;
        s_animation_header* h_target;
        s_animation_header* h_temp;
        s_animation_header* final_img;
        int32_t resource_idx;

        COMBINIT::Ship_Center_Offsets(ship_idx, &center_x, &center_y);

        center_x += (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20 + off_x;
        center_y += (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20 + off_y;

        allocate::Push_Block(MOX::screen_seg);
        h_target = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x57E4);
        h_temp = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x57E4);

        int16_t facing = MOX::combat_data[ship_idx].facing_dir;
        int32_t dir_idx = facing;

        if (facing > 4 && facing < 9) {
            dir_idx = abs(facing - 8);
        } else if (facing > 8 && facing < 13) {
            dir_idx = abs(facing + 8) & 0xF;
        } else if (facing > 12 && facing < 16) {
            dir_idx = abs(facing - 16);
        } else {
            dir_idx = abs(dir_idx);
        }

        switch (MOX::combat_data[ship_idx].size_class) {
            case 0:
                resource_idx = dir_idx + 0x35;
                break;
            case 1:
            case 2:
                resource_idx = dir_idx + 0x3A;
                break;
            case 3:
                resource_idx = dir_idx + 0x3F;
                break;
            case 4:
            case 5:
                resource_idx = dir_idx + 9;
                break;
            default:
                goto skip_load;
        }

        {
            char* path = ESTRINGS::E_Strings(0xE0);
            s_animation_header* loaded_img = (s_animation_header*)buffer::Buffer_Reload(path, resource_idx, MOX::global_cache_seg);
            bitmap::Draw_Picture_To_Bitmap(h_temp, loaded_img);
        }

    skip_load:
        if (facing > 4 && facing < 9) {
            bitmap::Flip_Bitmap(h_target, h_temp, 0);
        } else if (facing > 12 && facing < 16) {
            bitmap::Flip_Bitmap(h_target, h_temp, 3);
        } else {
            bitmap::Draw_Picture_To_Bitmap(h_target, h_temp);
        }

        if (scale_percent == 100) {
            bitmap::Set_Picture_Glassed(h_target);
            bitmap::Get_Bitmap_Actual_Size(h_target, &min_x, &min_y, &actual_w, &actual_h);
            final_img = h_target;
        } else {
            bitmap::Scale_Bitmap(h_temp, h_target, scale_percent, scale_percent);
            bitmap::Set_Picture_Glassed(h_temp);
            bitmap::Get_Bitmap_Actual_Size(h_temp, &min_x, &min_y, &actual_w, &actual_h);
            final_img = h_temp;
        }

        animate::Draw(center_x - actual_w / 2, center_y - actual_h / 2, final_img);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Draw_Plasma_Web(int16_t ship_idx, int16_t x_off, int16_t y_off) {
        int16_t draw_x;
        int16_t draw_y;
        int16_t sub_dir;
        int32_t asset_id;
        s_animation_header* seg;
        void* temp_bitmap;
        int8_t facing;

        allocate::Push_Block(MOX::screen_seg);
        uint32_t bitmap_size = bitmap::Get_Bitmap_Size(80, 80);
        temp_bitmap = allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);

        COMBINIT::Ship_Center_Offsets(ship_idx, &draw_x, &draw_y);

        draw_x += x_off;
        draw_y += y_off;

        facing = MOX::combat_data[ship_idx].facing_dir;
        sub_dir = (int16_t)(facing % 4);

        if (facing == 4 || facing == 12) {
            sub_dir = 4;
        }

        bool is_even_facing = ((facing / 4) & 1) == 0;

        switch (MOX::combat_data[ship_idx].size_class) {
            case 0:
                if (is_even_facing) {
                    asset_id = sub_dir + 31;
                } else {
                    asset_id = 35 - sub_dir;
                }
                break;

            case 1:
            case 2:
                if (is_even_facing) {
                    asset_id = sub_dir + 26;
                } else {
                    asset_id = 30 - sub_dir;
                }
                break;

            case 3:
                if (is_even_facing) {
                    asset_id = sub_dir + 21;
                } else {
                    asset_id = 25 - sub_dir;
                }
                break;

            case 4:
            case 5:
                if (is_even_facing) {
                    asset_id = sub_dir + 21;
                } else {
                    asset_id = 20 - sub_dir;
                }
                break;

            default:
                goto finish_drawing;
        }

        seg = (s_animation_header*)buffer::Buffer_Reload(ESTRINGS::E_Strings(224), asset_id, MOX::global_cache_seg);
        {
            int16_t frame_count = animate::Get_Animation_Frame_Count(seg);
            animate::Set_Animation_Frame(seg, MOX::ship_frame % frame_count);
        }
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bitmap, seg);

    finish_drawing:
        int8_t flip_dir = facing / 4;
        if (flip_dir != 0) {
            if (flip_dir == 1) {
                bitmap::Flip_Bitmap((s_animation_header*)temp_bitmap, (s_animation_header*)temp_bitmap, 0);
            } else if (flip_dir == 3) {
                bitmap::Flip_Bitmap((s_animation_header*)temp_bitmap, (s_animation_header*)temp_bitmap, 3);
            }
        }

        palette::Restore_Glass_Remap_Block(MOX::plasma_web_remap_colors);

        draw_x -= animate::Get_Width((s_animation_header*)temp_bitmap) / 2;
        draw_y -= animate::Get_Height((s_animation_header*)temp_bitmap) / 2;

        bitmap::Set_Picture_Glassed((s_animation_header*)temp_bitmap);
        animate::Draw(draw_x, draw_y, (s_animation_header*)temp_bitmap);

        allocate::Pop_Block(MOX::screen_seg);
        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
    }
    void __cdecl Draw_Stasis_Beam(int16_t source_ship_idx, int16_t target_ship_idx) {
        int16_t x1, y1, x2, y2;
        int16_t frame_count;
        int16_t step;
        int32_t sound_handle;
        s_animation_header* anim;
        s_animation_header* temp_bitmap;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            if (!CMBTDRW1::Ship_Completely_On_Screen(target_ship_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[target_ship_idx].x, MOX::combat_data[target_ship_idx].y);
            }

            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 1, MOX::global_cache_seg);
            animate::Draw_Palette(anim);
            frame_count = animate::Get_Animation_Frame_Count(anim);

            allocate::Push_Block(MOX::screen_seg);
            temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000 /* 0xC350 */);

            COMBINIT::Ship_Center_Offsets(source_ship_idx, &x1, &y1);
            x1 += (MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20;
            y1 += (MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20;

            COMBINIT::Ship_Center_Offsets(target_ship_idx, &x2, &y2);
            x2 += (MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20;
            y2 += (MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20;

            if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
                step = 2;
            } else {
                step = 1;
            }

            MOX::dont_draw_ship = source_ship_idx;
            sound_handle = ERICNET::Mox_Play_Sound(10);

            if (frame_count > 0) {
                for (int16_t frame_idx = 0; frame_idx < frame_count; frame_idx += step) {
                    timer::Mark_Time();
                    video::Set_Page_Off();
                    CMBTDRW1::Draw_Main_Combat_Screen();

                    anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 1, MOX::global_cache_seg);
                    animate::Set_Animation_Frame(anim, frame_idx);

                    bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bitmap, anim);
                    graphics::Set_Window(0, 0, 639 /* 0x27F */, 350 /* 0x15E */);
                    video::Clip_On();
                    bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, temp_bitmap);
                    CMBTDRW1::Draw_Ship(source_ship_idx, (MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20, (MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20);
                    graphics::Reset_Window();
                    video::Clip_Off();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(2);
                }
            }

            MOX::dont_draw_ship = -1;
            sound::Fadeout_Sound(sound_handle, 10);

            sound_handle = ERICNET::Mox_Play_Sound(10);
            int16_t ball_frame = 0;
            int16_t ball_step_increment = step * 10;
            for (int16_t ball_counter = 0; ball_counter < 10; ball_counter += step) {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();

                graphics::Set_Window(0, 0, 639 /* 0x27F */, 350 /* 0x15E */);
                video::Clip_On();
                Draw_Stasis_Ball(target_ship_idx, ball_frame, 0, 0);
                video::Clip_Off();
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);

                ball_frame += ball_step_increment;
            }

            sound::Fadeout_Sound(sound_handle, 10);
            allocate::Pop_Block(MOX::screen_seg);
        }
    }

    void __cdecl Gyro_Destablizer(int16_t source_idx, int16_t target_idx, int16_t attack_count) {
        int16_t rotations_left = 0;
        int16_t saved_dont_draw;
        int32_t sound_handle = 0;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            if (!CMBTDRW1::Ship_Completely_On_Screen(target_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[target_idx].x, MOX::combat_data[target_idx].y);
            }

            saved_dont_draw = MOX::dont_draw_ship;
            MOX::dont_draw_ship = target_idx;

            video::Set_Page_Back();
            CMBTDRW1::Full_Draw_Combat_Screen();
            CMBTDRW1::Draw_Main_Combat_Screen();
            video::Copy_Back_To_Off();
            video::Set_Page_Off();
            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();

            int16_t screen_x = (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20;
            int16_t screen_y = (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20;

            CMBTDRW1::Draw_Ship(target_idx, screen_x, screen_y);
            video::Clip_Off();
            graphics::Reset_Window();

            RUSS::Mox_Sync_Update();
            CMBTSPEC::Draw_Gyro_Special_Effect(source_idx, target_idx);

            for (int i = 0; i < 19; ++i) {
                rotations_left += (int16_t)game_random::Random(13);
            }

            while (true) {
                int16_t normalized_rotation = rotations_left % 16;
                if (normalized_rotation != 15 && normalized_rotation >= 2) break;
                rotations_left += (int16_t)game_random::Random(13);
            }

            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();
            sound_handle = ERICNET::Mox_Play_Sound(3);

            if (rotations_left > 0) {
                do {
                    video::Set_Page_Off();
                    int16_t next_dir = MOX::combat_data[target_idx].facing_dir + 1;
                    int16_t wrapped_dir = abs(next_dir) % 16;
                    if (next_dir < 0) {
                        wrapped_dir = -wrapped_dir;
                    }
                    MOX::combat_data[target_idx].facing_dir = (int8_t)wrapped_dir;

                    graphics::Set_Window(0, 0, 639, 350);
                    video::Clip_On();
                    CMBTDRW1::Draw_Ship(target_idx, screen_x, screen_y);
                    graphics::Reset_Window();
                    video::Clip_Off();
                    RUSS::Mox_Sync_Update();
                    rotations_left--;
                } while (rotations_left != 0);
            }

            video::Clip_Off();
            graphics::Reset_Window();
            video::Set_Page_Back();
            CMBTDRW1::Full_Draw_Combat_Screen();
            video::Set_Page_Off();
            MOX::dont_draw_ship = saved_dont_draw;
        }

        int16_t total_damage_accum = 0;
        if (attack_count > 0) {
            int16_t range = TECHDATA::weapons[WEAPON_GYRO_DESTABILIZER].max_damage - TECHDATA::weapons[WEAPON_GYRO_DESTABILIZER].min_damage + 1;
            for (int i = 0; i < attack_count; ++i) {
                int32_t roll = game_random::Random(range);
                int16_t base_dmg = TECHDATA::weapons[WEAPON_GYRO_DESTABILIZER].min_damage + (int16_t)roll - 1;
                total_damage_accum += (int16_t)(base_dmg * (MOX::combat_data[target_idx].size_class + 1));
            }
        }

        int16_t bonus = MOX::ordnance_bonus[MOX::combat_data[source_idx].owner];
        int16_t scaled_damage = (int16_t)((bonus * total_damage_accum) / 100);

        int16_t current_damage = MOX::combat_data[target_idx].structure_damage;
        int16_t max_structure = MOX::combat_data[target_idx].structure_max;
        int16_t applied_damage = scaled_damage;

        if (current_damage + scaled_damage > max_structure) {
            applied_damage = max_structure - current_damage;
        }

        MOX::combat_data[target_idx].structure_damage += applied_damage;
        tactical_view::Record_Unit_Hit(target_idx, applied_damage);

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            int16_t screen_x = (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20;
            int16_t screen_y = (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20;

            BEAMS::Add_Damage_Indicator_For_Ship(screen_x, screen_y, target_idx, 1, applied_damage, 0);
            BEAMS::Draw_Damage_Message_Queue_Until_Done();
            sound::Fadeout_Sound(sound_handle, 10);
        }
    }

    void __cdecl Tractor_Beam(int16_t source_ship_idx, int16_t target_ship_idx) {
        int16_t src_x;
        int16_t src_y;
        int16_t dst_x;
        int16_t dst_y;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            allocate::Push_Block(MOX::screen_seg);
            s_animation_header* h1 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000);

            COMBINIT::Ship_Center_Offsets(source_ship_idx, &src_x, &src_y);
            src_x += (MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20;
            src_y += (MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20;

            COMBINIT::Ship_Center_Offsets(target_ship_idx, &dst_x, &dst_y);
            dst_x += (MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20;
            dst_y += (MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20;

            for (int16_t frame = 0; frame < 8; ++frame) {
                timer::Mark_Time();
                video::Set_Page_Off();
                animate::Set_Animation_Frame((s_animation_header*)MOX::small_tractor_seg, frame);
                bitmap::Draw_Picture_To_Bitmap(h1, MOX::small_tractor_seg);
                bitmap::Set_Picture_Glassed(h1);
                palette::Restore_Glass_Remap_Block(MOX::tractor_remap_colors);
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                Draw_Clipped_Tractor_Line(src_x, src_y, dst_x, dst_y, h1);
                graphics::Reset_Window();
                video::Clip_Off();
                palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
                CMBTDRW1::Draw_Main_Combat_Screen();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }

            allocate::Pop_Block(MOX::screen_seg);
        }

        MOX::combat_data[source_ship_idx].special_cooldown = (uint16_t)target_ship_idx;
        COMBAT1::Recalculate_Combat_Ship(target_ship_idx, true);
        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    void __cdecl Draw_Anti_Missile_Rockets(int16_t ship_idx, int16_t target_x, int16_t target_y, int16_t missile_idx, int16_t) {
        int16_t x_offset = MOX::cur_combat_x * 20;
        int16_t y_offset = MOX::cur_combat_y * 20;

        int16_t missile_x_rel = MOX::missile[missile_idx].x - x_offset;
        int16_t missile_y_rel = MOX::missile[missile_idx].y - y_offset;

        int16_t target_x_rel = target_x - x_offset;
        int16_t target_y_rel = target_y - y_offset;

        int16_t off_x = 0;
        int16_t off_y = 0;
        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        int16_t curr_x = off_x + target_x_rel;
        int16_t curr_y = off_y + target_y_rel;
        int16_t start_x = curr_x;
        int16_t start_y = curr_y;

        int16_t speed;
        if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
            speed = 30;
        } else {
            speed = 20;
        }

        graphics::Save_Window();

        while (missile_x_rel != curr_x || missile_y_rel != curr_y) {
            timer::Mark_Time();
            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();
            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();

            if (ship_idx == MOX::dont_draw_ship) {
                CMBTDRW1::Draw_Ship(ship_idx, target_x_rel, target_y_rel);
            }

            special::Absolute_Interpolate_Line(&curr_x, &curr_y, start_x, start_y, missile_x_rel, missile_y_rel, speed);
            CMBTSPEC::Draw_Anti_Missile_Rocket_Picture(curr_x, curr_y);
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }
    }

    void __cdecl Anti_Missile_Rockets(int16_t ship_idx, int16_t ship_x, int16_t ship_y, int16_t missile_idx, int16_t weapon_slot_idx, int16_t tech_level) {
        int16_t cur_combat_x_offset = MOX::cur_combat_x * 20;
        int16_t cur_combat_y_offset = MOX::cur_combat_y * 20;
        int16_t count_destroyed = 0;

        int16_t target_x = MOX::missile[missile_idx].x - cur_combat_x_offset;
        int16_t target_y = MOX::missile[missile_idx].y - cur_combat_y_offset;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            Draw_Anti_Missile_Rockets(ship_idx, ship_x, ship_y, missile_idx, tech_level);
        }

        int16_t hit_chance = (int16_t)(anti_missile_rocket_accuracy.base -
            ((tech_level + 2) * anti_missile_rocket_accuracy.range_multiplier /
                anti_missile_rocket_accuracy.range_divisor) +
            1);
        uint8_t shots_avail = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left;

        if (shots_avail > 0) {
            for (int16_t i = 0; i < shots_avail; ++i) {
                if (MOX::missile[missile_idx].type == 0) {
                    break;
                }

                int16_t roll = (int16_t)game_random::Random(100);
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left--;
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].ammo--;

                if (hit_chance > roll) {
                    MOX::missile[missile_idx].quantity--;
                    tactical_view::Missile_Changed(missile_idx);
                    if (MOX::missile[missile_idx].quantity <= 0) {
                        tactical_view::Record_Missile_Hit(missile_idx, count_destroyed + 1);
                        CMBTMIS::Kill_Missile(missile_idx);
                    }
                    count_destroyed++;
                }
            }
        }

        if (MOX::missile[missile_idx].type > 0) tactical_view::Record_Missile_Hit(missile_idx, count_destroyed);
        if (count_destroyed > 0 && MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            ERICNET::Mox_Play_Sound(0x1c);
            s_animation_header* anim_seg = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 7, MOX::global_cache_seg);
            int16_t frame_count = animate::Get_Animation_Frame_Count(anim_seg);

            BEAMS::Add_Damage_Indicator_For_Missile(target_x, target_y, 1, count_destroyed);

            if (frame_count > 0) {
                for (int16_t frame = 0; frame < frame_count; ++frame) {
                    timer::Mark_Time();
                    video::Set_Page_Off();
                    CMBTDRW1::Draw_Main_Combat_Screen();
                    graphics::Set_Window(0, 0, 639, 350);
                    video::Clip_On();

                    if (MOX::dont_draw_ship == ship_idx) {
                        CMBTDRW1::Draw_Ship(ship_idx, ship_x - cur_combat_x_offset, ship_y - cur_combat_y_offset);
                    }

                    s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 7, MOX::global_cache_seg);
                    animate::Set_Animation_Frame(anim, frame);
                    animate::Draw(target_x - 12, target_y - 12, anim);

                    video::Clip_Off();
                    graphics::Reset_Window();
                    BEAMS::Draw_Damage_Message_Queue();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(1);
                }
            }
        }
    }

    void __cdecl Draw_Transporter_Bomb_Beam(int16_t ship_idx, int16_t target_x, int16_t target_y) {
        int16_t start_x, start_y;
        s_animation_header* anim;
        s_animation_header* beam_bitmap;
        int16_t frame_idx = 0;
        int32_t sound_handle;

        CMBTDRW1::Get_Ship_Center_Draw_XY(ship_idx, &start_x, &start_y);
        graphics::Save_Window();

        anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 15, MOX::global_cache_seg);
        animate::Draw_Palette(anim);

        allocate::Push_Block(MOX::screen_seg);
        beam_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000 /* 0xC350 */);

        MOX::dont_draw_ship = ship_idx;
        sound_handle = ERICNET::Mox_Play_Sound(0x14);
        palette::Restore_Glass_Remap_Block(MOX::stasis_bubble_remap_colors);

        for (int16_t i = 8; i > 0; --i) {
            timer::Mark_Time();
            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();

            graphics::Set_Window(0, 0, 639 /* 0x27f */, 350 /* 0x15e */);
            video::Clip_On();

            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 15, MOX::global_cache_seg);
            animate::Set_Animation_Frame(anim, frame_idx % 3);
            bitmap::Draw_Picture_To_Bitmap(beam_bitmap, anim);
            bitmap::Set_Picture_Glassed(beam_bitmap);
            bitmap::Draw_Bitmap_Line(start_x, start_y, target_x, target_y, beam_bitmap);

            graphics::Set_Window(0, 0, 639 /* 0x27f */, 350 /* 0x15e */);
            video::Clip_On();

            int16_t ship_draw_x = (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20;
            int16_t ship_draw_y = (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20;

            frame_idx++;
            CMBTDRW1::Draw_Ship(ship_idx, ship_draw_x, ship_draw_y);

            video::Clip_Off();
            graphics::Reset_Window();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }

        sound::Fadeout_Sound(sound_handle, 10);
        MOX::dont_draw_ship = -1;
        graphics::Restore_Window();
        allocate::Pop_Block(MOX::screen_seg);
        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
    }

    void __cdecl Missile_Launch_FX(int16_t missile_idx, int16_t tx, int16_t ty, int16_t is_targeting_missile) {
        int16_t max_dist;
        int16_t step;
        int16_t dist_acc;
        int16_t hit_flag;
        int32_t sound_handle;
        s_missile* missile;
        int16_t target_unit_idx;

        hit_flag = 0;
        max_dist = 8;
        missile = &MOX::missile[missile_idx];
        target_unit_idx = missile->target_unit_idx;

        if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
            step = 9;
        } else {
            step = 3;
        }

        if ((missile->type == WEAPON_PROTON_TORPEDO &&
                CMBTMIS::proton_torpedo_apply_speed_base == 0) ||
            missile->type == WEAPON_DRAGON_BREATH) {
            max_dist = 10000;
            missile->travel_dist = 10000;
        }

        if (MOX::super_fast_flag == 0) {
            switch (missile->type) {
                case WEAPON_NUCLEAR_MISSILE:
                    sound_handle = ERICNET::Mox_Play_Sound(0x20);
                    sound::Pitch_Bend_Sound(sound_handle, 500);
                    break;
                case WEAPON_MERCULITE_MISSILE:
                    ERICNET::Mox_Play_Sound(0x20);
                    break;
                case WEAPON_PULSON_MISSILE:
                    sound_handle = ERICNET::Mox_Play_Sound(0x20);
                    sound::Pitch_Bend_Sound(sound_handle, -1500);
                    break;
                case WEAPON_ZEON_MISSILE:
                    sound_handle = ERICNET::Mox_Play_Sound(0x20);
                    sound::Pitch_Bend_Sound(sound_handle, -2500);
                    break;
                case WEAPON_ANTI_MATTER_TORP:
                    ERICNET::Mox_Play_Sound(0x3D);
                    break;
                case WEAPON_PROTON_TORPEDO:
                    ERICNET::Mox_Play_Sound(0x3C);
                    break;
                case WEAPON_PLASMA_TORPEDO:
                    ERICNET::Mox_Play_Sound(0x3B);
                    break;
                default:
                    ERICNET::Mox_Play_Sound(0x20);
                    break;
            }
        }

        dist_acc = 0;
        if (max_dist > 0) {
            do {
                if (hit_flag != 0) {
                    break;
                }

                hit_flag = (uint8_t)CMBTMIS::Move_Missile(missile_idx, tx, ty, step << 2);

                if (MOX::super_fast_flag == 0) {
                    video::Set_Page_Off();
                    CMBTDRW1::Draw_Main_Combat_Screen();
                    RUSS::Mox_Sync_Update();
                }

                if (is_targeting_missile == 1) {
                    if (MOX::missile[target_unit_idx].quantity <= 0) {
                        hit_flag = 0;
                        CMBTMIS::Retarget_Missile(missile_idx);
                    }
                } else {
                    int16_t current_target_unit_idx = MOX::missile[missile_idx].target_unit_idx;
                    if (MOX::combat_data[current_target_unit_idx].unit_status != 0) {
                        hit_flag = 0;
                        CMBTMIS::Retarget_Missile(missile_idx);
                    }
                }

                if (hit_flag != 0) {
                    CMBTMIS::Resolve_Missile(missile_idx, tx, ty);
                }

                dist_acc += step;
            } while (dist_acc < max_dist);
        }
    }

    void __cdecl Stellar_Converter(int16_t attacker_idx, int16_t target_idx, int16_t num_shots) {
        int16_t x1, y1, x2, y2;
        int16_t x_off, y_off;
        s_animation_header* beam_anim;
        s_animation_header* impact_anim;
        s_animation_header* temp_bitmap;
        int16_t frame_count;
        int16_t frame_step;
        uint32_t sound_handle;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            if (CMBTDRW1::Ship_Completely_On_Screen(target_idx) == 0) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[target_idx].x, MOX::combat_data[target_idx].y);
            }

            buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 41, MOX::global_cache_seg);
            beam_anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 40, MOX::global_cache_seg);
            animate::Draw_Palette(beam_anim);
            palette::Refresh_Palette();

            allocate::Push_Block(MOX::screen_seg);
            temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000);

            COMBINIT::Ship_Center_Offsets(attacker_idx, &x_off, &y_off);
            x1 = x_off + (MOX::combat_data[attacker_idx].x - MOX::cur_combat_x) * 20;
            y1 = y_off + (MOX::combat_data[attacker_idx].y - MOX::cur_combat_y) * 20;

            COMBINIT::Ship_Center_Offsets(target_idx, &x_off, &y_off);
            x2 = x_off + (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20;
            y2 = y_off + (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20;

            frame_count = animate::Get_Animation_Frame_Count(beam_anim);
            if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
                frame_step = 2;
            } else {
                frame_step = 1;
            }

            sound_handle = ERICNET::Mox_Play_Sound(20);

            for (int16_t frame_idx = 0; frame_idx < frame_count; frame_idx += frame_step) {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();

                beam_anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 40, MOX::global_cache_seg);
                animate::Set_Animation_Frame(beam_anim, (int32_t)frame_idx);
                bitmap::Draw_Picture_To_Bitmap(temp_bitmap, beam_anim);

                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, temp_bitmap);

                if (frame_idx > 8) {
                    impact_anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 41, MOX::global_cache_seg);
                    animate::Set_Animation_Frame(impact_anim, frame_idx - 9);
                    palette::Restore_Glass_Remap_Block(MOX::stellar_converter_remap_colors);
                    animate::Draw(x2 - 60, y2 - 60, impact_anim);
                }

                graphics::Reset_Window();
                video::Clip_Off();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(2);
            }

            sound::Fadeout_Sound(sound_handle, 10);
            allocate::Pop_Block(MOX::screen_seg);
        }

        int16_t total_damage = 0;
        int16_t total_struct_damage = 0;
        int16_t bonus = MOX::ordnance_bonus[MOX::combat_data[attacker_idx].owner];
        int16_t base_damage = TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].max_damage;
        int16_t shots_hit = num_shots;
        if (CMBTFIR2::combat_bonus.displacement_device_affects_stellar_converter != 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(target_idx, SPECIAL_DISPLACEMENT_DEVICE)) {
            for (int16_t shot = 0; shot < num_shots; ++shot) {
                if (game_random::Random(100) <=
                    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent) {
                    --shots_hit;
                }
            }
        }
        int16_t damage_per_hit = (bonus * shots_hit * base_damage) / 100;

        for (int16_t i = 0; i < 4 && shots_hit > 0; ++i) {
            int16_t current_damage = damage_per_hit;
            int16_t current_struct_damage = 0;
            int16_t has_analyzer = COMBINIT::Does_Combat_Ship_Have_Special(attacker_idx, 1);
            int16_t has_focus = COMBINIT::Does_Combat_Ship_Have_Special(attacker_idx, 0x20);

            CMBTFIRE::Apply_Ship_Damage(target_idx, &current_damage, &current_struct_damage, i, 0, 0, has_focus, has_analyzer, 0);
            total_damage += current_damage;
            total_struct_damage += current_struct_damage;
        }

        tactical_view::Record_Unit_Hit(target_idx, total_damage, total_struct_damage);
        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
            BEAMS::Add_Damage_Indicator_For_Ship(
                (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20,
                (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20,
                target_idx,
                1,
                total_damage,
                total_struct_damage);
            BEAMS::Draw_Damage_Message_Queue_Until_Done();
        }
    }

    void __cdecl Plasma_Web(int16_t attacker_idx, int16_t target_idx, int16_t weapon_slot_idx) {
        int16_t accumulated_damage = 0;
        s_combat_data* source_ship = &MOX::combat_data[attacker_idx];
        s_combat_data* target_ship = &MOX::combat_data[target_idx];
        int16_t shots_left = source_ship->weapons[weapon_slot_idx].shots_left;
        int16_t weapon_id = source_ship->weapons[weapon_slot_idx].weapon_id;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            if (!CMBTDRW1::Ship_Completely_On_Screen(target_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(target_ship->x, target_ship->y);
            }

            s_animation_header* travel_seg = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0xE /* Travel sprite */, MOX::global_cache_seg);
            int16_t travel_frames = animate::Get_Animation_Frame_Count(travel_seg);

            int16_t start_x, start_y;
            COMBINIT::Ship_Center_Offsets(attacker_idx, &start_x, &start_y);
            start_x += (source_ship->x - MOX::cur_combat_x) * 20 - 20;
            start_y += (source_ship->y - MOX::cur_combat_y) * 20 - 20;

            int16_t dest_x, dest_y;
            COMBINIT::Ship_Center_Offsets(target_idx, &dest_x, &dest_y);
            dest_x += (target_ship->x - MOX::cur_combat_x) * 20 - 20;
            dest_y += (target_ship->y - MOX::cur_combat_y) * 20 - 20;

            int32_t travel_sound = ERICNET::Mox_Play_Sound(0x1B);
            int16_t step = (MOX::auto_combat == 1) ? 24 : 6;

            int16_t current_x = start_x;
            int16_t current_y = start_y;
            int16_t travel_frame_idx = 0;

            while (current_x != dest_x || current_y != dest_y) {
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();

                s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0xE, MOX::global_cache_seg);
                animate::Set_Animation_Frame(anim, (int32_t)travel_frame_idx);
                animate::Draw(current_x, current_y, anim);
                video::Clip_Off();

                travel_frame_idx = (travel_frame_idx + 1) % travel_frames;
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();
                special::Absolute_Interpolate_Line(&current_x, &current_y, start_x, start_y, dest_x, dest_y, step);
            }

            sound::Fadeout_Sound(travel_sound, 10);

            s_animation_header* impact_seg = travel_seg;
            switch (target_ship->size_class) {
                case 0: impact_seg = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x24, MOX::global_cache_seg); break;
                case 1:
                case 2: impact_seg = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x25, MOX::global_cache_seg); break;
                case 3: impact_seg = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x26, MOX::global_cache_seg); break;
                case 4:
                case 5: impact_seg = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x27, MOX::global_cache_seg); break;
                default: break;
            }

            int16_t impact_frames = animate::Get_Animation_Frame_Count(impact_seg);
            int16_t impact_x = current_x + (20 - animate::Get_Width(impact_seg) / 2);
            int16_t impact_y = current_y + (20 - animate::Get_Height(impact_seg) / 2);

            int32_t hit_sound = ERICNET::Mox_Play_Sound(0x44);
            int16_t impact_frame_idx = 0;
            int16_t frame_inc = (MOX::auto_combat == 1) ? 2 : 1;

            while (impact_frame_idx < impact_frames) {
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();

                palette::Restore_Glass_Remap_Block(MOX::plasma_web_remap_colors);

                s_animation_header* anim = impact_seg;
                switch (target_ship->size_class) {
                    case 0: anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x24, MOX::global_cache_seg); break;
                    case 1:
                    case 2: anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x25, MOX::global_cache_seg); break;
                    case 3: anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x26, MOX::global_cache_seg); break;
                    case 4:
                    case 5: anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x27, MOX::global_cache_seg); break;
                    default: break;
                }

                animate::Set_Animation_Frame(anim, (int32_t)impact_frame_idx);
                animate::Draw(impact_x, impact_y, anim);

                palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
                video::Clip_Off();
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();

                impact_frame_idx += frame_inc;
            }

            sound::Fadeout_Sound(hit_sound, 10);
        }

        int16_t ord_bonus = MOX::ordnance_bonus[source_ship->owner];
        int32_t min_dmg = ((int32_t)TECHDATA::weapons[weapon_id].min_damage * ord_bonus) / 100;
        int32_t max_dmg = ((int32_t)TECHDATA::weapons[weapon_id].max_damage * ord_bonus) / 100;
        int32_t diff = max_dmg - min_dmg;

        if (shots_left > 0) {
            for (int16_t i = 0; i < shots_left; ++i) {
                accumulated_damage += (int16_t)(((int32_t)game_random::Random(100) * diff) / 100 + min_dmg);
            }
        }

        target_ship->plasma_web_damage += accumulated_damage;
    }

    void __cdecl Destroy_Ship_FX(int16_t ship_idx, int16_t x, int16_t y, int16_t explosion_type) {
        int16_t off_x = 0;
        int16_t off_y = 0;
        int16_t frame_count;
        int16_t step;
        int16_t i;
        int16_t old_dont_draw;
        s_animation_header* anim;
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        if (CMBTDRW1::Ship_Completely_On_Screen(ship_idx) == 0) {
            timer::Mark_Time();
            COMBINIT::Snap_Center_Combat_Screen(x / 20, y / 20);
            timer::Release_Time(7);
        }

        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        if (ship->previous_owner < 10) {
            if (explosion_type != 1) {
                int16_t damage = CMBTFIR2::Damage_From_Ship_Exploding(ship_idx);
                Do_Spherical_SFX(ship_idx, x, y, explosion_type, damage);
                ship->unit_status = 5 /* DESTROYED */;
                return;
            }

            old_dont_draw = MOX::dont_draw_ship;

            switch (ship->size_class) {
                case 0:
                    off_x -= 12;
                    off_y -= 9;
                    anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 3, MOX::global_cache_seg);
                    break;
                case 1:
                case 2:
                    off_x -= 23;
                    off_y -= 20;
                    anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 4, MOX::global_cache_seg);
                    break;
                case 3:
                case 4:
                case 5:
                    off_x -= 47;
                    off_y -= 44;
                    anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 5, MOX::global_cache_seg);
                    break;
            }

            frame_count = animate::Get_Animation_Frame_Count(anim);

            int16_t screen_off_x = (MOX::cur_combat_x * -20) + off_x + x;
            int16_t screen_off_y = (MOX::cur_combat_y * -20) + off_y + y;

            ship->plasma_web_damage = 0;
            MOX::dont_draw_ship = ship_idx;

            video::Set_Page_Back();
            CMBTDRW1::Full_Draw_Combat_Screen();
            video::Copy_Back_To_Off();

            if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
                step = 2;
            } else {
                step = 1;
            }

            Play_Ship_Explosion_SFX((int16_t)ship->size_class);
            if (leader_dies_with_no != 0) {
                Play_Leader_Death_SFX(ship_idx);
            }

            int16_t half_frame = frame_count / 2;

            for (i = 0; i < frame_count; i += step) {
                timer::Mark_Time();
                if (i >= half_frame) {
                    ship->unit_status = 5 /* DESTROYED */;
                }

                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();

                if (ship->unit_status == 0) {
                    CMBTDRW1::Draw_Ship(ship_idx, (MOX::cur_combat_x * -20) + x, (MOX::cur_combat_y * -20) + y);
                }

                switch (ship->size_class) {
                    case 0:
                        anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 3, MOX::global_cache_seg);
                        break;
                    case 1:
                    case 2:
                        anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 4, MOX::global_cache_seg);
                        break;
                    case 3:
                    case 4:
                    case 5:
                        anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 5, MOX::global_cache_seg);
                        break;
                }

                animate::Set_Animation_Frame(anim, i);
                animate::Draw(screen_off_x, screen_off_y, anim);

                video::Clip_Off();
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }

            MOX::dont_draw_ship = old_dont_draw;
            COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
            CMBTDRW1::Refresh_Combat_Screen_Full();
        } else {
            int16_t facing_dir = ship->facing_dir;
            CMBTDRW1::Get_Draw_Ship_Offsets(ship_idx, &off_x, &off_y);

            int16_t screen_off_x = (MOX::cur_combat_x * -20) + off_x + x;
            int16_t screen_off_y = (MOX::cur_combat_y * -20) + off_y + y;

            ship->plasma_web_damage = 0;
            ship->unit_status = 5 /* DESTROYED */;

            video::Set_Page_Back();
            CMBTDRW1::Full_Draw_Combat_Screen();

            if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
                step = 2;
            } else {
                step = 1;
            }

            uint8_t color_map[32];
            for (int k = 0; k < 32; ++k) {
                color_map[k] = (uint8_t)(k + 0x60);
            }

            int quadrant = (facing_dir - 1) / 4;
            if (quadrant < 0) quadrant = 0;

            for (i = 0; i < 16; i += step) {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();

                int entry_id;
                int16_t sfx_id;
                char* monster_lbx = ESTRINGS::E_Strings(0xE0);

                switch (ship->previous_owner) {
                    case 10:
                        entry_id = (ship->size_class == 2) ? 0x48 : 0x47;
                        sfx_id = 0x3F;
                        break;
                    case 11:
                        entry_id = (ship->size_class == 2) ? 0x4A : 0x49;
                        sfx_id = 0x42;
                        break;
                    case 12:
                        entry_id = (ship->size_class == 2) ? 0x46 : 0x45;
                        sfx_id = 0x41;
                        break;
                    case 13:
                        entry_id = (ship->size_class == 2) ? 0x4C : 0x4B;
                        sfx_id = 0x40;
                        break;
                    case 14:
                        entry_id = (ship->size_class == 2) ? 0x4E : 0x4D;
                        sfx_id = 0x3E;
                        break;
                    default:
                        entry_id = 0;
                        sfx_id = -1;
                        break;
                }

                anim = (s_animation_header*)buffer::Buffer_Reload(monster_lbx, entry_id, MOX::global_cache_seg);

                if (i == 0 && sfx_id != -1) {
                    ERICNET::Mox_Play_Sound(sfx_id);
                }

                switch (quadrant) {
                    case 0:
                        animate::Set_Animation_Frame(anim, (facing_dir << 4) + i);
                        bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, anim);
                        break;
                    case 1:
                        animate::Set_Animation_Frame(anim, ((8 - facing_dir) << 4) + i);
                        bitmap::Draw_Picture_To_Bitmap(MOX::ship_bitmap, anim);
                        bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 0);
                        break;
                    case 2:
                        animate::Set_Animation_Frame(anim, ((facing_dir - 8) << 4) + i);
                        bitmap::Draw_Picture_To_Bitmap(MOX::draw_ship_bitmap, anim);
                        bitmap::Flip_Bitmap(MOX::ship_bitmap, MOX::draw_ship_bitmap, 0);
                        bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 3);
                        break;
                    case 3:
                        animate::Set_Animation_Frame(anim, ((16 - facing_dir) << 4) + i);
                        bitmap::Draw_Picture_To_Bitmap(MOX::ship_bitmap, anim);
                        bitmap::Flip_Bitmap(MOX::draw_ship_bitmap, MOX::ship_bitmap, 3);
                        break;
                }

                if (ship->owner == MOX::combat_defender_player) {
                    replace::Replace_Color_Range(MOX::draw_ship_bitmap, color_map, 32, 32);
                }

                animate::Draw(screen_off_x, screen_off_y, MOX::draw_ship_bitmap);

                video::Clip_Off();
                graphics::Reset_Window();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }

            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();
            RUSS::Mox_Sync_Update();
            COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
            CMBTDRW1::Refresh_Combat_Screen_Full();
        }
    }

    void __cdecl Draw_Anti_Missile_Rocket_Picture(int16_t x, int16_t y) {
        int16_t x1 = x - 1;
        int16_t x2 = x + 1;
        int16_t y1 = y - 1;
        int16_t y2 = y + 1;

        // Fill background square
        graphics::Fill(x1, y1, x2, y2, 25);

        // Draw accented corners
        fields::Clipped_Dot(x1, y1, 16);
        fields::Clipped_Dot(x1, y2, 16);
        fields::Clipped_Dot(x2, y1, 16);
        fields::Clipped_Dot(x2, y2, 16);
    }

    void __cdecl Play_Ship_Explosion_SFX(int16_t size_class) {
        int32_t random_val = game_random::Random(3);
        int16_t switch_idx = (int16_t)(random_val + size_class - 1);

        switch (switch_idx) {
            case 0:
                ERICNET::Mox_Play_Sound(0x1d);
                break;
            case 1:
                ERICNET::Mox_Play_Sound(0x18);
                break;
            case 2:
                ERICNET::Mox_Play_Sound(0x18);
                break;
            case 3:
                ERICNET::Mox_Play_Sound(0x18);
                break;
            case 4:
                ERICNET::Mox_Play_Sound(0x18);
                break;
            case 5:
                ERICNET::Mox_Play_Sound(0x1e);
                ERICNET::Mox_Play_Sound(0x2b);
                break;
            default:
                ERICNET::Mox_Play_Sound(0x18);
                break;
        }
    }

    void __cdecl Draw_Gyro_Special_Effect(int16_t source_ship_idx, int16_t target_ship_idx) {
        int16_t x1, y1;
        int16_t x2, y2;
        int16_t step;

        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x44, MOX::global_cache_seg);
        animate::Draw_Palette(anim);

        anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 8, MOX::global_cache_seg);
        int16_t frame_count = animate::Get_Animation_Frame_Count(anim);

        allocate::Push_Block(MOX::screen_seg);
        s_animation_header* block = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000);

        COMBINIT::Ship_Center_Offsets(source_ship_idx, &x1, &y1);
        x1 += (MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20;
        y1 += (MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20;

        COMBINIT::Ship_Center_Offsets(target_ship_idx, &x2, &y2);
        x2 += (MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20;
        y2 += (MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20;

        if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
            step = 2;
        } else {
            step = 1;
        }

        int16_t frame = 0;
        int32_t sound_id = ERICNET::Mox_Play_Sound(11);

        if (frame_count > 0) {
            do {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();

                anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 8, MOX::global_cache_seg);
                animate::Set_Animation_Frame(anim, (int32_t)frame);
                bitmap::Draw_Picture_To_Bitmap(block, anim);

                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();

                bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, block);

                CMBTDRW1::Draw_Ship(
                    source_ship_idx,
                    (int16_t)((MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20),
                    (int16_t)((MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20));

                CMBTDRW1::Draw_Ship(
                    target_ship_idx,
                    (int16_t)((MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20),
                    (int16_t)((MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20));

                graphics::Reset_Window();
                video::Clip_Off();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);

                frame += step;
            } while (frame < frame_count);
        }

        sound::Fadeout_Sound(sound_id, 10);

        frame = 0;
        sound_id = ERICNET::Mox_Play_Sound(11);

        do {
            timer::Mark_Time();
            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();

            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();

            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 8, MOX::global_cache_seg);
            animate::Set_Animation_Frame(anim, (int32_t)((frame % 3) + frame_count - 3));
            bitmap::Draw_Picture_To_Bitmap((s_animation_header*)block, anim);

            bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, block);

            int32_t next_dir = MOX::combat_data[target_ship_idx].facing_dir + 1;
            int32_t wrapped_dir = abs(next_dir) % 16;
            if (next_dir < 0) {
                wrapped_dir = -wrapped_dir;
            }
            MOX::combat_data[target_ship_idx].facing_dir = (int8_t)wrapped_dir;

            CMBTDRW1::Draw_Ship(
                target_ship_idx,
                (int16_t)((MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20),
                (int16_t)((MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20));

            CMBTDRW1::Draw_Ship(
                source_ship_idx,
                (int16_t)((MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20),
                (int16_t)((MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20));

            video::Clip_Off();
            graphics::Reset_Window();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(2);

            frame += step;
        } while (frame < 8);

        allocate::Pop_Block(MOX::screen_seg);
        sound::Fadeout_Sound(sound_id, 10);
    }

    void __cdecl Draw_Ship_Collapsing(int16_t ship_idx, int16_t x, int16_t y, int16_t scale, int8_t rotation_offset) {
        int16_t center_off_x;
        int16_t center_off_y;

        if (scale > 99) {
            scale = 99;
        }

        if (scale < 2) {
            // Mark ship status as destroyed/finished collapsing
            MOX::combat_data[ship_idx].unit_status = 5;
            return;
        }

        // 0x62 = 98 pixels
        bitmap::Create_Blank_Bitmap(98, 98, MOX::combat_bitmap);
        bitmap::Create_Blank_Bitmap(98, 98, MOX::draw_ship_bitmap);

        // Temporarily rotate the ship for this draw call
        MOX::combat_data[ship_idx].facing_dir += rotation_offset;
        CMBTDRW1::Draw_Ship_To_Bitmap(ship_idx, 0, 0, 0);
        MOX::combat_data[ship_idx].facing_dir -= rotation_offset;

        // Scale the source ship bitmap into the combat buffer
        bitmap::Scale_Bitmap(MOX::combat_bitmap, MOX::draw_ship_bitmap, scale, scale);

        // Calculate frame offsets based on ship size/picture and draw to screen
        COMBINIT::Ship_Center_Offsets(ship_idx, &center_off_x, &center_off_y);
        ERIC::Draw_Centered(center_off_x + x, center_off_y + y, MOX::combat_bitmap);
    }

    void __cdecl Do_Spherical_SFX(int16_t source_ship_idx, int16_t effect_x, int16_t effect_y, int16_t effect_type, int16_t shot_count) {
        static const int16_t anim_frames[125] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xd, 0xd, 0x16, 0x2e, 0x3d, 0x3d, 0x56, 0x5f, 0x6b, 0x72, 0x76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xd, 0x10, 0x19, 0x1e, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0, 0, 0xb, 0x15, 0x2d, 0x40, 0x41, 0x55, 0x61, 0x6c, 0x73, 0x73, 0, 0, 0};

        int16_t center_dx_2;
        int16_t center_dy_2;

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0 && CMBTDRW1::Ship_Completely_On_Screen(source_ship_idx) == 0) {
            COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[source_ship_idx].x, MOX::combat_data[source_ship_idx].y);
        }

        int16_t center_x, center_y;
        COMBINIT::Ship_Center_Offsets(source_ship_idx, &center_x, &center_y);
        center_x = center_x + MOX::cur_combat_x * -20 + effect_x;
        center_y = center_y + MOX::cur_combat_y * -20 + effect_y;

        allocate::Push_Block(MOX::screen_seg);
        COLONY::Load_Ship_Bitm();

        int16_t sfx_type = effect_type;
        const CMBTFIRE::SPHERICAL_EFFECT spherical_effect =
            Spherical_Effect_From_Type(sfx_type);
        bool draw_ships_collapsing_flag = true;
        void* remap_colors = nullptr;

        switch (sfx_type) {
            case 0:
            case 1:
                draw_ships_collapsing_flag = true;
                remap_colors = MOX::pulsar_remap_colors;
                ERICNET::Mox_Play_Sound(0x14);
                break;
            case 2:
                draw_ships_collapsing_flag = true;
                ERICNET::Mox_Play_Sound(0x14);
                break;
            case 3:
                remap_colors = MOX::warp_core_remap_colors;
                draw_ships_collapsing_flag = false;
                ERICNET::Mox_Play_Sound(0x2F);
                ERICNET::Mox_Play_Sound(8);
                Play_Leader_Death_SFX(source_ship_idx);
                break;
            case 4:
                remap_colors = MOX::warp_core_remap_colors;
                draw_ships_collapsing_flag = false;
                ERICNET::Mox_Play_Sound(0x19);
                ERICNET::Mox_Play_Sound(8);
                if (leader_dies_with_no != 0) {
                    Play_Leader_Death_SFX(source_ship_idx);
                }
                break;
        }

        s_animation_header* anim = nullptr;
        switch (sfx_type) {
            case 0:
            case 1:
                anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x34, MOX::global_cache_seg);
                break;
            case 2:
                anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 6, MOX::global_cache_seg);
                break;
            case 3:
                anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::SPHERSFX_LBX, 0, MOX::global_cache_seg);
                break;
            case 4:
                anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::SPHERSFX_LBX, 1, MOX::global_cache_seg);
                break;
        }

        int16_t width = animate::Get_Width(anim);
        int16_t frame_count = animate::Get_Animation_Frame_Count(anim);
        int32_t radius_sq = (width / 2) * (width / 2);

        int16_t ship_indices[MAX_COMBAT_SHIPS];
        int16_t ship_dist_ratios[MAX_COMBAT_SHIPS];
        int32_t ship_dist_sq[MAX_COMBAT_SHIPS];
        int32_t ships_in_range_count = 0;

        if (MOX::combat_total_ship_count > 1) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].unit_status == 0 &&
                    CMBTMOV1::Ship_Is_In_Stasis(i) == 0 &&
                    i != source_ship_idx &&
                    MOX::combat_data[i].special_status_flag != 4) {
                    int16_t cx, cy;
                    CMBTDRW1::Get_Ship_Center_Draw_XY(i, &cx, &cy);
                    int32_t dx = (int32_t)cx - center_x;
                    int32_t dy = (int32_t)cy - center_y;
                    int32_t dist_sq = dx * dx + dy * dy;

                    if (CMBTFIRE::Spherical_Effect_In_Range(
                            source_ship_idx, i, TARGET_TYPE_SHIP, spherical_effect)) {
                        int32_t abs_dy = dy;
                        if (abs_dy < 0) abs_dy = -abs_dy;
                        int32_t abs_dx = dx;
                        if (abs_dx < 0) abs_dx = -abs_dx;

                        int32_t max_d = abs_dy;
                        if (abs_dx > abs_dy) {
                            max_d = abs_dx;
                        }

                        ship_indices[ships_in_range_count] = i;
                        ship_dist_sq[ships_in_range_count] = dist_sq;
                        ship_dist_ratios[ships_in_range_count] = (int16_t)(max_d / 20);
                        ships_in_range_count++;
                    }
                }
            }
        }

        COMBINIT::Ship_Center_Offsets(source_ship_idx, &center_dx_2, &center_dy_2);

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            MOX::dont_draw_ship = source_ship_idx;
            video::Set_Page_Back();
            CMBTDRW1::Draw_Main_Combat_Screen();
            video::Copy_Back_To_Off();

            int16_t step = 1;
            if (MOX::net_flag != 0 || MOX::speedx2_flag == 1) {
                step = 2;
            }

            int16_t ship_collapse_3_val = 100;
            int16_t ship_collapse_4_val = 132;

            if (frame_count >= 0) {
                for (int16_t frame = 0; frame <= frame_count; frame += step) {
                    graphics::Reset_Window();
                    timer::Mark_Time();
                    video::Set_Page_Off();
                    graphics::Set_Window(0, 0, 639, 350);
                    video::Clip_On();

                    if (!draw_ships_collapsing_flag && MOX::combat_data[source_ship_idx].unit_status != 5) {
                        if (sfx_type == 1 || sfx_type == 0) {
                            CMBTDRW1::Draw_Ship(source_ship_idx, center_x - center_dx_2, center_y - center_dy_2);
                        } else if (sfx_type == 3) {
                            Draw_Ship_Collapsing(source_ship_idx, center_x - center_dx_2, center_y - center_dy_2, ship_collapse_3_val, (int8_t)frame);
                        } else if (sfx_type == 4) {
                            Draw_Ship_Collapsing(source_ship_idx, center_x - center_dx_2, center_y - center_dy_2, ship_collapse_4_val, (int8_t)frame);
                        }
                    }

                    int32_t frame_offset = sfx_type * 25;
                    int16_t frame_val = anim_frames[frame_offset + frame];
                    int32_t frame_val_sq = (int32_t)frame_val * frame_val;

                    for (int32_t j = 0; j < ships_in_range_count; ++j) {
                        bool draw_ship = true;
                        if ((sfx_type == 0 || sfx_type == 1 || sfx_type == 2) &&
                            ((frame < 5 && ship_dist_ratios[j] == 1) ||
                                (frame > 0 && frame < 7 && ship_dist_ratios[j] == 2) ||
                                (frame > 4 && frame < 13 && ship_dist_ratios[j] == 3) ||
                                (frame > 8 && ship_dist_ratios[j] == 4))) {
                            int16_t ship_idx = ship_indices[j];
                            int16_t cx = MOX::combat_data[ship_idx].x - MOX::cur_combat_x;
                            cx = cx * 20 - 3 + (int16_t)game_random::Random(5);
                            int16_t cy = MOX::combat_data[ship_idx].y - MOX::cur_combat_y;
                            cy = cy * 20 - 3 + (int16_t)game_random::Random(5);
                            CMBTDRW1::Draw_Ship(ship_idx, cx, cy);
                            draw_ship = false;
                        } else if (sfx_type != 0 && sfx_type != 1) {
                            int32_t dist_sq = ship_dist_sq[j];
                            int32_t size_bonus = (int32_t)MOX::combat_data[ship_indices[j]].size_class * 25;
                            int32_t max_dist = frame_val_sq + size_bonus;

                            if (dist_sq <= max_dist) {
                                if (frame_val < 85 || (frame_val >= 85 && dist_sq <= frame_val_sq - 625)) {
                                    int16_t ship_idx = ship_indices[j];
                                    int16_t cx = MOX::combat_data[ship_idx].x - MOX::cur_combat_x;
                                    cx = cx * 20 - 3 + (int16_t)game_random::Random(5);
                                    int16_t cy = MOX::combat_data[ship_idx].y - MOX::cur_combat_y;
                                    cy = cy * 20 - 3 + (int16_t)game_random::Random(5);
                                    CMBTDRW1::Draw_Ship(ship_idx, cx, cy);
                                    draw_ship = false;
                                }
                            }
                        }

                        if (draw_ship) {
                            int16_t ship_idx = ship_indices[j];
                            int16_t cx = (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20;
                            int16_t cy = (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20;
                            CMBTDRW1::Draw_Ship(ship_idx, cx, cy);
                        }
                    }

                    if (sfx_type != 2) {
                        palette::Restore_Glass_Remap_Block(remap_colors);
                    }

                    switch (sfx_type) {
                        case 0:
                        case 1:
                            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 0x34, MOX::global_cache_seg);
                            break;
                        case 2:
                            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::CMBTSFX_LBX, 6, MOX::global_cache_seg);
                            break;
                        case 3:
                            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::SPHERSFX_LBX, 0, MOX::global_cache_seg);
                            break;
                        case 4:
                            anim = (s_animation_header*)buffer::Buffer_Reload(COLONY::SPHERSFX_LBX, 1, MOX::global_cache_seg);
                            break;
                    }

                    animate::Set_Animation_Frame(anim, (int32_t)frame);
                    ERIC::Draw_Centered(center_x, center_y, anim);

                    if (sfx_type != 2) {
                        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
                    }

                    if (draw_ships_collapsing_flag && MOX::combat_data[source_ship_idx].unit_status != 5) {
                        if (sfx_type == 1 || sfx_type == 0 || sfx_type == 2) {
                            CMBTDRW1::Draw_Ship(source_ship_idx, center_x - center_dx_2, center_y - center_dy_2);
                        } else if (sfx_type == 3) {
                            Draw_Ship_Collapsing(source_ship_idx, center_x - center_dx_2, center_y - center_dy_2, ship_collapse_3_val, (int8_t)frame);
                        } else if (sfx_type == 4) {
                            Draw_Ship_Collapsing(source_ship_idx, center_x - center_dx_2, center_y - center_dy_2, ship_collapse_3_val + 40, (int8_t)frame);
                        }
                    }

                    video::Clip_Off();
                    graphics::Reset_Window();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(1);

                    ship_collapse_3_val -= step * 10;
                    ship_collapse_4_val -= step * 8;
                }
            }

            MOX::dont_draw_ship = -1;
        }

        for (int32_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (MOX::missile[i].type > 0 &&
                TECHDATA::weapons[MOX::missile[i].type].type != WEAPON_TYPE_TORPEDO &&
                (TECHDATA::weapons[MOX::missile[i].type].type != WEAPON_TYPE_FIGHTER || game_random::Random(2) != 1)) {
                int32_t mx = (int32_t)MOX::missile[i].x + (int32_t)MOX::cur_combat_x * -20;
                int32_t my = (int32_t)MOX::missile[i].y + (int32_t)MOX::cur_combat_y * -20;
                int32_t dx = mx - center_x;
                if (dx < 0) dx = -dx;
                int32_t dy = my - center_y;
                if (dy < 0) dy = -dy;
                int32_t dist_sq = dx * dx + dy * dy;

                if (CMBTFIRE::Spherical_Effect_In_Range(
                        source_ship_idx, (int16_t)i, TARGET_TYPE_MISSILE, spherical_effect)) {
                    int16_t damage = 0;
                    int16_t owner = CMBTMIS::Missile_Owner((int16_t)i);
                    int32_t kill_ratio = 0;

                    if (sfx_type == 0) {
                        for (int16_t k = 0; k < shot_count; ++k) {
                            damage += TECHDATA::weapons[WEAPON_PLASMA_FLUX].min_damage + (int16_t)game_random::Random(TECHDATA::weapons[WEAPON_PLASMA_FLUX].max_damage - TECHDATA::weapons[WEAPON_PLASMA_FLUX].min_damage + 1) - 1;
                        }
                        damage = (int16_t)((MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] * damage) / 100);
                    } else if (sfx_type == 1) {
                        for (int16_t k = 0; k < shot_count; ++k) {
                            damage += TECHDATA::weapons[WEAPON_PULSAR].min_damage + (int16_t)game_random::Random(TECHDATA::weapons[WEAPON_PULSAR].max_damage - TECHDATA::weapons[WEAPON_PULSAR].min_damage + 1) - 1;
                        }
                        damage = (int16_t)((MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] * damage) / 100);
                    } else if (sfx_type == 2) {
                        for (int16_t k = 0; k < shot_count; ++k) {
                            damage += TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].min_damage + (int16_t)game_random::Random(TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].max_damage - TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].min_damage + 1) - 1;
                        }
                        damage = (int16_t)((MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] * damage) / 100);
                    } else {
                        kill_ratio = (dist_sq * 100) / radius_sq;
                        damage = shot_count;
                    }

                    int32_t chance = ((100 - kill_ratio) * (int32_t)damage) / 100;
                    int16_t missile_hits = (int16_t)CMBTMIS::Get_Missile_Hits(
                        MOX::missile[i].type, owner, MOX::missile[i].specials);
                    int16_t qty = MOX::missile[i].quantity;

                    if (qty > 0) {
                        for (int16_t k = 0; k < qty; ++k) {
                            if ((int)game_random::Random(100) <= (int32_t)(chance * 25) / missile_hits) {
                                MOX::missile[i].quantity--;
                                tactical_view::Missile_Changed((int16_t)i);
                            }
                        }
                    }

                    tactical_view::Record_Missile_Hit((int16_t)i, qty - MOX::missile[i].quantity);
                    if (MOX::missile[i].quantity <= 0) {
                        CMBTMIS::Kill_Missile((int16_t)i);
                    }
                }
            }
        }

        for (int32_t j = 0; j < ships_in_range_count; ++j) {
            int16_t damage = 0;
            int32_t kill_ratio = 0;

            if (sfx_type == 0) {
                for (int16_t k = 0; k < shot_count; ++k) {
                    damage += TECHDATA::weapons[WEAPON_PLASMA_FLUX].min_damage + (int16_t)game_random::Random(TECHDATA::weapons[WEAPON_PLASMA_FLUX].max_damage - TECHDATA::weapons[WEAPON_PLASMA_FLUX].min_damage + 1) - 1;
                }
                damage = (int16_t)((MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] * damage) / 100);
            } else if (sfx_type == 1) {
                for (int16_t k = 0; k < shot_count; ++k) {
                    damage += TECHDATA::weapons[WEAPON_PULSAR].min_damage + (int16_t)game_random::Random(TECHDATA::weapons[WEAPON_PULSAR].max_damage - TECHDATA::weapons[WEAPON_PULSAR].min_damage + 1) - 1;
                }
                damage = (int16_t)((MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] * damage) / 100);
            } else if (sfx_type == 2) {
                for (int16_t k = 0; k < shot_count; ++k) {
                    damage += TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].min_damage + (int16_t)game_random::Random(TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].max_damage - TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].min_damage + 1) - 1;
                }
                damage = (int16_t)((MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] * damage) / 100);
            } else {
                kill_ratio = (ship_dist_sq[j] * 100) / radius_sq;
                damage = shot_count;
            }

            int32_t chance = ((100 - kill_ratio) * (int32_t)damage) / 100;
            if (chance < 2) {
                chance = 1;
            }

            int32_t damage_out = 0;
            int16_t struct_damage = 0;
            int16_t ship_idx = ship_indices[j];

            if (MOX::combat_data[ship_idx].size_class != -1 && MOX::combat_data[ship_idx].size_class + 1 > 0) {
                for (int16_t k = 0; k < MOX::combat_data[ship_idx].size_class + 1; ++k) {
                    int32_t rnd = game_random::Random((int32_t)chance);
                    if (rnd <= 1) {
                        damage_out++;
                    } else {
                        damage_out += game_random::Random((int32_t)chance);
                    }
                }
            }

            if (sfx_type == 2) {
                if (compressor_per_size_class_damage == 0) {
                    damage_out = chance;
                }

                s_combat_data& target = MOX::combat_data[ship_idx];
                int32_t stored_damage = damage_out;
                const int32_t remaining_structure =
                    (int32_t)target.structure_max - target.structure_damage;
                if (stored_damage > remaining_structure) {
                    stored_damage = remaining_structure;
                }
                target.structure_damage =
                    (int16_t)((int32_t)target.structure_damage + stored_damage);
                struct_damage = 0;
            } else {
                int16_t applied_damage = (int16_t)damage_out;
                int16_t facing = CMBTDRW1::Relative_Bearing(ship_idx, source_ship_idx);
                facing = CMBTFIRE::Facing_Shield(facing);
                CMBTFIRE::Apply_Ship_Damage(ship_idx, &applied_damage, &struct_damage, facing, 0, 0, 0, 0, 0);
                damage_out = applied_damage;
            }

            tactical_view::Record_Unit_Hit(ship_idx, (int16_t)damage_out, struct_damage);
            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                BEAMS::Add_Damage_Indicator_For_Ship(
                    (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20,
                    (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20,
                    ship_idx,
                    1,
                    (int16_t)damage_out,
                    struct_damage);
            }
        }

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            BEAMS::Draw_Damage_Message_Queue_Until_Done();
        }

        for (int32_t j = 0; j < ships_in_range_count; ++j) {
            int16_t ship_idx = ship_indices[j];
            if (MOX::combat_data[ship_idx].unit_status == 0) {
                int16_t dead = CMBTFIRE::Is_Combat_Ship_Dead(ship_idx);
                if (dead != 0) {
                    CMBTFIRE::Destroy_Ship(ship_idx, (int16_t)MOX::combat_data[ship_idx].x * 20, (int16_t)MOX::combat_data[ship_idx].y * 20, dead);
                }
            }
        }

        allocate::Pop_Block(MOX::screen_seg);

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            CMBTDRW1::Refresh_Combat_Screen_Full();
        }
    }
} // namespace CMBTSPEC