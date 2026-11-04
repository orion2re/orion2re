#pragma once

namespace BEAMS {
    struct s_beam_target_shape {
        int16_t facing, size;
    };
    s_beam_target_shape Beam_Target_Shape(int16_t target_id);

    struct LocalColor {
        int32_t g;
        int32_t b;
        int32_t r;
    };

    extern uint8_t g_pierced_shield_flag;

    extern int16_t old_color[2];

    extern int16_t g_weapon_n;
    extern int16_t g_sfx_n;

    int8_t __cdecl Is_Steve_Stuff_Weapon(int16_t weapon_id);

    void __cdecl Add_Weapon_Palette_Fragment_To_Current_Palette(int16_t source_fragment, int16_t dest_start, int16_t count);

    void __cdecl Build_Steve_Stuff_Struct_For_Weapon(int16_t weapon_id, uint16_t specials, s_weapon_visual_fx* out);

    /**
     * Draw a beam segment and related impact visuals for one animation frame.
     * @param attacker_x Beam start x-position.
     * @param attacker_y Beam start y-position.
     * @param target_x Beam target x-position.
     * @param target_y Beam target y-position.
     */
    void __cdecl Draw_Generic_Beam(
        int16_t attacker_x, int16_t attacker_y, int16_t target_x, int16_t target_y, int16_t attacker_facing, int16_t target_facing, int16_t duration_frames, int16_t palette_base_index, int16_t bolt_color_jitter_chance, int16_t bolt_color_jitter_range, int16_t frame_step_distance, int16_t bolt_sparkle_phase, int16_t bolt_draw_caps_flag, int16_t bolt_point_source_end_flag, int16_t bolt_tip_highlight_flag, int16_t bolt_full_core_gradient_flag, int16_t bolt_apply_sparkles_flag, int16_t bolt_suppress_side_strands_flag, int16_t reverse_draw_flag, int16_t draw_mauler_ball_flag, int16_t beam_renderer_mode, int16_t bolt_core_max_offset, int16_t bolt_core_min_offset, int16_t bolt_outer_max_offset, int16_t bolt_outer_min_offset, int16_t frame_index, int16_t target_size_class, uint16_t hit_result_flags, uint16_t special_flag, int16_t is_non_ship_target, int16_t burst_variant, int16_t reflection_state, int16_t attacker_size_class, s_weapon_visual_fx* weapon_visual_fx);

    extern s_animation_header* mauler_ball_seg;

    extern s_animation_header* l_anims;

    extern uint8_t shield_hit_now_flag;

    extern uint8_t reflection_field_hit_now_flag;

    extern uint8_t ship_hit_now_flag[2];

    extern uint8_t ship_firing_now_flag[2];

    extern int32_t g_beam_fire_sfx_handle[10];

    extern int32_t g_shield_sfx_handle;

    extern int32_t g_reflection_field_sfx_handle;

    extern int16_t max_frames;

    extern int16_t beam_angle;

    extern uint8_t shield_diameters[8];

    extern int16_t shield_hit_draw_facing;

    extern int16_t reflection_field_hit_draw_facing;

    extern int16_t shield_display_status;

    extern uint8_t reflection_field_diameters[4];

    extern int16_t reflection_field_display_status;

    extern int16_t g_reflection_state_flag;

    extern uint8_t g_firing_reflected_beam_flag;

    extern int32_t low_ramp_values[20];

    extern int32_t low_primary_cross_values[20];

    extern int32_t low_saturated_values[20];

    extern int32_t moderate_primary_cross_values[20];

    extern int32_t moderate_saturated_values[20];

    extern int32_t high_primary_cross_values[20];

    extern int32_t high_saturated_values[20];

    extern int16_t n_damage_msgs;

    extern s_beam_dmg_msg msg_queue[35];

    void __cdecl Reset_Beam_Swapping_Colors();

    void __cdecl Init_Beams();

    extern s_animation_header* laser_burst_anim[2][2][16];

    void __cdecl Load_Ship_Burst();

    extern s_animation_header* explosion_anim[2][16];

    void __cdecl Load_Ship_Explosion();

    extern s_animation_header* shield_anims[5][4][2];

    void __cdecl Load_Shield_Hit();

    s_animation_header* __cdecl Shield_Anims(int16_t beam_idx, int16_t size_idx, int16_t is_special, int16_t is_heavy);

    int16_t __cdecl Ship_Size_To_Shield_Size(int16_t ship_size);

    int16_t __cdecl Rot_To_Base_Rot(int16_t rot);

    int16_t __cdecl Rot_To_Rot_Method(int16_t rot);

    int16_t __cdecl Planet_Size_To_Shield_Size(int16_t planet_size);

