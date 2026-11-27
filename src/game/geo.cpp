#include "pch.h"
#include "legacy_ids.h"
#include "ship_ids.h"

namespace GEO {
    int16_t* dads;
    int16_t orbit_consts[7][2] = {
        {225, 118},
        {464, 242},
        {704, 372},
        {949, 502},
        {1199, 632},
        {1423, 751},
        {1650, 873},
    };

    void __cdecl Initialize_Black_Hole_Blocks(int16_t distance) {
        int16_t black_hole_indices[MAX_STARS];
        int16_t num_black_holes = Collect_Black_Holes(black_hole_indices);
        const int32_t black_hole_block_count =
            static_cast<int32_t>(std::extent_v<decltype(s_star_data::black_hole_blocks)>);

        // Clear black_hole_blocks bitfield for all stars
        auto& state = game::current_state;
        for (auto& [id, star] : state.stars) {
            for (int32_t k = 0; k < black_hole_block_count; ++k) {
                star.black_hole_blocks[k] = 0;
            }
        }

        // For every pair of stars (i, j), check if any black hole lies on the path between them
        for (int16_t i = 0; i < std::ssize(state.star_order); ++i) {
            auto& star_record = state.stars.at(state.star_order[i]);
            if (star_record.spectral_class == STAR_CLASS_BLACK_HOLE) {
                continue;
            }

            int16_t x1 = star_record.x;
            int16_t y1 = star_record.y;

            for (int16_t j = 0; j < i; ++j) {
                auto& target = state.stars.at(state.star_order[j]);
                if (target.spectral_class == STAR_CLASS_BLACK_HOLE) {
                    continue;
                }

                for (int16_t k = 0; k < num_black_holes; ++k) {
                    const auto& black_hole = state.stars.at(state.star_order[black_hole_indices[k]]);
                    if (Point_Within_D_Of_Seg(black_hole.x, black_hole.y, distance, x1, y1, target.x, target.y)) {
                        // Mark path as blocked in both stars' bitfields
                        struct_::Set_Bit_Field(star_record.black_hole_blocks, j);
                        struct_::Set_Bit_Field(target.black_hole_blocks, i);
                        break;
                    }
                }
            }
        }
    }

    /**
     * @brief Iterates through all stars and updates their nebula status.
     */
    void __cdecl Initialize_Star_In_Nebula_Info() {
        for (auto& [id, star] : game::current_state.stars) {
            star.in_nebula = Point_Is_In_Some_Nebula(star.x, star.y);
        }
    }

    /**
     * Calculates the distance in parsecs between two stars in the galaxy map.
     */
    int32_t __cdecl Parsecs_Between_Stars(int16_t star_idx_1, int16_t star_idx_2) {
        const auto& first = game::Destination_Record_At_Row(game::current_state, star_idx_1);
        const auto& second = game::Destination_Record_At_Row(game::current_state, star_idx_2);
        return GEO::Parsecs_Between_Points(first.x, first.y, second.x, second.y);
    }

    /**
     * @brief Calculates the distance in parsecs between two points (x1, y1) and (x2, y2).
     * 
     * The distance is calculated as ceil(sqrt((dx^2 + dy^2) / 900)).
     * Note: 900 corresponds to 30^2, implying 1 parsec = 30 coordinate units.
     */
    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int32_t dx = (int32_t)x1 - (int32_t)x2;
        int32_t dy = (int32_t)y1 - (int32_t)y2;
        uint32_t dist_sq = (uint32_t)(dx * dx + dy * dy);

        // Initial estimate for distance in parsecs
        uint32_t parsecs = special::isqrt(dist_sq / 900);

        // Correction loop to ensure we return the ceiling of the square root
        // effectively calculating: ceil(sqrt(dist_sq / 900.0))
        while (parsecs * parsecs * 900 < dist_sq) {
            parsecs++;
        }

