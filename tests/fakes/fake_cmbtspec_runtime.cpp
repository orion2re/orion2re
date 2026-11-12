#include "pch.h"

#include "game/cmbtmis.h"
#include "game/cmbtfire.h"

namespace {
    s_combat_data combat_data[2];
    s_missile missiles[MAX_COMBAT_FLYERS];
    s_animation_header animation;
    int32_t random_call_count;
    int32_t apply_ship_damage_call_count;
    int32_t applied_ship_damage;
    int32_t cloak_frame_count;
    int16_t damage_indicator_total;
    int32_t missile_move_call_count;
    int32_t missile_resolve_call_count;
    uint32_t random_result;
    bool use_random_result;
    int16_t bitmap_line_x1;
    int16_t bitmap_line_y1;
    int16_t bitmap_line_x2;
    int16_t bitmap_line_y2;
}

namespace TEST_CMBTSPEC {
    int draw_count, glass_count, flip_count, bounds_count;
    int16_t draw_x, draw_y, flip_mode;
    int16_t requested_width, requested_height;
    int32_t last_resource, last_string, last_allocation;
    int remaining_queries, grid_calls, recalculate_calls, full_draw_calls;
    int16_t grid_ship, recalculated_ship;
    bool reset_movement;
    int explosion_queries;
    int16_t explosion_ship;
    int replace_calls;
    int16_t replaced_start, replaced_count;
    uint8_t replaced_colors[32];
    int restore_window_calls, sound_calls, loop_calls;
    int16_t last_sound;
    int32_t loop_channel, faded_channel;
    int8_t loop_count;
    int16_t window[4], saved_window[4];

    void Set_Frames_(uint16_t count) { animation.frame_count = count; }

    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(&animation, 0, sizeof(animation));
        animation.width = 100;
        animation.height = 100;
        MOX::draw_ship_bitmap = MOX::ship_bitmap = MOX::combat_bitmap = &animation;
        MOX::small_tractor_seg = &animation;
        MOX::ship_frame = 0;
        MOX::cur_ship = 0;
        MOX::auto_combat = 0;
        MOX::combat_defender_player = 1;
        CMBTSPEC::g_engine_sfx_handle = 0;
        draw_count = glass_count = flip_count = bounds_count = 0;
        requested_width = requested_height = draw_x = draw_y = 0;
        flip_mode = -1;
        last_resource = last_string = last_allocation = -1;
        remaining_queries = grid_calls = recalculate_calls = full_draw_calls = 0;
        grid_ship = recalculated_ship = explosion_ship = -1;
        reset_movement = false;
        explosion_queries = replace_calls = 0;
        replaced_start = replaced_count = 0;
        memset(replaced_colors, 0, sizeof(replaced_colors));
        restore_window_calls = sound_calls = loop_calls = 0;
        last_sound = -1;
        loop_channel = faded_channel = -1;
        loop_count = 0;
        const int16_t initial_window[] = {3, 4, 100, 120};
        memcpy(window, initial_window, sizeof(window));
        memcpy(saved_window, window, sizeof(window));
        random_call_count = 0;
        apply_ship_damage_call_count = 0;
        applied_ship_damage = 0;
        CMBTFIR2::combat_bonus = {};
        CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = 30;
        cloak_frame_count = 0;
        damage_indicator_total = 0;
        missile_move_call_count = 0;
        missile_resolve_call_count = 0;
        random_result = 0;
        use_random_result = false;
        bitmap_line_x1 = 0;
        bitmap_line_y1 = 0;
        bitmap_line_x2 = 0;
        bitmap_line_y2 = 0;

        MOX::combat_total_ship_count = 2;
        MOX::apply_damage_to_planet = 0;
        MOX::super_fast_flag = 1;
        MOX::ordnance_bonus[0] = 100;
        MOX::combat_data[0].owner = 0;
        MOX::combat_data[1].structure_max = 1000;
        MOX::dont_draw_ship = -1;
        MOX::net_flag = 0;
        MOX::speedx2_flag = 0;
        TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].min_damage = 10;
        TECHDATA::weapons[WEAPON_SPATIAL_COMPRESSOR].max_damage = 10;
    }

    int32_t Random_Call_Count_() {
        return random_call_count;
    }

    int32_t Apply_Ship_Damage_Call_Count_() {
        return apply_ship_damage_call_count;
    }

    int32_t Applied_Ship_Damage_() {
        return applied_ship_damage;
    }

    int32_t Cloak_Frame_Count_() {
        return cloak_frame_count;
    }

    int16_t Damage_Indicator_Total_() {
        return damage_indicator_total;
    }

    int32_t Missile_Move_Call_Count_() {
        return missile_move_call_count;
    }

    int32_t Missile_Resolve_Call_Count_() {
        return missile_resolve_call_count;
    }

    void Set_Random_Result_(uint32_t value) {
        random_result = value;
        use_random_result = true;
    }

    int16_t Bitmap_Line_X1_() {
        return bitmap_line_x1;
    }

    int16_t Bitmap_Line_Y1_() {
        return bitmap_line_y1;
    }

    int16_t Bitmap_Line_X2_() {
        return bitmap_line_x2;
    }

    int16_t Bitmap_Line_Y2_() {
        return bitmap_line_y2;
    }
}

