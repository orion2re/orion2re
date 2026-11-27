#pragma once
#include "tech_grant_context.h"

struct s_animation_header;
struct s_leader_data;
namespace game { struct s_game_state; }

typedef struct s_officer_picture_slot s_officer_picture_slot, *Ps_officer_picture_slot;

struct s_officer_picture_slot {
    struct s_animation_header* pic;
    int16_t officer_id;
};

namespace OFFICER {
    extern char s_OFFICER_LBX_00556980[16];
    extern uint8_t leader_warlord_bonus;
    extern uint8_t rejected_offer_keeps_timer;
    extern uint8_t leaders_no_limit;
    extern uint8_t loknar_gives_no_tech;

    struct s_commando_hire_cost {
        uint8_t colony;
        uint8_t ship;
    };

    struct s_leader_progression {
        uint16_t experience_thresholds[5];
        struct {
            uint8_t base_amount;
            uint8_t increment_step;
            uint8_t charismatic_bonus;
        } availability;
        struct {
            uint8_t initial_delay;
            uint8_t charismatic_bonus;
            uint8_t repulsive_penalty;
        } hire_chance;
        struct {
            uint8_t advanced_government_leader_id;
            uint8_t random_skills_colony_leader_id;
            uint8_t random_skills_ship_officer_id;
        } special_leaders;
    };

    extern s_commando_hire_cost commando_hire_cost;
    extern s_leader_progression progression;
    extern uint8_t megawealth_doubles_hire_cost;

    struct s_orion_reward_technology {
        uint16_t application;
    };

    extern s_orion_reward_technology orion_reward_technology;

    s_animation_header* __cdecl Load_Officer_Picture(
        int32_t unused,
        int16_t leader_idx,
        s_animation_header* dest,
        uint8_t use_far_reload,
        int16_t pic_type);
    extern s_colors normal_text_colors;
    extern s_colors red_colors;
    extern s_colors selected_text_colors;
    extern int16_t officer_dismiss_btn;
    extern uint8_t offscrn_skip_draw_ships;
    extern uint8_t full_draw;
    extern int8_t officer_button_mode;
    extern int16_t officer_pool_btn;
    extern uint8_t offscrn_flash_incr;
    extern uint8_t offscrn_ask_for_hire_popup;
    extern game::id_ship fleet_icon_ship_id_to_move;
    extern int8_t offscrn_flash_dir;
    extern game::id_star officer_star_displayed;
    extern uint8_t offscrn_flash_value;
    extern game::id_star officer_star_chosen;
    extern s_prev_leader_data* prev_leaders;
    extern s_officer_picture_slot* ldrpict;
    extern const char* PTR_s_OFFICER_LBX;

    int16_t __cdecl Get_Officer_Base_Level(int16_t xp, int8_t warlord);

    int16_t __cdecl Get_Officer_Level(game::id_leader leader);
    inline int16_t Get_Officer_Level(int16_t catalog_slot) { return Get_Officer_Level(leader_slots::Decode(catalog_slot)); }

    int16_t __cdecl Bonus_For_Famous_Heroes(game::id_player player_idx);

    int16_t __cdecl Owned_Officer_Level(game::id_leader leader);
    int16_t Owned_Officer_Level(const game::s_game_state& state, game::id_leader leader);
    inline int16_t Owned_Officer_Level(int16_t catalog_slot) { return Owned_Officer_Level(leader_slots::Decode(catalog_slot)); }
    inline int16_t Owned_Officer_Level(const game::s_game_state& state, int16_t catalog_slot) { return Owned_Officer_Level(state, leader_slots::Decode(catalog_slot)); }
    int16_t Owned_Officer_Level(const s_leader_data& leader, bool warlord);

    int32_t __cdecl Officer_Skill_Bonus(game::id_leader leader, int16_t skill_idx);
    inline int32_t Officer_Skill_Bonus(uint32_t, int16_t catalog_slot, int16_t skill_idx) {
        return Officer_Skill_Bonus(leader_slots::Decode(catalog_slot), skill_idx);
    }

    int16_t __cdecl Officer_Skill_Value(const s_leader_data& leader);
    inline int16_t Officer_Skill_Value(const game::s_game_state& state, int16_t catalog_slot) {
        return Officer_Skill_Value(state.leaders.at(catalog_slot));
    }

    bool __cdecl Officer_Has_General_Skill(game::id_player player, game::id_leader leader, int16_t skill_id);
    inline bool Officer_Has_General_Skill(game::id_player player, int16_t catalog_slot, int16_t skill_id) {
        return Officer_Has_General_Skill(player, leader_slots::Decode(catalog_slot), skill_id);
    }

