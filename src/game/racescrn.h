#pragma once

struct s_player;
struct s_animation_header;

typedef struct s_racescrn_icon_layout s_racescrn_icon_layout, *Ps_racescrn_icon_layout;
typedef struct s_racescrn_mouse_group s_racescrn_mouse_group, *Ps_racescrn_mouse_group;
typedef struct s_racescrn_race_display s_racescrn_race_display, *Ps_racescrn_race_display;

struct s_racescrn_icon_layout {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    int16_t spacing_x;
    void(__cdecl* render_fn)(struct s_racescrn_mouse_group*);
};

struct s_racescrn_mouse_group {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    int16_t total_height;
    int16_t total_width;
    uint16_t count;
    int16_t field7_0xe;
    int16_t icon_width;
    int16_t spacing_x;
    struct s_animation_header* sprite1;
    struct s_animation_header* sprite2;
    void(__cdecl* render_fn)(struct s_racescrn_mouse_group*);
};

struct s_racescrn_race_display {
    int16_t list_idx;
    int16_t player_idx;
    struct s_player* player;
    int16_t race;
    int8_t is_empty_slot;
    int8_t is_eliminated;
    struct s_racescrn_mouse_group spy_icons_group;
    int16_t agent_mission_idx;
    int16_t relationship_slider_y;
    int16_t relationshiop_field_id;
    char* relationship_text;
    int16_t main_interaction_field_id;
    char* player_name;
    struct s_animation_header* race_portrait_img;
    struct s_animation_header* relationship_slider_img;
    struct s_animation_header* eliminated_overlay_img;
    struct s_animation_header* spy_icon_img;
    struct s_animation_header* sabotage_icon_img;
    struct s_animation_header* assassination_icon_img;
};

namespace RACESCRN {
    void Load_Relations_Strings();
    using render_fn = void (*)(s_racescrn_mouse_group*);

    extern s_racescrn_icon_layout mouse_group_data;
    extern int16_t g_spy_xfer_count;
    extern s_racescrn_mouse_group* g_agent_group_p;
    extern s_racescrn_mouse_group* g_mouse_group_p;
    extern int32_t net_state;
    extern s_racescrn_race_display* g_display_p;
    extern uint8_t g_in_who_mode;
    extern s_animation_header* g_mouse_seg;
    extern int16_t g_agent_field;
    extern uint8_t g_in_spy_mode;
    extern s_animation_header* arrow_seg;

    extern char trade_treaty_buffer[30];
    extern char ignoring_str[20];
    extern s_animation_header* netbox_seg;
    extern char agent_bonus_str[20];
    extern char no_contact_str[20];
    extern char declare_war_buffer[120];
    extern char spy_bonus_str[20];
    extern char giving_treaty_buffer[80];
    extern char research_treaty_buffer[30];
    extern char relation_text[17][15];
    extern uint8_t slider_remap_list[26];
    extern uint8_t relation_name_table[17];
    extern s_racescrn_icon_layout spy_group_data[MAX_PLAYERS - 1];
    extern int16_t race_bar[14];
    extern int16_t race_spies[14];
    extern int16_t race_spy_btns[14];
    extern int16_t race_slider_x[MAX_PLAYERS - 1];
    extern s_racescrn_icon_layout agent_group_data;
    extern int16_t race_picture[14];
    extern int16_t race_text[14];
    extern s_colors race_normal_color;
    extern s_colors race_high_color;
    extern s_colors banner_color_high;
    extern s_colors banner_color_low;
    extern s_colors banner_color_edge;
    extern char receiving_treaty_buffer[80];

    extern char diplomacy_net_wait_buffer[100];
    extern char diplomacy_net_requesting_buffer[100];

    /** Initializes the race display slots for the other players and the shared agent icon group.
     *  @param race_displays Output display array.
     *  @param agent_group Output icon group for unassigned agents.
     */
    void __cdecl Init_Race_Display_Data(s_racescrn_race_display* race_displays, s_racescrn_mouse_group* agent_group);