namespace MOX {
    int16_t ship_frame, auto_combat, combat_defender_player, cur_ship;
    s_animation_header* ship_bitmap;
    s_animation_header* small_tractor_seg;
    void* tractor_remap_colors;
    void* plasma_web_remap_colors;
    void* stasis_bubble_remap_colors;
    s_combat_data* combat_data = ::combat_data;
    s_missile* missile = missiles;
    int16_t combat_total_ship_count;
    uint16_t apply_damage_to_planet;
    int16_t super_fast_flag;
    int16_t cur_combat_x;
    int16_t cur_combat_y;
    int16_t dont_draw_ship;
    int16_t net_flag;
    int16_t speedx2_flag;
    int16_t ordnance_bonus[15];
    s_animation_header* screen_seg;
    void* global_cache_seg;
    void* pulsar_remap_colors;
    void* warp_core_remap_colors;
    void* shield_remap_colors;
    void* stellar_converter_remap_colors;
    s_animation_header* combat_bitmap;
    s_animation_header* draw_ship_bitmap;
}

namespace TECHDATA {
    s_tech_weapon_data weapons[WEAPON_COUNT];
}

namespace CMBTFIR2 {
    s_combat_bonus combat_bonus = {};
    int16_t __cdecl Damage_From_Ship_Exploding(int16_t ship) {
        ++TEST_CMBTSPEC::explosion_queries;
        TEST_CMBTSPEC::explosion_ship = ship;
        return 42;
    }
}

namespace COLONY {
    const char CMBTSFX_LBX[12] = "";
    const char SPHERSFX_LBX[13] = "";

    void __cdecl Load_Ship_Bitm() {
    }
}

namespace allocate {
    int32_t __cdecl Get_Remaining_Block_Space(void*) {
        ++TEST_CMBTSPEC::remaining_queries;
        return 60000;
    }
    void* __cdecl Allocate_Next_Block(void*, uint32_t size) {
        TEST_CMBTSPEC::last_allocation = size;
        return &animation;
    }

    void __cdecl Push_Block(void*) {
    }

    void __cdecl Pop_Block(void*) {
    }
}

namespace buffer {
    void* __cdecl Buffer_Reload(const char*, int32_t entry, void*) {
        TEST_CMBTSPEC::last_resource = entry;
        return &animation;
    }
}

namespace animate {
    void __cdecl Draw_Palette(s_animation_header*) {
    }

    void __cdecl Draw(int16_t x, int16_t y, s_animation_header*) {
        ++TEST_CMBTSPEC::draw_count;
        TEST_CMBTSPEC::draw_x = x;
        TEST_CMBTSPEC::draw_y = y;
    }
}

namespace ERICNET {
    int32_t __cdecl Mox_Play_Sound(int16_t sound) {
        ++TEST_CMBTSPEC::sound_calls;
        TEST_CMBTSPEC::last_sound = sound;
        return 7;
    }
}

namespace COMBINIT {
    int16_t __cdecl Does_Combat_Ship_Have_Special(int16_t ship_idx, int16_t special_id) {
        const uint8_t mask = (uint8_t)(1u << (special_id % 8));
        return (MOX::combat_data[ship_idx].special_device_flags[special_id / 8] & mask) != 0 &&
            (MOX::combat_data[ship_idx].special_device_damage_flags[special_id / 8] & mask) == 0;
    }

    void __cdecl Ship_Center_Offsets(int16_t, int16_t* x, int16_t* y) {
        *x = 0;
        *y = 0;
    }

    void __cdecl Snap_Center_Combat_Screen(int16_t, int16_t) {
    }
}

namespace CMBTDRW1 {
    void __cdecl Full_Draw_Combat_Screen() { ++TEST_CMBTSPEC::full_draw_calls; }
    int16_t __cdecl Ship_On_Screen(int16_t) {
        return 1;
    }

