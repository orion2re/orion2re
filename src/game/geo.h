#pragma once
#include <cstdint>

namespace GEO {
    extern int16_t* dads;
    extern int16_t orbit_consts[7][2];

    /** Initializes route-blockage bitfields for stars near black holes.
     * @param distance Maximum distance from a route segment that still counts as blocked.
     */
    void __cdecl Initialize_Black_Hole_Blocks(int16_t distance);
    void __cdecl Initialize_Star_In_Nebula_Info();

    uint8_t __cdecl Star_Is_In_Some_Nebula(int16_t star_idx);

    int32_t __cdecl Parsecs_Between_Stars(int16_t star_idx1, int16_t star_idx2);
    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

    bool Black_Hole_Blocks_Points(const game::s_game_state& state, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

    /** Collects the indices of all black-hole stars.
     * @param black_hole_indices Output array that receives star indices.
     */
    int16_t __cdecl Collect_Black_Holes(int16_t* black_hole_indices);

    /** Returns whether all non-black-hole stars remain connected by valid travel links. */
    bool __cdecl Map_Is_Connected();

    /** Returns whether a map point lies inside any nebula.
     * @param x Point x coordinate.
     * @param y Point y coordinate.
     */
    bool __cdecl Point_Is_In_Some_Nebula(int16_t x, int16_t y);
    /** Computes display-space orbit coordinates and rotation frame for a star orbit.
     * @param star_idx Source star index.
     * @param orbit_idx Orbit slot index.
     * @param out_x Output x coordinate relative to the star.
     * @param out_y Output y coordinate relative to the star.
     * @param orbit_pos Output orbit animation frame.
     */
    void __cdecl Get_Orbit_Display_X_Y(int16_t star_idx, int32_t orbit_idx, int32_t* out_x, int32_t* out_y, int32_t* orbit_pos);
    /** Returns the nearest orbit slot for a display-space orbit coordinate.
     * @param x Relative x coordinate.
     * @param y Relative y coordinate.
     */
    int32_t __cdecl Get_Orbit_From_X_Y(int32_t x, int32_t y);

    bool Point_Is_In_Warp_Field_Interdictor_Of_Star(int16_t x, int16_t y, game::owner_slot faction, const s_star_data& star);

    bool __cdecl Point_Is_In_Nebula_N(int16_t x, int16_t y, int16_t nebula_idx);

    /** Advances a fleet toward a target star for up to the requested turn count.
     * @param faction Owner of the moving fleet.
     * @param start_x Starting x coordinate.
     * @param start_y Starting y coordinate.
     * @param target Destination star.
     * @param max_turns Maximum turns to simulate; zero means one turn.
     * @param out_x Output x coordinate after movement.
     * @param out_y Output y coordinate after movement.
     * @param nebula_flag Optional output set when movement enters a nebula.
     * @param interdictor_flag Output set when movement is stopped by an interdictor.
     */
    void Move_Player_1_Turn_To_Star(game::owner_slot faction, int16_t start_x, int16_t start_y, const s_star_data& target, int16_t max_turns, int16_t* out_x, int16_t* out_y, int8_t* nebula_flag, int8_t* interdictor_flag);

    /** Returns whether a point lies within distance d of a line segment.
     * @param px Point x coordinate.
     * @param py Point y coordinate.
     * @param d Distance threshold.
     * @param x1 First segment endpoint x coordinate.
     * @param y1 First segment endpoint y coordinate.
     * @param x2 Second segment endpoint x coordinate.
     * @param y2 Second segment endpoint y coordinate.
     */
    bool __cdecl Point_Within_D_Of_Seg(int16_t px, int16_t py, int16_t d, int16_t x1, int16_t y1, int16_t x2, int16_t y2);

    /** Compares two points against the oriented side of a line.
     * @param x1 First point x coordinate.
     * @param y1 First point y coordinate.
     * @param x2 Second point x coordinate.
     * @param y2 Second point y coordinate.
     * @param lx1 First line endpoint x coordinate.
     * @param ly1 First line endpoint y coordinate.
     * @param lx2 Second line endpoint x coordinate.
     * @param ly2 Second line endpoint y coordinate.
     */
    int32_t __cdecl Points_On_Same_Side_Of_Line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t lx1, int32_t ly1, int32_t lx2, int32_t ly2);

    /** Accumulates expected net damage across hit chances from 1 to 100.
     * @param min_damage Minimum rolled damage.
     * @param max_damage Maximum rolled damage.
     * @param min_chance Lowest chance that can deal damage.
     * @param base_chance Base hit chance before the per-roll increment.
     * @param reduction Flat damage reduction applied to each result.
     */
    int32_t __cdecl Actual_Expected_Damage(int32_t min_damage, int32_t max_damage, int32_t min_chance, int64_t base_chance, int32_t reduction);

    /** Finds whether two nodes share a set and optionally merges them.
     * @param node1 First node index.
     * @param node2 Second node index.
     * @param do_union Whether to merge distinct sets.
     */
    bool __cdecl Union_Find(int16_t node1, int16_t node2, bool do_union);
} // namespace GEO