    /** Recomputes spacing and extents for an icon group.
     *  @param group Group to update.
     */
    void __cdecl Calculate_Icon_Group_Data(s_racescrn_mouse_group* group);
    /** Copies layout and sprite data into an icon group, then recalculates its derived fields.
     *  @param group Group to initialize.
     *  @param layout Source layout rectangle.
     *  @param count Initial icon count.
     *  @param sprite1 Foreground icon sprite.
     *  @param sprite2 Optional destination bitmap.
     */
    void __cdecl Setup_Icon_Group_Data(s_racescrn_mouse_group* group, s_racescrn_icon_layout* layout, int16_t count, s_animation_header* sprite1, s_animation_header* sprite2);
    /** Draws the race portraits and banner frames for each active display slot.
     *  @param race_displays Display array to render.
     */
    void __cdecl Draw_Race_Photos(s_racescrn_race_display* race_displays);
    /** Draws player names, treaty text, and spy/agent bonus text for each display slot.
     *  @param display Display array to render.
     */
    void __cdecl Draw_Race_Text(s_racescrn_race_display* display);
    /** Draws the relationship bars and slider markers for each display slot.
     *  @param displays Display array to render.
     */
    void __cdecl Draw_Relations_Sliders(s_racescrn_race_display* displays);
    /** Creates the bottom-row action buttons and returns the highest field ID.
     *  @param ok_btn_id Output OK button field ID.
     *  @param agriculture_btn_id Output action button field ID.
     *  @param agriculture_field_id Output radio state field ID.
     *  @param research_btn_id Output action button field ID.
     *  @param research_field_id Output radio state field ID.
     *  @param industry_btn_id Output action button field ID.
     *  @param industry_field_id Output radio state field ID.
     *  @param diplomacy_btn_id Output action button field ID.
     *  @param diplomacy_field_id Output radio state field ID.
     */
    int16_t __cdecl Setup_Main_Fields(int16_t* ok_btn_id, int16_t* agriculture_btn_id, int16_t* agriculture_field_id, int16_t* research_btn_id, int16_t* research_field_id, int16_t* industry_btn_id, int16_t* industry_field_id, int16_t* diplomacy_btn_id, int16_t* diplomacy_field_id);
    /** Adds per-race spy and relationship interaction fields for the main screen mode.
     *  @param race_displays Display array to update.
     *  @param base_field_id Lowest field ID to preserve.
     */
    int8_t __cdecl Setup_Race_Display_Fields(s_racescrn_race_display* race_displays, int16_t base_field_id);
    /** Draws the full race screen for the current input and mode state. */
    void __cdecl Draw_Race_Screen();

    /** Draws an icon group either directly to the screen or into its destination bitmap.
     *  @param group Group to draw.
     */
    void __cdecl Draw_Icon_Group(s_racescrn_mouse_group* group);

    /** Highlights the icons under the cursor and returns how many would be moved.
     *  @param group Group being inspected.
     *  @param x_pos Cursor X coordinate.
     *  @param color Highlight color index.
     *  @param show_count Whether to print the moved-icon count.
     */
    int16_t __cdecl Draw_Icon_Group_Cursor(s_racescrn_mouse_group* group, int16_t x_pos, uint8_t color, bool show_count);

    /** Moves the currently carried spies into the target group, respecting capacity.
     *  @param target_group Destination icon group.
     */
    void __cdecl Move_Spy_Group_From_Mouse(s_racescrn_mouse_group* target_group);
    /** Transfers up to count icons from one group to another and refreshes both layouts.
     *  @param src Source icon group.
     *  @param dst Destination icon group.
     *  @param count Maximum number of icons to move.
     */
    void __cdecl Move_Icon_Group_Icons(s_racescrn_mouse_group* src, s_racescrn_mouse_group* dst, uint16_t count);
    /** Rebuilds the mouse-carried spy bitmap, including the transfer-count overlay.
     *  @param spy_icon Icon sprite drawn into the mouse bitmap.
     */
    void __cdecl Redraw_Spy_Mouse(s_animation_header* spy_icon);
    /** Adds full-slot selection fields for the target-selection mode.
     *  @param race_displays Display array to update.
     *  @param field_id_base Lowest field ID to preserve.
     */
    int8_t __cdecl Setup_Race_Who_Fields(s_racescrn_race_display* race_displays, int16_t field_id_base);
    /** Returns the spy group associated with a field ID.
     *  @param field_id Field ID to resolve.
     *  @param out_index Output display index when a race slot is matched.
     */
    s_racescrn_mouse_group* __cdecl Get_Spy_Group_For_Field(int16_t field_id, int16_t* out_index);
    /** Moves the currently selected spy icons into the mouse group and redraws the cursor bitmap.
     *  @param mouse_group Source group to pull from.
     *  @param mouse_sprite Icon sprite drawn into the mouse bitmap.
     */
    void __cdecl Move_Spies_Group(s_racescrn_mouse_group* mouse_group, s_animation_header* mouse_sprite);
    /** Normalizes mission indices after spy transfers. */
    void __cdecl Adjust_Spy_Mission_Data();
    /** Writes the current spy counts and mission selections back to player state.
     *  @param race_displays Display array to commit.
     */
    void __cdecl Update_Spy_Stuff(s_racescrn_race_display* race_displays);

    /** Runs the race screen until the player exits. */
    void __cdecl Race_Screen();
} // namespace RACESCRN