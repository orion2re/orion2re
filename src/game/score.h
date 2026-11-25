#pragma once
#include <cstdint>
#include "object_ids.h"

struct s_hof;
struct s_animation_header;
struct s_colors;

typedef struct s_hiscore s_hiscore, *Ps_hiscore;

struct s_hiscore {
    struct s_animation_header* mouse_cursor_anim;
    struct s_animation_header* main_background_anim;
    struct s_animation_header* report_bitmap;
    struct s_animation_header* race_icon_anims[14];
    struct s_animation_header* race_icon_frame_anim;
    struct s_animation_header* background_anim;
    int16_t screen_field_handle;
    int16_t current_input_state;
    int16_t exit_hotkey_handle;
    int16_t prev_input_state;
    int16_t entry_field_handles[10];
    game::id_player player;
    int16_t score_antares;
    uint8_t is_first_draw;
    int16_t score_captured_colonists;
    struct s_colors colors;
    int16_t score_elimination;
    struct s_colors temp_colors;
    struct s_colors alt_colors;
    int16_t num_opponents_defeated;
    int16_t raw_captured_population;
    int16_t raw_captured_colonies;
    int8_t captured_pop_race_ids[16];
    int16_t reg_pop;
    int16_t raw_population;
    int16_t score_orion;
    int16_t score_population;
    int16_t raw_technology;
    int16_t score_technology;
    int16_t score_raw_subtotal;
    int16_t score_time;
    int16_t score_final;
    int16_t score_council;
    int16_t next_draw_y;
    int16_t timer_ticks;
    int16_t score_modifier_pct;
    int8_t flash_color_offset;
    int8_t flash_direction;
    uint8_t flash_timer_ticks;
    int8_t highlighted_entry_idx;
    undefined field40_0xbc;
    struct s_hiscore_pop pop_units[16];
    int16_t sort_list[10][2];
};

namespace SCORE {
    struct s_victory_bonus {
        uint32_t orion;
        uint32_t antares;
        uint32_t council;
    };

    extern s_hof hof;
    extern char default_names[10][20];

    extern s_hiscore* hi;

    extern int16_t entry_to_flash;

    extern char difficulty_string[GAME_DIFFICULTY_COUNT][15];
    extern s_victory_bonus victory_bonus;

    /** Returns the hall-of-fame file version, or 0 if the file cannot be read. */
    int16_t __cdecl HOF_Version_Number();

    /** Creates a default hall-of-fame file with shuffled placeholder entries. */
    void __cdecl Create_Default_HOF_File();

    /** Reads and normalizes the hall-of-fame file into SCORE::hof. */
    int8_t __cdecl Read_HOF_File();

    /** Loads hall-of-fame screen assets and allocates the shared screen state. */
    void __cdecl Load_Hall_Of_Fame_Screen();

    /** Initializes hall-of-fame fields, colors, and sorted entry state. */
    void __cdecl Init_Hall_Of_Fame_Screen();

    /** Compares two {score,index} pairs in descending order for qsort. */
    int __cdecl qsort_hof_scores_descending(const void* a, const void* b);

    /** Compares two {score,index} pairs in ascending order for qsort. */
    int __cdecl qsort_hof_scores_ascending(const void* a, const void* b);

    /** Loads the hall-of-fame font colors and flash state. */
    void __cdecl Get_Hall_Of_Fame_Colors();

    /** Draws the hall-of-fame screen and any flashing entry highlight. */
    void __cdecl Draw_Hall_Of_Fame_Screen();

    /** Runs the end-of-game high-score screen and returns the final score. */
    int16_t __cdecl Hi_Score_Screen(game::id_player player);

    /** Builds a sortable {score,index} list from the current hall-of-fame data. */
    void __cdecl Sort_HOF_Scores(int16_t sort_list[10][2]);

    /** Reorders hall-of-fame records according to a sorted {score,index} list. */
    void __cdecl Move_Sorted_HOF_Data(int16_t sort_list[10][2]);

    /** Writes the current hall-of-fame data to disk. */
    void __cdecl Write_HOF_File();

    /** Draws the time-score row. */
    void __cdecl Draw_Time_Score();

    /** Draws one labeled score row with dotted fill and a right-aligned value. */
    void __cdecl Print_Score_String(int16_t x, int16_t y, int16_t score, const char* label);

    /** Draws the population-score row. */
    void __cdecl Draw_Population_Score();

    /** Draws a generic score row and then invokes an optional detail callback. */
    void __cdecl Draw_Generic_Score(int16_t value, const char* label, void(__cdecl* callback)(), int width);

    /** Draws defeated-race portraits for the elimination score section. */
    void __cdecl Draw_Eliminated_Race_Pictures();

    /** Draws the elimination-score row. */
    void __cdecl Draw_Elimination_Score();

    /** Draws the captured-colonies score row. */
    void __cdecl Draw_Captured_Colonies_Score();

