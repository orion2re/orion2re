#pragma once
#include <cstdint>

struct s_animation_header;
struct s_coldraw_road;
struct s_coldraw_veggie_cell;

namespace COLDRAW {
    // Updates the global icon-spacing reduction so a row can fit within the requested width.
    // @param start_x Left edge of the row.
    // @param available_width Width available for the row.
    // @param icon_count Number of icons that must fit.
    // @param icon_spacing Nominal spacing between icons.
    void __cdecl Calculate_Squish_Step(int16_t start_x, int16_t available_width, int16_t icon_count, int16_t icon_spacing);

    extern s_coldraw_road road_list[7][7];

    extern s_coldraw_veggie_cell veggie_list[6][6];

    // Maps a road draw-order index to colony-road grid coordinates.
    void __cdecl Road_Back_To_Front(int16_t index, int16_t* row, int16_t* col);

    // Draws all road segments in back-to-front order.
    void __cdecl Draw_Road_List();

    // Initializes the road animation list for the colony-road grid.
    void __cdecl Load_Road_List_Anims();

    // Builds road connections from the current colony building layout.
    void __cdecl Build_Road_List_Based_On_Bldg_List();

    extern uint8_t DAT_5a7760;
    extern uint8_t DAT_5a778a;

    // Connects two building cells through the road grid.
    void __cdecl Draw_Simplest_Path(int16_t previous_row, int16_t previous_col, int16_t row, int16_t col, uint8_t* first_segment_out, uint8_t* second_segment_out);

    // Traverses occupied building cells and links consecutive entries.
    void __cdecl Link_Buildings();

    // Counts the road connections attached to one colony-road cell.
    int16_t __cdecl N_Roads_Around_CR(int16_t row, int16_t col);

    // Chooses a random matching road connection from a cell and its immediate neighbors.
    int16_t __cdecl Pick_Random_Road_At_And_Around_CR(int16_t* row, int16_t* col, int16_t* connection_idx, int16_t search_count);

    // Runs the four-cell road-connection search used by the road builder.
    uint8_t __cdecl Do_Pick_Random_Road_At_And_Around_CR(int16_t* row, int16_t* col, int16_t* connection_idx);

    // Populates decorative vegetation sprites for empty colony cells.
    void __cdecl Build_Veggie_List_Based_On_Bldg_List();

    // Picks the vegetation tileset family for a colony's planet climate.
    int16_t __cdecl Veggie_Type_For_Colony(game::id_colony colony_idx);

    // Chooses a vegetation animation entry for one empty colony cell.
    int16_t __cdecl Pick_Random_Veggie_Anim_Entry_For_Colony_CR(game::id_colony colony_idx, int16_t row, int16_t col);

    // Clears all vegetation draw flags before rebuilding the list.
    void __cdecl Clear_Veggie_Draw_Flags();

    // Draws one vegetation cell for the requested pass count.
    void __cdecl Draw_Veggie_CR(int16_t row_idx, int16_t col_idx, uint8_t draw_pass_count);

    // Scans, lays out, or draws colony population icons depending on mode.
    int16_t __cdecl Do_Colony_Info_Pop_Stuff_For_Pop(game::id_colony colony_idx, int16_t job_filter, int16_t mode, int16_t left_x, int16_t top_y, int16_t right_x, uint8_t show_farm_shadows, int16_t* pop_index_by_slot, int16_t* last_slot_idx_ptr, int16_t* scroll_value_ptr, int16_t* scroll_field_id_ptr);

    // Draws production, import, and shortage icons for one colony resource row.
    void __cdecl Draw_Colony_Prod_Both(game::id_colony colony_idx, int16_t prod_type, int16_t x, int16_t y, int16_t width, int16_t spacing, int8_t frame);

    // Draws the standard colony production row.
    void __cdecl Draw_Colony_Prod(game::id_colony colony_idx, int16_t prod_type, int16_t x, int16_t y, int16_t width, int16_t spacing);

    // Draws colony morale icons, optionally in the compact mini layout.
    void __cdecl Draw_Info_Morale_Both(game::id_colony colony_idx, int16_t x, int16_t y, int32_t width, uint8_t is_mini);

    // Draws the standard colony morale row.
    void __cdecl Draw_Info_Morale(game::id_colony colony_idx, int16_t x, int16_t y, int32_t width);

    // Draws the current-versus-total production-capacity bar.
    void __cdecl Draw_Colony_PC_Bar(int16_t x, int16_t y, int16_t total, int16_t current);

    // Draws the compact production row used by smaller colony panels.
    void __cdecl Draw_Colony_Wee_Prod(game::id_colony colony_idx, int16_t prod_idx, int16_t x, int16_t y, int16_t width, int16_t height);

    // Draws the compact morale row used by smaller colony panels.
    void __cdecl Draw_Info_Wee_Morale(game::id_colony colony_idx, int16_t x, int16_t y, int32_t width);
} // namespace COLDRAW