    bool __cdecl Officer_Has_Special_Skill(game::id_player player, game::id_leader leader, int16_t skill_idx);
    inline bool Officer_Has_Special_Skill(game::id_player player, int16_t catalog_slot, int16_t skill_idx) {
        return Officer_Has_Special_Skill(player, leader_slots::Decode(catalog_slot), skill_idx);
    }

    int16_t __cdecl Get_Research_Cost_Skill_Value(int32_t cost);

    void __cdecl Get_Random_Skills(uint32_t* primary_mask, uint32_t* secondary_mask, int16_t skill_class, int16_t count, int8_t randomize_class);

    void __cdecl Increase_Officer_Level_By_One(int16_t leader_idx);
    void Increase_Officer_Level_By_One(s_leader_data& leader);

    void __cdecl Officers_Screen();

    void __cdecl Deassign_Officer(game::id_leader leader);
    void Deassign_Officer(game::s_game_state& state, game::id_leader leader);
    inline void Deassign_Officer(int16_t catalog_slot) { Deassign_Officer(leader_slots::Decode(catalog_slot)); }

    void __cdecl Move_Officer_To_Pool(game::id_leader leader);
    void Move_Officer_To_Pool(game::s_game_state& state, game::id_leader leader);
    inline void Move_Officer_To_Pool(int16_t catalog_slot) { Move_Officer_To_Pool(leader_slots::Decode(catalog_slot)); }

    void __cdecl Allocate_Adv_Civ_Game_Officers();

    bool __cdecl One_Leader_With_Galactic_Lore(game::id_player player_idx);

    int16_t __cdecl Officer_Maintenance(game::id_player player, game::id_leader leader);
    int16_t Officer_Maintenance(const game::s_game_state& world, game::id_player player, game::id_leader leader);
    inline int16_t Officer_Maintenance(game::id_player player, int16_t catalog_slot) {
        return Officer_Maintenance(player, leader_slots::Decode(catalog_slot));
    }

    int32_t __cdecl Officer_Cost(game::id_player player, game::id_leader leader);
    int32_t Officer_Cost(const game::s_game_state& state, game::id_player player, game::id_leader leader);
    inline int32_t Officer_Cost(game::id_player player, int16_t catalog_slot) { return Officer_Cost(player, leader_slots::Decode(catalog_slot)); }

    bool Is_Fleet_Officer_Screen();

    void __cdecl Draw_Officer_Screen();

    void __cdecl Draw_Officer_Screen_To_Back();

    void __cdecl Reload_Officer_Screen_Pictures();

    void __cdecl Build_Captain_Id_List();

    void __cdecl Get_Officer_Pictures();

    s_animation_header* __cdecl Load_Darkened_Officer_Picture(game::id_player player_num, int16_t officer_id, void* buffer, uint8_t load_now);

    void __cdecl Load_Officer_Skill_Name_Descriptions();

    void __cdecl Reload_Offscrn_Small_Ship_Icons();

    void __cdecl Add_Officer_Screen_Fields(
        int16_t* hire_btn,
        int16_t* cancel_hire_btn,
        int16_t* ship_ldrs_btn,
        int16_t* colony_ldrs_btn,
        int16_t* return_btn,
        int16_t* help_field_1,
        int16_t* help_field_2,
        int16_t* up_btn,
        int16_t* dn_btn,
        int16_t* next_btn,
        int16_t* prev_btn,
        int16_t* big_icon_fields);

    void __cdecl Initialize_Scroll_Bar(s_scroll_bar* sb);

    void __cdecl Fade_Into_Leaders_Screen();

    void __cdecl Already_Hired_Message(int16_t officer_id);

    void __cdecl Not_Enough_BCs_Message(int16_t leader_idx, int32_t cost);

    uint8_t __cdecl Ask_For_Hire(int16_t leader_idx);

    int16_t __cdecl Do_Hire_Officer(int16_t leader_idx);

    void __cdecl Leader_Cheat_Popup();


    int8_t __cdecl Officer_Made_Level_Report(game::id_player player_idx, int8_t process_popups);
    void __cdecl Check_For_Officer_Level();
    void __cdecl Decrement_Officer_ETA();
    void __cdecl Random_Officer_Check(game::id_player player_idx);
    void __cdecl Generate_Random_Officer(game::id_player player_index);
    void __cdecl Assert_Marooned_Leaders();

    void __cdecl Kill_Officer(game::id_leader leader);
    void Kill_Officer(game::s_game_state& state, game::id_leader leader);
    inline void Kill_Officer(int16_t catalog_slot) { Kill_Officer(leader_slots::Decode(catalog_slot)); }

