#pragma once
#include <cstdint>

namespace COLLAND {
    extern s_animation_header* l_anims;
    extern int32_t pod1l_sfx;
    extern int32_t pod2d_sfx;
    extern int32_t pod2l_sfx;
    extern int32_t pod1d_sfx;
    extern int16_t fields;

    /** Loads the landing-screen animation sequence from the colony LBX. */
    void __cdecl Load_Anims();
    /** Registers the hidden full-screen input field used by the landing screen. */
    void Add_Fields();

    /** Draws and updates the colony landing animation screen and related UI text. */
    void __cdecl Draw_Colony_Landing_Screen();

    /** Prints the selected planet's summary text for the landing screen. */
    void __cdecl Print_Planet_Data(game::id_planet planet_idx);

    /** Runs the colony landing screen loop, including palette setup, animation, and exit handling. */
    void __cdecl Colony_Landing_Screen();
} // namespace COLLAND
