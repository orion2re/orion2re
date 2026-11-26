#pragma once
#include <cstdint>
#include <variant>

struct s_animation_header;
struct s_colony_combat_popup_data;
struct s_combat_bomb_result;
struct s_invasion_side_info;
struct s_potential_colony_info;

typedef struct s_colony_combat_popup_data s_colony_combat_popup_data, *Ps_colony_combat_popup_data;

struct s_colony_combat_popup_data {
    game::id_colony colony_idx;
    game::owner_slot attacking_faction;
    int16_t attacker_unit_counts[UNIT_COUNT];
    int8_t has_stellar_converter;
    game::owner_slot defending_faction;
    int16_t defender_unit_counts[UNIT_COUNT];
    int16_t n_pops;
    int16_t n_bldgs;
    int16_t best_shield_bldg;
    struct s_combat_bomb_result* bomb;
    struct s_invasion_side_info* attacker;
    struct s_invasion_side_info* defender;
    int32_t random_seed;
    int16_t bombardment_result;
    undefined field14_0x2d;
    undefined field15_0x2e;
    undefined field16_0x2f;
};

namespace MAINPUPS {
    extern char* g_msg_ptr;

    extern s_animation_header* anims[38];

    extern uint8_t on_officer_screen_flag;

    extern int16_t g_officer;

    extern int16_t officer_gains_level_state;

    extern int16_t last_scanned_field;
    extern int16_t g_state;
    extern int8_t selection_state_flag;
    extern uint8_t g_using_colony_ship;
    extern int16_t g_max;
    extern int16_t g_min;
    extern char* g_msg;
    extern int16_t g_var;
    extern int16_t g_temp_tax_rate;
    extern int16_t field_state;
    extern s_colors main_scr_font_colors;

    extern uint8_t g_special;
    extern int16_t g_x;
    extern int16_t g_y;
    extern int16_t g_star_n;
    extern int16_t field_mode;
    extern uint8_t no_flicker_between_mainpups_flag;
    extern int16_t autofunction_ctr;
    extern int16_t num_field_saves;
    extern s_fieldsav_data stats_array[3];
    extern uint8_t explode_flag;
    extern int16_t g_refresh_mode;
    extern MOX2::action_func_t eric_saved_autofunction2;
    extern MOX2::action_func_t eric_saved_autofunction;

    extern MOX2::action_func_t temp_af;

    extern s_colony_combat_popup_data* g_data_ptr;

    void __cdecl Load_Tax_Rate_Popup();
    void __cdecl Get_Picture_Draw_XY(s_animation_header* anim);

    void __cdecl Load_Colony_Fields();

    void __cdecl Set_G_Special_Value(int16_t star_idx);

    void __cdecl Update_Selection_Fields();

    void __cdecl Draw_Selection_Popup();

    game::id_planet __cdecl Evaluate_Selection_Input(int16_t* selected_field, int16_t* field_data, int16_t* refresh_flag);

    /**
     * Evaluates a system-display click for colony, outpost, troop-drop, or dragon selection popups.
     * @param mouse_x Current X-side field scan input.
     * @param mouse_y Current Y-side field scan input.
     * @param selection_confirmed Set to 1 when a valid target is chosen.
     */
    game::id_planet __cdecl Check_System_Display_Fields_Selection(int16_t* mouse_x, int16_t* mouse_y, int16_t* selection_confirmed);

    int8_t __cdecl New_Colony_Confirmation_Box(game::id_planet planet_idx);

    void __cdecl Load_SLIDER_Popup();
    void __cdecl Load_Slider_Fields();
    void __cdecl Add_Slider_Fields(int16_t min_val, int16_t max_val, int16_t* value_ptr);

    /**
     * Sets up popup rendering state and centers the popup art over the selected star.
     * @param star_idx Star to focus in the system display, or -1 to use the current player's home star.
     * @param popup_anim Popup art used to compute the centered draw position.
     */
    void __cdecl Do_Popup_Setup(int16_t star_idx, s_animation_header* popup_anim);

    void __cdecl Add_Tax_Rate_Fields();
    void __cdecl Eric_Pop_Fields();

