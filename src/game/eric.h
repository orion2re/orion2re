#pragma once
#include <cstdint>

struct s_animation_header;

namespace ERIC {
    struct s_fade_speed {
        uint8_t fade_in;
        uint8_t fade_out;
    };

    extern uint8_t fade_percent;

    extern s_fade_speed fade_speed;

    extern uint8_t hacky_need_to_fade_to_colony_palette_flag;

    extern uint16_t saved_sfx_l;

    extern uint16_t saved_music_l;

    extern MOX2::action_func_t cinematic_saved_autofunction;
    extern uint8_t mind_control_ship_class;

    /* Initializes Eric-related state and dependent colony/event systems. */
    void __cdecl Eric_Init();

    /* Resets Eric-local defaults. */
    void __cdecl Eric_Defaults();

    /* Returns the index of value in list, or -1 when it is absent.
       value: Value to search for.
       list: int16_t candidate values to scan; may be misaligned.
       count: Number of entries in list. */
    int16_t __cdecl Get_Sint_Index_From_List(int16_t value, const void* list, uint16_t count);

    /* Returns the current screen fade percentage. */
    uint16_t __cdecl Get_Fade_Percent();

    /* Quickly fades the current palette in to full brightness. */
    void __cdecl Fast_Fade_In();

    /* Quickly fades the current palette out to black. */
    void __cdecl Fast_Fade_Out();

    /* Returns whether the next screen transition should fade to the colony palette. */
    bool __cdecl Fast_Fade_Out_Needed_For_Colony_Palette();

    /* Sets the current fade percentage and refreshes the mouse cursor when fully visible. */
    void __cdecl Set_Fade_Percent(int16_t percent);

    /* Prints text centered within the given rectangle.
       x/y: Top-left corner of the box.
       width/height: Box dimensions.
       text: Paragraph text to render. */
    void __cdecl Print_Paragraph_Centered(int16_t x, int16_t y, int16_t width, int16_t height, const char* text);

    /* Prints text vertically centered within the given box height.
       x/y: Text box origin.
       width: Paragraph width.
       box_height: Height available for centering.
       text: Paragraph text to render.
       color: Paragraph color index. */
    void __cdecl Print_Paragraph_Centered_Vertically(int16_t x, int16_t y, int16_t width, uint16_t box_height, char* text, int16_t color);

    /* Returns a clock-driven cycling value in the range [0, modulus).
       divisor: Clock divisor controlling cycle speed.
       modulus: Number of cycle states. */
    int16_t __cdecl Global_Cycler(int16_t divisor, int16_t modulus);

    /* Draws an animation centered on the given point. */
    void __cdecl Draw_Centered(int16_t x, int16_t y, s_animation_header* anim);

    /* Draws an animation bottom-centered on the given point. */
    void __cdecl Draw_Bottom_Centered(int16_t x, int16_t y, s_animation_header* animation);

    /* Draws an animation bottom-centered without glass effects. */
    void __cdecl Draw_Bottom_Centered_No_Glass(int16_t x, int16_t y, s_animation_header* animation);

    /* Builds formatted text into dest, either replacing or extending the current contents.
       dest: Destination buffer.
       dest_size: Size of dest in bytes.
       build_mode: 0 replaces dest; nonzero appends using the localized join format.
       format: printf-style source format string. */
    void __cdecl String_Builder2(char* dest, int32_t dest_size, int16_t build_mode, const char* format, ...);

    /* Returns the format prefix used when a numeric value should display as negative-colored text. */
    const char* __cdecl Red_If_Negative_Fmt_String(int16_t value);

    /* Draws a boxed highlight and cycles its palette entry.
       x1/y1/x2/y2: Box bounds.
       box_color: Box border color.
       reserved_arg: Preserved unused parameter from the original interface.
       cycle_delay: Palette cycle timing value. */
    void __cdecl Pulsing_Box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t box_color, int32_t reserved_arg, int16_t cycle_delay);

    /* Plays the defeat cinematic. */
    void __cdecl Play_Loose_Flic();

    /* Plays the standard victory cinematic. */
    void __cdecl Play_Normal_Win_Flic();

    /* Plays the all-victories cinematic sequence. */
    void __cdecl Play_All_Flics();

    /* Plays the Orion capture cinematic. */
    void __cdecl Play_Take_Orion_Flic();

    /* Draws the cinematic popup background into the offscreen page. */
    void __cdecl Cinematic_Autofunction();

    /* Saves state and prepares the cinematic popup environment. */
    void __cdecl Do_Cinematic_Popup_Setup();

    /* Restores state after a cinematic popup finishes. */
    void __cdecl Do_Cinematic_Popup_Cleanup();

    /* Switches the active screen refresh mode. */
    void __cdecl Set_Refresh_Mode(int16_t mode);

    /* Plays the dimensional portal cinematic. */
    void __cdecl Play_Dimensional_Portal_Flic();

    /* Plays the planetary destruction cinematic. */
    void __cdecl Play_Planetary_Destruction_Flic();

    /* Plays the Antarans attack cinematic. */
    void __cdecl Play_Antarans_Attack_Flic();

    /* Returns whether value appears in list. */
    bool __cdecl Sint_Is_In_List(int16_t value, int16_t* list, uint16_t count);

    /* Plays the diplomatic victory cinematic sequence. */
    void __cdecl Play_Diplomatic_Win_Flic();

    /* Plays the space amoeba cinematic. */
    void __cdecl Play_Amoeba_Flic();

    /* Returns whether the player has a ship of at least the requested size at the star. */
    int8_t __cdecl Player_Has_Ship_Size_Or_Larger_At_Star(int16_t player_idx, int16_t minimum_size, int16_t star_idx);

    /* Returns whether the player has a leader with the given general skill at the star. */
    int8_t __cdecl Player_Has_Leader_With_General_Skill_At_Star(int16_t player_idx, int16_t skill_idx, int16_t star_idx);

    /* Returns whether the player can mind-control the target colony. */
    int8_t __cdecl Player_Can_Mind_Control_Colony(int16_t player_idx, game::id_colony colony_idx);

    /* Returns whether the player can mind-control the colony, showing help text on failure. */
    int8_t __cdecl Player_Can_Mind_Control_Colony_With_Help(int16_t player_idx, game::id_colony colony_idx);

    /* Forces colony-palette fade handling on the next transition. */
    void __cdecl Force_Fade_To_Colony_Palette(uint8_t flag);

    /* Returns the player's capitol colony, or a random owned colony if none is flagged. */
    game::id_colony __cdecl Player_Capitol_World(int16_t player_idx);

    /* Returns a random colony owned by the requested player. */
    game::id_colony __cdecl Pick_Random_Colony(int16_t owner_idx);
} // namespace ERIC
