#pragma once
#include <cstdint>

struct s_ship_design;
struct s_ship_data;
struct s_animation_header;
struct s_ship_icon;
struct s_colors;

namespace SHIPS {
    /**
     * Builds a non-player ship design and places it at or near the requested star.
     * @param type Non-player ship type id.
     * @param system_idx Star index used as the spawn origin.
     * @param owner Owner value forwarded to the design loader.
     */
    int16_t __cdecl Build_Nonplayer_Ship(int16_t type, int16_t system_idx, uint8_t owner);

    /** Rebuilds the cached ship-icon list for the current map state. */
    void __cdecl Build_Ship_Icons();

    /**
     * Compares two ship-icon records by stack slot for qsort ordering.
     * @param a Pointer to the first s_ship_icon.
     * @param b Pointer to the second s_ship_icon.
     */
    int __cdecl qsort_ship_icon(const void* a, const void* b);

    /** Refreshes cached screen coordinates for every ship icon. */
    void __cdecl Set_Ship_Icon_XYs();

    /**
     * Draws a directional multi-colored line between two map points.
     * @param x1 Starting X coordinate.
     * @param y1 Starting Y coordinate.
     * @param x2 Ending X coordinate.
     * @param y2 Ending Y coordinate.
     * @param colors Eight-color ramp used by the line renderer.
     * @param phase Animation phase offset.
     */
    void __cdecl Draw_Directional_Multi_Colored_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_colors* colors, int16_t phase);

    /**
     * Resolves a ship icon's current on-screen position.
     * @param ship_icon_idx Index into MOX::ship_icon.
     * @param x Receives the screen X coordinate.
     * @param y Receives the screen Y coordinate.
     */
    void __cdecl Get_Ship_Icon_Coords(int16_t ship_icon_idx, int16_t* x, int16_t* y);

    /**
     * Resolves a moving ship icon's in-space screen position.
     * @param ship_icon_idx Index into MOX::ship_icon.
     * @param x Receives the screen X coordinate.
     * @param y Receives the screen Y coordinate.
     */
    void __cdecl Get_Ship_Icon_Coords_In_Space(int16_t ship_icon_idx, int16_t* x, int16_t* y);

    /**
     * Computes orbiting-icon anchor points around a star.
     * @param star_idx Star index being drawn.
     * @param ship_icon_type_idx Ship icon type index used to query icon dimensions.
     * @param out_x1 Receives the primary orbit X coordinate.
     * @param out_y1 Receives the primary orbit Y coordinate.
     * @param out_x2 Receives the secondary orbit X coordinate.
     * @param out_y2 Receives the secondary orbit Y coordinate.
     * @param out_arr_x Receives stacked orbit X coordinates.
     * @param out_arr_y Receives stacked orbit Y coordinates.
     */
    void __cdecl Get_XYs_For_Orbiting_Ships(int16_t star_idx, int16_t ship_icon_type_idx, int16_t* out_x1, int16_t* out_y1, int16_t* out_x2, int16_t* out_y2, int16_t* out_arr_x, int16_t* out_arr_y);

    /**
     * Loads the current ship-icon dimensions for a zoom level.
     * @param zoom_level Map zoom level to query.
     * @param width Receives the icon width in pixels.
     * @param height Receives the icon height in pixels.
     */
    void __cdecl Get_Ship_Icon_Dimensions(int16_t zoom_level, uint8_t* width, uint8_t* height);

    /**
     * Loads the ship-icon animation segment for a ship type and highlight state.
     * @param ship_type_idx Ship type or owner-derived icon selector.
     * @param is_highlighted Non-zero to load the highlighted variant.
     */
    s_animation_header* __cdecl Get_Ship_Icon_Pict_Seg(int16_t ship_type_idx, uint8_t is_highlighted);

    /**
     * Draws a destination line from a ship icon toward its target.
     * @param x1 Destination X coordinate.
     * @param y1 Destination Y coordinate.
     * @param x2 Ship-icon X coordinate.
     * @param y2 Ship-icon Y coordinate.
     * @param colors Color ramp used for the line.
     */
    void __cdecl Draw_Ship_Destination_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_colors* colors);

    /**
     * Prints the ETA label beside a moving ship icon.
     * @param node_idx Ship-node index for the moving stack.
     * @param style_toggle Font-style selector derived from map zoom.
     * @param x_offset Ship-icon X coordinate.
     * @param y_offset Ship-icon Y coordinate.
     */
    void __cdecl Print_Eta_On_Ship_Icon(int16_t node_idx, int16_t style_toggle, int16_t x_offset, int16_t y_offset);

    /** Draws ship destination lines for visible relevant fleets. */
    void __cdecl Do_Ship_Destination_Lines();

    /**
     * Cycles overlapping ship icons that are within the supplied distance threshold.
     * @param in_out_ship_icon_idx Input icon index and updated selected icon index.
     * @param threshold_x Maximum X delta for overlap grouping.
     * @param threshold_y Maximum Y delta for overlap grouping.
     */
    int16_t __cdecl Overlapped_Ship_Icon_Button(int16_t* in_out_ship_icon_idx, int16_t threshold_x, int16_t threshold_y);

    /**
     * Creates a non-player ship and chooses its initial map coordinates.
     * @param system_idx Star index used as the spawn origin.
     * @param type Non-player ship type id.
     * @param design Ship design template to instantiate.
     * @param randomize_xy Non-zero to place the ship near the star instead of directly on it.
     */
    int16_t __cdecl Set_Nonplayer_Ship_XY(int16_t system_idx, int16_t type, s_ship_design* design, uint8_t randomize_xy);

    /**
     * Sets a non-player ship's destination or leaves it parked at the supplied star.
     * @param star_idx Destination or parking star index.
     * @param ship_idx Ship index to update.
     * @param is_moving Non-zero to create a moving order.
     */
    int16_t __cdecl Set_Nonplayer_Ship_Destination(int16_t star_idx, int16_t ship_idx, uint8_t is_moving);

    /**
     * Marks a ship destroyed and relocates an assigned officer if needed.
     * @param ship_idx Ship index to retire.
     */
    void __cdecl Kill_Ship(game::s_world_state* world_state, int16_t ship_idx);

    /**
     * Clears the auto-colonization marker stored in a colony or outpost hull.
     * @param ship Ship record to update.
     */
    void __cdecl Clear_Auto_Colonization_Flag(s_ship_data* ship);

    /**
     * Checks whether an enemy ship stack is moving toward one of the player's colonies or outposts.
     * @param node_idx Ship-node index for the stack being inspected.
     */
    bool __cdecl Enemy_Ship_Heading_Toward_Our_Colony(int16_t node_idx);

    /** Sends all currently selected ships in the fleet box to attack Antares. */
    int16_t __cdecl Deploy_Ships_To_Attack_Antares();
} // namespace SHIPS