    void __cdecl Load_New_Officer_Popup_For_Officer_Screen();

    void __cdecl Load_New_Officer_Popup();

    void __cdecl Update_Random_New_Officer_Fields();

    void __cdecl Draw_Random_New_Officer_Popup();

    extern int16_t mp_fields[26];

    void __cdecl Eric_Push_Fields();

    void __cdecl Add_Selection_Fields();

    void __cdecl Draw_Tax_Rate_Popup();

    void __cdecl Draw_Slider_Popup();

    void __cdecl Evaluate_Slider_Input(int16_t* input, int16_t* scanned_field, int16_t* done);

    void __cdecl Mainpups_Tested_Execute_Autofunction();

    void __cdecl Leader_Technology_String(game::id_player player, int16_t leader_idx, char* output_buf, uint32_t output_buf_size);

    void __cdecl Evaluate_Tax_Rate_Input(int16_t* input_val, int16_t* scan_val, int16_t* exit_flag);

    void __cdecl Do_Popup_Cleanup();

    void __cdecl Load_Colony_Combat_Popup();

    void __cdecl Draw_Colony_Combat_Popup();

    /**
     * Rebuilds the colony-combat popup fields for the current mode and transient combat state.
     * @param popup_mode Popup mode selector used by the combat popup loop.
     * @param combat_state_ptr Current bombing/invasion substate used to choose available actions.
     */
    void __cdecl Update_Colony_Combat_Fields(int16_t popup_mode, int16_t* combat_state_ptr);

    /**
     * Adds the actionable colony-combat buttons for the current popup mode and combat state.
     * @param popup_mode Popup mode selector used by the combat popup loop.
     * @param combat_state_ptr Current bombing/invasion substate used to choose available actions.
     */
    void __cdecl Add_Colony_Combat_Fields(int16_t popup_mode, int16_t* combat_state_ptr);

    void __cdecl Setup_To_Apply_Bomb_Hits(s_combat_bomb_result* result, int16_t* p_bomb_hits_remaining, int16_t* p_bio_hits_remaining);

    void __cdecl Apply_Next_Bomb_Hit(s_combat_bomb_result* bomb, int16_t* state, int16_t* bomb_hits, int16_t* bio_hits);

    int16_t __cdecl Evaluate_Colony_Combat_Input(int16_t* field_id_ptr, int16_t* unused_input, int16_t* close_popup_flag, int16_t* selection_idx_ptr);

    void __cdecl Load_Combat_Colony_Picture_To_Screen_Seg(game::id_colony colony_idx);

    void __cdecl Load_System_Discovery_Popup();

    void __cdecl Clear_System_Special_Slots();

    void __cdecl Update_Discovery_Fields();

    void __cdecl Draw_Discovery_Popup();

    void __cdecl Do_Evaluate_Discovery_Input(int16_t* mouse_x, int16_t* mouse_y);

    void __cdecl Evaluate_Discovery_Input(int16_t* input_1, int16_t* input_2);

    void __cdecl Marooned_Officer_Fails_Popup(int16_t star_idx, int16_t officer_idx);

    void __cdecl Marooned_Officer_Joins_Popup(int16_t star_idx, int16_t officer_idx);

    void __cdecl Build_Generic_Planet_Or_Colony_Description_String_Formatted(game::id_planet planet_idx, char* buffer, uint32_t buffer_size);

    game::id_star Dragon_New_Star_Popup(game::id_player player, game::id_star origin);

    void __cdecl Load_Dragon_New_Star_Popup();

    void __cdecl Draw_Dragon_New_Star_Popup();

    void __cdecl Update_Dragon_New_Star_Fields();

    game::id_star Evaluate_Dragon_New_Star_Input(int16_t field_idx, int16_t mouse_field, int16_t* status, game::id_star origin);

    using defense_selection = std::variant<std::monostate, game::owner_slot, game::id_colony>;
    defense_selection __cdecl Defense_Colony_Selection_Popup(int16_t star_idx, game::id_player player);

    void __cdecl Load_Colony_Defense_Selection_Popup();

