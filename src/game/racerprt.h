#pragma once
#include <cstdint>

struct s_racescrn_icon_layout;

namespace RACERPRT {
    extern s_colors report_normal_color;
    extern uint8_t race_report_jim_hack;
    extern s_racescrn_icon_layout spy_group_data;

    /**
     * Draws the race report screen for the selected player.
     * @param player_idx Index of the player whose race report is displayed.
     */
    void __cdecl Race_Report_Screen(game::id_player player);
} // namespace RACERPRT