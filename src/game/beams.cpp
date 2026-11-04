#include "pch.h"
#include "tactical_view.h"

namespace BEAMS {
    uint8_t g_pierced_shield_flag;
    int16_t old_color[2] = {-1, -1};
    int16_t g_weapon_n;
    int16_t g_sfx_n;
    s_animation_header* mauler_ball_seg;
    s_animation_header* l_anims;
    uint8_t shield_hit_now_flag;
    uint8_t reflection_field_hit_now_flag;
    uint8_t ship_hit_now_flag[2];
    uint8_t ship_firing_now_flag[2];
    int32_t g_beam_fire_sfx_handle[10];
    int32_t g_shield_sfx_handle;
    int32_t g_reflection_field_sfx_handle;
    int16_t max_frames;
    int16_t beam_angle = 15;
    uint8_t shield_diameters[8] = {25, 46, 63, 76, 117, 155, 193, 215};
    int16_t shield_hit_draw_facing;
    int16_t reflection_field_hit_draw_facing;
    int16_t shield_display_status;
    uint8_t reflection_field_diameters[4] = {29, 52, 71, 86};
    int16_t reflection_field_display_status;
    int16_t g_reflection_state_flag;
    uint8_t g_firing_reflected_beam_flag;
    int32_t low_ramp_values[20] = {
        7, 25, 35, 45, 55, 70, 89, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t low_primary_cross_values[20] = {
        80, 60, 40, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t low_saturated_values[20] = {
        110, 125, 155, 250, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t moderate_primary_cross_values[20] = {
        90, 80, 60, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t moderate_saturated_values[20] = {
        115, 140, 200, 250, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t high_primary_cross_values[20] = {
        95, 85, 70, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t high_saturated_values[20] = {
        150, 200, 300, 300, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int16_t n_damage_msgs;
    s_beam_dmg_msg msg_queue[35];
    s_animation_header* laser_burst_anim[2][2][16];
    s_animation_header* explosion_anim[2][16];
    s_animation_header* shield_anims[5][4][2];

    static const s_colors* Build_Damage_Indicator_Color_Window(s_colors* dst, const uint8_t* src) {
        memcpy(dst->colors, src, sizeof(dst->colors));
        return dst;
    }

    static void Set_Weapon_Visual_FX_Colors(s_weapon_visual_fx* fx, int32_t r1, int32_t g1, int32_t b1, int32_t r2, int32_t g2, int32_t b2) {
        fx->r1 = r1;
        fx->g1 = g1;
        fx->b1 = b1;
        fx->r2 = r2;
        fx->g2 = g2;
        fx->b2 = b2;
    }

    void __cdecl Reset_Beam_Swapping_Colors() {
        BEAMS::old_color[0] = -1;
        BEAMS::old_color[1] = -1;
    }

    /** Initialize beam animation data and reset cached beam palette state. */
    void __cdecl Init_Beams() {
        l_anims = (s_animation_header*)farload::Far_Reload_Next(COLONY::BEAMS_LBX, 0, MOX::screen_seg);
        Reset_Beam_Swapping_Colors();
    }

    /**
     * Load the reflection-field animation for the given rotation and size.
     * @param rotation_idx Reflection-field rotation variant.
     * @param size_idx Reflection-field size variant.
     */
    s_animation_header* __cdecl Reflection_Field_Anims(int16_t rotation_idx, int16_t size_idx) {
        int16_t entry_num = (int16_t)(size_idx * 5 + rotation_idx + 0x59);

        return (s_animation_header*)buffer::Buffer_Reload(
            COLONY::BEAMS_LBX,
            (int32_t)entry_num,
            MOX::global_cache_seg);
    }

    void __cdecl Load_Reflection_Field_Hit(int8_t should_load) {
        if (should_load != 0) {
            s_animation_header* anim = BEAMS::Reflection_Field_Anims(0, 0);
            animate::Draw_Palette(anim);
            palette::Refresh_Palette();
        }
    }

    void __cdecl Load_Ship_Burst() {
        int entry_idx;

        for (int i = 0; i < 2; ++i) {
            if (i == 0) {
                entry_idx = 1;
            } else {
                entry_idx = 33;
            }

            for (int j = 0; j < 2; ++j) {
                for (int k = 0; k < 16; ++k) {
                    laser_burst_anim[i][j][k] = (s_animation_header*)farload::Far_Reload_Next(
                        COLONY::BEAMS_LBX,
                        entry_idx,
                        MOX::screen_seg);
                    entry_idx++;
                }
            }
        }
    }

    void __cdecl Load_Combat_Beam_Stuff(int8_t reflection_hit_param) {
        Init_Beams();
        FIREPTS::Load_FP_Data_From_LBX();
        Load_Ship_Burst();
        Load_Ship_Explosion();
        Load_Shield_Hit();
        Load_Reflection_Field_Hit(reflection_hit_param);

        mauler_ball_seg = (s_animation_header*)(farload::Far_Reload_Next(COLONY::BEAMS_LBX, 0x58, MOX::screen_seg));
    }

    /** Preload the ship explosion animations. */
    void __cdecl Load_Ship_Explosion() {
        for (int i = 0; i < 2; ++i) {
            int entry_index = i + 0x41;
            for (int j = 0; j < 16; ++j) {
                explosion_anim[i][j] = (s_animation_header*)farload::Far_Reload_Next(
                    COLONY::BEAMS_LBX,
                    entry_index,
                    MOX::screen_seg);
            }
        }
    }

    /** Draw the requested ship explosion animation frame at the given position. */
    void __cdecl Draw_Ship_Explosion(int16_t x, int16_t y, int16_t type_idx, int16_t frame_idx, int16_t size_idx) {
        s_animation_header* anim = explosion_anim[size_idx][type_idx];
        int16_t count1 = animate::Get_Animation_Frame_Count(anim);
        animate::Set_Animation_Frame(anim, frame_idx % (count1 - 1));

        anim = explosion_anim[size_idx][type_idx];
        int16_t count2 = animate::Get_Animation_Frame_Count(anim);
        animate::Set_Animation_Frame(anim, frame_idx % (count2 - 1));

        animate::Draw(x - 8, y - 8, anim);
    }

    /**
     * @brief Retrieves or reloads the shield animation header based on ship size and flags.
     * @param beam_idx The type index of the beam or shield visual.
     * @param size_idx The ship size index (e.g., 0=Small, 1=Medium, etc.).
     * @param is_special Flag indicating a special or modified state.
     * @param is_heavy Flag indicating a heavy variant of the shield.
     * @return A pointer to the s_animation_header for the requested animation.
     */
    s_animation_header* __cdecl Shield_Anims(int16_t beam_idx, int16_t size_idx, int16_t is_special, int16_t is_heavy) {
        if (size_idx >= 4) {
            // For large sizes, reload from LBX using a specific offset
            return (s_animation_header*)buffer::Buffer_Reload(COLONY::BEAMS_LBX, size_idx * 5 + beam_idx + 113 /* 0x71 */, MOX::global_cache_seg);
        }

        if (is_heavy != 0) {
            // Heavy variant offset
            return (s_animation_header*)buffer::Buffer_Reload(COLONY::BEAMS_LBX, size_idx + 129 /* 0x81 */, MOX::global_cache_seg);
        }

        if (size_idx < 4 && is_special == 0) {
            // Return from the pre-cached shield animations
            return BEAMS::shield_anims[beam_idx][size_idx][0];
        }

        // Default reload path
        return (s_animation_header*)buffer::Buffer_Reload(COLONY::BEAMS_LBX, size_idx * 5 + beam_idx + 109 /* 0x6D */, MOX::global_cache_seg);
    }

    void __cdecl Load_Shield_Hit() {
        int8_t palettes[16];
        int entry_index = 0x43; /* 67 */

        for (int j = 0; j < 4; ++j) {
            for (int i = 0; i < 5; ++i) {
                // Load hit animations from BEAMS.LBX starting at entry 67.
                // The animations are stored in a 5x4 grid (shield type x direction).
                shield_anims[i][j][0] = (s_animation_header*)farload::Far_Reload_Next(
                    COLONY::BEAMS_LBX,
                    entry_index,
                    MOX::screen_seg);
                entry_index++;
            }
        }

        // Generate 16 brightness levels for shield impact effects.
        for (int i = 0; i < 16; ++i) {
            palettes[i] = (int8_t)(i * 4 + 30);
        }

        bitmap::Create_Brighten_Bitmap_Palettes(MOX::screen_seg, palettes, 16);
    }

    /**
     * Remove a damage-message entry by shifting later entries down.
     * @param index Queue slot to remove.
     */
    void __cdecl Squish_Out_Msg_Queue_Slot(int16_t index) {
        // BUG: removing an entry from the queue while iterating it in Draw_Damage_Message_Queue
        // will cause the next entry to be skipped.
        for (int i = (int)index; i < 34; ++i) {
            msg_queue[i] = msg_queue[i + 1];
        }
        n_damage_msgs--;
    }

    /**
     * Enqueue a floating combat damage message when any damage component is nonzero.
     * @param x Message x-position.
     * @param y Message y-position.
     */
    void __cdecl Add_Damage_Message_To_Queue(int16_t x, int16_t y, int16_t total_dmg, int16_t struct_dmg, int16_t shield_dmg, uint8_t side) {
        if (BEAMS::n_damage_msgs < 35 /* 0x23 */) {
            if (total_dmg > 0 || struct_dmg > 0 || shield_dmg > 0) {
                int16_t current_index = BEAMS::n_damage_msgs;
                s_beam_dmg_msg& msg = BEAMS::msg_queue[current_index];

                msg.anim_timer = 0;
                msg.x = x;
                msg.y = y;
                msg.total_dmg = total_dmg;
                msg.hull_dmg = struct_dmg;
                msg.shield_dmg = shield_dmg;
                msg.side = side;

                BEAMS::n_damage_msgs++;
            }
        }
    }

    void __cdecl Add_Damage_Indicator_For_Ship(int16_t dx, int16_t dy, int16_t ship_idx, uint8_t side, int16_t total_damage, int16_t struct_damage) {
        int16_t x_center;
        int16_t y_center;
        int16_t size_offset;

        // Size class: 0=Tiny, 1=Small, 2=Medium, 3=Large, 4=Huge, 5=Titan/Starbase
        size_offset = (int16_t)MOX::combat_data[ship_idx].size_class * 8;

        COMBINIT::Ship_Center_Offsets(ship_idx, &x_center, &y_center);

        x_center += dx;
        y_center += dy;

        if (side != 0) {
            x_center += size_offset;
        } else {
            x_center -= size_offset;
        }

        y_center -= size_offset;

        BEAMS::Add_Damage_Message_To_Queue(x_center, y_center, total_damage, struct_damage, 0, side);
    }

    void __cdecl Draw_Shield_Hit_Once_Through(int16_t ship_idx, int16_t hit_rotation, int16_t draw_x, int16_t draw_y) {
        int8_t size_class = MOX::combat_data[ship_idx].size_class;
        int16_t shield_size = Ship_Size_To_Shield_Size(size_class);

        int16_t shield_status = 0;
        int16_t offset_x;
        int16_t offset_y;

        COMBINIT::Ship_Center_Offsets(ship_idx, &offset_x, &offset_y);

        int16_t shield_size2 = Ship_Size_To_Shield_Size(size_class);
        int16_t shield_radius = shield_diameters[shield_size2] / 2;

        int16_t start_x = draw_x - shield_radius;
        int16_t start_y = draw_y - shield_radius;

        s_animation_header* anim = Shield_Anims(0, shield_size, 0, 0);
        animate::Get_Animation_Frame_Count(anim);

        shield_hit_now_flag = 1;

        do {
            do {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 0x27F, 0x15E);
                video::Clip_On();

                if (ship_idx == MOX::dont_draw_ship) {
                    CMBTDRW1::Draw_Ship(ship_idx, draw_x - offset_x, draw_y - offset_y);
                }

                Adjust_Shield_Display_Status(&shield_status);
                Draw_Shield_Hit(start_x, start_y, ship_idx, 0, 0, hit_rotation, shield_status);
                Draw_Damage_Message_Queue();

                timer::Release_Time(1);
                RUSS::Mox_Sync_Update();
                video::Clip_Off();
                graphics::Reset_Window();

                shield_hit_now_flag = 0;
            } while (shield_display_status != 0);
        } while (Done_Drawing_Damage_Message_Queue() == 0);
    }

    /** Draw one floating damage indicator at its current animation offset. */
    void __cdecl Draw_Damage_Indicator(int16_t x, int16_t y, int16_t total_dmg, int16_t hull_dmg, int16_t shield_dmg, uint8_t side, int16_t anim_timer) {
        const uint8_t total_dmg_color_steps[27] = {
            0xb0, 0xb9, 0xba, 0xb0, 0xb8, 0xb9, 0xb0, 0xb7, 0xb8, 0xb0, 0xb6, 0xb7, 0xb0, 0xb5, 0xb6, 0xb0, 0xb4, 0xb5, 0xb0, 0xb3, 0xb4, 0xb0, 0xb2, 0xb3, 0xb0, 0xb1, 0xb2};

        const uint8_t shield_dmg_color_steps[27] = {
            0x50, 0x57, 0x57, 0x50, 0x56, 0x57, 0x50, 0x55, 0x56, 0x50, 0x54, 0x55, 0x50, 0x53, 0x54, 0x50, 0x52, 0x53, 0x50, 0x51, 0x52, 0x50, 0x50, 0x51, 0x50, 0x50, 0x50};

        const uint8_t hull_dmg_color_steps[27] = {
            0xc0, 0xcc, 0xc5, 0xc0, 0xcc, 0xc5, 0xc0, 0xcc, 0xc5, 0xc0, 0xc4, 0xcc, 0xc0, 0xcb, 0xc4, 0xc0, 0xc3, 0xcb, 0xc0, 0xc3, 0xcb, 0xc0, 0xca, 0xc3, 0xc0, 0xc2, 0xca};
        const uint8_t dmg_color_blob_template[0x58] = {
            0xb0, 0xb9, 0xba, 0xb0, 0xb8, 0xb9, 0xb0, 0xb7, 0xb8, 0xb0, 0xb6, 0xb7, 0xb0, 0xb5, 0xb6, 0xb0, 0xb4, 0xb5, 0xb0, 0xb3, 0xb4, 0xb0, 0xb2, 0xb3, 0xb0, 0xb1, 0xb2, 0x00, 0x50, 0x57, 0x57, 0x50, 0x56, 0x57, 0x50, 0x55, 0x56, 0x50, 0x54, 0x55, 0x50, 0x53, 0x54, 0x50, 0x52, 0x53, 0x50, 0x51, 0x52, 0x50, 0x50, 0x51, 0x50, 0x50, 0x50, 0x00, 0xc0, 0xcc, 0xc5, 0xc0, 0xcc, 0xc5, 0xc0, 0xcc, 0xc5, 0xc0, 0xc4, 0xcc, 0xc0, 0xcb, 0xc4, 0xc0, 0xc3, 0xcb, 0xc0, 0xc3, 0xcb, 0xc0, 0xca, 0xc3, 0xc0, 0xc2, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t dmg_color_blob[sizeof(dmg_color_blob_template)];
        s_colors font_colors;

        memcpy(dmg_color_blob, dmg_color_blob_template, sizeof(dmg_color_blob));

        y -= anim_timer;

        if (side != 1) {
            x -= anim_timer;
        } else {
            x += anim_timer;
        }

        if (anim_timer >= 9) {
            return;
        }

        graphics::Set_Window(0, 0, 0x27f, 0x15e);
        video::Clip_On();

        if (total_dmg > 0) {
            fonts::Set_Font_Style_Shadow_Down(1, Build_Damage_Indicator_Color_Window(&font_colors, &dmg_color_blob[anim_timer * 3]));
            fonts::Print_Integer(x, y, total_dmg);
        }

        if (shield_dmg > 0) {
            fonts::Set_Font_Style_Shadow_Down(1, Build_Damage_Indicator_Color_Window(&font_colors, &dmg_color_blob[0x1c + anim_timer * 3]));
            fonts::Print_Integer(x - 10, y, shield_dmg);
        }

        if (hull_dmg > 0) {
            fonts::Set_Font_Style_Shadow_Down(1, Build_Damage_Indicator_Color_Window(&font_colors, &dmg_color_blob[0x38 + anim_timer * 3]));
            fonts::Print_Integer(x - 20, y, hull_dmg);
        }

        video::Clip_Off();
    }

    /** Return whether all queued damage messages have finished animating. */
    int8_t __cdecl Done_Drawing_Damage_Message_Queue() {
        return n_damage_msgs == 0;
    }

    /** Advance and draw all active floating damage messages. */
    void __cdecl Draw_Damage_Message_Queue() {
        for (int16_t i = 0; i < BEAMS::n_damage_msgs; ++i) {
            s_beam_dmg_msg& msg = BEAMS::msg_queue[i];

            BEAMS::Draw_Damage_Indicator(
                msg.x,
                msg.y,
                msg.total_dmg,
                msg.hull_dmg,
                msg.shield_dmg,
                msg.side,
                msg.anim_timer);

            msg.anim_timer++;

            if (msg.anim_timer >= 9) {
                // Note: Possible bug in original code where incrementing i after removal
                // might skip the next message entry.
                BEAMS::Squish_Out_Msg_Queue_Slot(i);
            }
        }

        BEAMS::Done_Drawing_Damage_Message_Queue();
    }

    void __cdecl Adjust_Shield_Display_Status(int16_t* shield_status) {
        s_animation_header* anim_header = BEAMS::Shield_Anims(0, 0, 0, 0);
        int16_t total_frames = animate::Get_Animation_Frame_Count(anim_header);

        switch (BEAMS::shield_display_status) {
            case 0:
                if (BEAMS::shield_hit_now_flag != 0) {
                    BEAMS::shield_display_status = 1;
                    *shield_status = 0;
                }
                break;

            case 1:
                (*shield_status)++;
                if (*shield_status >= 4) {
                    BEAMS::shield_display_status = 2;
                }
                break;

            case 2:
                (*shield_status)++;
                if (BEAMS::shield_hit_now_flag != 0) {
                    if (*shield_status >= 7) {
                        *shield_status = 4;
                    }
                } else {
                    if (*shield_status >= 7) {
                        BEAMS::shield_display_status = 3;
                    }
                }
                break;

            case 3:
                (*shield_status)++;
                if (*shield_status >= total_frames) {
                    BEAMS::shield_display_status = 0;
                }
                break;

            default:
                break;
        }
    }

    /** Map a combat ship size to the shield animation size index. */
    int16_t __cdecl Ship_Size_To_Shield_Size(int16_t ship_size) {
        static const int16_t shield_sizes[SHIP_SIZE_DOOM_STAR + 1] = {
            0, // SHIP_SIZE_SMALL
            1, // SHIP_SIZE_MEDIUM
            1, // SHIP_SIZE_LARGE
            2, // SHIP_SIZE_HUGE
            3, // SHIP_SIZE_TITAN
            3  // SHIP_SIZE_DOOM_STAR
        };

        return shield_sizes[ship_size];
    }

    /** Keep redrawing combat until all damage popups expire. */
    void __cdecl Draw_Damage_Message_Queue_Until_Done() {
        do {
            timer::Mark_Time();
            video::Set_Page_Off();
            CMBTDRW1::Draw_Main_Combat_Screen();
            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();
            BEAMS::Draw_Damage_Message_Queue();
            timer::Release_Time(1);
            RUSS::Mox_Sync_Update();
            video::Clip_Off();
            graphics::Reset_Window();
        } while (!BEAMS::Done_Drawing_Damage_Message_Queue());
    }

    int16_t __cdecl Get_Max_Frames() {
        return BEAMS::max_frames;
    }

    /** Return the approximate tile-space distance between two combat points. */
    int16_t __cdecl Get_Ship_Range(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int16_t dx = x2 - x1;
        if (dx < 0) {
            dx = -dx;
        }

        int16_t dy = y2 - y1;
        if (dy < 0) {
            dy = -dy;
        }

        if (dx <= dy) {
            dx = dy;
        }

        return dx;
    }

    /** Draw the current shield-hit frame for the target ship or planet. */
    void __cdecl Draw_Shield_Hit(int16_t x, int16_t y, int16_t ship_idx, int16_t is_special, int16_t is_heavy, int16_t hit_rotation, int16_t frame_index) {
        int16_t base_rot = BEAMS::Rot_To_Base_Rot(hit_rotation);
        int16_t rot_method = BEAMS::Rot_To_Rot_Method(hit_rotation);

        if (BEAMS::shield_display_status == 0) {
            return;
        }

        int16_t shield_size_class;
        int16_t shield_size;

        if (ship_idx != 0) {
            shield_size_class = MOX::combat_data[ship_idx].size_class;
            shield_size = BEAMS::Ship_Size_To_Shield_Size(shield_size_class);
        } else {
            int16_t planet_idx = MOX::colony[MOX::cmbt_colony_id].planet;
            shield_size_class = MOX::combat_data[0].size_class + 6;
            shield_size = BEAMS::Planet_Size_To_Shield_Size(MOX::planet[planet_idx].size);
        }

        uint8_t diam = BEAMS::shield_diameters[shield_size];
        uint32_t buf_size = (uint32_t)diam * diam + 32;

        allocate::Push_Block(MOX::screen_seg);
        s_animation_header* sprite1 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, buf_size);
        bitmap::Create_Blank_Bitmap(diam, diam, sprite1);

        s_animation_header* sprite2 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, buf_size);
        bitmap::Create_Blank_Bitmap(diam, diam, sprite2);

        if (frame_index == 0 && BEAMS::g_shield_sfx_handle == 0) {
            if (BEAMS::g_reflection_state_flag == 0) {
                BEAMS::g_shield_sfx_handle = ERICNET::Mox_Play_Sound(38);
            } else {
                BEAMS::g_shield_sfx_handle = ERICNET::Mox_Play_Sound(42);
            }

            if (ship_idx != 0) {
                sound::Pitch_Bend_Sound(BEAMS::g_shield_sfx_handle, -(shield_size_class * 2500));
            } else {
                sound::Pitch_Bend_Sound(BEAMS::g_shield_sfx_handle, -15000);
            }
        } else {
            s_animation_header* anim = BEAMS::Shield_Anims(base_rot, shield_size, is_special, is_heavy);
            int16_t frame_count = animate::Get_Animation_Frame_Count(anim);

            if ((frame_count - 1 <= frame_index) || (BEAMS::g_shield_sfx_handle != 0 && BEAMS::shield_display_status == 0)) {
                sound::Fadeout_Sound(BEAMS::g_shield_sfx_handle, 10);
                BEAMS::g_shield_sfx_handle = 0;
            }
        }

        s_animation_header* anim = BEAMS::Shield_Anims(base_rot, shield_size, is_special, is_heavy);
        int16_t anim_frame_count = animate::Get_Animation_Frame_Count(anim);
        int16_t anim_frame = frame_index % anim_frame_count;

        anim = BEAMS::Shield_Anims(base_rot, shield_size, is_special, is_heavy);
        animate::Set_Animation_Frame(anim, anim_frame);

        anim = BEAMS::Shield_Anims(base_rot, shield_size, is_special, is_heavy);
        bitmap::Draw_Picture_To_Bitmap(sprite1, anim);

        s_animation_header* draw_src = sprite1;
        s_animation_header* draw_dst = sprite2;

        switch (rot_method) {
            case 0:
                bitmap::Set_Picture_Glassed(sprite1);
                draw_dst = sprite2;
                draw_src = sprite1;
                break;
            case 1:
                bitmap::Flip_Bitmap(sprite2, sprite1, 0);
                draw_dst = sprite1;
                draw_src = sprite2;
                break;
            case 2:
                bitmap::Flip_Bitmap(sprite2, sprite1, 3);
                bitmap::Flip_Bitmap(sprite1, sprite2, 0);
                draw_dst = sprite2;
                draw_src = sprite1;
                break;
            case 3:
                bitmap::Flip_Bitmap(sprite2, sprite1, 3);
                draw_dst = sprite1;
                draw_src = sprite2;
                break;
            default:
                break;
        }

        if (rot_method <= 3) {
            bitmap::Draw_Picture_To_Bitmap(draw_dst, draw_src);
        }

        if (ship_idx == 0) {
            int16_t planet_idx = MOX::colony[MOX::cmbt_colony_id].planet;
            int16_t current_shield_size = BEAMS::Planet_Size_To_Shield_Size(MOX::planet[planet_idx].size);
            uint8_t diam2 = BEAMS::shield_diameters[current_shield_size];

            int16_t c_cx, c_cy;
            COMBINIT::Ship_Center_Offsets(ship_idx, &c_cx, &c_cy);

            int16_t half_diam2 = diam2 / 2;
            int16_t x_offset = half_diam2 - c_cx;
            int16_t y_offset = half_diam2 - c_cy;

            int16_t s_cx, s_cy;
            CMBTDRW1::Get_Draw_Ship_Offsets(ship_idx, &s_cx, &s_cy);

            int16_t draw_x = x + x_offset + s_cx;
            int16_t draw_y = y + y_offset + s_cy;

            bitmap::Draw_Picture_To_Bitmap(sprite2, MOX::combat_planet_seg);

            int16_t actual_x, actual_y, actual_w, actual_h;
            bitmap::Get_Bitmap_Actual_Size(sprite2, &actual_x, &actual_y, &actual_w, &actual_h);

            bitmap::Mask_Brighten_Bitmap(
                (actual_w / 2) - half_diam2,
                (actual_h / 2) - half_diam2,
                sprite2,
                sprite1,
                -16);

            graphics::Set_Window(0, 0, 0x27F, 0x15E);
            video::Clip_On();

            animate::Draw(draw_x, draw_y, sprite2);
        } else {
            CMBTDRW1::Draw_Ship_To_Bitmap(ship_idx, 0, 0, 0);

            int16_t current_shield_size = BEAMS::Ship_Size_To_Shield_Size(MOX::combat_data[ship_idx].size_class);
            uint8_t diam2 = BEAMS::shield_diameters[current_shield_size];

            int16_t c_cx, c_cy;
            COMBINIT::Ship_Center_Offsets(ship_idx, &c_cx, &c_cy);

            int16_t half_diam2 = diam2 / 2;
            int16_t x_offset = half_diam2 - c_cx;
            int16_t y_offset = half_diam2 - c_cy;

            int16_t s_cx, s_cy;
            CMBTDRW1::Get_Draw_Ship_Offsets(ship_idx, &s_cx, &s_cy);

            int16_t draw_x = x + x_offset + s_cx;
            int16_t draw_y = y + y_offset + s_cy;

            int16_t actual_x, actual_y, actual_w, actual_h;
            bitmap::Get_Bitmap_Actual_Size(MOX::draw_ship_bitmap, &actual_x, &actual_y, &actual_w, &actual_h);

            bitmap::Mask_Brighten_Bitmap(
                (actual_w / 2) - half_diam2,
                (actual_h / 2) - half_diam2,
                MOX::draw_ship_bitmap,
                sprite1,
                -16);

            graphics::Set_Window(0, 0, 0x27F, 0x15E);
            video::Clip_On();

            animate::Draw(draw_x, draw_y, MOX::draw_ship_bitmap);
        }

        bitmap::Set_Picture_Glassed(sprite1);
        animate::Draw(x, y, sprite1);

        allocate::Pop_Block(MOX::screen_seg);
        fields::Get_Input();
    }

    /** Queue a damage popup next to a missile target. */
    void __cdecl Add_Damage_Indicator_For_Missile(int16_t x, int16_t y, uint8_t direction, int16_t damage) {
        int16_t adjusted_x;

        if (direction == 0) {
            adjusted_x = x - 8;
        } else {
            adjusted_x = x + 8;
        }

        int16_t adjusted_y = y - 8;

        BEAMS::Add_Damage_Message_To_Queue(adjusted_x, adjusted_y, damage, 0, 0, direction);
    }

    /** Queue a damage popup for a biological attack result. */
    void __cdecl Add_Damage_Indicator_For_Biological(int16_t x, int16_t y, uint8_t side_flag, int16_t damage) {
        int16_t target_x;

        if (side_flag != 0) {
            target_x = x + 8;
        } else {
            target_x = x - 8;
        }

        BEAMS::Add_Damage_Message_To_Queue(target_x, y - 8, 0, 0, damage, side_flag);
    }

    /** Draw fighter beam fire for the current frame and report whether more frames remain. */
    int8_t __cdecl Draw_Fighter_Beam(int16_t* fighter_coords, int16_t count, int16_t target_x, int16_t target_y, int16_t weapon_type, int16_t frame) {
        uint32_t original_seed = game_random::Get_Random_Seed();

        graphics::Set_Window(0, 0, 639, 350);
        video::Clip_On();

        if (count > 0) {
            for (int16_t i = 0; i < count; ++i) {
                int16_t fx = fighter_coords[i * 2];
                int16_t fy = fighter_coords[i * 2 + 1];

                int16_t q1 = angle::Get_Angle(fx - target_x, target_y - fy);
                q1 = (int16_t)(q1 / 22 + 8);
                int16_t dir1 = (q1 < 0) ? -(int16_t)((-q1) & 0xF) : (int16_t)(q1 & 0xF);

                int16_t q2 = angle::Get_Angle(target_x - fx, fy - target_y);
                q2 = (int16_t)(q2 / 22 + 8);
                int16_t dir2 = (q2 < 0) ? -(int16_t)((-q2) & 0xF) : (int16_t)(q2 & 0xF);

                if (BEAMS::Get_Max_Frames() < frame) {
                    return 0;
                }

                int16_t extra_flag = ((i % 3) == 0) ? 1 : 0;
                BEAMS::Fire_Weapon(fx, fy, target_x, target_y, dir1, dir2, weapon_type, frame, 0, 0, 0, 0, 1, 0, 0, -1, 0, extra_flag);
            }
        }

        video::Clip_Off();
        int16_t max_frames = BEAMS::Get_Max_Frames();
        game_random::Set_Random_Seed(original_seed);

        if (max_frames > frame) {
            return 1;
        }

        return 0;
    }

    /**
     * Convert a 16-step facing into the base shield animation rotation index.
     * @param rot Input facing value.
     */
    int16_t __cdecl Rot_To_Base_Rot(int16_t rot) {
        int16_t base_rotations[16] = {
            0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1};

        int16_t idx = (int16_t)(rot & 0xF);
        return base_rotations[idx];
    }

    /**
     * Convert a 16-step facing into the bitmap flip method used for that rotation.
     * @param rot Input facing value.
     */
    int16_t __cdecl Rot_To_Rot_Method(int16_t rot) {
        int16_t methods[16] = {
            0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3};

        int16_t idx = (int16_t)(rot & 0xF);
        return methods[idx];
    }

    /** Map a planet size to the shield animation size index. */
    int16_t __cdecl Planet_Size_To_Shield_Size(int16_t planet_size) {
        static const int16_t sizes[PLANET_SIZE_HUGE + 1] = {
            4, // PLANET_SIZE_TINY
            5, // PLANET_SIZE_SMALL
            6, // PLANET_SIZE_MEDIUM
            7, // PLANET_SIZE_LARGE
            7  // PLANET_SIZE_HUGE
        };

        return sizes[planet_size];
    }

    void __cdecl Fire_Weapon(int16_t attacker_x, int16_t attacker_y, int16_t target_x, int16_t target_y, int16_t attacker_facing, int16_t target_facing, int16_t weapon_id, int16_t current_frame, int16_t fx_slot_idx, int16_t target_size_class, int16_t hit_result_flags, int16_t special_flag, int16_t is_non_ship_target, int16_t fire_point_idx, int16_t reflection_state, int16_t attacker_size_class, uint16_t weapon_specials, int16_t unused_fighter_beam_flag) {
        (void)unused_fighter_beam_flag;

        int16_t duration_frames;
        int16_t palette_fragment_index;
        int16_t beam_renderer_mode;
        int16_t draw_mauler_ball_flag = 0;
        int16_t bolt_apply_sparkles_flag = 0;
        int16_t burst_variant = 0;
        int16_t loop_count;
        int16_t bolt_outer_min_offset;
        int16_t bolt_outer_max_offset;
        int16_t bolt_core_min_offset;
        int16_t bolt_suppress_side_strands_flag;
        int16_t reverse_draw_flag;
        int16_t bolt_draw_caps_flag;
        int16_t bolt_point_source_end_flag;
        int16_t bolt_sparkle_phase;
        int16_t bolt_color_jitter_range;
        int16_t bolt_color_jitter_chance;
        int16_t bolt_core_max_offset;
        int16_t bolt_full_core_gradient_flag;
        int16_t beam_fire_sfx_id;
        int16_t bolt_tip_highlight_flag;
        int16_t frame_step_distance;
        int16_t is_multi_beam = 0;

        switch (weapon_id) {
            case WEAPON_MASS_DRIVER:
                palette_fragment_index = 7;
                duration_frames = 1;
                burst_variant = 1;
                draw_mauler_ball_flag = 0;
                frame_step_distance = 0x2D;
                bolt_core_max_offset = 10;
                bolt_color_jitter_chance = 0;
                bolt_color_jitter_range = 0;
                bolt_sparkle_phase = 1;
                bolt_core_min_offset = 0;
                bolt_outer_max_offset = 7;
                bolt_outer_min_offset = 0;
                bolt_draw_caps_flag = 1;
                bolt_point_source_end_flag = 1;
                bolt_tip_highlight_flag = 1;
                beam_renderer_mode = 1;
                bolt_full_core_gradient_flag = 0;
                beam_fire_sfx_id = 12;
                bolt_apply_sparkles_flag = 0;
                bolt_suppress_side_strands_flag = 0;
                reverse_draw_flag = 0;
                is_multi_beam = 1;
                break;
            case WEAPON_GAUSS_CANNON:
                palette_fragment_index = 2;
                beam_renderer_mode = 1;
                bolt_color_jitter_chance = 0x28;
                bolt_color_jitter_range = 3;
                frame_step_distance = 0x2D;
                duration_frames = palette_fragment_index;
                draw_mauler_ball_flag = 0;
                bolt_core_max_offset = 10;
                bolt_outer_max_offset = 7;
                burst_variant = beam_renderer_mode;
                bolt_sparkle_phase = beam_renderer_mode;
                bolt_core_min_offset = draw_mauler_ball_flag;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = beam_renderer_mode;
                bolt_point_source_end_flag = draw_mauler_ball_flag;
                bolt_tip_highlight_flag = beam_renderer_mode;
                bolt_full_core_gradient_flag = beam_renderer_mode;
                beam_fire_sfx_id = 15;
                bolt_apply_sparkles_flag = draw_mauler_ball_flag;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                is_multi_beam = beam_renderer_mode;
                break;
            case WEAPON_LASER_CANNON:
                palette_fragment_index = 0;
                bolt_color_jitter_chance = 10;
                duration_frames = 0x5A;
                bolt_color_jitter_range = 1;
                frame_step_distance = 0x37;
                bolt_core_min_offset = 7;
                bolt_sparkle_phase = bolt_color_jitter_range;
                bolt_outer_max_offset = palette_fragment_index;
                bolt_outer_min_offset = palette_fragment_index;
                bolt_core_max_offset = bolt_color_jitter_chance;
                bolt_draw_caps_flag = palette_fragment_index;
                bolt_point_source_end_flag = palette_fragment_index;
                bolt_tip_highlight_flag = palette_fragment_index;
                draw_mauler_ball_flag = 0;
                beam_renderer_mode = 0;
                bolt_full_core_gradient_flag = palette_fragment_index;
                bolt_apply_sparkles_flag = bolt_color_jitter_range;
                bolt_suppress_side_strands_flag = palette_fragment_index;
                reverse_draw_flag = palette_fragment_index;
                beam_fire_sfx_id = bolt_color_jitter_range;
                break;
            case WEAPON_PARTICLE_BEAM:
                duration_frames = 0x3C;
                palette_fragment_index = 1;
                bolt_sparkle_phase = palette_fragment_index;
                bolt_color_jitter_range = 3;
                bolt_color_jitter_chance = 0x32;
                frame_step_distance = 0x2D;
                draw_mauler_ball_flag = 0;
                bolt_core_max_offset = 15;
                bolt_core_min_offset = 7;
                bolt_outer_max_offset = bolt_color_jitter_range;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = palette_fragment_index;
                bolt_point_source_end_flag = palette_fragment_index;
                bolt_tip_highlight_flag = palette_fragment_index;
                beam_renderer_mode = palette_fragment_index;
                beam_fire_sfx_id = 23;
                bolt_full_core_gradient_flag = palette_fragment_index;
                bolt_apply_sparkles_flag = draw_mauler_ball_flag;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_FUSION_BEAM:
                palette_fragment_index = 8;
                bolt_color_jitter_chance = 0x28;
                duration_frames = 0x280;
                draw_mauler_ball_flag = 0;
                bolt_color_jitter_range = 5;
                bolt_core_max_offset = 15;
                bolt_core_min_offset = 10;
                bolt_outer_max_offset = 7;
                frame_step_distance = bolt_color_jitter_chance;
                bolt_sparkle_phase = draw_mauler_ball_flag;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = 1;
                bolt_point_source_end_flag = 1;
                bolt_tip_highlight_flag = 1;
                beam_renderer_mode = 2;
                beam_fire_sfx_id = 9;
                bolt_full_core_gradient_flag = 1;
                bolt_apply_sparkles_flag = 1;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = 1;
                break;
            case WEAPON_ION_PULSE_CANNON:
                duration_frames = 0x28;
                palette_fragment_index = 3;
                bolt_color_jitter_range = palette_fragment_index;
                beam_renderer_mode = 1;
                bolt_color_jitter_chance = 0x32;
                frame_step_distance = 0x2D;
                bolt_sparkle_phase = beam_renderer_mode;
                draw_mauler_ball_flag = 0;
                bolt_core_max_offset = 15;
                bolt_core_min_offset = 7;
                bolt_outer_max_offset = palette_fragment_index;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = beam_renderer_mode;
                bolt_point_source_end_flag = beam_renderer_mode;
                bolt_tip_highlight_flag = beam_renderer_mode;
                beam_fire_sfx_id = 14;
                bolt_full_core_gradient_flag = beam_renderer_mode;
                bolt_apply_sparkles_flag = beam_renderer_mode;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_GRAVITON_BEAM:
                palette_fragment_index = 6;
                bolt_sparkle_phase = 1;
                bolt_color_jitter_chance = 0x1E;
                duration_frames = 0x280;
                bolt_core_max_offset = 7;
                bolt_color_jitter_range = 5;
                frame_step_distance = 0x23;
                draw_mauler_ball_flag = 0;
                bolt_outer_max_offset = 3;
                bolt_core_min_offset = bolt_core_max_offset;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = bolt_sparkle_phase;
                bolt_point_source_end_flag = draw_mauler_ball_flag;
                bolt_tip_highlight_flag = bolt_sparkle_phase;
                beam_renderer_mode = 2;
                beam_fire_sfx_id = 11;
                bolt_full_core_gradient_flag = draw_mauler_ball_flag;
                bolt_apply_sparkles_flag = draw_mauler_ball_flag;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_NEUTRON_BLASTER:
                palette_fragment_index = 2;
                bolt_color_jitter_range = 3;
                duration_frames = 0x5A;
                beam_renderer_mode = 1;
                bolt_color_jitter_chance = 0x1E;
                frame_step_distance = 0x2D;
                bolt_core_max_offset = 15;
                draw_mauler_ball_flag = 0;
                bolt_core_min_offset = 7;
                bolt_sparkle_phase = beam_renderer_mode;
                bolt_outer_max_offset = bolt_color_jitter_range;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = beam_renderer_mode;
                bolt_point_source_end_flag = beam_renderer_mode;
                bolt_tip_highlight_flag = beam_renderer_mode;
                beam_fire_sfx_id = 22;
                bolt_full_core_gradient_flag = beam_renderer_mode;
                bolt_apply_sparkles_flag = draw_mauler_ball_flag;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_PHASOR:
            case WEAPON_PHASOR_EYE:
                palette_fragment_index = 0;
                draw_mauler_ball_flag = 0;
                duration_frames = 0x5A;
                bolt_color_jitter_chance = 15;
                beam_renderer_mode = 1;
                bolt_color_jitter_range = 3;
                frame_step_distance = 0x32;
                bolt_core_min_offset = 7;
                bolt_outer_max_offset = 6;
                bolt_outer_min_offset = 2;
                bolt_sparkle_phase = beam_renderer_mode;
                bolt_core_max_offset = bolt_color_jitter_chance;
                bolt_draw_caps_flag = beam_renderer_mode;
                bolt_point_source_end_flag = draw_mauler_ball_flag;
                bolt_tip_highlight_flag = beam_renderer_mode;
                beam_fire_sfx_id = 19;
                bolt_full_core_gradient_flag = draw_mauler_ball_flag;
                bolt_apply_sparkles_flag = beam_renderer_mode;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_DISRUPTER:
                palette_fragment_index = 0;
                draw_mauler_ball_flag = 0;
                duration_frames = 0x28;
                bolt_color_jitter_range = 3;
                bolt_color_jitter_chance = duration_frames;
                beam_renderer_mode = 1;
                frame_step_distance = 0x2D;
                bolt_core_max_offset = 15;
                bolt_core_min_offset = 7;
                bolt_sparkle_phase = beam_renderer_mode;
                bolt_outer_max_offset = bolt_color_jitter_range;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = beam_renderer_mode;
                bolt_point_source_end_flag = beam_renderer_mode;
                bolt_tip_highlight_flag = beam_renderer_mode;
                beam_fire_sfx_id = 2;
                bolt_full_core_gradient_flag = beam_renderer_mode;
                bolt_apply_sparkles_flag = draw_mauler_ball_flag;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_DEATH_RAY:
            case WEAPON_CRYSTAL_RAY:
                palette_fragment_index = 6;
                bolt_color_jitter_range = 3;
                duration_frames = 0x5A;
                bolt_sparkle_phase = 1;
                bolt_color_jitter_chance = 0x28;
                frame_step_distance = 0x23;
                bolt_core_max_offset = 15;
                draw_mauler_ball_flag = 0;
                bolt_core_min_offset = 7;
                bolt_outer_max_offset = bolt_color_jitter_range;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = bolt_sparkle_phase;
                bolt_point_source_end_flag = bolt_sparkle_phase;
                bolt_tip_highlight_flag = bolt_sparkle_phase;
                beam_renderer_mode = 2;
                beam_fire_sfx_id = 21;
                bolt_full_core_gradient_flag = bolt_sparkle_phase;
                bolt_apply_sparkles_flag = draw_mauler_ball_flag;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_PLASMA_CANNON:
                palette_fragment_index = 0;
                draw_mauler_ball_flag = 0;
                duration_frames = 0x3C;
                bolt_color_jitter_chance = 0x32;
                beam_fire_sfx_id = 13;
                bolt_color_jitter_range = 5;
                bolt_core_min_offset = 10;
                bolt_outer_max_offset = 8;
                frame_step_distance = bolt_color_jitter_chance;
                bolt_sparkle_phase = 1;
                bolt_core_max_offset = beam_fire_sfx_id;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = bolt_sparkle_phase;
                bolt_point_source_end_flag = bolt_sparkle_phase;
                bolt_tip_highlight_flag = bolt_sparkle_phase;
                beam_renderer_mode = 2;
                bolt_full_core_gradient_flag = bolt_sparkle_phase;
                bolt_apply_sparkles_flag = bolt_sparkle_phase;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                break;
            case WEAPON_SPATIAL_COMPRESSOR:
                beam_renderer_mode = 2;
                bolt_sparkle_phase = 1;
                bolt_color_jitter_chance = 0x1E;
                bolt_color_jitter_range = 3;
                frame_step_distance = 0x37;
                palette_fragment_index = beam_renderer_mode;
                bolt_core_min_offset = 10;
                bolt_core_max_offset = 15;
                duration_frames = 0x28;
                draw_mauler_ball_flag = 0;
                bolt_outer_max_offset = bolt_core_min_offset;
                bolt_outer_min_offset = draw_mauler_ball_flag;
                bolt_draw_caps_flag = bolt_sparkle_phase;
                bolt_point_source_end_flag = bolt_sparkle_phase;
                bolt_tip_highlight_flag = bolt_sparkle_phase;
                bolt_full_core_gradient_flag = bolt_sparkle_phase;
                bolt_apply_sparkles_flag = bolt_sparkle_phase;
                bolt_suppress_side_strands_flag = draw_mauler_ball_flag;
                reverse_draw_flag = draw_mauler_ball_flag;
                beam_fire_sfx_id = bolt_sparkle_phase;
                break;
            case WEAPON_MAULER_DEVICE:
            case WEAPON_DRAGON_BREATH:
            case WEAPON_PLASMA_BREATH:
                duration_frames = 0x14;
                palette_fragment_index = 5;
                bolt_color_jitter_range = palette_fragment_index;
                beam_renderer_mode = 1;
                bolt_color_jitter_chance = 0x1E;
                frame_step_distance = 0x28;
                bolt_sparkle_phase = beam_renderer_mode;
                bolt_outer_min_offset = 0;
                bolt_core_max_offset = 15;
                bolt_core_min_offset = 10;
                bolt_outer_max_offset = 7;
                bolt_draw_caps_flag = beam_renderer_mode;
                bolt_point_source_end_flag = beam_renderer_mode;
                bolt_tip_highlight_flag = beam_renderer_mode;
                draw_mauler_ball_flag = beam_renderer_mode;
                bolt_full_core_gradient_flag = beam_renderer_mode;
                bolt_apply_sparkles_flag = beam_renderer_mode;
                bolt_suppress_side_strands_flag = bolt_outer_min_offset;
                reverse_draw_flag = bolt_outer_min_offset;
                is_multi_beam = beam_renderer_mode;
                beam_fire_sfx_id = palette_fragment_index;
                break;
            case 46:
                palette_fragment_index = 6;
                bolt_color_jitter_chance = 10;
                duration_frames = 0x28;
                bolt_sparkle_phase = 1;
                bolt_color_jitter_range = 3;
                frame_step_distance = 0x32;
                bolt_core_max_offset = 15;
                bolt_outer_min_offset = 7;
                bolt_core_min_offset = 10;
                bolt_outer_max_offset = 10;
                bolt_draw_caps_flag = 1;
                bolt_point_source_end_flag = 1;
                draw_mauler_ball_flag = 0;
                bolt_tip_highlight_flag = 1;
                beam_renderer_mode = 2;
                beam_fire_sfx_id = 18;
                bolt_full_core_gradient_flag = 1;
                bolt_apply_sparkles_flag = 1;
                bolt_suppress_side_strands_flag = 1;
                reverse_draw_flag = 0;
                break;
            default:
                return;
        }

        if (!Is_Steve_Stuff_Weapon(weapon_id)) {
            if (old_color[fx_slot_idx] != palette_fragment_index) {
                old_color[fx_slot_idx] = palette_fragment_index;
                Add_Weapon_Palette_Fragment_To_Current_Palette(palette_fragment_index, (fx_slot_idx + 9) << 4, 16);
            }
            if ((weapon_specials & 2) != 0) {
                beam_renderer_mode++;
                if (beam_renderer_mode >= 2) {
                    beam_renderer_mode = 2;
                }
            }
            if ((weapon_specials & 0x80) != 0) {
                frame_step_distance *= 2;
                duration_frames /= 2;
            }
            if ((weapon_specials & 4) != 0) {
                beam_renderer_mode--;
                if (beam_renderer_mode <= 0) {
                    beam_renderer_mode = 0;
                }
            }
            if ((weapon_specials & 8) != 0) {
                bolt_tip_highlight_flag = 1;
            }
            if ((weapon_specials & 0x10) != 0) {
                if (duration_frames != 0x280) {
                    duration_frames *= 2;
                }
            }
            if ((weapon_specials & 0x20) != 0) {
                bolt_full_core_gradient_flag = 0;
            }
            if ((weapon_specials & 0x40) != 0) {
                bolt_tip_highlight_flag = 1;
            }
        }

        if (is_multi_beam != 0 && attacker_size_class != -1) {
            loop_count = 3;
        } else {
            loop_count = 1;
        }

        g_weapon_n = weapon_id;
        g_sfx_n = beam_fire_sfx_id;

        s_weapon_visual_fx fx;
        s_weapon_visual_fx* pfx = nullptr;

        if (Is_Steve_Stuff_Weapon(weapon_id)) {
            Build_Steve_Stuff_Struct_For_Weapon(weapon_id, weapon_specials, &fx);
            frame_step_distance = (int16_t)fx.sfx_sprite_id;
            duration_frames = (int16_t)fx.duration;
            pfx = &fx;
        }

        if (loop_count > 0) {
            int16_t frame_idx_offset = 0;
            int palette_offset = (fx_slot_idx + 9) << 4;

            do {
                int16_t frame_index = current_frame + frame_idx_offset;

                Draw_Generic_Beam(
                    attacker_x, attacker_y, target_x, target_y, attacker_facing, target_facing, duration_frames, palette_offset, bolt_color_jitter_chance, bolt_color_jitter_range, frame_step_distance, bolt_sparkle_phase, bolt_draw_caps_flag, bolt_point_source_end_flag, bolt_tip_highlight_flag, bolt_full_core_gradient_flag, bolt_apply_sparkles_flag, bolt_suppress_side_strands_flag, reverse_draw_flag, draw_mauler_ball_flag, beam_renderer_mode, bolt_core_max_offset, bolt_core_min_offset, bolt_outer_max_offset, bolt_outer_min_offset, frame_index, target_size_class, hit_result_flags, special_flag, is_non_ship_target, burst_variant, reflection_state, attacker_size_class, pfx);

                frame_idx_offset++;
            } while (frame_idx_offset < loop_count);
        }

        if (fire_point_idx == current_frame || current_frame == 0) {
            g_beam_fire_sfx_handle[fx_slot_idx * 5 + fire_point_idx] = ERICNET::Mox_Play_Sound(beam_fire_sfx_id);
        } else if (ship_firing_now_flag[fx_slot_idx] == 0 || Get_Max_Frames() - 1 <= current_frame) {
            sound::Fadeout_Sound(g_beam_fire_sfx_handle[fx_slot_idx * 5 + fire_point_idx], 10);
            g_beam_fire_sfx_handle[fx_slot_idx * 5 + fire_point_idx] = 0;
        }
    }

    s_beam_target_shape Beam_Target_Shape(int16_t target_id) {
        // In-range missile indices retain their legacy ship-slot presentation.
        if (target_id >= 0 && target_id < MAX_COMBAT_SHIPS)
            return {MOX::combat_data[target_id].facing_dir, MOX::combat_data[target_id].size_class};
        return {0, 0};
    }

    /**
     * Drive the full beam-weapon visual sequence for one attack exchange.
     * @param fx_entries Beam effect entries to play in sequence.
     */
    void __cdecl Beam_SFX(int16_t attacker_ship_id, int16_t attacker_x, int16_t attacker_y, int16_t target_id, int16_t target_x, int16_t target_y, s_beam_fx_entry* fx_entries, int16_t is_non_ship_target) {
        int16_t current_fx_idx[2] = {0, -1};
        int16_t frame_idx[2] = {0, 0};
        int16_t rand_offsets[6] = {0, 0, 0, 0, 0, 0};
        int16_t special_timer[2] = {0, 0};
        int16_t fx_idx = 0;

        uint32_t seed = game_random::Get_Random_Seed();
        int16_t ship_range = Get_Ship_Range(attacker_x, attacker_y, target_x, target_y);

        g_pierced_shield_flag = 0;
        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
        Shield_Anims(0, 0, 1, 0);
        Reset_Beam_Swapping_Colors();

        if (fx_entries[0].weapon_id == 0x2e || TECHDATA::weapons[fx_entries[0].weapon_id].type == 0) {
            int16_t attacker_facing_dir = MOX::combat_data[attacker_ship_id].facing_dir;
            const auto target_shape = Beam_Target_Shape(target_id);
            int16_t target_facing_dir = target_shape.facing;

            int16_t attacker_center_x, attacker_center_y;
            COMBINIT::Ship_Center_Offsets(attacker_ship_id, &attacker_center_x, &attacker_center_y);
            attacker_center_x += attacker_x;
            attacker_center_y += attacker_y;

            int16_t target_center_x, target_center_y;
            if (is_non_ship_target == 0) {
                COMBINIT::Ship_Center_Offsets(target_id, &target_center_x, &target_center_y);
                target_center_x += target_x;
                target_center_y += target_y;
            } else {
                target_center_x = target_x;
                target_center_y = target_y;
            }

            for (int32_t i = 0; i < 10; ++i) {
                g_beam_fire_sfx_handle[i] = 0;
            }

            if ((fx_entries[0].specials & 0x80) != 0) {
                special_timer[0] = 3;
            }

            bool bExitLoop = false;
            do {
                int16_t has_reflected = 0;
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 0x27f, 0x15e);
                video::Clip_On();

                if (MOX::dont_draw_ship == target_id && is_non_ship_target == 0) {
                    CMBTDRW1::Draw_Ship(target_id, target_x, target_y);
                }
                if (MOX::dont_draw_ship == attacker_ship_id) {
                    CMBTDRW1::Draw_Ship(attacker_ship_id, attacker_x, attacker_y);
                }

                for (int16_t i = 0; i < 2; ++i) {
                    int16_t current_fx = current_fx_idx[i];
                    if (current_fx >= 0) {
                        s_beam_fx_entry* fx = &fx_entries[current_fx];
                        int16_t weapon_id = fx->weapon_id;
                        int16_t shot_count = fx->shot_count;
                        int16_t attacker_picture_num = MOX::combat_data[attacker_ship_id].picture_num;
                        int16_t attacker_owner = MOX::combat_data[attacker_ship_id].owner;

                        int16_t target_bearing;
                        if (is_non_ship_target == 0) {
                            target_bearing = CMBTDRW1::Relative_Bearing(attacker_ship_id, target_id);
                        } else {
                            target_bearing = CMBTDRW1::Relative_Bearing_XY(attacker_ship_id, target_x, target_y);
                        }
                        target_bearing = CMBTFIRE::Facing_Shield(target_bearing);

                        int16_t fp_count;
                        if (attacker_ship_id <= 0 || attacker_owner >= MAX_PLAYERS || g_reflection_state_flag == 2) {
                            fp_count = 1;
                        } else {
                            fp_count = FIREPTS::N_Fire_Points(weapon_id, shot_count);
                        }

                        if (frame_idx[i] == 0) {
                            bool apply_random = false;
                            if (fx->result_flags == 0 && fx->special_flag == 0 && fx->is_damage_entry == 0) {
                                if (target_id != 0) {
                                    if (is_non_ship_target == 0) {
                                        int16_t shield_size = Ship_Size_To_Shield_Size(MOX::combat_data[target_id].size_class);
                                        int16_t max_radius = ship_range / 2;
                                        int16_t shield_radius = shield_diameters[shield_size] / 2;
                                        if (shield_radius <= max_radius) {
                                            max_radius = shield_radius;
                                        }
                                        rand_offsets[i + 2] = (int16_t)game_random::Random(max_radius + 1) - max_radius / 2;
                                        rand_offsets[i] = (int16_t)game_random::Random(max_radius + 1) - max_radius / 2;
                                    } else {
                                        apply_random = true;
                                    }
                                } else if (fx->accum_hit_x == 0) {
                                    int16_t max_radius = ship_range / 2;
                                    if (max_radius > 30) max_radius = 30;
                                    int16_t radius = (max_radius + (max_radius >> 31 & 3)) / 4 + 1;
                                    int16_t base = max_radius / 2;
                                    rand_offsets[i + 2] = base + (int16_t)game_random::Random(radius);
                                    rand_offsets[i + 2] *= (int16_t)(game_random::Random(2) - 1);
                                    rand_offsets[i] = base + (int16_t)game_random::Random(radius);
                                    rand_offsets[i] *= (int16_t)(game_random::Random(2) - 1);
                                } else {
                                    apply_random = true;
                                }
                            } else {
                                if (target_id == 0 && fx->accum_hit_x == 0) {
                                    int16_t max_radius = ship_range / 2;
                                    if (max_radius > 30) max_radius = 30;
                                    int16_t radius = (max_radius + (max_radius >> 31 & 3)) / 4 + 1;
                                    int16_t base = max_radius / 2;
                                    rand_offsets[i + 2] = base + (int16_t)game_random::Random(radius);
                                    rand_offsets[i + 2] *= (int16_t)(game_random::Random(2) - 1);
                                    rand_offsets[i] = base + (int16_t)game_random::Random(radius);
                                    rand_offsets[i] *= (int16_t)(game_random::Random(2) - 1);
                                } else {
                                    apply_random = true;
                                }
                            }

                            if (apply_random) {
                                if (is_non_ship_target == 0 || fx->accum_hit_x != 0) {
                                    rand_offsets[i + 2] = 0;
                                    rand_offsets[i] = 0;
                                } else {
                                    int16_t max_radius = ship_range / 2;
                                    int16_t shield_radius = shield_diameters[0] / 2;
                                    if (max_radius <= shield_radius) {
                                        shield_radius = max_radius;
                                    }
                                    int16_t radius = (shield_radius + (shield_radius >> 31 & 3)) / 4 + 1;
                                    int16_t base = shield_radius / 2;
                                    rand_offsets[i + 2] = base + (int16_t)game_random::Random(radius);
                                    rand_offsets[i + 2] *= (int16_t)(game_random::Random(2) - 1);
                                    rand_offsets[i] = base + (int16_t)game_random::Random(radius);
                                    rand_offsets[i] *= (int16_t)(game_random::Random(2) - 1);
                                }
                            }
                        }

                        if (fp_count > 0) {
                            for (int32_t fp_idx_val = 0; fp_idx_val < fp_count; ++fp_idx_val) {
                                int16_t fp_x = 0, fp_y = 0;
                                if (attacker_ship_id > 0) {
                                    FIREPTS::Calc_FP_Offsets(&fp_x, &fp_y, weapon_id, shot_count, attacker_owner, attacker_ship_id, attacker_picture_num, (uint8_t)attacker_facing_dir, (uint8_t)target_bearing, fp_idx_val);
                                }
                                Fire_Weapon(attacker_center_x + fp_x, attacker_center_y + fp_y, target_center_x + rand_offsets[i + 2], target_center_y + rand_offsets[i], attacker_facing_dir, target_facing_dir, fx->weapon_id, frame_idx[i], i, target_shape.size, fx->result_flags, fx->special_flag, is_non_ship_target, fp_idx_val, g_reflection_state_flag, MOX::combat_data[attacker_ship_id].size_class, fx->specials, 1);
                            }
                        }
                    }

                    bool skip_frame_increment = false;
                    if (current_fx < 16) {
                        if (current_fx <= -1) {
                            int16_t other_i = (i == 0) ? 1 : 0;
                            int16_t other_fx = current_fx_idx[other_i];
                            if (other_fx != -1 && other_fx < 16) {
                                if (fx_entries[other_fx].weapon_id <= WEAPON_NO_WEAPONS || frame_idx[other_i] >= 9) {
                                    skip_frame_increment = true;
                                }
                            }
                        } else {
                            int16_t max_frames = Get_Max_Frames();
                            if (frame_idx[i] >= max_frames) {
                                skip_frame_increment = true;
                            }
                        }

                        if (skip_frame_increment) {
                            frame_idx[i] = 0;
                            special_timer[i]--;
                            if (special_timer[i] <= 0) {
                                special_timer[i] = 0;
                                s_beam_fx_entry* next_fx = &fx_entries[fx_idx + 1];
                                if (next_fx->weapon_id <= WEAPON_NO_WEAPONS || fx_idx + 1 >= 16) {
                                    current_fx_idx[i] = (int16_t)0xffff;
                                } else {
                                    if (next_fx->is_damage_entry == 0) {
                                        fx_idx++;
                                        current_fx_idx[i] = fx_idx;
                                        if (fx_entries[fx_idx + 1].is_damage_entry != 0) {
                                            g_reflection_state_flag = 1;
                                            reflection_field_display_status = 1;
                                        }
                                        if ((fx_entries[fx_idx].specials & 0x80) != 0) {
                                            special_timer[i] = 3;
                                        }
                                    } else {
                                        int16_t other_i = (i == 0) ? 1 : 0;
                                        if (current_fx_idx[other_i] == (int16_t)0xfc18 &&
                                            (ship_hit_now_flag[other_i] != 0 || current_fx_idx[i] <= -1)) {
                                            shield_hit_now_flag = 0;
                                            shield_display_status = 0;

                                            s_beam_fx_entry reflected_fx[16];
                                            struct_::Clear_Structure(reflected_fx, sizeof(reflected_fx));

                                            reflected_fx[0].weapon_id = next_fx->weapon_id;
                                            reflected_fx[0].shot_count = next_fx->shot_count;
                                            reflected_fx[0].result_flags = next_fx->result_flags;
                                            reflected_fx[0].special_flag = next_fx->special_flag;
                                            reflected_fx[0].accum_hit_x = next_fx->accum_hit_x;
                                            reflected_fx[0].accum_hit_y = next_fx->accum_hit_y;
                                            reflected_fx[0].specials = next_fx->specials;
                                            reflected_fx[0].is_damage_entry = next_fx->is_damage_entry;
                                            reflected_fx[1].weapon_id = WEAPON_NO_WEAPONS;
                                            reflected_fx[1].is_damage_entry = 0;

                                            g_firing_reflected_beam_flag = 1;
                                            g_reflection_state_flag = 2;
                                            Beam_SFX(target_id, target_x, target_y, attacker_ship_id, attacker_x, attacker_y, reflected_fx, 0);
                                            reflection_field_display_status = 0;
                                            g_firing_reflected_beam_flag = 0;

                                            fx_idx++;
                                            has_reflected = 1;
                                            current_fx_idx[other_i] = (int16_t)0xfc18;

                                            timer::Mark_Time();
                                            video::Set_Page_Off();
                                            CMBTDRW1::Draw_Main_Combat_Screen();
                                            graphics::Set_Window(0, 0, 0x27f, 0x15e);
                                            video::Clip_On();
                                            if (MOX::dont_draw_ship == target_id && is_non_ship_target == 0) {
                                                CMBTDRW1::Draw_Ship(target_id, target_x, target_y);
                                            }
                                            if (MOX::dont_draw_ship == attacker_ship_id) {
                                                CMBTDRW1::Draw_Ship(attacker_ship_id, attacker_x, attacker_y);
                                            }
                                        } else {
                                            current_fx_idx[i] = (int16_t)0xfc18;
                                            if (next_fx->is_damage_entry != 0) {
                                                g_reflection_state_flag = 1;
                                                reflection_field_display_status = 1;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (!skip_frame_increment) {
                        if (MOX::speedx2_flag != 0 || MOX::net_flag != 0) {
                            if (frame_idx[i] + 2 < Get_Max_Frames()) {
                                frame_idx[i] += 2;
                            } else {
                                frame_idx[i]++;
                            }
                        } else {
                            frame_idx[i]++;
                        }
                    }
                } // for i loop

                if (current_fx_idx[0] <= -1 && current_fx_idx[1] <= -1) {
                    shield_hit_now_flag = 0;
                    reflection_field_hit_now_flag = 0;
                }

                if (current_fx_idx[0] <= -1 && current_fx_idx[1] <= -1 &&
                    shield_display_status == 0 && reflection_field_display_status == 0) {
                    if (Done_Drawing_Damage_Message_Queue() != 0 && has_reflected == 0) {
                        bExitLoop = true;
                        g_reflection_state_flag = 0;
                    }
                }

                if (is_non_ship_target == 0) {
                    Adjust_Shield_Display_Status(&shield_hit_draw_facing);
                    uint8_t shield_special = 0;
                    if (current_fx_idx[0] > -1) {
                        shield_special = (uint8_t)fx_entries[current_fx_idx[0]].special_flag;
                    }
                    if (current_fx_idx[1] > -1) {
                        shield_special = (uint8_t)(shield_special + (uint8_t)fx_entries[current_fx_idx[0] == 0].special_flag);
                    }
                    if (g_pierced_shield_flag != 0) {
                        shield_special = 1;
                    }

                    if (target_id == 0) {
                        int16_t colony_id = MOX::cmbt_colony_id;
                        int16_t planet_idx = MOX::colony[colony_id].planet;
                        int16_t planet_size = MOX::planet[planet_idx].size;
                        int16_t shield_size = Planet_Size_To_Shield_Size(planet_size);
                        int16_t shield_diam = shield_diameters[shield_size];
                        int16_t draw_facing = shield_hit_draw_facing;
                        int16_t hit_facing = Get_SFX_Draw_Shield_Facing2(attacker_ship_id, target_id);
                        int16_t max_radius = shield_diam / 2;
                        int16_t dx = target_center_x - max_radius;
                        int16_t dy = target_center_y - max_radius;
                        Draw_Shield_Hit(dx, dy, target_id, shield_special != 0, 0, hit_facing, (int16_t)draw_facing);
                    } else {
                        int16_t shield_size = Ship_Size_To_Shield_Size(MOX::combat_data[target_id].size_class);
                        int16_t shield_diam = shield_diameters[shield_size];
                        int16_t specials = 0;
                        if (current_fx_idx[0] > -1) {
                            specials = fx_entries[current_fx_idx[0]].specials & 0x100;
                        }
                        int16_t draw_facing = shield_hit_draw_facing;
                        int16_t hit_facing = Get_SFX_Draw_Shield_Facing2(attacker_ship_id, target_id);
                        int16_t max_radius = shield_diam / 2;
                        int16_t dx = target_center_x - max_radius;
                        int16_t dy = target_center_y - max_radius;
                        Draw_Shield_Hit(dx, dy, target_id, shield_special != 0, specials, hit_facing, (int16_t)draw_facing);
                    }

                    if (reflection_field_display_status != 0) {
                        if (g_reflection_state_flag == 2) {
                            Adjust_Reflection_Field_Display_Status(&reflection_field_hit_draw_facing);
                            int16_t refl_center_x, refl_center_y;
                            COMBINIT::Ship_Center_Offsets(attacker_ship_id, &refl_center_x, &refl_center_y);
                            refl_center_x += attacker_x;
                            refl_center_y += attacker_y;
                            int16_t shield_size = Ship_Size_To_Shield_Size(MOX::combat_data[attacker_ship_id].size_class);
                            int16_t refl_diam = reflection_field_diameters[shield_size];
                            int16_t draw_facing = reflection_field_hit_draw_facing;
                            int16_t hit_facing = Get_SFX_Draw_Shield_Facing2(target_id, attacker_ship_id);
                            int16_t refl_radius = refl_diam / 2;
                            Draw_Reflection_Field_Hit(refl_center_x - refl_radius, refl_center_y - refl_radius, MOX::combat_data[attacker_ship_id].size_class, hit_facing, draw_facing);
                        } else if (g_reflection_state_flag == 1) {
                            Adjust_Reflection_Field_Display_Status(&reflection_field_hit_draw_facing);
                            int16_t shield_size = Ship_Size_To_Shield_Size(MOX::combat_data[target_id].size_class);
                            int16_t refl_diam = reflection_field_diameters[shield_size];
                            int16_t refl_center_x, refl_center_y;
                            COMBINIT::Ship_Center_Offsets(target_id, &refl_center_x, &refl_center_y);
                            refl_center_x += target_x;
                            refl_center_y += target_y;
                            int16_t draw_facing = reflection_field_hit_draw_facing;
                            int16_t hit_facing = Get_SFX_Draw_Shield_Facing2(attacker_ship_id, target_id);
                            int16_t refl_radius = refl_diam / 2;
                            Draw_Reflection_Field_Hit(refl_center_x - refl_radius, refl_center_y - refl_radius, MOX::combat_data[target_id].size_class, hit_facing, draw_facing);
                        }
                    }
                } else {
                    shield_display_status = 0;
                    reflection_field_display_status = 0;
                }

                for (int16_t i = 0; i < 2; ++i) {
                    if (is_non_ship_target == 0) {
                        int16_t fx_idx_val = current_fx_idx[i];
                        if (fx_idx_val > -1 && ship_hit_now_flag[i] != 0) {
                            s_beam_fx_entry* fx = &fx_entries[fx_idx_val];
                            if (fx->accum_hit_x > 0 || fx->accum_hit_y > 0) {
                                uint8_t hit_facing = (uint8_t)Do_Get_SFX_Draw_Shield_Facing(attacker_ship_id, target_id);
                                Add_Damage_Indicator_For_Ship(target_x, target_y, target_id, hit_facing, fx->accum_hit_x, fx->accum_hit_y);
                                ERICNET::Mox_Play_Sound(0x1d);
                                fx->accum_hit_x = -1;
                                fx->accum_hit_y = -1;
                            }
                        }
                    } else {
                        int16_t fx_idx_val = current_fx_idx[i];
                        if (fx_idx_val > -1 && ship_hit_now_flag[i] != 0) {
                            s_beam_fx_entry* fx = &fx_entries[fx_idx_val];
                            if (fx->accum_hit_x > 0 || fx->accum_hit_y > 0) {
                                Add_Damage_Indicator_For_Missile(target_center_x, target_center_y, 1, fx->accum_hit_x);
                                MOX::missile[target_id].quantity -= fx->accum_hit_x;
                                tactical_view::Missile_Changed(target_id);
                                if (MOX::missile[target_id].quantity <= 0) {
                                    CMBTMIS::Kill_Missile(target_id);
                                }
                                if (fx->accum_hit_x != 0) {
                                    ERICNET::Mox_Play_Sound(0x1c);
                                }
                                fx->accum_hit_x = -1;
                            }
                        }
                    }
                }

                Draw_Damage_Message_Queue();
                timer::Release_Time(1);
                RUSS::Mox_Sync_Update();
                video::Clip_Off();
                graphics::Reset_Window();
            } while (!bExitLoop);

            ship_hit_now_flag[0] = 0;
            ship_hit_now_flag[1] = 0;
            ship_firing_now_flag[0] = 0;
            ship_firing_now_flag[1] = 0;
            Reset_Beam_SFX_Handles();
            shield_display_status = 0;
            reflection_field_display_status = 0;
            video::Clip_Off();
            game_random::Set_Random_Seed(seed);
        }
    }

    void __cdecl Draw_Ship_Burst(int16_t x, int16_t y, int16_t dir, int16_t base_damage_class, int16_t frame, int16_t is_palette_not_90) {
        s_animation_header* anim = laser_burst_anim[base_damage_class][is_palette_not_90][dir];
        animate::Set_Animation_Frame(anim, frame % 3);
        ERIC::Draw_Centered(x, y, anim);
    }

    void __cdecl Reset_Beam_SFX_Handles() {
        g_shield_sfx_handle = 0;
        memset(g_beam_fire_sfx_handle, 0, sizeof(g_beam_fire_sfx_handle));
    }

    void __cdecl Draw_Reflection_Field_Hit(int16_t x, int16_t y, int16_t ship_size, int16_t rotation, int16_t frame) {
        int16_t base_rot = Rot_To_Base_Rot(rotation);
        int16_t rot_method = Rot_To_Rot_Method(rotation);
        int16_t shield_size = Ship_Size_To_Shield_Size(ship_size);

        uint32_t diameter = shield_diameters[shield_size];
        uint32_t size = (diameter * diameter) + 0x20;

        allocate::Push_Block(MOX::screen_seg);
        s_animation_header* h1 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, size);
        s_animation_header* psVar5 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, size);

        if (reflection_field_display_status == 0) {
            allocate::Pop_Block(MOX::screen_seg);
            return;
        }

        if (frame == 0 && g_reflection_field_sfx_handle == 0) {
            int16_t sfx_id = (g_reflection_state_flag == 0) ? 0x26 : 0x2A;
            g_reflection_field_sfx_handle = ERICNET::Mox_Play_Sound(sfx_id);
            sound::Pitch_Bend_Sound((int32_t)g_reflection_field_sfx_handle, ship_size * -2500);
        } else {
            s_animation_header* anim = Reflection_Field_Anims(base_rot, shield_size);
            if ((animate::Get_Animation_Frame_Count(anim) - 1) <= frame) {
                sound::Fadeout_Sound(g_reflection_field_sfx_handle, 10);
                g_reflection_field_sfx_handle = 0;
            }
        }

        s_animation_header* anim_frames = Reflection_Field_Anims(base_rot, shield_size);
        int16_t frame_count = animate::Get_Animation_Frame_Count(anim_frames);
        int16_t actual_frame = frame % frame_count;

        s_animation_header* anim_to_set = Reflection_Field_Anims(base_rot, shield_size);
        animate::Set_Animation_Frame(anim_to_set, actual_frame);

        s_animation_header* anim_to_draw = Reflection_Field_Anims(base_rot, shield_size);
        bitmap::Draw_Picture_To_Bitmap(h1, anim_to_draw);

        s_animation_header* draw_header = nullptr;
        switch (rot_method) {
            case 0:
                bitmap::Set_Picture_Glassed(h1);
                draw_header = h1;
                break;
            case 1:
                bitmap::Flip_Bitmap(psVar5, h1, 0);
                bitmap::Set_Picture_Glassed(psVar5);
                draw_header = psVar5;
                break;
            case 2:
                bitmap::Flip_Bitmap(psVar5, h1, 3);
                bitmap::Flip_Bitmap(h1, psVar5, 0);
                bitmap::Set_Picture_Glassed(h1);
                draw_header = h1;
                break;
            case 3:
                bitmap::Flip_Bitmap(psVar5, h1, 3);
                bitmap::Set_Picture_Glassed(psVar5);
                draw_header = psVar5;
                break;
            default:
                break;
        }

        if (draw_header != nullptr) {
            animate::Draw(x, y, draw_header);
        }

        allocate::Pop_Block(MOX::screen_seg);
    }

    /** Advance the reflection-field animation state machine. */
    void __cdecl Adjust_Reflection_Field_Display_Status(int16_t* frame_index) {
        int16_t frame_count = animate::Get_Animation_Frame_Count(Reflection_Field_Anims(0, 0));

        if (g_reflection_state_flag == 0 && reflection_field_display_status != 0) {
            reflection_field_display_status = 0;
            return;
        }

        switch (reflection_field_display_status) {
            case 0:
                if (reflection_field_hit_now_flag != 0 || g_reflection_state_flag >= 1) {
                    reflection_field_display_status = 1;
                    *frame_index = 0;
                }
                break;
            case 1:
                (*frame_index)++;
                if (*frame_index >= 4) {
                    reflection_field_display_status = 2;
                }
                break;
            case 2:
                (*frame_index)++;
                if (reflection_field_hit_now_flag != 0) {
                    if (*frame_index >= 7) {
                        *frame_index = 4;
                    }
                } else {
                    if (*frame_index >= 7) {
                        reflection_field_display_status = 3;
                    }
                }
                break;
            case 3:
                (*frame_index)++;
                if (frame_count <= *frame_index) {
                    reflection_field_display_status = 0;
                    g_reflection_state_flag = 0;
                }
                break;
        }
    }

    void __cdecl Build_Steve_Stuff_Struct_For_Weapon(int16_t weapon_id, uint16_t weapon_specials, s_weapon_visual_fx* out_fx) {
        if (weapon_id <= WEAPON_NO_WEAPONS || weapon_id > 46) {
            memset(out_fx, 0, sizeof(*out_fx));
            return;
        }

        s_weapon_visual_fx weapon_fx_template;

        switch (weapon_id) {
            case WEAPON_MASS_DRIVER:
                Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x59, 0x6b, 0xc3, 0xda, 0xfc, 0xff);
                weapon_fx_template.thickness = 1;
                weapon_fx_template.anim_style = 1;
                weapon_fx_template.sfx_sprite_id = 4;
                weapon_fx_template.duration = 20;
                weapon_fx_template.wide_bolt_size_bonus = 0;
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_GAUSS_CANNON:
                Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x59, 0x67, 0x6c, 0xb2, 0xc8, 0xc8);
                weapon_fx_template.thickness = 1;
                weapon_fx_template.anim_style = 1;
                weapon_fx_template.sfx_sprite_id = 6;
                weapon_fx_template.duration = 20;
                weapon_fx_template.wide_bolt_size_bonus = 0;
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_LASER_CANNON:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xc8, 0x14, 0x20, 0xff, 0x50, 0x0a);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 0;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xc8, 0x14, 0x20, 0xff, 0x50, 0x0a);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 2;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_PARTICLE_BEAM:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x5e, 0x70, 0xcd, 0xba, 0xdd, 0xff);
                    weapon_fx_template.thickness = 0;
                    weapon_fx_template.anim_style = 0;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x5e, 0x70, 0xcd, 0xba, 0xdd, 0xff);
                    weapon_fx_template.thickness = 0;
                    weapon_fx_template.anim_style = 2;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_FUSION_BEAM:
                if (((weapon_specials & 0x10) != 0) && ((weapon_specials & 0x100) != 0)) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xd2, 0x46, 0x00, 0xff, 0xa2, 0x00);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 3;
                    weapon_fx_template.sfx_sprite_id = 100;
                    weapon_fx_template.duration = 25;
                    weapon_fx_template.wide_bolt_size_bonus = 1;
                } else if ((weapon_specials & 0x10) != 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xd2, 0x46, 0x00, 0xff, 0xa2, 0x00);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 2;
                    weapon_fx_template.sfx_sprite_id = 120;
                    weapon_fx_template.duration = 30;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else if ((weapon_specials & 0x100) != 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xd2, 0x46, 0x00, 0xff, 0xa2, 0x00);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 1;
                    weapon_fx_template.sfx_sprite_id = 100;
                    weapon_fx_template.duration = 25;
                    weapon_fx_template.wide_bolt_size_bonus = 1;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xd2, 0x46, 0x00, 0xff, 0xa2, 0x00);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 1;
                    weapon_fx_template.sfx_sprite_id = 120;
                    weapon_fx_template.duration = 30;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_ION_PULSE_CANNON:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x94, 0x9b, 0x49, 0xfe, 0xfe, 0x94);
                    weapon_fx_template.thickness = 2;
                    weapon_fx_template.anim_style = 1;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x94, 0x9b, 0x49, 0xfe, 0xfe, 0x94);
                    weapon_fx_template.thickness = 2;
                    weapon_fx_template.anim_style = 2;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_GRAVITON_BEAM:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x44, 0x98, 0x8f, 0x93, 0xff, 0xff);
                    weapon_fx_template.thickness = 2;
                    weapon_fx_template.anim_style = 3;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x44, 0x98, 0x8f, 0x93, 0xff, 0xff);
                    weapon_fx_template.thickness = 2;
                    weapon_fx_template.anim_style = 3;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_NEUTRON_BLASTER:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x92, 0x53, 0xba, 0xff, 0xb8, 0xff);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 1;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x92, 0x53, 0xba, 0xff, 0xb8, 0xff);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 2;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_PHASOR:
            case WEAPON_PHASOR_EYE:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xa9, 0x2b, 0x00, 0xff, 0xa6, 0x60);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 0;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xa9, 0x2b, 0x00, 0xff, 0xa6, 0x60);
                    weapon_fx_template.thickness = 1;
                    weapon_fx_template.anim_style = 2;
                    weapon_fx_template.sfx_sprite_id = 200;
                    weapon_fx_template.duration = 50;
                    weapon_fx_template.wide_bolt_size_bonus = 0;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_DISRUPTER:
                Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0x6b, 0xa2, 0x16, 0xf2, 0xff, 0x35);
                weapon_fx_template.thickness = 1;
                weapon_fx_template.anim_style = 1;
                weapon_fx_template.sfx_sprite_id = 40;
                weapon_fx_template.duration = 30;
                weapon_fx_template.wide_bolt_size_bonus = 0;
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_DEATH_RAY:
            case WEAPON_CRYSTAL_RAY:
                Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xc9, 0x53, 0xc4, 0xff, 0xb8, 0xff);
                weapon_fx_template.thickness = 1;
                weapon_fx_template.anim_style = 3;
                weapon_fx_template.sfx_sprite_id = 200;
                weapon_fx_template.duration = 50;
                weapon_fx_template.wide_bolt_size_bonus = 0;
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_PLASMA_CANNON:
                if ((weapon_specials & 0x10) == 0) {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xd2, 0x53, 0x00, 0xff, 0xff, 0x52);
                    weapon_fx_template.thickness = 2;
                    weapon_fx_template.anim_style = 1;
                    weapon_fx_template.sfx_sprite_id = 100;
                    weapon_fx_template.duration = 30;
                    weapon_fx_template.wide_bolt_size_bonus = 1;
                } else {
                    Set_Weapon_Visual_FX_Colors(&weapon_fx_template, 0xd2, 0x53, 0x00, 0xff, 0xff, 0x52);
                    weapon_fx_template.thickness = 2;
                    weapon_fx_template.anim_style = 3;
                    weapon_fx_template.sfx_sprite_id = 100;
                    weapon_fx_template.duration = 30;
                    weapon_fx_template.wide_bolt_size_bonus = 1;
                }
                memcpy(out_fx, &weapon_fx_template, sizeof(weapon_fx_template));
                break;

            case WEAPON_MAULER_DEVICE:
            case WEAPON_DRAGON_BREATH:
            case WEAPON_PLASMA_BREATH:
            case 46:
                memset(out_fx, 0, sizeof(*out_fx));
                break;

            default:
                memset(out_fx, 0, sizeof(*out_fx));
                return;
        }
    }

    int8_t __cdecl Is_Steve_Stuff_Weapon(int16_t weapon_id) {
        switch (weapon_id) {
            case WEAPON_MASS_DRIVER:
            case WEAPON_GAUSS_CANNON:
            case WEAPON_LASER_CANNON:
            case WEAPON_PARTICLE_BEAM:
            case WEAPON_FUSION_BEAM:
            case WEAPON_ION_PULSE_CANNON:
            case WEAPON_GRAVITON_BEAM:
            case WEAPON_NEUTRON_BLASTER:
            case WEAPON_PHASOR:
            case WEAPON_DISRUPTER:
            case WEAPON_DEATH_RAY:
            case WEAPON_PLASMA_CANNON:
            case WEAPON_PHASOR_EYE:
            case WEAPON_CRYSTAL_RAY:
                return 1;
            case WEAPON_SPATIAL_COMPRESSOR:
            case WEAPON_MAULER_DEVICE:
            case WEAPON_DRAGON_BREATH:
            case WEAPON_PLASMA_BREATH:
            case 46:
                return 0;
            default:
                return 0;
        }
    }

    /** Move a point toward the target by the requested distance along the line segment. */
    void __cdecl Rob_Interpolate_Line(int16_t* p_x, int16_t* p_y, int16_t target_x, int16_t target_y, int16_t step) {
        int16_t start_x = *p_x;
        int16_t start_y = *p_y;
        int32_t low_x = 0;
        int32_t low_y = 0;

        int32_t dx = target_x - start_x;
        int32_t dy = target_y - start_y;

        if (step < 1) {
            return;
        }

        int32_t max_dist = abs(dx);
        if (abs(dy) > max_dist) {
            max_dist = abs(dy);
        }

        if (max_dist <= step) {
            *p_x = target_x;
            *p_y = target_y;
            return;
        }

        if (dx == 0) {
            if (dy <= 0) {
                *p_y = start_y - step;
            } else {
                *p_y = start_y + step;
            }
            return;
        }

        if (dy == 0) {
            if (dx <= 0) {
                *p_x = start_x - step;
            } else {
                *p_x = start_x + step;
            }
            return;
        }

        int32_t high_x = dx;
        int32_t high_y = dy;
        int32_t cur_x = dx >> 1;
        int32_t cur_y = dy >> 1;

        while (true) {
            int32_t cur_dist = abs(cur_x);
            if (abs(cur_y) > cur_dist) {
                cur_dist = abs(cur_y);
            }

            if (cur_dist == step) {
                break;
            }

            if (cur_dist < step) {
                low_x = cur_x;
                low_y = cur_y;
                cur_x += (high_x - cur_x) / 2;
                cur_y += (high_y - cur_y) / 2;
            } else {
                high_x = cur_x;
                high_y = cur_y;
                cur_x += (low_x - cur_x) / 2;
                cur_y += (low_y - cur_y) / 2;
            }
        }

        *p_x = start_x + cur_x;
        *p_y += (int16_t)cur_y;
    }

    /** Return whether the shield-hit facing falls on the front half of the target. */
    int8_t __cdecl Do_Get_SFX_Draw_Shield_Facing(int16_t attacker_id, int16_t target_id) {
        int32_t facing = Get_SFX_Draw_Shield_Facing2(target_id, attacker_id);
        return ((facing + 4) % 16) < 8;
    }

    /** Compute the shield-hit facing from one ship toward another. */
    int16_t __cdecl Get_SFX_Draw_Shield_Facing2(int16_t ship1_idx, int16_t ship2_idx) {
        int16_t p1_x_offset;
        int16_t p1_y_offset;
        int16_t p2_x_offset;
        int16_t p2_y_offset;

        COMBINIT::Ship_Center_Offsets(ship1_idx, &p1_x_offset, &p1_y_offset);
        COMBINIT::Ship_Center_Offsets(ship2_idx, &p2_x_offset, &p2_y_offset);

        int16_t x1 = MOX::combat_data[ship1_idx].x * 20 + p1_x_offset;
        int16_t y1 = MOX::combat_data[ship1_idx].y * 20 + p1_y_offset;
        int16_t x2 = MOX::combat_data[ship2_idx].x * 20 + p2_x_offset;
        int16_t y2 = MOX::combat_data[ship2_idx].y * 20 + p2_y_offset;

        int16_t angle = angle::Get_Angle(x1 - x2, y2 - y1);

        return (angle / 22) % 16;
    }

    /** Copy a weapon palette fragment into the active palette window. */
    void __cdecl Add_Weapon_Palette_Fragment_To_Current_Palette(int16_t source_fragment, int16_t dest_start, int16_t count) {
        if (count > 0) {
            for (int16_t i = 0; i < count; i++) {
                uint8_t red;
                uint8_t green;
                uint8_t blue;

                remap::Get_Palette_Animation_Color(
                    l_anims,
                    (source_fragment * 16) + i,
                    &red,
                    &green,
                    &blue);

                palstore::Store_Single_Palette_Color(
                    dest_start + i,
                    red,
                    green,
                    blue);
            }
        }
    }

    /**
     * Draw a beam segment and related impact visuals for one animation frame.
     * @param attacker_x Beam start x-position.
     * @param attacker_y Beam start y-position.
     * @param target_x Beam target x-position.
     * @param target_y Beam target y-position.
     */
    void __cdecl Draw_Generic_Beam(
        int16_t attacker_x, int16_t attacker_y, int16_t target_x, int16_t target_y, int16_t attacker_facing, int16_t target_facing, int16_t duration_frames, int16_t palette_base_index, int16_t bolt_color_jitter_chance, int16_t bolt_color_jitter_range, int16_t frame_step_distance, int16_t bolt_sparkle_phase, int16_t bolt_draw_caps_flag, int16_t bolt_point_source_end_flag, int16_t bolt_tip_highlight_flag, int16_t bolt_full_core_gradient_flag, int16_t bolt_apply_sparkles_flag, int16_t bolt_suppress_side_strands_flag, int16_t reverse_draw_flag, int16_t draw_mauler_ball_flag, int16_t beam_renderer_mode, int16_t bolt_core_max_offset, int16_t bolt_core_min_offset, int16_t bolt_outer_max_offset, int16_t bolt_outer_min_offset, int16_t frame_index, int16_t target_size_class, uint16_t hit_result_flags, uint16_t special_flag, int16_t is_non_ship_target, int16_t burst_variant, int16_t reflection_state, int16_t attacker_size_class, s_weapon_visual_fx* weapon_visual_fx) {
        int16_t angle = angle::Get_Angle(attacker_x - target_x, target_y - attacker_y);
        ship_hit_now_flag[0] = 0;
        ship_hit_now_flag[1] = 0;

        int16_t beam_dir = ((angle / 22) + 8) % 16;
        if (beam_dir < 0) {
            beam_dir = -beam_dir;
        }

        int16_t is_palette_not_90 = (palette_base_index != 0x90) ? 1 : 0;
        int16_t ship_range = Get_Ship_Range(attacker_x, attacker_y, target_x, target_y);

        if (attacker_size_class == -1) {
            beam_renderer_mode = 0;
            if (duration_frames > 20) {
                duration_frames = 20;
            }
            burst_variant = 1;
            frame_step_distance = 5;
        }

        if (weapon_visual_fx != nullptr) {
            duration_frames = (int16_t)weapon_visual_fx->sfx_sprite_id;
            frame_step_distance = (int16_t)weapon_visual_fx->duration;
        }

        int16_t eff_duration = duration_frames;
        if (duration_frames < 60) {
            eff_duration = 60;
        }
        if (ship_range < eff_duration) {
            eff_duration = ship_range;
        }

        int32_t min_frames = (attacker_size_class == -1) ? 2 : 3;
        int32_t frames = (int32_t)(ship_range + eff_duration) / frame_step_distance;

        if (frames < min_frames) {
            frames = min_frames;
        }
        if (frames > 6) {
            frame_step_distance *= 2;
        }

        frames = (int32_t)(ship_range + eff_duration) / frame_step_distance;
        if (frames < min_frames) {
            frames = min_frames;
        }
        max_frames = (int16_t)frames;

        if (max_frames <= frame_index) {
            return;
        }

        if (frame_index == 0 && attacker_size_class != -1) {
            Draw_Ship_Burst(attacker_x, attacker_y, beam_dir, burst_variant, 0, is_palette_not_90);
            return;
        }

        int16_t target_len = frame_step_distance * frame_index;
        int16_t attacker_len = target_len - duration_frames;

        if (weapon_visual_fx != nullptr && weapon_visual_fx->anim_style == 2) {
            target_len = ship_range;
            attacker_len = 0;
        }

        if (target_len < 0) {
            target_len = 0;
        }
        if (ship_range < target_len) {
            target_len = ship_range;
        }

        int16_t bx2 = attacker_x;
        int16_t by2 = attacker_y;
        Rob_Interpolate_Line(&bx2, &by2, target_x, target_y, target_len);

        if ((bx2 == target_x && by2 == target_y) || (max_frames - 2 <= frame_index)) {
            ship_hit_now_flag[is_palette_not_90] = 1;
            bx2 += game_random::Random(3) - 1;
            game_random::Random(3);
        } else {
            ship_hit_now_flag[is_palette_not_90] = 0;
        }

        bx2 = attacker_x;
        by2 = attacker_y;

        int16_t reduce_len;
        if (hit_result_flags == 0 || special_flag != 0 || g_pierced_shield_flag != 0 || attacker_size_class == -1) {
            if (reflection_state == 1 && special_flag == 0 && g_pierced_shield_flag == 0 && attacker_size_class != -1) {
                int16_t size_idx = Ship_Size_To_Shield_Size(target_size_class);
                reduce_len = reflection_field_diameters[size_idx] / 3;
            } else {
                g_pierced_shield_flag = 1;
                reduce_len = 0;
            }
        } else {
            int16_t size_idx = Ship_Size_To_Shield_Size(target_size_class);
            reduce_len = shield_diameters[size_idx] / 3;
        }

        int16_t cap_reduce = duration_frames - 1;
        if (reduce_len < cap_reduce) {
            cap_reduce = reduce_len;
        }

        int16_t dist = target_len - cap_reduce;
        Rob_Interpolate_Line(&bx2, &by2, target_x, target_y, dist);

        int16_t bx1 = attacker_x;
        int16_t by1 = attacker_y;

        if (attacker_len < 0) {
            attacker_len = 0;
        }
        if (ship_range < attacker_len) {
            attacker_len = ship_range;
        }

        if (weapon_visual_fx != nullptr && weapon_visual_fx->anim_style == 2) {
            attacker_len = 0;
        }

        if (reflection_state == 2) {
            int16_t size = attacker_size_class;
            if (size < 0) {
                size = 0;
            }
            int16_t size_idx = Ship_Size_To_Shield_Size(size);
            attacker_len += reflection_field_diameters[size_idx] / 3;
        }

        if (attacker_len != 0) {
            Rob_Interpolate_Line(&bx1, &by1, target_x, target_y, attacker_len);
        }

        if (reverse_draw_flag != 0 && weapon_visual_fx == nullptr) {
            int16_t temp_x = bx1;
            bx1 = bx2;
            bx2 = temp_x;

            int16_t temp_y = by1;
            by1 = by2;
            by2 = temp_y;
        }

        if (weapon_visual_fx == nullptr) {
            if (bx1 != bx2 || by1 != by2) {
                if (beam_renderer_mode == 1) {
                    bolt::Bolt_Line(bx1, by1, bx2, by2, palette_base_index, beam_angle, bolt_color_jitter_chance, bolt_color_jitter_range, bolt_sparkle_phase, bolt_draw_caps_flag, bolt_point_source_end_flag, bolt_tip_highlight_flag, bolt_full_core_gradient_flag, bolt_apply_sparkles_flag, bolt_suppress_side_strands_flag, bolt_core_max_offset, bolt_core_min_offset, bolt_outer_max_offset, bolt_outer_min_offset);
                } else if (beam_renderer_mode == 2) {
                    int16_t mid3 = (bolt_outer_max_offset + bolt_core_max_offset) / 2;
                    int16_t mid4 = (bolt_outer_min_offset + bolt_core_min_offset) / 2;
                    bolt::Wide_Bolt_Line(bx1, by1, bx2, by2, palette_base_index, beam_angle, bolt_color_jitter_chance, bolt_color_jitter_range, bolt_sparkle_phase, bolt_draw_caps_flag, bolt_point_source_end_flag, bolt_tip_highlight_flag, bolt_full_core_gradient_flag, bolt_apply_sparkles_flag, bolt_suppress_side_strands_flag, bolt_core_max_offset, bolt_core_min_offset, mid3, mid4, bolt_outer_max_offset, bolt_outer_min_offset);
                } else if (beam_renderer_mode == 0) {
                    if (bolt_outer_min_offset < bolt_core_min_offset) bolt_outer_min_offset = bolt_core_min_offset;
                    if (bolt_outer_max_offset < bolt_core_max_offset) bolt_outer_max_offset = bolt_core_max_offset;
                    bolt::Bolt_Line(bx1, by1, bx2, by2, palette_base_index, beam_angle, bolt_color_jitter_chance, bolt_color_jitter_range, bolt_sparkle_phase, bolt_draw_caps_flag, bolt_point_source_end_flag, bolt_tip_highlight_flag, bolt_full_core_gradient_flag, bolt_apply_sparkles_flag, bolt_suppress_side_strands_flag, 0, 0, bolt_outer_max_offset, bolt_outer_min_offset);
                }

                if (draw_mauler_ball_flag != 0 && ship_hit_now_flag[is_palette_not_90] == 0) {
                    ERIC::Draw_Centered(bx2, by2, mauler_ball_seg);
                }
            }
        } else {
            if (reflection_state == 2) {
                attacker_x = bx1;
                attacker_y = by1;
            }
            uint32_t hit_flag = (hit_result_flags == 0) ? 1 : 0;
            Draw_Beam2(attacker_x, attacker_y, target_x, target_y, bx1, by1, bx2, by2, hit_flag, frame_index, palette_base_index, *weapon_visual_fx);
        }

        if (reverse_draw_flag != 0 && weapon_visual_fx == nullptr) {
            int16_t temp_x = bx1;
            bx1 = bx2;
            bx2 = temp_x;

            int16_t temp_y = by1;
            by1 = by2;
            by2 = temp_y;
        }

        if (frame_index < 3 || (attacker_x == bx1 && attacker_y == by1)) {
            if (attacker_size_class != -1) {
                Draw_Ship_Burst(attacker_x, attacker_y, beam_dir, burst_variant, frame_index, is_palette_not_90);
            }
            ship_firing_now_flag[is_palette_not_90] = 1;
        } else {
            ship_firing_now_flag[is_palette_not_90] = 0;
        }

        if (ship_hit_now_flag[is_palette_not_90] != 0 && attacker_size_class != -1) {
            int16_t exp_frame;
            if (frame_index < max_frames - 1) {
                exp_frame = (frame_index - 1 + (-10 - ship_range) / frame_step_distance) % 5;
            } else {
                exp_frame = frame_index - max_frames + 7;
            }

            if (exp_frame < 0) {
                exp_frame = -exp_frame;
            }

            if (special_flag != 0 || hit_result_flags != 0) {
                int16_t final_target_facing = target_facing;
                if (is_non_ship_target != 0) {
                    final_target_facing = 0;
                }
                Draw_Ship_Explosion(target_x, target_y, final_target_facing, exp_frame, is_palette_not_90);
            }
        }

        if (hit_result_flags != 0 && XY_In_Shield_Radius(bx2, by2, target_x, target_y, target_size_class)) {
            shield_hit_now_flag = 1;
        } else {
            shield_hit_now_flag = 0;
        }

        if (reflection_state == 1) {
            if (XY_In_Reflection_Field_Radius(bx2, by2, target_x, target_y, target_size_class)) {
                reflection_field_hit_now_flag = 1;
                return;
            }
        }
        reflection_field_hit_now_flag = 0;
    }

    void __cdecl Set_Beam_Colors(
        int32_t palette_idx,
        int32_t r1,
        int32_t g1,
        int32_t b1,
        int32_t r2,
        int32_t g2,
        int32_t b2,
        int32_t intensity) {
        LocalColor colors[16];

        for (int i = 0; i < 7; ++i) {
            int32_t val = low_ramp_values[i];
            colors[i].r = (r1 * val) / 100;
            colors[i].g = (g1 * val) / 100;
            colors[i].b = (b1 * val) / 100;
        }

        colors[7].r = r1;
        colors[7].g = g1;
        colors[7].b = b1;

        int32_t cross_factor = 115; // 0x73
        for (int i = 0; i < 4; ++i) {
            if (intensity == 0 || intensity == 1 || intensity == 2) {
                int32_t primary_weight;
                if (intensity == 0) {
                    primary_weight = low_primary_cross_values[i];
                } else if (intensity == 1) {
                    primary_weight = moderate_primary_cross_values[i];
                } else {
                    primary_weight = high_primary_cross_values[i];
                }

                int32_t inv_weight = 100 - primary_weight;
                int32_t scaled_weight = primary_weight * cross_factor;

                colors[8 + i].r = ((r1 * scaled_weight) / 100 + r2 * inv_weight) / 100;
                colors[8 + i].g = ((g1 * scaled_weight) / 100 + g2 * inv_weight) / 100;
                colors[8 + i].b = ((b1 * scaled_weight) / 100 + b2 * inv_weight) / 100;
            }

            if (colors[8 + i].r > 255) colors[8 + i].r = 255;
            if (colors[8 + i].g > 255) colors[8 + i].g = 255;
            if (colors[8 + i].b > 255) colors[8 + i].b = 255;

            cross_factor += 15; // 0xF
        }

        colors[12].r = r2;
        colors[12].g = g2;
        colors[12].b = b2;

        for (int i = 0; i < 3; ++i) {
            if (intensity == 0 || intensity == 1 || intensity == 2) {
                int32_t sat_weight;
                if (intensity == 0) {
                    sat_weight = low_saturated_values[i];
                } else if (intensity == 1) {
                    sat_weight = moderate_saturated_values[i];
                } else {
                    sat_weight = high_saturated_values[i];
                }

                colors[13 + i].r = (r2 * sat_weight) / 100;
                colors[13 + i].g = (g2 * sat_weight) / 100;
                colors[13 + i].b = (b2 * sat_weight) / 100;
            }

            if (colors[13 + i].r > 255) colors[13 + i].r = 255;
            if (colors[13 + i].g > 255) colors[13 + i].g = 255;
            if (colors[13 + i].b > 255) colors[13 + i].b = 255;
        }

        for (int i = 0; i < 16; ++i) {
            colors[i].r /= 4;
            colors[i].g /= 4;
            colors[i].b /= 4;
        }

        for (int i = 0; i < 16; ++i) {
            video::current_palette[palette_idx + i].changed = 1;
            video::current_palette[palette_idx + i].r = (uint8_t)colors[i].r;
            video::current_palette[palette_idx + i].g = (uint8_t)colors[i].g;
            video::current_palette[palette_idx + i].b = (uint8_t)colors[i].b;
        }
    }

    void __cdecl Draw_Beam2(int16_t start_x, int16_t start_y, int16_t end_x, int16_t end_y, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t hit_flag, int32_t frame_index, int32_t palette_base_index, s_weapon_visual_fx fx) {
        (void)hit_flag;

        if ((x1 != x2 || y1 != y2) && fx.anim_style < 4) {
            int32_t mod_thick = frame_index % 2;

            switch (fx.anim_style) {
                case 0: {
                    int32_t arg16;
                    int32_t arg17;
                    if (frame_index == 0) {
                        strings::Swap(&x1, &x2);
                        strings::Swap(&y1, &y2);
                        arg17 = 3;
                        arg16 = 10;
                    } else if (frame_index == 1) {
                        arg17 = 6;
                        arg16 = 10;
                    } else {
                        arg17 = 3;
                        arg16 = 12;
                    }
                    bolt::Bolt_Line(x1, y1, x2, y2, palette_base_index, 12, 30, 3, frame_index, 0, 0, 1, 1, 0, 0, arg16, arg17, 4, 0);
                    break;
                }
                case 1: {
                    int32_t cond = fx.sfx_sprite_id >= 10 ? 1 : 0;
                    int32_t size = fx.wide_bolt_size_bonus * 2 + 7;
                    bolt::Wide_Bolt_Line(x1, y1, x2, y2, palette_base_index, 12, 50, 3, (uint16_t)frame_index, 0, 1, cond, 1, 0, 0, 12, 3, size, 1, 2, 0);
                    break;
                }
                case 2: {
                    int32_t div_val = mod_thick + 1;
                    int32_t arg18 = 4 / div_val;
                    int32_t arg17 = 8 / div_val;
                    int32_t arg16 = 12 / div_val;

                    int32_t rand_y = game_random::Random(5) + end_y - 3;
                    int32_t rand_x = game_random::Random(5) + end_x - 3;

                    bolt::Bolt_Line(start_x, start_y, rand_x, rand_y, palette_base_index, 12, 100, 4, frame_index * 4, 0, 0, 0, 0, 1, 0, arg16, arg17, arg18, 0);
                    break;
                }
                case 3: {
                    int32_t div_val1 = mod_thick + 1;
                    int32_t div_val2 = mod_thick * 2 + 1;
                    int32_t div_val3 = mod_thick + 2;

                    int32_t arg20_21 = 2 / div_val1;
                    int32_t arg18_19 = 5 / div_val2 + 1;
                    int32_t arg16_17 = 21 / div_val3;

                    bolt::Wide_Bolt_Line(start_x, start_y, end_x, end_y, palette_base_index, 16, 100, 3, (uint16_t)(frame_index * 2), 1, 0, 0, 0, 1, 0, arg16_17, arg16_17, arg18_19, arg18_19, arg20_21, arg20_21);
                    break;
                }
            }
        }

        Set_Beam_Colors(palette_base_index,
            fx.r1,
            fx.g1,
            fx.b1,
            fx.r2,
            fx.g2,
            fx.b2,
            fx.thickness);
        palette::Refresh_Palette();
    }

    /** Return whether the point falls within the target shield hit radius. */
    bool __cdecl XY_In_Shield_Radius(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t ship_size) {
        int16_t shield_idx = Ship_Size_To_Shield_Size(ship_size);
        int16_t radius = (int16_t)((uint8_t)shield_diameters[shield_idx] >> 1);

        if (abs((int16_t)(x1 - x2)) <= radius &&
            abs((int16_t)(y1 - y2)) <= radius) {
            return true;
        }

        return false;
    }

    /** Return whether the point falls within the target reflection-field radius. */
    bool __cdecl XY_In_Reflection_Field_Radius(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t ship_size) {
        int16_t shield_size = Ship_Size_To_Shield_Size(ship_size);
        int16_t radius = reflection_field_diameters[shield_size] >> 1;

        if (abs((int16_t)(x1 - x2)) <= radius &&
            abs((int16_t)(y1 - y2)) <= radius) {
            return true;
        }

        return false;
    }
} // namespace BEAMS