        return (int32_t)parsecs;
    }

    /**
     * Checks if a specific star is located within a nebula.
     * @param star_index The index into the global star array.
     */
    uint8_t __cdecl Star_Is_In_Some_Nebula(int16_t star_index) {
        const auto& star = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_index));
        return Point_Is_In_Some_Nebula(star.x, star.y);
    }

    /**
     * Checks if a point (x, y) is within the opaque portion of a nebula's graphical representation.
     * The nebula is selected by index, and the coordinates are transformed relative to the nebula's
     * world position before being scaled down and checked against the animation frame data.
     */
    static bool Point_Is_In_Nebula(int16_t x, int16_t y, const s_nebula* nebula) {
        if (x < nebula->x || y < nebula->y) {
            return false;
        }

        const int32_t rel_x = ((int32_t)x - nebula->x) / 3;
        const int32_t rel_y = ((int32_t)y - nebula->y) / 3;

        // Access the 4th variant of the nebula animation
        const s_animation_header* anim = MOX::nebula_pict_seg[(uint8_t)nebula->type % 12][3];

        if (rel_x >= (int32_t)anim->width || rel_y >= (int32_t)anim->height) {
            return false;
        }

        const uint8_t* pixel_data = Animation_Frame_Data_(anim, 0);
        return pixel_data[rel_y * anim->width + rel_x] > 5;
    }

    bool __cdecl Point_Is_In_Nebula_N(int16_t x, int16_t y, int16_t nebula_idx) {
        return Point_Is_In_Nebula(x, y, &MOX::nebula[nebula_idx]);
    }

    std::span<const s_nebula> Session_Nebulas_For(const game::s_game_state& world) {
        if (&world != &game::current_state || !MOX::nebula || MOX::NUM_NEBULAS <= 0) return {};
        return {MOX::nebula, static_cast<size_t>(MOX::NUM_NEBULAS)};
    }

    bool __cdecl Point_Is_In_Some_Nebula(int16_t x, int16_t y) {
        bool in_any_nebula = false;
        int16_t i = 0;

        while (true) {
            // Check if current index is within the number of active nebulas
            if ((int16_t)MOX::NUM_NEBULAS <= i) {
                return in_any_nebula;
            }

            if (Point_Is_In_Nebula_N(x, y, i)) {
                in_any_nebula = true;
            }

            i++;

            // If we found the point is in a nebula, break and return
            if (in_any_nebula) {
                break;
            }
        }

        return in_any_nebula;
    }

    bool __cdecl Map_Is_Connected() {
        int16_t local_dads_buffer[MAX_STARS]; // Stack space allocated for union-find dads
        GEO::dads = local_dads_buffer;

        for (int16_t i = 0; i < std::ssize(game::current_state.stars); ++i) {
            GEO::dads[i] = -1;
        }

        for (int16_t i = 0; i < std::ssize(game::current_state.star_order); ++i) {
            const auto star_id = game::current_state.star_order[i];
            auto& star_record = game::current_state.stars.at(star_id);
            if (star_record.spectral_class != STAR_CLASS_BLACK_HOLE) {
                for (int16_t j = i + 1; j < std::ssize(game::current_state.stars); ++j) {
                    if (game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, j)).spectral_class != STAR_CLASS_BLACK_HOLE) {
                        // Check if star 'i' is blocked from star 'j' by a black hole
                        if (!struct_::Test_Bit_Field(star_record.black_hole_blocks, j)) {
                            if (Parsecs_Between_Stars(i, j) <= 8) {
                                Union_Find(i, j, 1);
                            }
                        }
                    }
                }
            }
        }

        int32_t disconnected_components = 0;
        for (int16_t i = 0; i < std::ssize(game::current_state.star_order); ++i) {
            const auto star_id = game::current_state.star_order[i];
            if (game::current_state.stars.at(star_id).spectral_class != STAR_CLASS_BLACK_HOLE) {
                if (GEO::dads[i] == -1) {
                    disconnected_components++;
                }
            }
        }

        GEO::dads = nullptr;
        return disconnected_components == 1;
    }

    /**
     * Checks if a line segment between (x1, y1) and (x2, y2) is blocked by any black holes.
     * A path is considered blocked if any black hole is within 45 coordinate units of the segment.
     */
    bool Black_Hole_Blocks_Points(const game::s_game_state& state, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        if (x1 == x2 && y1 == y2) return false;
        for (const auto& [id, star] : state.stars) {
            if (star.spectral_class == STAR_CLASS_BLACK_HOLE &&
                Point_Within_D_Of_Seg(star.x, star.y, 45, x1, y1, x2, y2)) return true;
        }
        return false;
    }

    int16_t __cdecl Collect_Black_Holes(int16_t* black_hole_indices) {
        int16_t black_hole_count = 0;

        for (int16_t i = 0; i < std::ssize(game::current_state.star_order); ++i) {
            const auto star_id = game::current_state.star_order[i];
            // STAR_CLASS_BLACK_HOLE is represented by value 6
            if (game::current_state.stars.at(star_id).spectral_class == STAR_CLASS_BLACK_HOLE) {
                black_hole_indices[black_hole_count] = i;
                black_hole_count++;
            }
        }

        return black_hole_count;
    }

    void __cdecl Get_Orbit_Display_X_Y(int16_t star_idx, int32_t orbit_idx, int32_t* out_x, int32_t* out_y, int32_t* orbit_pos) {
        const int32_t orbit_time = (star_idx * 7) + (int32_t)MOX::stardate;
        const int32_t angle_numerator = orbit_time * 5311;
        const int32_t angle_divisor = (orbit_idx * 100) + star_idx + 150;
        const int32_t orbit_angle = (angle_numerator / angle_divisor) % 360;
        const int16_t orbit_angle16 = (int16_t)orbit_angle;

        // The binary reads radii from the second pair in the orbit constants table.
        const int16_t orbit_radius_x = GEO::orbit_consts[orbit_idx + 1][0];
        const int16_t orbit_radius_y = GEO::orbit_consts[orbit_idx + 1][1];

        int32_t rounded_x = angle::Cos(orbit_angle16, orbit_radius_x) + 5;
        if (rounded_x >= 0) {
            rounded_x += 5; // Result: (cos + 10) / 10
        } else {
            rounded_x -= 5; // Result: cos / 10
        }
        *out_x = rounded_x / 10;

        // Y coordinate calculation with rounding and inversion
        int32_t rounded_y = angle::Sin(orbit_angle16, orbit_radius_y) - 5;
        if (rounded_y >= 0) {
            rounded_y += 5; // Result: sin / 10
        } else {
            rounded_y -= 5; // Result: (sin - 10) / 10
        }
        *out_y = -(rounded_y / 10);

        // Calculate rotation frame (32 frames total)
        int32_t orbit_frame = ((orbit_angle + 6) * 32) / 360;
        if (orbit_frame == 32) {
            orbit_frame = 0;
        }
        *orbit_pos = orbit_frame;
    }

    /**
     * Checks if a point (x, y) is within the radius of a warp field interdictor
     * belonging to a player other than the moving fleet's faction.
     */
    bool Point_Is_In_Warp_Field_Interdictor_Of_Star(int16_t x, int16_t y, game::owner_slot faction, const s_star_data& star) {
        bool result = false;

        // Preserve the legacy bit selection for both player and nonplayer fleets.
        const auto shift = static_cast<uint32_t>(faction.code) & (MAX_PLAYERS - 1);
        const auto other_players_mask = ~game::player_bitmap::Only_Slot(shift);

        if ((star.has_warp_field_interdictor & other_players_mask).any()) {
            int32_t dx = (int32_t)x - (int32_t)star.x;
            int32_t dy = (int32_t)y - (int32_t)star.y;

            if (dx * dx + dy * dy <= SHIPMOVE::warp_interdictor_range) {
                result = true;
            }
        }

        return result;
    }

    int32_t __cdecl Get_Orbit_From_X_Y(int32_t x, int32_t y) {
        int32_t result_orbit = -1;

        if (x != 0 || y != 0) {
            uint32_t min_dist_sq = 0xFFFFFFFF;
            int32_t best_index = -1;

            for (int32_t i = 0; i < 7; ++i) {
                // Get_Angle arguments: (short[1]*x, short[0]*-y)
                int16_t angle_val = angle::Get_Angle(GEO::orbit_consts[i][1] * x, GEO::orbit_consts[i][0] * -y);

                // Rounding for Cosine result
                int32_t cos_raw = angle::Cos(angle_val, GEO::orbit_consts[i][0]);
                int32_t scaled_cos;
                if (cos_raw + 5 >= 0) {
                    scaled_cos = (cos_raw + 10) / 10;
                } else {
                    scaled_cos = cos_raw / 10;
                }

                // Rounding for Sine result
                int32_t sin_raw = angle::Sin(angle_val, GEO::orbit_consts[i][1]);
                int32_t scaled_sin;
                if (sin_raw - 5 >= 0) {
                    scaled_sin = sin_raw / 10;
                } else {
                    scaled_sin = (sin_raw - 10) / 10;
                }

                // Distance squared calculation: (cos_res - x)^2 + (sin_res + y)^2
                int32_t dx = scaled_cos - x;
                int32_t dy = scaled_sin + y;
                uint32_t dist_sq = (uint32_t)(dx * dx + dy * dy);

                if (dist_sq < min_dist_sq) {
                    min_dist_sq = dist_sq;
                    best_index = i;
                }
            }

            // Return adjusted index if within valid middle-range [1, 5]
            if (best_index > 0 && best_index < 6) {
                result_orbit = best_index - 1;
            }
        }

        return result_orbit;
    }

    /**
     * Calculates the movement of a player's fleet towards a target star for one turn or multiple turns, 
     * accounting for movement speed (30 units/turn), nebula entry, and warp interdictors.
     */
    void Move_Player_1_Turn_To_Star(game::owner_slot faction, int16_t start_x, int16_t start_y, const s_star_data& target, int16_t max_turns, int16_t* out_x, int16_t* out_y, int8_t* nebula_flag, int8_t* interdictor_flag) {
        Move_Player_1_Turn_To_Star(faction, start_x, start_y, target, max_turns, out_x, out_y,
            nebula_flag, interdictor_flag, Session_Nebulas_For(game::current_state));
    }

    void Move_Player_1_Turn_To_Star(game::owner_slot faction, int16_t start_x, int16_t start_y, const s_star_data& target,
        int16_t max_turns, int16_t* out_x, int16_t* out_y, int8_t* nebula_flag, int8_t* interdictor_flag, std::span<const s_nebula> nebulas) {
        int16_t target_x = target.x;
        int16_t target_y = target.y;

        int32_t dx = (int32_t)target_x - (int32_t)start_x;
        int32_t dy = (int32_t)target_y - (int32_t)start_y;
        uint32_t dist_sq = (uint32_t)(dx * dx + dy * dy);
        uint32_t dist = special::isqrt(dist_sq);

        int16_t turns_limit = max_turns;
        if (turns_limit == 0) {
            turns_limit = 1;
        }

        bool in_nebula = false;
        bool reached_target = false;
        *interdictor_flag = 0;

        int16_t turn = 1;
        while (true) {
            // If fleet enters a nebula and caller requested a nebula flag, stop movement immediately
            if (nebula_flag != nullptr && in_nebula) {
                break;
            }

            // Check termination conditions: interdicted, destination reached, or turn limit reached
            if (*interdictor_flag != 0 || reached_target || turn > turns_limit) {
                break;
            }

            // Check if movement in this turn exceeds remaining distance
            // Movement speed is 30 units per turn (30^2 = 900)
            if ((int32_t)(turn * turn * 900) >= (int32_t)dist_sq) {
                reached_target = true;
                *out_x = target_x;
                *out_y = target_y;
            } else {
                // Linear interpolation to find current coordinates based on distance traveled (turn * 30)
                int32_t move_x;
                if (dx < 0) {
                    move_x = (dx * (int32_t)turn * 30 - (int32_t)dist + 1) / (int32_t)dist;
                } else {
                    move_x = (dx * (int32_t)turn * 30 + (int32_t)dist - 1) / (int32_t)dist;
                }
                *out_x = start_x + (int16_t)move_x;

                int32_t move_y;
                if (dy < 0) {
                    move_y = (dy * (int32_t)turn * 30 - (int32_t)dist + 1) / (int32_t)dist;
                } else {
                    move_y = (dy * (int32_t)turn * 30 + (int32_t)dist - 1) / (int32_t)dist;
                }
                *out_y = start_y + (int16_t)move_y;
            }

            // Check if current position is inside a nebula
            if (nebula_flag != nullptr) {
                if (std::ranges::any_of(nebulas, [&](const s_nebula& nebula) {
                    return Point_Is_In_Nebula(*out_x, *out_y, &nebula);
                })) {
                    in_nebula = true;
                    turn++;
                    continue;
                }
            }

            // Check for enemy warp interdictors blocking the path
            if (Point_Is_In_Warp_Field_Interdictor_Of_Star(*out_x, *out_y, faction, target)) {
                *interdictor_flag = 1;
            }

            turn++;
        }

        if (in_nebula && nebula_flag != nullptr) {
            *nebula_flag = 1;
        }
    }

    bool __cdecl Point_Within_D_Of_Seg(int16_t px, int16_t py, int16_t d, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        bool is_within_distance = false;
        int32_t line_dy = (int32_t)y1 - (int32_t)y2;
        int32_t line_dx = (int32_t)x2 - (int32_t)x1;

        int32_t dot_product = (px - x2) * line_dy + (py - y2) * line_dx;
        int32_t abs_dot = (dot_product ^ (dot_product >> 31)) - (dot_product >> 31);

        int32_t line_len_sq = line_dx * line_dx + line_dy * line_dy;
        int32_t distance_sq = d * d;

        if ((uint32_t)(0xFFFFFFFFu / (uint32_t)distance_sq) < (uint32_t)line_len_sq) {
            do {
                if (abs_dot <= 0xFFFF) {
                    is_within_distance = true;
                    break;
                }

                int32_t sqrt_line_len = special::isqrt(line_len_sq);
                if (abs_dot <= sqrt_line_len * d) {
                    is_within_distance = true;
                    break;
                }

                if ((sqrt_line_len + 1) * d < abs_dot) {
                    return false;
                }

                y2 /= 2;
                x2 /= 2;
                y1 /= 2;
                x1 /= 2;
                d /= 2;
                py /= 2;
                px /= 2;

                line_dy = (int32_t)y1 - (int32_t)y2;
                line_dx = (int32_t)x2 - (int32_t)x1;
                dot_product = (px - x2) * line_dy + (py - y2) * line_dx;
                abs_dot = (dot_product ^ (dot_product >> 31)) - (dot_product >> 31);

                line_len_sq = line_dx * line_dx + line_dy * line_dy;
                distance_sq = d * d;

            } while ((uint32_t)(0xFFFFFFFFu / (uint32_t)distance_sq) < (uint32_t)line_len_sq);
        }

        if (!is_within_distance) {
            if (abs_dot > 0xFFFF) {
                return false;
            }

            const uint32_t lhs = (uint32_t)distance_sq * (uint32_t)line_len_sq;
            const uint32_t rhs = (uint32_t)abs_dot * (uint32_t)abs_dot;
            if (lhs < rhs) {
                return false;
            }
        }

        if (dot_product == 0) {
            if (x2 > x1) {
                if (x1 > px || x2 < px) {
                    return false;
                }
            } else if (x2 < x1) {
                if (x2 > px || x1 < px) {
                    return false;
                }
            } else {
                if (y2 >= y1) {
                    if (y1 > py || y2 < py) {
                        return false;
                    }
                } else {
                    if (y2 > py || y1 < py) {
                        return false;
                    }
                }
            }
            return true;
        } else {
            int32_t step_len_sq = line_dy * line_dy + line_dx * line_dx;
            int32_t scaled_dy = line_dy;
            int32_t scaled_dx = line_dx;

            if (step_len_sq <= distance_sq) {
                do {
                    scaled_dy += line_dy;
                    scaled_dx += line_dx;
                    step_len_sq = scaled_dy * scaled_dy + scaled_dx * scaled_dx;
                } while (step_len_sq <= distance_sq);
            }

            int32_t candidate_x = scaled_dy + px;
            int32_t candidate_y = scaled_dx + py;

            if (Points_On_Same_Side_Of_Line(candidate_x, candidate_y, px, py, x1, y1, x2, y2) > 0) {
                candidate_x = px - scaled_dy;
                candidate_y = py - scaled_dx;
                Points_On_Same_Side_Of_Line(candidate_x, candidate_y, px, py, x1, y1, x2, y2);
            }

            if (Points_On_Same_Side_Of_Line(x1, y1, x2, y2, candidate_x, candidate_y, px, py) > 0) {
                return false;
            }

            return true;
        }
    }

    int32_t __cdecl Actual_Expected_Damage(int32_t min_damage, int32_t max_damage, int32_t min_chance, int64_t base_chance, int32_t reduction) {
        int64_t expected_total = 0;

        if (reduction < max_damage) {
            for (int32_t i = 1; i <= 100; ++i) {
                int64_t chance;

                if (i >= 96) {
                    chance = 100;
                } else {
                    chance = base_chance + i;
                    if (chance > 100) {
                        chance = 100;
                    }
                }

                if (min_chance <= chance) {
                    int64_t expected;

                    if (chance == 100) {
                        expected = max_damage;
                    } else if (max_damage <= min_damage) {
                        expected = min_damage;
                    } else {
                        expected = ((max_damage - min_damage + 1) * (chance - min_chance)) / (100 - min_chance) + min_damage;
                    }

                    int64_t net_damage = expected - reduction;
                    if (net_damage > 0) {
                        expected_total += net_damage;
                    }
                }
            }
        }

        return expected_total > INT32_MAX ? INT32_MAX : (int32_t)expected_total;
    }

    int32_t __cdecl Points_On_Same_Side_Of_Line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t lx1, int32_t ly1, int32_t lx2, int32_t ly2) {
        int32_t dx_line = lx2 - lx1;
        int32_t dy_line = ly2 - ly1;

        int32_t cross1 = (y1 - ly1) * dx_line + (lx1 - x1) * dy_line;

        if (cross1 == 0) {
            return 0;
        }

        int32_t dx2 = x2 - lx1;
        int32_t dy2 = y2 - ly1;

        if (cross1 > 0) {
            return dx_line * dy2 - dy_line * dx2;
        } else {
            return dy_line * dx2 - dx_line * dy2;
        }
    }

    bool __cdecl Union_Find(int16_t node1, int16_t node2, bool do_union) {
        int16_t root1 = node1;
        int16_t parent1 = dads[node1];
        if (parent1 >= 0) {
            do {
                root1 = dads[root1];
            } while (dads[root1] >= 0);
        }

        int16_t root2 = node2;
        if (dads[node2] >= 0) {
            do {
                root2 = dads[root2];
            } while (dads[root2] >= 0);
        }

        if (parent1 >= 0) {
            int16_t curr1 = node1;
            do {
                int16_t next1 = dads[curr1];
                dads[curr1] = root1;
                curr1 = next1;
            } while (dads[curr1] >= 0);
        }

        if (dads[node2] >= 0) {
            int16_t curr2 = node2;
            do {
                int16_t next2 = dads[curr2];
                dads[curr2] = root2;
                curr2 = next2;
            } while (dads[curr2] >= 0);
        }

        if (do_union && root2 != root1) {
            if (game_random::Random(2) == 1) {
                dads[root1] = root2;
            } else {
                dads[root2] = root1;
            }
        }

        return root2 == root1;
    }
} // namespace GEO
