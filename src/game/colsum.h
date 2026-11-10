#pragma once
#include <cstdint>

struct s_animation_header;
struct s_colsum_col_pop_ref;

namespace COLSUM {
    enum e_research_value_display : uint8_t {
        RESEARCH_VALUE_CLASSIC,
        RESEARCH_VALUE_ACCUMULATED,
        RESEARCH_VALUE_REMAINING,
        RESEARCH_VALUE_PERCENTAGE_1,
        RESEARCH_VALUE_PERCENTAGE_2,
    };

    extern uint8_t print_research_value;

    extern int16_t x_fields[14];
    extern int16_t g_sort_index;
    extern s_animation_header* anims[21];
    extern uint8_t draw_planet_sum;

    extern int16_t first;

    extern int16_t slider_bar_position;

    extern int16_t list_col_prod_fields[10];
    extern int16_t list_col[10];

    extern s_colsum_col_pop_ref list_col_pop_ref[30];

    extern int16_t list_col_coord_vars[32];

    extern int16_t list_col_pop_count[32];

    extern int16_t list_buy_fields[10];
    extern int16_t list_fields[10];
    extern int16_t list_buy_data[10];

    extern int16_t list_col_job_fields[30];
    extern int16_t colony_summary_saved_screen;

    void __cdecl Clear_List_Col_Array();

    void __cdecl Init_Fields();

    void __cdecl Add_Fields();

    void __cdecl Add_Fields_Pop_For(int16_t colony_idx);

    void __cdecl Update_Col_List();

    void __cdecl Reset_List_Col_Array();

    void __cdecl Load_Col_Sum_Anims();

    extern uint8_t colsum_strings_been_loaded;

    extern int8_t event_good_lookup[36];

    extern char* wee_event_strings[36];

    void __cdecl Load_Colsum_Strings();

    bool __cdecl Event_Good(int16_t event_id);

    void __cdecl Sort_Col_List();

    int16_t __cdecl cmp(int16_t lhs, int16_t rhs);

    int16_t __cdecl cmp_Alpha(int16_t colony_idx_1, int16_t colony_idx_2);

    int16_t __cdecl cmp_Pops(int16_t colony_idx1, int16_t colony_idx2);

    int16_t __cdecl cmp_Food(int16_t colony_idx1, int16_t colony_idx2);

    int16_t __cdecl cmp_Industry(int16_t colony_idx_1, int16_t colony_idx_2);

    int16_t __cdecl cmp_Research(int16_t colony_idx_1, int16_t colony_idx_2);

    int16_t __cdecl cmp_Prod(int16_t colony_idx_1, int16_t colony_idx_2);

    int16_t __cdecl cmp_BC(int16_t colony_idx_1, int16_t colony_idx_2);

    int16_t __cdecl Switched_cmp(int16_t a1, int16_t a2);

    void __cdecl Draw_Colony_Summary_Screen();

    void __cdecl Draw_Colony_Summary();

    void __cdecl Update_First(int16_t visible_rows, int16_t* first_visible_index, int16_t* slider_position);

    void __cdecl Reset_List_Scroll();

    void __cdecl Decrement_First(int16_t num_items);

    void __cdecl Increment_First(int16_t count);

    extern const char s_0_0055110c[3];
    extern const char s_1_00551110[3];

    extern const char s__005510f0[2];
    extern const char s_s_005510f4[2];
    extern const char s_f_005510f8[2];
    extern const char s_p_005510fc[2];

    void __cdecl Draw_Empire_Info();

    void __cdecl Draw_Galaxy_Map();

    void __cdecl Draw_Colony_Summary_For_Colony(int16_t index);

    void __cdecl Draw_Info_Pop_For(int16_t colony_idx);

    void __cdecl Colsum_Connect_Galaxy_Map_Stars(int16_t star_idx1, int16_t star_idx2, s_colors* pattern);

    void __cdecl Draw_Bar_Indicator();

    bool __cdecl Evaluate_Input(int16_t* input_val, int16_t* scan_val);

    bool __cdecl Evaluate_Col_List_Input(int16_t* field_ptr, int16_t* scan_val);

    void __cdecl Evaluate_Colony_Pop_Input(int16_t* input_field_1, int16_t* input_field_2, int16_t colony_idx_in_list);

    int16_t __cdecl Get_Selected_Pop(int16_t colony_idx, int16_t job_idx);

    int16_t __cdecl Get_Scanned_Pop(int16_t colony_list_row, int16_t job_type);

    void __cdecl Check_Galaxy_Map_Fields(int16_t* clicked_field, int16_t* hovered_field);

    void __cdecl Draw_Scan_Info();

    void __cdecl Colony_Summary_Screen();

    int16_t __cdecl Pick_Best_Planet_To_Goto_At_Star(int16_t star_idx);

    int16_t __cdecl Prod_To_Sort_Type(int16_t production_id);

    void __cdecl Draw_Colony_Scan_Info();
} // namespace COLSUM