#pragma once

#include "object_ids.h"

struct s_colors;
struct s_player;

namespace raceopt {
    struct s_race_stat_layout {
        int16_t font_style;
        int16_t line_step;
    };

    enum e_primary_pick {
        PRIMARY_PICK_GROWTH,
        PRIMARY_PICK_FOOD,
        PRIMARY_PICK_PRODUCTION,
        PRIMARY_PICK_RESEARCH,
        PRIMARY_PICK_INCOME,
        PRIMARY_PICK_SHIP_DEFENSE,
        PRIMARY_PICK_SHIP_ATTACK,
        PRIMARY_PICK_GROUND_COMBAT,
        PRIMARY_PICK_SPYING,
        PRIMARY_PICK_COUNT,
    };

    enum e_primary_choice {
        PRIMARY_CHOICE_PENALTY,
        PRIMARY_CHOICE_BONUS,
        PRIMARY_CHOICE_MAJOR_BONUS,
        PRIMARY_CHOICE_COUNT,
    };

    enum e_government_pick {
        GOVERNMENT_PICK_FEUDAL,
        GOVERNMENT_PICK_DICTATORSHIP,
        GOVERNMENT_PICK_DEMOCRACY,
        GOVERNMENT_PICK_UNIFICATION,
        GOVERNMENT_PICK_COUNT,
    };

    enum e_special_pick {
        SPECIAL_PICK_LOW_GRAVITY,
        SPECIAL_PICK_HIGH_GRAVITY,
        SPECIAL_PICK_AQUATIC,
        SPECIAL_PICK_SUBTERRANEAN,
        SPECIAL_PICK_LARGE_HOME_WORLD,
        SPECIAL_PICK_RICH_HOME_WORLD,
        SPECIAL_PICK_POOR_HOME_WORLD,
        SPECIAL_PICK_ARTIFACTS_HOME_WORLD,
        SPECIAL_PICK_CYBERNETIC,
        SPECIAL_PICK_LITHOVORE,
        SPECIAL_PICK_REPULSIVE,
        SPECIAL_PICK_CHARISMATIC,
        SPECIAL_PICK_UNCREATIVE,
        SPECIAL_PICK_CREATIVE,
        SPECIAL_PICK_TOLERANT,
        SPECIAL_PICK_FANTASTIC_TRADERS,
        SPECIAL_PICK_TELEPATHIC,
        SPECIAL_PICK_LUCKY,
        SPECIAL_PICK_OMNISCIENT,
        SPECIAL_PICK_STEALTHY_SHIPS,
        SPECIAL_PICK_TRANS_DIMENSIONAL,
        SPECIAL_PICK_WARLORD,
        SPECIAL_PICK_COUNT,
    };

    struct s_primary_pick {
        int8_t cost;
        int8_t value;
    };

    struct s_pick_rules {
        uint8_t starting;
        int8_t negative_limit;
        uint8_t mutation_bonus;
        s_primary_pick primary[PRIMARY_PICK_COUNT][PRIMARY_CHOICE_COUNT];
        int8_t government[GOVERNMENT_PICK_COUNT];
        int8_t special[SPECIAL_PICK_COUNT];
    };

    struct s_special_pick_identity {
        TRAIT trait;
        int8_t selected_value;
    };

    extern s_pick_rules pick_rules;
    extern const TRAIT primary_pick_traits[PRIMARY_PICK_COUNT];
    extern const GOVERNMENT selectable_governments[GOVERNMENT_PICK_COUNT];
    extern const s_special_pick_identity special_pick_identities[SPECIAL_PICK_COUNT];

    void Format_Primary_Pick_Value(char* output, size_t output_size, int primary_pick, int8_t value);
    void Format_Numeric_Trait(char* output, size_t output_size, int16_t trait_idx, int16_t value);
    s_race_stat_layout Race_Stat_Layout(const int8_t* traits);

    extern int16_t negative_picks;
    extern int16_t exit_ken;
    extern s_colors color_opt;
    extern char* opt_string[64];
    extern uint8_t* accept_button_seg;
    extern uint8_t* default_button_seg;
    extern int16_t option_button[22];
    extern int16_t defalt;
    extern int16_t group_button[41];
    extern int16_t accept;
    extern int16_t picks;
    extern s_animation_header* off_button_seg;
    extern int16_t option_toggle[24];
    extern int16_t button_variable[10];
    extern s_animation_header* special_button_seg;
    extern int16_t name_button;
    extern char cur_race_name[16];

    /**
     * Converts a player's stored trait values into the race-option UI state.
     * @param player Player whose trait array is decoded.
     */
    void __cdecl Convert_Player_To_Flags(s_player* player);

    /**
     * Writes the current race-option UI state back into a player's trait array.
     * @param player Player whose traits are updated.
     */
    void __cdecl Convert_Flags_To_Player(s_player* player);

    /** Adds the standard race-option screen input fields. */
    void __cdecl Add_Race_Options_Fields();

    /**
     * Computes the effective pick cost for an option after dependency rules.
     * @param ability_idx Option index in the racial ability table.
     */
    int16_t __cdecl Real_Ability_Cost(int16_t ability_idx);

    /** Recalculates the current pick total and negative-pick subtotal. */
    void __cdecl Calculate_Picks();

    /** Draws the current race-option screen state onto the active page. */
    void __cdecl Draw_Racial_Option_Screen();

    /** Draws the static background artwork and labels for the race-option screen. */
    void __cdecl Draw_Race_Option_Background();

    /** Reloads the localized option strings and formats configured primary choices. */
    void __cdecl Reload_Option_Strings();

    /** Reloads the race-option screen graphics and text resources. */
    void __cdecl Reload_Racial_Option_Screen();

    /**
     * Converts an EM trait buffer into the race-option UI state.
     * @param player_flags Trait buffer to decode.
     */
    void __cdecl Convert_Player_To_Flags_EM(int8_t* player_flags);

    /**
     * Serializes the current EM race-option UI state into a trait buffer.
     * @param flags_out Destination trait buffer.
     */
    void __cdecl Convert_Flags_To_Player_EM(int8_t* flags_out);

    /** Populates and activates the contextual help regions for the race-option screen. */
    void __cdecl Set_Race_Option_Help_List();

    /**
     * Adds the EM mutation screen input fields allowed by the baseline traits and pick budget.
     * @param baseline_flags Baseline trait selections used to validate upgrades.
     * @param available_picks Pick budget available for selectable upgrades.
     */
    void __cdecl Add_Race_Options_Fields_EM(int8_t* baseline_flags, int16_t available_picks);

    /**
     * Prepares and applies the evolutionary-upgrade selection flow for a player.
     * @param player_idx Index of the player receiving the upgrade.
     */
    void __cdecl Setup_Evolutionary_Upgrade(s_player& player);

    /**
     * Displays the evolutionary mutation picker and writes back the chosen traits.
     * @param traits Trait buffer edited by the mutation screen.
     */
    void __cdecl Evolutionary_Mutation_Screen(int8_t* traits);

    /**
     * Calculates the score modifier implied by a player's current race picks.
     * @param player_idx Index of the player to score.
     */
    int16_t __cdecl Player_Score_Modifier(game::id_player player);

    /** Recalculates picks for the EM flow, including its fixed bonus picks. */
    void __cdecl Calculate_EM_Picks();
} // namespace raceopt