    void __cdecl Adjust_Shield_Display_Status(int16_t* shield_status);

    void __cdecl Draw_Shield_Hit(int16_t x, int16_t y, int16_t ship_idx, int16_t is_special, int16_t is_heavy, int16_t hit_rotation, int16_t frame_index);

    s_animation_header* __cdecl Reflection_Field_Anims(int16_t rotation_idx, int16_t size_idx);

    void __cdecl Load_Reflection_Field_Hit(int8_t should_load);

    void __cdecl Load_Combat_Beam_Stuff(int8_t reflection_hit_param);

    void __cdecl Add_Damage_Message_To_Queue(int16_t x, int16_t y, int16_t total_dmg, int16_t struct_dmg, int16_t shield_dmg, uint8_t side);

    void __cdecl Add_Damage_Indicator_For_Ship(int16_t dx, int16_t dy, int16_t ship_idx, uint8_t side, int16_t total_damage, int16_t struct_damage);

    void __cdecl Draw_Shield_Hit_Once_Through(int16_t ship_idx, int16_t hit_rotation, int16_t draw_x, int16_t draw_y);

    void __cdecl Squish_Out_Msg_Queue_Slot(int16_t index);

    void __cdecl Draw_Damage_Indicator(int16_t x, int16_t y, int16_t total_dmg, int16_t hull_dmg, int16_t shield_dmg, uint8_t side, int16_t anim_timer);

    int8_t __cdecl Done_Drawing_Damage_Message_Queue();

    void __cdecl Draw_Damage_Message_Queue();

    void __cdecl Draw_Damage_Message_Queue_Until_Done();

    int16_t __cdecl Get_Max_Frames();

    int16_t __cdecl Get_Ship_Range(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

    void __cdecl Adjust_Reflection_Field_Display_Status(int16_t* frame_index);

    int16_t __cdecl Get_SFX_Draw_Shield_Facing2(int16_t ship1_idx, int16_t ship2_idx);

    int8_t __cdecl Do_Get_SFX_Draw_Shield_Facing(int16_t attacker_id, int16_t target_id);

    void __cdecl Draw_Reflection_Field_Hit(int16_t x, int16_t y, int16_t ship_size, int16_t rotation, int16_t frame);

    void __cdecl Reset_Beam_SFX_Handles();

    void __cdecl Fire_Weapon(int16_t attacker_x, int16_t attacker_y, int16_t target_x, int16_t target_y, int16_t attacker_facing, int16_t target_facing, int16_t weapon_id, int16_t current_frame, int16_t fx_slot_idx, int16_t target_size_class, int16_t hit_result_flags, int16_t special_flag, int16_t is_non_ship_target, int16_t fire_point_idx, int16_t reflection_state, int16_t attacker_size_class, uint16_t weapon_specials, int16_t unused_fighter_beam_flag);

    int8_t __cdecl Draw_Fighter_Beam(int16_t* fighter_coords, int16_t count, int16_t target_x, int16_t target_y, int16_t weapon_type, int16_t frame);

    void __cdecl Add_Damage_Indicator_For_Missile(int16_t x, int16_t y, uint8_t direction, int16_t damage);

    void __cdecl Add_Damage_Indicator_For_Biological(int16_t x, int16_t y, uint8_t side_flag, int16_t damage);

    void __cdecl Beam_SFX(int16_t attacker_ship_id, int16_t attacker_x, int16_t attacker_y, int16_t target_id, int16_t target_x, int16_t target_y, s_beam_fx_entry* fx_entries, int16_t is_non_ship_target);

    void __cdecl Rob_Interpolate_Line(int16_t* p_x, int16_t* p_y, int16_t target_x, int16_t target_y, int16_t step);

    void __cdecl Draw_Ship_Burst(int16_t x, int16_t y, int16_t dir, int16_t base_damage_class, int16_t frame, int16_t is_palette_not_90);

    void __cdecl Draw_Beam2(int16_t start_x, int16_t start_y, int16_t end_x, int16_t end_y, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t hit_flag, int32_t frame_index, int32_t palette_base_index, s_weapon_visual_fx fx);

    void __cdecl Set_Beam_Colors(
        int32_t palette_idx,
        int32_t r1,
        int32_t g1,
        int32_t b1,
        int32_t r2,
        int32_t g2,
        int32_t b2,
        int32_t intensity);

    void __cdecl Draw_Ship_Explosion(int16_t x, int16_t y, int16_t type_idx, int16_t frame_idx, int16_t size_idx);

    bool __cdecl XY_In_Shield_Radius(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t ship_size);

    bool __cdecl XY_In_Reflection_Field_Radius(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t ship_size);
} // namespace BEAMS