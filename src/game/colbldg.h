#pragma once
#include <cstdint>

struct s_animation_header;
struct s_colors;
struct s_mouse_list;
struct s_ship_data;
struct s_ship_design;
struct s_ship_weapons;

namespace STRBUILD {
    struct s_builder;
}

namespace COLBLDG {
    enum e_build_list_color : uint8_t {
        BUILD_LIST_COLOR_DEFAULT,
        BUILD_LIST_COLOR_ACCENT_1,
        BUILD_LIST_COLOR_ACCENT_2,
        BUILD_LIST_COLOR_ACCENT_3,
        BUILD_LIST_COLOR_ACCENT_4,
        BUILD_LIST_COLOR_ACCENT_5,
        BUILD_LIST_COLOR_ACCENT_6,
        BUILD_LIST_COLOR_ACCENT_7,
        BUILD_LIST_COLOR_ACCENT_8,
        BUILD_LIST_COLOR_ACCENT_9,
        BUILD_LIST_COLOR_COUNT,
    };

    enum e_build_list_highlight : uint8_t {
        BUILD_LIST_HIGHLIGHT_NORMAL,
        BUILD_LIST_HIGHLIGHT_QUEUED,
        BUILD_LIST_HIGHLIGHT_COUNT,
    };

    enum e_build_list_product : uint8_t {
        BUILD_LIST_PRODUCT_TRADE_GOODS,
        BUILD_LIST_PRODUCT_HOUSING,
        BUILD_LIST_PRODUCT_BUILDING_BASE,
        BUILD_LIST_PRODUCT_COUNT = BUILD_LIST_PRODUCT_BUILDING_BASE + BUILDING_COUNT - 1,
    };

    struct s_build_list_color {
        uint8_t base;
        uint8_t dim;
        uint8_t dark;
    };

    struct s_build_list_format {
        int8_t order;
        e_build_list_color color;
    };

    extern s_build_list_color
        build_list_colors[BUILD_LIST_COLOR_COUNT][BUILD_LIST_HIGHLIGHT_COUNT];
    extern s_build_list_format build_list_format[BUILD_LIST_PRODUCT_COUNT];

    static inline bool Colony_Production_Is_Building(int16_t production_id) {
        return production_id > BUILDING_NO_BUILDING && production_id < BUILDING_COUNT;
    }

    static inline bool Colony_Production_Is_Queued_Ship(int16_t production_id) {
        return production_id <= COLONY_PRODUCTION_QUEUED_SHIP_BASE;
    }

    static inline bool Colony_Production_Is_Ship_Design(int16_t production_id) {
        return production_id > COLONY_PRODUCTION_QUEUED_SHIP_BASE &&
            production_id <= COLONY_PRODUCTION_SHIP_DESIGN_BASE;
    }

    static inline bool Colony_Production_Is_Ship(int16_t production_id) {
        return Colony_Production_Is_Queued_Ship(production_id) ||
            Colony_Production_Is_Ship_Design(production_id);
    }

    static inline int16_t Colony_Production_Ship_Index(int16_t production_id) {
        return COLONY_PRODUCTION_QUEUED_SHIP_BASE - production_id;
    }

    static inline int16_t Colony_Production_Ship_Design_Index(int16_t production_id) {
        return COLONY_PRODUCTION_SHIP_DESIGN_BASE - production_id;
    }

    static inline int16_t Colony_Production_From_Ship_Index(int16_t ship_idx) {
        return COLONY_PRODUCTION_QUEUED_SHIP_BASE - ship_idx;
    }

    static inline int16_t Colony_Production_From_Ship_Design_Index(int16_t design_idx) {
        return COLONY_PRODUCTION_SHIP_DESIGN_BASE - design_idx;
    }

    extern int16_t building_indexes[54];
    extern int16_t n_building_indexes;
    // Size: short[54]
    extern int16_t building_fields[54];

    int __cdecl qsort_building_by_name(const void* a, const void* b);

    int16_t __cdecl Calc_Font_Off();

    void __cdecl Get_Font_Off_For_Buildings(int16_t* offset);

    void __cdecl Get_Font_Off_For_Military(int16_t* font_offset);

    void __cdecl Calculate_Building_Array();

    void __cdecl Calculate_Military_Array();