    void __cdecl Draw_Defense_Selection_Popup();

    void __cdecl Update_Colony_Defense_Fields();

    defense_selection __cdecl Evaluate_Defense_Selection_Input(int16_t* field_idx, int16_t* mouse_status, int16_t* selection_made);

    /**
     * Evaluates defense-selection clicks on the system display and returns the chosen colony or hostile owner.
     * @param mouse_x Current X-side field scan input.
     * @param mouse_y Current Y-side field scan input.
     * @param selection_made Set to 1 when a valid defense target is chosen.
     */
    defense_selection __cdecl Check_System_Display_Fields_Defense_Selection(int16_t* mouse_x, int16_t* mouse_y, int16_t* selection_made);

    void __cdecl Player_Attacked_Popup(game::owner_slot attacker, game::owner_slot defender, int16_t star_idx, game::id_colony colony_idx);

    int16_t __cdecl Slider_Popup(int16_t min, int16_t max, int16_t initial_value, char* msg, int16_t popup_style);

    void __cdecl Clear_New_Officer_Slots();

    void __cdecl Add_Officer_Gains_Level_Fields();

    void __cdecl Load_Officer_Gains_Level_Popup();

    void __cdecl Update_Officer_Gains_Level_Fields();

    void __cdecl Draw_Officer_Gains_Level_Popup();

    game::id_planet __cdecl New_Colony_Selection_Popup(int16_t star_idx, uint8_t using_colony_ship);
    game::id_planet __cdecl New_Outpost_Selection_Popup(int16_t star_id);

    game::id_planet __cdecl Drop_Troops_Selection_Popup(int16_t target_idx);

    bool __cdecl Random_New_Officer_Popup(game::id_player player);

    void __cdecl New_System_Discovery_Popup(int16_t star_idx, uint8_t special_flag);

    void __cdecl Officer_Gains_Level_Popup(int16_t star_handle, int16_t officer_idx);

    const char* __cdecl System_Special_Desc(int16_t desc_id);

    s_animation_header* __cdecl System_Special_Anim(int16_t star_idx, int16_t anim_type);

    /**
     * Draws a titled system-special popup with the supplied animation and descriptive text.
     * @param x Left screen coordinate for the popup.
     * @param y Top screen coordinate for the popup.
     * @param anim Special-case illustration to draw inside the popup.
     * @param title Title text shown at the top of the popup.
     * @param text Body text shown beside the illustration.
     */
    void __cdecl Draw_Special_Info_Popup(int16_t x, int16_t y, s_animation_header* anim, const char* title, const char* text);

    int16_t __cdecl Get_Colony_Best_Shield_Bldg(game::id_colony colony_idx);

    int16_t __cdecl Colony_Selection_Popup(
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        game::id_colony colony_idx,
        int16_t bombardment_result,
        int8_t has_stellar_converter);

    void __cdecl Update_CCP_Display_Data_Invasion(
        s_colony_combat_popup_data* ccp_data,
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        game::id_colony colony_idx,
        int16_t bombardment_result,
        int8_t has_stellar_converter);

    void __cdecl Update_CCP_Display_Data_Bombing(
        s_colony_combat_popup_data* ccp_data,
        s_combat_bomb_result* bomb,
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        game::id_colony colony_idx,
        int16_t bombing_result,
        int8_t has_stellar_converter);

    /**
     * Runs the colony-combat popup loop until the user closes it or selects an action.
     * @param popup_data Display state used by the popup while it is active.
     * @param popup_mode Combat mode selector used to choose invasion or bombing behavior.
     */
    int16_t __cdecl Colony_Combat_Popup(s_colony_combat_popup_data* popup_data, int16_t popup_mode);

    int16_t __cdecl Colony_Bombing_Popup(
        s_combat_bomb_result* bomb,
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        game::id_colony colony_idx,
        int16_t bombing_result,
        int8_t has_stellar_converter);

    int8_t __cdecl Player_Has_Colony_Or_Outpost_At_Star(game::id_player player, game::id_star star);
} // namespace MAINPUPS