    int16_t __cdecl Ship_Completely_On_Screen(int16_t) {
        return 1;
    }

    void __cdecl Get_Ship_Center_Draw_XY(int16_t, int16_t* x, int16_t* y) {
        *x = 0;
        *y = 0;
    }

    int16_t __cdecl Relative_Bearing(int16_t, int16_t) {
        return 0;
    }

    void __cdecl Draw_Main_Combat_Screen() {
    }

    void __cdecl Draw_Ship(int16_t, int16_t, int16_t) {
    }

    void __cdecl Draw_Ship_To_Bitmap(int16_t, int16_t, int16_t, int16_t) {
        ++cloak_frame_count;
    }

    void __cdecl Get_Draw_Ship_Offsets(int16_t, int16_t* x, int16_t* y) {
        *x = 0;
        *y = 0;
    }

    void __cdecl Refresh_Combat_Screen_Full() {
    }
}

namespace bitmap {
    int32_t __cdecl Get_Bitmap_Size(int16_t width, int16_t height) {
        TEST_CMBTSPEC::requested_width = width;
        TEST_CMBTSPEC::requested_height = height;
        return 512;
    }
    void __cdecl Flip_Bitmap(s_animation_header*, s_animation_header*, int16_t mode) {
        ++TEST_CMBTSPEC::flip_count;
        TEST_CMBTSPEC::flip_mode = mode;
    }
    void __cdecl Set_Picture_Glassed(s_animation_header*) { ++TEST_CMBTSPEC::glass_count; }
    void __cdecl Get_Bitmap_Actual_Size(s_animation_header*, int16_t* x, int16_t* y, int16_t* width, int16_t* height) {
        ++TEST_CMBTSPEC::bounds_count;
        *x = *y = 0;
        *width = 30;
        *height = 20;
    }
    void __cdecl Draw_Picture_To_Bitmap(s_animation_header*, s_animation_header*) {
    }

    void __cdecl Create_Blank_Bitmap(int16_t, int16_t, void*) {
    }

    void __cdecl Draw_Bitmap_Line(
        int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_animation_header*) {
        bitmap_line_x1 = x1;
        bitmap_line_y1 = y1;
        bitmap_line_x2 = x2;
        bitmap_line_y2 = y2;
    }

    void __cdecl Scale_Bitmap(
        s_animation_header*, s_animation_header*, int16_t, int16_t) {
    }
}

namespace line {
    int16_t __cdecl Clip_Line(
        int16_t* x1, int16_t* y1, int16_t* x2, int16_t* y2) {
        if (*x1 == *x2) {
            if (*y2 < *y1) {
                const int16_t old_y1 = *y1;
                *y1 = *y2;
                *y2 = old_y1;
            }
            return 0;
        }

        if (*x2 < *x1) {
            const int16_t old_x1 = *x1;
            const int16_t old_y1 = *y1;
            *x1 = *x2;
            *y1 = *y2;
            *x2 = old_x1;
            *y2 = old_y1;
        }

        if (*y1 != *y2 && *y2 < *y1) {
            const int16_t old_x1 = *x1;
            const int16_t old_y1 = *y1;
            *x1 = *x2;
            *y1 = *y2;
            *x2 = old_x1;
            *y2 = old_y1;
        }
        return 0;
    }
}

namespace CMBTMOV1 {
    int8_t __cdecl Ship_Is_In_Stasis(int16_t) {
        return 0;
    }
}

namespace CMBTFIRE {
    bool __cdecl Spherical_Effect_In_Range(
        int16_t, int16_t, int16_t, SPHERICAL_EFFECT) {
        return true;
    }

    int16_t __cdecl Facing_Shield(int16_t facing) {
        return facing;
    }

    uint16_t __cdecl Apply_Ship_Damage(
        int16_t, int16_t* damage, int16_t*, int16_t, uint16_t, uint16_t,
        int16_t, int16_t, int16_t) {
        ++apply_ship_damage_call_count;
        applied_ship_damage += *damage;
        return 0;
    }

    int16_t __cdecl Is_Combat_Ship_Dead(int16_t) {
        return 0;
    }

    void __cdecl Destroy_Ship(int16_t, int16_t, int16_t, int16_t) {
    }
}

namespace CMBTMIS {
    char __cdecl Move_Missile(int16_t, int16_t, int16_t, int16_t) {
        ++missile_move_call_count;
        return 1;
    }

