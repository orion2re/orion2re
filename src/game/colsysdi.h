#pragma once

#include <cstddef>

namespace COLSYSDI {
    // Draws the colony system-display planet list at the given screen position.
    // x: Left edge of the system-display panel.
    // y: Top edge of the system-display panel.
    void __cdecl Draw_Col_Sys_Disp(::game::s_world_state* world_state, int16_t x, int16_t y);

    // Returns the cached animation for a planet climate and size combination.
    // climate: Planet climate index used to select the animation.
    // planet_size: Planet size index used to select the animation variant.
    s_animation_header* __cdecl Planet_Anims(int16_t climate, int16_t planet_size);

    // Formats the summary text shown for a planet in the system-display panel.
    // summary_text: Destination buffer for the formatted summary text.
    // summary_text_size: Size of the destination buffer in bytes.
    // planet_idx: Planet index to summarize.
    // font_player_idx: Player slot used for colony text colorization, or -1 to skip recoloring.
    void __cdecl Planet_Summary_String(::game::s_world_state* world_state, char* summary_text, size_t summary_text_size, ::game::id_planet planet_idx, int16_t font_player_idx);

    // Draws the scan marker for the colony's currently selected planet row.
    // x: Left edge of the scan-marker column.
    // y: Base y position used by the system-display row layout.
    void __cdecl Draw_Col_Sys_Disp_Last_Scanned(::game::s_world_state* world_state, int16_t x, int16_t y);

    // Adds hidden interaction fields for the visible planet rows and summaries.
    // x: Left edge of the planet-icon column.
    // y: Top edge of the first row.
    // end_x: Right edge of the summary-text hit area.
    void __cdecl Add_Col_Sys_Disp_Fields(::game::s_world_state* world_state, int16_t x, int16_t y, int16_t end_x);
} // namespace COLSYSDI