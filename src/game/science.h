#pragma once

namespace SCIENCE {
    extern uint8_t finds_presented_by_scientist;

    extern const char* spy_anim_fmt;
    extern const char* trooper_anim_fmt;
    extern const char* spy_anim_find_pat;
    extern const char* trooper_anim_find_pat;

    extern s_colors science_high_color;

    extern s_colors science_normal_color;

    extern int16_t bm_scale_x[5];

    extern int16_t bm_scale_y[5];

    extern int16_t bm_scaled_x1[5];

    extern int16_t bm_scaled_y1[5];

    extern void* science_mouse_bitmap;

    /* Displays the science-room sequence for a stolen technology.
     * tech_idx: Technology id to present.
     * other_player_idx: Other player referenced by the decoded message.
     */
    void __cdecl Show_Off_Stolen_Tech(uint16_t tech_idx, int16_t other_player_idx);

    /* Presents a technology found on an Ancient Artifacts planet. */
    void __cdecl Show_Off_Artifact_Tech(uint16_t tech_idx, ::game::id_planet planet_idx);

    /* Draws the current science-room animation layers for the active frame.
     * unused_arg: Unused legacy callback-style argument.
     * draw_multi_anim: Non-zero to draw the intro/discovery multi-file animation.
     * draw_anim_2: Non-zero to draw SCIENCE.LBX animation 2.
     * draw_anim_1: Non-zero to draw SCIENCE.LBX animation 1.
     */
    void __cdecl Update_Science_Anims(int32_t unused_arg, uint8_t draw_multi_anim, uint8_t draw_anim_2, uint8_t draw_anim_1);

    /* Auto-function hook used while the science room screen is active. */
    void __cdecl Draw_Science_Room();

    /* Runs the science-room presentation loop for one or more discoveries.
     * intro_animation_name: Intro animation LBX name.
     * discovery_count: Number of discovery entries to process.
     * discovery_data: Packed discovery entry data for the presentation.
     * footer_text: Footer paragraph shown under the discovery text.
     */
    void __cdecl Science_Room(char* intro_animation_name, uint16_t discovery_count, int16_t* discovery_data, char* footer_text);

    /* Shows the science-room sequence for the active player's completed research.
     * player_idx: Player whose newly completed research should be presented.
     */
    void __cdecl Show_Off_Researched_Tech(int16_t player_idx);

    /* Shows the science-room sequence for captured technologies.
     * captured_tech_ids: Packed captured technology entry data.
     * tech_count: Number of entries in captured_tech_ids.
     * message_arg: Text decoding argument for the footer message.
     */
    void __cdecl Show_Off_Captured_Tech(int16_t* captured_tech_ids, uint16_t tech_count, int16_t message_arg);
} // namespace SCIENCE