    int16_t __cdecl Resolve_Missile(int16_t, int16_t, int16_t, bool) {
        ++missile_resolve_call_count;
        return 0;
    }

    void __cdecl Retarget_Missile(int16_t) {
    }

    int16_t __cdecl Missile_Owner(int16_t) {
        return 0;
    }

    int32_t __cdecl Get_Missile_Hits(int16_t, int16_t, uint16_t) {
        return 1;
    }

    void __cdecl Kill_Missile(int16_t) {
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t maximum) {
        ++random_call_count;
        return use_random_result ? random_result : maximum;
    }
}

namespace BEAMS {
    void __cdecl Add_Damage_Indicator_For_Missile(
        int16_t, int16_t, uint8_t, int16_t) {
    }

    void __cdecl Add_Damage_Indicator_For_Ship(
        int16_t, int16_t, int16_t, uint8_t, int16_t total_damage, int16_t) {
        damage_indicator_total = total_damage;
    }

    void __cdecl Draw_Damage_Message_Queue() {
    }

    void __cdecl Draw_Damage_Message_Queue_Until_Done() {
    }
}

namespace palette {
    void __cdecl Refresh_Palette() {
    }

    void __cdecl Restore_Glass_Remap_Block(void*) {
    }
}

namespace video {
    void __cdecl Set_Page_Back() {
    }

    void __cdecl Set_Page_Off() {
    }

    void __cdecl Copy_Back_To_Off() {
    }

    void __cdecl Clip_On() {
    }

    void __cdecl Clip_Off() {
    }
}

namespace graphics {
    void __cdecl Restore_Window() {
        ++TEST_CMBTSPEC::restore_window_calls;
        memcpy(TEST_CMBTSPEC::window, TEST_CMBTSPEC::saved_window, sizeof(TEST_CMBTSPEC::window));
    }
    void __cdecl Save_Window() {
        memcpy(TEST_CMBTSPEC::saved_window, TEST_CMBTSPEC::window, sizeof(TEST_CMBTSPEC::window));
    }

    void __cdecl Fill(int16_t, int16_t, int16_t, int16_t, uint8_t) {
    }

    void __cdecl Set_Window(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        const int16_t values[] = {x1, y1, x2, y2};
        memcpy(TEST_CMBTSPEC::window, values, sizeof(values));
    }

    void __cdecl Reset_Window() {
        Set_Window(0, 0, 639, 479);
    }
}

namespace fields {
    void __cdecl Clipped_Dot(int16_t, int16_t, uint8_t) {
    }
}

namespace sound {
    void __cdecl Loop_Sound(int32_t channel, int8_t count) {
        ++TEST_CMBTSPEC::loop_calls;
        TEST_CMBTSPEC::loop_channel = channel;
        TEST_CMBTSPEC::loop_count = count;
    }
    void __cdecl Pitch_Bend_Sound(int32_t, int32_t) {
    }

    void __cdecl Fadeout_Sound(int16_t channel, int8_t) {
        TEST_CMBTSPEC::faded_channel = channel;
    }
}

namespace timer {
    void __cdecl Mark_Time() {
    }

    void __cdecl Release_Time(int32_t) {
    }
}

namespace RUSS {
    void __cdecl Mox_Sync_Update() {
    }
}

namespace ERIC {
    void __cdecl Draw_Centered(int16_t, int16_t, s_animation_header*) {
    }
}

namespace COMBAT1 {
    void __cdecl Assign_Combat_Grids(int16_t ship) {
        ++TEST_CMBTSPEC::grid_calls;
        TEST_CMBTSPEC::grid_ship = ship;
    }
    void Recalculate_Combat_Ship(int16_t ship, bool reset) {
        ++TEST_CMBTSPEC::recalculate_calls;
        TEST_CMBTSPEC::recalculated_ship = ship;
        TEST_CMBTSPEC::reset_movement = reset;
    }
}

namespace ESTRINGS {
    char* __cdecl E_Strings(int16_t index) {
        TEST_CMBTSPEC::last_string = index;
        static char asset[] = "COMBATFX.LBX";
        return asset;
    }
}

namespace replace {
    void __cdecl Replace_Color_Range(s_animation_header*, uint8_t* colors, int16_t start, int16_t count) {
        ++TEST_CMBTSPEC::replace_calls;
        TEST_CMBTSPEC::replaced_start = start;
        TEST_CMBTSPEC::replaced_count = count;
        memcpy(TEST_CMBTSPEC::replaced_colors, colors, std::min<int16_t>(count, 32));
    }
}