    /** Draws captured population icons for the captured-colonies section. */
    void __cdecl Draw_Cap_Pop();

    /** Draws the technology-score row. */
    void __cdecl Draw_Technology_Score();

    /** Draws the Orion-score row. */
    void __cdecl Draw_Orion_Score();

    /** Draws the Antares-score row. */
    void __cdecl Draw_Antares_Score();

    /** Draws the council-victory score row when applicable. */
    void __cdecl Draw_Council_Score(void);

    /** Draws the total-score summary row. */
    void __cdecl Draw_Total_Score();

    /** Draws population icons for either regular or captured population. */
    void __cdecl Draw_Pop(uint8_t is_captured, int16_t max_pop, int16_t current_pop);

    /** Draws the regular-population icon section. */
    void __cdecl Draw_Reg_Pop();

    /** Draws the full high-score report screen. */
    void __cdecl Draw_Hi_Score_Screen();

    /** Loads the high-score report screen assets and buffers. */
    void __cdecl Load_Hi_Score_Screen();

    /** Initializes the high-score report color sets. */
    void __cdecl Get_Hi_Score_Colors();

    /** Adds the hidden input fields used by the high-score screen. */
    void __cdecl Add_Hi_Score_Screen_Fields();

    /** Initializes population/icon state for a player's high-score report. */
    void __cdecl Init_Hi_Score(game::id_player player);

    /** Returns the total colony population owned by the specified player. */
    int16_t __cdecl Get_Total_Population(game::id_player player);

    /** Chooses population icon race/icon pairs for the player's score report. */
    void __cdecl Set_Hi_Score_Pop_Icon_Ids(game::id_player player);

    /**
     * Selects one matching temporary population entry for a race and returns its colony/pop indices.
     * @param tmp_pop_list Temporary population entries to search.
     * @param race_idx Race to match.
     * @param entry_count Number of entries in tmp_pop_list.
     * @param out_colony_idx Receives the selected colony index.
     * @param out_pop_idx Receives the selected population index.
     */
    void __cdecl Get_From_Tmp_Pop(s_hiscore_tmp_pop* tmp_pop_list, uint16_t race_idx, int16_t entry_count, game::id_colony* out_colony_idx, int16_t* out_pop_idx);

    /** Compares two uint8_t values in ascending order for qsort. */
    int __cdecl ascending_qsort_uchar(const void* a, const void* b);

    /** Builds the captured-colonist race list shown in the score report. */
    void __cdecl Build_Captured_Colonists_Array(game::id_player player);

    /** Updates the hall of fame after the end-of-game score screen completes. */
    void __cdecl End_Of_Game_Hi_Score(game::id_player player);

    /** Runs the hall-of-fame screen, optionally flashing the specified entry. */
    void __cdecl Hall_Of_Fame_Screen(int16_t entry_to_flash_idx);

    /** Calculates a player's final score without opening the score UI. */
    int16_t __cdecl Do_Calculate_Hi_Score(game::id_player player);

    /** Calculates and stores all high-score components for the specified player. */
    void __cdecl Calculate_Hi_Scores(game::id_player player);

    /** Returns the elimination score for the specified player. */
    int16_t __cdecl Get_Elimination_Score(game::id_player player);

    /** Returns the technology score for the specified player. */
    int16_t __cdecl Get_Technology_Score(game::id_player player);

    /** Counts researched technologies and hyper technologies for a player. */
    void __cdecl Get_Technology(game::id_player player, int16_t* tech_count, int16_t* hyper_tech_count);

    /** Returns non-zero if the player captured Antares. */
    int8_t __cdecl Captured_Antares(game::id_player player);

    /** Returns the Antares bonus score for the specified player. */
    int16_t __cdecl Get_Antares_Score(game::id_player player);

    /** Returns non-zero if the player captured Orion. */
    int8_t __cdecl Captured_Orion(game::id_player player);

    /** Returns the Orion bonus score for the specified player. */
    int16_t __cdecl Get_Orion_Score(game::id_player player);

    /** Returns the captured-colonist score for the specified player. */
    int32_t __cdecl Get_Captured_Colonist_Score(game::id_player player);

    /** Returns and stores the total population score for the specified player. */
    int16_t __cdecl Get_Population_Score(game::id_player player);

    /** Returns the time-based score component for the current game state. */
    int16_t __cdecl Get_Time_Score();

    /** No-op callback used by field automation. */
    void __cdecl nullsub();

    /** Returns non-zero if the player won the council vote. */
    int8_t __cdecl Won_Council_Vote(game::id_player player);

    /** Returns the council-victory bonus score for the specified player. */
    int16_t __cdecl Get_Won_Council_Score(game::id_player player);

    /** Returns the fully calculated score for the specified player. */
    int16_t __cdecl Player_Score(game::id_player player);
} // namespace SCORE
