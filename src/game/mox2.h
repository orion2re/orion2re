#pragma once

namespace MOX2 {
    /** Seeds the game RNG from the command-line override or randomized startup state. */
    void __cdecl Set_Game_Random_Seed();

    /** Convenience wrapper that reseeds the game RNG. */
    inline void __cdecl Set_Game_Seed() {
        Set_Game_Random_Seed();
    }

    /** Resets map-related globals and screen state to their default values. */
    void __cdecl Init_Map_Defaults();
    /** Loads default nebula art, then resets the map state. */
    void __cdecl Init_Map_Pictures_Default_();
    extern uint8_t user_wants_n_space_monsters;
    extern uint8_t pan_main_menu_panel;
    extern uint8_t skip_intro;
    extern bool show_exit_message;
    extern uint8_t quick_start;
    extern bool use_explicit_game_directory;
    extern char leave_game_message[100];

    /** Reads and validates the language selected in language.ini, falling back to English. */
    void __cdecl Set_Mox_Language();

    /** Reads an optional CD fallback path from orioncd.ini and checks for DIPLOMAT.LBX. */
    void __cdecl Set_Mox_Alt_Path();

    /** Runs the main screen-dispatch loop until the exit screen is selected. */
    void __cdecl Screen_Control();

    extern const char s_language_ini[13];
    extern const char s_language_ini_write_mode[3];
    /** Performs startup, enters the main screen loop, and exits with a localized leave message. */
    void __cdecl main2();
    /**
     * Adapts main2 to a callback-style entry point.
     * @param unused Unused callback context.
     */
    int SDLCALL thunk_main2(void* unused);
} // namespace MOX2

namespace MOX2 {
    typedef void(__cdecl* action_func_t)();

    /**
     * Stores the current auto-update callback in the caller-provided slot.
     * @param function_slot Receives the current callback pointer.
     */
    void __cdecl Get_Auto_Function(action_func_t* function_slot);

    /**
     * Invokes the callback stored in the caller-provided slot if one is present.
     * @param function_entry Points to the callback pointer to execute.
     */
    void __cdecl Execute_Auto_Function(action_func_t* function_entry);

    /**
     * Replaces the current auto-update callback with the caller-provided one.
     * @param function_slot Supplies the callback pointer to store.
     */
    void __cdecl Put_Auto_Function(action_func_t* function_slot);
} // namespace MOX2
