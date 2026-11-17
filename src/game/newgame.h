#pragma once
#include <cstdint>

namespace NEWGAME {
    extern int16_t g_x;
    extern int16_t g_y;
    extern s_colors color_ken;
    extern int16_t opp_button_variable;
    extern int16_t diff_button_variable;
    extern int16_t xenon_attacks_button_variable;
    extern int16_t civ_button_variable;
    extern uint16_t combat_button_variable;
    extern int16_t gal_size_button_variable;
    extern int16_t random_events_button_variable;
    extern int16_t gal_age_button_variable;
    extern int16_t fields[10];
    extern s_animation_header* anims[7];
    extern s_animation_header* tech_level_anims[STARTING_CIVILIZATION_LEVEL_COUNT];
    extern s_animation_header* difficulty_anims[GAME_DIFFICULTY_COUNT];
    extern s_animation_header* num_players_anims[MAX_PLAYERS];
    extern s_animation_header* gal_age_anims[GALAXY_AGE_COUNT];
    extern s_animation_header* gal_size_anims[GALAXY_SIZE_COUNT - 1];
    extern s_animation_header* background_seg;
    extern char* gal_size_button_strings[GALAXY_SIZE_COUNT - 1];
    extern char* civ_button_strings[STARTING_CIVILIZATION_LEVEL_COUNT];
    extern char* diff_button_strings[GAME_DIFFICULTY_COUNT];
    extern char* gal_age_button_strings[GALAXY_AGE_COUNT];
    extern char* combat_button_strings[2];
    extern char localized_gal_size_label[20];
    extern char localized_postwarp_label[20];
    extern char localized_random_label[20];
    extern char* opp_button_strings[MAX_PLAYERS];

    /** Returns the fixed count or Random marker requested for game initialization. */
    int16_t Player_Count_For_Initialization();

    /**
     * Loads the localized labels used by the new game option controls.
     */
    void __cdecl Load_New_Game_Strings();

    /**
     * Reloads the new game background and option animations from LBX resources.
     */
    void __cdecl Reload_Newgame_Screen();

    /**
     * Copies persisted game settings into the screen-local selection variables.
     */
    void __cdecl Globals_To_Temps();

    /**
     * Rebuilds the interactive fields for the current new game options.
     */
    void __cdecl Update_Fields();

    /**
     * Draws the preview pictures for the currently selected options.
     */
    void __cdecl Draw_Pictures();

    /**
     * Draws the full new game screen for the current selection state.
     */
    void __cdecl Draw_Newgame_Screen();

    /**
     * Copies the screen-local selections back into the persisted game settings.
     */
    void __cdecl Temps_To_Globals();

    /**
     * Runs the new game setup screen and returns whether setup was accepted.
     */
    int16_t __cdecl Newgame_Screen(game::s_world_state* world_state);
} // namespace NEWGAME