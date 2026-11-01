#pragma once
#include <cstddef>
#include <cstdint>

struct s_list_item_research;
struct s_list_item;
struct s_list_state;
struct s_colors;
struct s_animation_header;
struct s_fieldsav_data;
struct s_player;

typedef struct s_list_item_research s_list_item_research, *Ps_list_item_research;

struct s_list_item_research {
    int16_t x;
    int16_t y;
    int16_t id;
    uint8_t current_app_index;
    int16_t selected_app_index;
    char field_name[62];
    uint8_t group_id;
    uint8_t group_color_index;
    int16_t button_field_id;
    int16_t button_value;
    struct s_animation_header* button_image;
    int16_t tech_field_id;
    int32_t research_cost_remaining;
    char buf[20];
    int8_t visible_app_count;
    char app_names[4][64];
    int16_t label_x;
    int16_t app_label_y[4];
    int16_t app_click_field_ids[4];
    int16_t app_ids[4];
    int16_t list_win_x;
};

namespace TECH {
    extern bool allow_future_technology_browsing;

    struct s_research_choice {
        int16_t field = 0;
        int16_t applications[4] = {};
        int16_t count = 0;
    };

    // The entry builder excludes the current field; change-research includes it.
    bool Read_Research_Choice(const s_player* player, int16_t group, bool include_current, s_research_choice* choice);
    void Copy_Application_Name(int16_t application, int16_t player, char* buffer, size_t size);

    /** Opens the technology selection screen for the current player. */
    void __cdecl Tech_Select();

    /** Displays the technology screen in select or change mode.
     * @param changing_tech Nonzero to open the change-research variant.
     */
    void __cdecl _Tech_Select(uint8_t changing_tech);

    /** Restores palette, help, and audio state after leaving the technology screen. */
    void __cdecl Reset_Screen();

    /** Returns the research cost for a field or application.
     * @param id Technology field or application identifier.
     * @param is_tech_app Nonzero when @p id names an application instead of a field.
     * @param player_idx Player whose cost modifiers should be used.
     */
    int32_t __cdecl New_Get_Tech_Cost(int16_t id, uint8_t is_tech_app, int16_t player_idx);

    extern int32_t tech_select_pos[8];
    extern int32_t tech_change_pos[8];
    extern int16_t app_name_y[8];
    extern int16_t list_window_x_offsets[8];
    extern int16_t tech_button_pos[16];
    extern uint8_t entry_to_group[8];
    extern s_colors tech_select_colors[5];
    extern s_colors tech_change_colors[5];
    extern int16_t g_scrn_y;
    extern int16_t g_scrn_x;
    extern int16_t g_first_tech_field;
    extern int16_t g_last_tech_field;
    extern uint8_t g_cursor_box_color;
    extern uint8_t g_changing_tech;
    extern s_list_item_research* g_selected_entry_p;
    extern s_list_item_research* g_entries_p;
    extern s_colors tech_color[5];
    extern s_list_state g_tech_list_header;
    extern s_fieldsav_data tech_field_save;
    extern s_list_item* g_selected_list_entry_p;
    extern char tech_group_names[10][20];

    /** Initializes the fixed on-screen positions for the eight research entries. */
    void __cdecl Init_Entry_Positions(s_list_item_research* entries);
    /** Builds the visible research-entry data for the current player.
     * @param entries Destination array of eight research entries.
     * @param research_cost_offset Research already accumulated toward the displayed field.
     */
    int16_t __cdecl Init_Entry_Data(s_list_item_research* entries, int32_t research_cost_offset);
    /** Creates the per-entry radio-button fields for selectable research entries. */
    void __cdecl Setup_Entry_Buttons(s_list_item_research* entries, s_animation_header** images);
    /** Draws the main technology selection screen state. */
    void __cdecl Draw_Tech_Select();
    /** Draws one research entry's field, cost, and visible applications. */
    void __cdecl Display_Entry_Text(s_list_item_research* item);
    /** Draws all eight research entries. */
    void __cdecl Display_All_Entry_Text(s_list_item_research* entries);
    /** Marks the entry matching a clicked field id as selected. */
    s_list_item_research* __cdecl Set_Selected_Entry(s_list_item_research* research_list, int16_t field_id);
    /** Returns the currently selected research entry, if any. */
    s_list_item_research* __cdecl Get_Selected_Entry(s_list_item_research* entries);
    /** Commits a selected research entry to a player, if one exists. */
    bool __cdecl Commit_Selected_Entry(
        s_player* player, const s_list_item_research* selected_entry);
    /** Redraws the selected research entry highlight state. */
    void __cdecl Display_Selected_Entry_Text(s_list_item_research* entry);
    /** Draws the selection arrow next to the active application line. */
    void __cdecl Draw_Little_Arrow(s_list_item_research* entry);
    /** Shows the help text and displayed cost for a technology application.
     * @param unused_x Retained legacy x-coordinate parameter; currently unused.
     * @param app_id Technology application to describe.
     */
    void __cdecl Draw_Application_Description(int32_t unused_x, uint16_t app_id);
    /** Opens the grouped technology list popup for a selected entry.
     * @param entry Entry whose group should be listed.
     * @param is_change_mode Nonzero when opened from the change-research screen.
     */
    int16_t __cdecl _Tech_List(s_list_item_research* entry, uint8_t is_change_mode);

    /** Populates the list popup with all visible technologies in a group. */
    void __cdecl Get_Group_List(s_list_state* list, int16_t group_id);

    /** Draws the grouped technology list popup state. */
    void __cdecl Draw_Tech_List();

    /** Opens the change-research flow from the current screen. */
    void __cdecl Tech_Change();

    /** Returns the display name for a technology application.
     * @param tech_app_id Technology application identifier.
     * @param player_idx Player whose hyper-tech suffix should be reflected in the name.
     */
    char* __cdecl Technology_Applications_Name(int16_t tech_app_id, int16_t player_idx);
    /** Maps a hyper-tech field id to its synthetic application id. */
    int16_t __cdecl Get_Hyper_Tech_App_ID(int16_t tech_field_id);
    /** Returns the display name for a technology field. */
    char* __cdecl Technology_Fields_Name(int16_t tech_field_id);
    extern int16_t tech_field_y1[4];
    extern int16_t tech_field_y2[4];
} // namespace TECH