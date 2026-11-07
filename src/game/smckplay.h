#pragma once

#include "smacker.h"

namespace smckplay {
    extern ::smk smk;
    extern uint32_t lastkey;

    /* Copies the current Smacker palette into the active video palette range. */
    void __cdecl Set_Smacker_Palette();

    /* Opens a Smacker animation using the requested file and Smacker open flags.
     * file_name: Path to the Smacker file to open.
     * audio_enabled: When nonzero, enables embedded animation audio.
     * options: Additional Smacker open flags.
     */
    int8_t __cdecl Open_Smacker_Animation(const char* file_name, char audio_enabled, uint32_t options);

    /* Returns the width of the currently opened Smacker animation in pixels. */
    int32_t __cdecl Get_Smacker_Animation_Width();

    /* Returns the height of the currently opened Smacker animation in pixels. */
    int32_t __cdecl Get_Smacker_Animation_Height();

    /* Draws the current Smacker frame into the offscreen surface at the given position.
     * left: Destination x coordinate.
     * top: Destination y coordinate.
     */
    void __cdecl Do_Draw_Smacker_Animation(int32_t left, int32_t top);

    /* Draws the current Smacker frame at the global Smacker draw position. */
    void __cdecl Draw_Smacker_Animation();

    /* Returns the total number of frames in the currently opened Smacker animation. */
    uint32_t __cdecl Get_Smacker_Animation_Total_Frames();

    /* Returns the current frame index of the opened Smacker animation. */
    uint32_t __cdecl Get_Smacker_Animation_Frame();

    /* Closes the currently opened Smacker animation, if any. */
    void __cdecl Close_Smacker_Animation();
} // namespace smckplay