    int16_t __cdecl Star_N_Uncolonized_Worlds(int16_t star_index);

    int16_t __cdecl N_Prods_In_Queue(int16_t product_id);

    int16_t __cdecl Get_Prod_Queue_Len();

    bool __cdecl Has_Prod_In_Queue(int16_t building_id);

    void __cdecl Draw_Icon(s_animation_header* anim);

    extern int16_t n_military_indexes;
    extern int16_t military_indexes[54];
    extern int16_t military_fields[54];

    void __cdecl Build_Arrays();

    /**
     * Creates a queued ship instance from a player design at a colony.
     * @param player_idx Owning player index.
     * @param design_idx Ship design slot to copy.
     * @param colony_idx Colony that supplies the ship location.
     */
    int16_t __cdecl Create_Ship(int16_t player_idx, int16_t design_idx, int16_t colony_idx);

    /** Creates a queued ship instance from the supplied design at a colony. */
    int16_t __cdecl Create_Ship_From_Design(int16_t player_idx, const s_ship_design* design, int16_t colony_idx);

    /** Stores the transient design used by the cross-screen refit workflow. */
    void Set_Pending_Refit_Design(int16_t ship_idx, const s_ship_design* design);

    /** Returns the transient refit design, or null when no refit is pending. */
    s_ship_design* Pending_Refit_Design();

    /** Clears the transient refit design. */
    void Clear_Pending_Refit_Design();

    /** Opens the colony build-queue popup and runs its input loop. */
    void __cdecl Build_Queue_Popup();

    int16_t __cdecl Colbldg_Create_Ship(int16_t design_slot);

    extern uint8_t return_to_build_popup_from_custom_design;
    extern uint8_t field_mode;
    extern int16_t active_prod[4];
    extern int16_t current_item[7];
    extern int16_t last_app;
    extern int16_t last_setting;
    extern int16_t scanned_field;
    extern char g_msg[800];
    extern int16_t colony_bldg_saved_screen;
    extern int16_t scanned_prod;
    extern int16_t refitting_ship_n;
    extern int16_t colony_auto_building;
    extern bool warn_blockers;
    extern int16_t fields[11];
    extern int16_t build_queue_hotkeys[11];
    extern s_colors l_font_color_array;
    extern s_colors l_font_dim_color_array;
    extern int16_t queue_fields[7];

    extern uint8_t* buffer;

    extern s_animation_header* bldg_bitm;

    extern uint8_t* desc_record_ptr;

    extern s_animation_header* anims[7];
    extern s_animation_header* colsum_pal_anim;

    void __cdecl Load_Build_Anims();

    void __cdecl Clear_Last_App_Description_String();

    void __cdecl Add_Build_Queue_Fields();

    void __cdecl Add_Buildings_Fields();

    void __cdecl Add_Militarys_Fields();

    void __cdecl Clear_Last_App_Desc();

    void __cdecl Update_Fields();

    bool __cdecl Apply_Build_Queue_Hotkey(int16_t field_id);

    void __cdecl Draw_Build_Queue_Popup();

    void __cdecl Draw_Buildings_List();

    void __cdecl Draw_Militarys_List();

    int16_t __cdecl Auto_Design_Type(int16_t design_code);

    void __cdecl Draw_Build_Queue();

    void __cdecl Draw_Current_Selection();

    /**
     * Prints cost and timing lines for the selected production item.
     * @param colony_idx Colony supplying the production context.
     * @param product_idx Building, ship, or special product identifier.
     * @param line_count In/out line counter used for vertical placement.
     */
    void __cdecl Draw_Cost_And_Time_Info(int16_t colony_idx, int16_t product_idx, int16_t* line_count);

    void __cdecl Draw_Ship_Design_Info2(s_ship_design* design);

    /**
     * Builds a formatted paragraph describing a ship design.
     * @param design Ship design to describe.
     * @param buffer Destination paragraph buffer.
     */
    void __cdecl Build_Ship_Design_Info_Formatted_Paragraph(s_ship_design* design, STRBUILD::s_builder* builder);

    /**
     * Builds a formatted paragraph describing a ship and its crew state.
     * @param ship Ship instance to describe.
     * @param buffer Destination paragraph buffer.
     */
    void __cdecl Build_Ship_Info_Formatted_Paragraph(s_ship_data* ship, STRBUILD::s_builder* builder);