    void __cdecl Give_Officer_XP(game::id_player player, game::id_leader leader, int16_t xp_amount);
    inline void Give_Officer_XP(game::id_player player, int16_t catalog_slot, int16_t xp_amount) {
        Give_Officer_XP(player, leader_slots::Decode(catalog_slot), xp_amount);
    }

    void __cdecl Check_Officer_Fields(int16_t input, int16_t scanned, int16_t* officer_fields, int16_t* portrait_fields);

    int16_t __cdecl Scan_Officer_Fields(
        int16_t* field_x_a,
        int16_t* field_x_b,
        int16_t current_x,
        int16_t scan_x,
        int8_t* out_current_officer_id,
        int16_t* out_scanned_officer_id,
        int16_t* out_skill_id);

    /**
     * Scan the visible fleet big-icon fields for hover and click matches.
     * @param current_field_id Current field ID under the cursor.
     * @param scanned_field_id Newly scanned field ID from input processing.
     * @param out_current_icon_idx Receives the currently hovered icon index.
     * @param out_scanned_icon_idx Receives the clicked or activated icon index.
     */
    uint32_t __cdecl Scan_Offscrn_Big_Icon_Fields(int16_t current_field_id, int16_t scanned_field_id, int16_t* out_current_icon_idx, int16_t* out_scanned_icon_idx);

    int16_t __cdecl Officer_Screen_User_Box(const char* message, int16_t type);

    void __cdecl Dismiss_Officer(int16_t leader_idx);

    void Assign_Captain(game::id_leader officer, game::id_ship key);
    void Assign_Governor(game::id_leader officer, game::id_star key);

    int8_t Get_Ship_Leader_ETA(game::s_game_state& state, const s_player& player, game::id_leader leader, const s_ship_data& ship);

    int8_t __cdecl Get_Star_Leader_ETA(const game::s_game_state& state, game::id_player player, game::id_leader leader, int16_t star_slot);

    void __cdecl Assign_Leader_To_Ship(game::s_game_state& state, game::id_leader leader, s_ship_data* ship, int8_t eta);

    void __cdecl Ai_Assign_Leader_To_Ship(game::s_game_state& state, s_player* player, game::id_leader leader, s_ship_data* ship);

    bool __cdecl Leader_Can_Be_Assigned(int16_t leader_idx);

    char* __cdecl Leader_Name(int16_t leader_idx, char include_title, void* star_ptr = nullptr);

    void __cdecl Print_ETA_On_Officer_Picture(int16_t leader_idx, int16_t x, int16_t y, int16_t w);

    extern char* officer_skill_description_string[27];

    extern char* officer_skill_name_string[27];

    void __cdecl Text_Box_With_Refresh(const char* title, const char* body);

    /**
     * Select the localized LBX file name for officer skill text.
     * @param lbx_name Receives the selected 8.3 LBX file name.
     * @param lbx_name_size Size of the destination buffer in bytes.
     */
    void __cdecl Get_Skill_Description_LBX_Name(char* lbx_name, uint32_t lbx_name_size);

    void __cdecl Print_Officer_Skill_Help(int16_t skill_id, int16_t bonus, int16_t leader_idx);

    void __cdecl Find_Selected_Leader(int16_t leader_idx);

    void Give_Player_Loknar_Special(game::id_player player_idx, int16_t combat_star, int16_t* techs, int16_t* num_techs);

    void __cdecl Get_Available_Antaran_Tech(game::id_player player_idx, int16_t* available_techs, int16_t* available_count);

    void __cdecl Get_Random_Antaran_Tech(game::id_player player_idx, int16_t count, int16_t* tech_list, int16_t* num_techs);

    void __cdecl Rescued_Loknar_Popup(int16_t* tech_app_ids, int16_t tech_app_count);

    void __cdecl Make_Loknar_Message(char* output, uint32_t output_size, const int16_t* tech_app_ids, int16_t tech_app_count);

    int16_t __cdecl N_Fleet_Officers_Owned_By_Player(game::id_player player_idx);

    int16_t __cdecl N_Star_Officers_Owned_By_Player(game::id_player player_idx);

    int16_t __cdecl Chance_To_Hire_Hero(game::id_player player_idx);

    bool __cdecl Player_Can_Hire_More_Officers(game::id_player player_idx);

    void __cdecl Deselect_Offpop_Icons();
    void __cdecl Move_Officer_To_Limbo(int16_t officer_idx);
    void __cdecl Change_Screen_Type(int8_t* out_flag);

    void __cdecl Wrapper_For_Offscrn_System_Popup(int16_t system_id);

    void __cdecl Print_Galmap_Scanned_Ship(int16_t icon_idx);

    void __cdecl Print_Scanned_Ship_Name();

