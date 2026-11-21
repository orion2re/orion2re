#include "pch.h"

namespace MOX {
    int16_t NUM_STARS = 0;
    int16_t cur_map_scale = 0;
    int16_t cur_map_x = 0;
    int16_t cur_map_y = 0;
    int8_t game_type = 0;
    s_antaran antarans = {};
    int16_t PLAYER_NUM = 0;
    int32_t stardate = 0;
    s_settings settings = {};
    uint8_t NUM_NEBULAS = 0;


    int16_t NUM_SHIPS = 0;
    int16_t max_zoom_count = 0;
    uint8_t star_bg_index = 0;
    uint16_t fleet_icon_button_count = 0;
    int16_t NUM_PLAYERS = 0;
    s_player* player = nullptr;
    s_ship_data* ship = nullptr;
    s_colony* colony = nullptr;
    s_star_data* star = nullptr;
    int16_t bill_savegame[6] = {};
    uint16_t antaran_resource_level = 0;
    uint16_t chance_for_antaran_invasion = 0;
    uint16_t disallow_saves_code = 0;
    int16_t max_map_scale = 0;
    int16_t MAP_MAX_X = 0;
    int16_t MAP_MAX_Y = 0;
    s_nebula* nebula = nullptr;
    s_movable_box moveable_box[5] = {};
    int16_t moveable_box_order[5] = {};
} // namespace MOX

namespace REPORT {
    int16_t n_reports = 0;
    s_report reports[MAX_REPORTS] = {};
    uint8_t done_player_reports = 0;
    uint8_t something_interesting_happened[MAX_PLAYERS] = {};
    uint8_t done_colonize_check[BITMAP(MAX_STARS)] = {};
    uint8_t displayed_msgs = 0;
    uint8_t displayed_gnn = 0;
} // namespace REPORT

namespace EVENTS {
    s_event_data event_data[36] = {};
    uint32_t last_event_year = 0;
    int16_t event_delay_count = 0;
} // namespace EVENTS

namespace MSG_ {
    uint8_t first_msg[MAX_PLAYERS] = {};
    uint8_t n_msgs[MAX_PLAYERS] = {};
    s_MSG msgs[MAX_MESSAGES] = {};
    uint8_t max_msgs[MAX_PLAYERS] = {};
} // namespace MSG_

namespace diplodef {
    int16_t next_council_meeting_turn = 0;
    int16_t COUNCIL_FLAG = 0;
} // namespace diplodef

namespace INITSHIP {
    void __cdecl Load_Doom_Star_Ship_Design(
        int16_t owner_idx, s_ship_design* design, bool strategic_combat) {
        memset(design, 0, sizeof(*design));
        design->size = SHIP_SIZE_DOOM_STAR;
        design->ship_type = SHIP_TYPE_COMBAT;
        design->previous_owner = static_cast<uint8_t>(owner_idx);
        design->picture_num = strategic_combat ? 0x2B : 0;
    }
} // namespace INITSHIP

namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char*) {
        std::abort();
    }
} // namespace exitm