    /**
     * Dispatches to the ship or design paragraph builder for binary-compatibility callers.
     * @param ship_or_design Either a ship pointer or a design pointer.
     * @param buffer Destination paragraph buffer.
     * @param include_crew_details Nonzero when ship-specific crew details should be printed.
     */
    void __cdecl Build_Ship_Info_Formatted_Paragraph_Binary(void* ship_or_design, STRBUILD::s_builder* builder, char include_crew_details);

    /**
     * Formats a weapon entry for ship-description output.
     * @param weapon Weapon instance to describe.
     * @param dest Destination string buffer.
     * @param include_postfix_modifiers Nonzero to append trailing modifier text.
     * @param weapon_count Number of copies mounted for pluralization and count output.
     */
    void __cdecl Build_Weapon_String(s_ship_weapons* weapon, char* dest, uint32_t dest_size, uint8_t include_postfix_modifiers, uint8_t weapon_count);

    int16_t __cdecl First_Prod_Colony_Queue_Slot(int16_t colony_idx, int16_t product_id);

    void __cdecl Delete_From_Colony_Queue(int16_t colony_idx, int16_t queue_idx);

    void __cdecl Delete_Prod_From_Colony_Queue(int16_t colony_idx, int16_t product_id);

    void __cdecl Delete_Ship(int16_t ship_index);

    void __cdecl Squish_Out_Colony_Queue_Slot(int16_t colony_idx, int16_t queue_idx);

    void __cdecl Clear_Colony_Queue(int16_t colony_idx);

    void __cdecl Print_App_Description(uint16_t x, int16_t y, uint16_t width, int16_t app_id);

    void __cdecl Print_Setting_Description(int16_t x, int16_t y, int16_t width, int16_t setting_id);

    void __cdecl Draw_Ship_Design_Info(int16_t id);

    void __cdecl Draw_Building_Centered_In_Window(int16_t building_id);

    void __cdecl Get_Setting_Description_String(char* buffer, int16_t setting_id);

    void __cdecl Get_App_Description_String(char* dest_str, uint32_t dest_str_size, int16_t app_id);

    void __cdecl Draw_Building_Info(int16_t building_idx);

    void __cdecl Draw_Ship_Info(int16_t item);

    void __cdecl Draw_Active_Prod();

    char* __cdecl Selection_Name(int16_t id);
    void Copy_Selection_Name(int16_t id, char* out, uint32_t size, int16_t owner = -1);
    char* Read_Option_String(int16_t id, int16_t owner);

    extern s_mouse_list mouse_list_to[4];
    extern s_mouse_list mouse_list_what;
    extern s_mouse_list mouse_list_which_ship;
    extern uint8_t hacky_speed_refitted_ship_flag;

    void __cdecl Add_Refited_Ship_To_Queue();
    int8_t __cdecl Something_Behind_Settting();
    void __cdecl Squish_Out_Queue_Slot(int16_t slot_idx);
    void __cdecl Do_Draw_Build_Queue();
    void __cdecl Do_Exit_Screen_Cleanup();
    void __cdecl Remove_Ships(int8_t flag);
    int16_t __cdecl First_Prod_Queue_Slot(int16_t prod_idx);
    void __cdecl Delete_From_Queue(int16_t slot_idx);
    int8_t __cdecl Add_To_Queue(int16_t prod_idx);
    /**
     * Moves the selected queue entry before another queue slot.
     * @param source_slot_idx Slot currently holding the item to move.
     * @param target_slot_idx Slot that should receive the moved item.
     */
    void __cdecl Insert_Before_Queue_Slot(int16_t source_slot_idx, int16_t target_slot_idx);
    void __cdecl Defragment_Queue();

    /**
     * Updates build-queue state from the latest field selection and scan result.
     * @param selected_field_id In/out selected field identifier.
     * @param scanned_field_id In/out scanned field identifier.
     */
    void __cdecl Evaluate_Input(int16_t* selected_field_id, int16_t* scanned_field_id);

    void __cdecl Print_Selection_Centered(const char* text);

    char* __cdecl Option_String(int16_t option_id);
} // namespace COLBLDG