    void __cdecl Print_Officer_Data(
        int16_t officer_list_index,
        int16_t officer_id,
        int16_t text_x,
        int16_t text_y,
        int16_t portrait_x,
        int16_t portrait_y,
        int16_t portrait_width,
        int16_t portrait_status_offset_y,
        int16_t right_column_offset_x,
        int16_t unk_10);

    void __cdecl Wrapper_For_Offscrn_Data(int16_t officer_list_index, int16_t officer_id);

    bool __cdecl Leaders_In_Hiring_Pool();

    void __cdecl Get_Officer_Costs(int16_t officer_idx, int32_t* out_cost, int16_t* out_maintenance);

    void __cdecl Set_Officer_To_Player(game::id_leader officer, game::id_player player, uint8_t is_instant);
    void Set_Officer_To_Player(game::s_game_state& state, game::id_leader officer, game::id_player player, uint8_t is_instant, const game::s_tech_grant_context& context);
    inline void Set_Officer_To_Player(int16_t catalog_slot, game::id_player player, uint8_t is_instant) {
        Set_Officer_To_Player(leader_slots::Decode(catalog_slot), player, is_instant);
    }

    void __cdecl Give_Leader_Specials_To_Player(int16_t officer_idx, game::id_player player_idx);

    void __cdecl Hire_Officer(game::id_player player_idx, int16_t leader_idx);

    void __cdecl Check_For_Colony_Calculation_Update();

    void __cdecl Move_From_Limbo_To_Pool();

    void __cdecl Save_Officers_Original_Colony_Id();


    void __cdecl Draw_Colony_Box();

    void __cdecl Draw_Fleet_Box();

    void __cdecl Assign_Leader_To_Star(game::s_game_state& state, s_player* player, game::id_leader leader, s_star_data* star, int8_t eta);


    void __cdecl Load_Fleet_Galaxy_Stars();

    void __cdecl Load_Officer_Skill_Icons(void* buffer);

    uint8_t __cdecl Hire_Officer_Popup(game::id_player player_num, int16_t officer_id);

    void __cdecl Decrement_Officer_ETA();

    game::id_leader __cdecl Select_Leader_For_Hire(game::id_player player);

    int8_t __cdecl Leader_Available_For_Hire(int16_t leader_idx);


    int32_t __cdecl Calculate_Officer_Cost(game::id_player player, game::id_leader leader);
    inline int32_t Calculate_Officer_Cost(game::id_player player, int16_t catalog_slot) {
        return Calculate_Officer_Cost(player, leader_slots::Decode(catalog_slot));
    }

    bool __cdecl Marooned_Hero_At_Star_Can_Join(game::id_star star, game::id_leader* hero, game::id_player player);
    bool Marooned_Hero_At_Star_Can_Join(game::s_game_state& state, game::id_star star, game::id_leader* hero, game::id_player player, const game::s_tech_grant_context& context);

    void Ai_Assign_Leader_To_Star(game::s_game_state& world, game::id_player player, game::id_leader leader, s_star_data& star);

    void __cdecl Wrapper_For_Offscrn_Help_Fields();

    void __cdecl Add_Display_Popup_Fields(int16_t system_id);

    bool __cdecl Officer_Is_Female(int16_t officer_idx);

    void __cdecl Get_Italian_The(int16_t leader_idx, char* dest, uint32_t dest_size);

    void __cdecl Wrapper_For_Officer_Popup_Help_Fields(int16_t field_x, int16_t officer_idx);

    void __cdecl Wrapper_For_Colony_Officer_Popup_Data(int16_t x, int16_t y, int16_t officer_idx);

    void __cdecl Wrapper_To_Add_Officer_Popup_Help_Fields(
        int16_t skill_id_base,
        int16_t skill_count_minus_one,
        int16_t popup_param);

    void __cdecl Get_N_Skills_Per_Officer_Displayed(int16_t* skill_counts);

    extern s_officer_skill_help officer_skill_help[4];

    extern const char s__00551100[1];

    /**
     * Add hidden help fields for each visible officer skill row.
     * @param help_fields Receives five field IDs followed by five skill IDs.
     */
    void __cdecl Add_Skill_Description_Help_Fields(
        int16_t left,
        int16_t top,
        uint32_t unused,
        int16_t leader_index,
        int16_t* help_fields);

    void __cdecl Officer_Name(char* buffer, uint32_t buffer_size, game::id_player player_id, int16_t officer_idx);

    void __cdecl Draw_Officer_Skill_Icon(int16_t x, int16_t y, int16_t skill_idx);

    int16_t __cdecl Get_Skill_Data_Max_Pixel_Width(int16_t leader_idx);

    int16_t __cdecl Get_Advanced_Government_Type(game::id_player player_idx);
} // namespace OFFICER
