#pragma once

#include <cstdint>

namespace MAP_SCALE {
    enum e_maximum_galaxy_zoom_mode : uint8_t {
        MAXIMUM_GALAXY_ZOOM_HALF,
        MAXIMUM_GALAXY_ZOOM_CLASSIC,
    };

    inline uint8_t maximum_galaxy_zoom_mode = MAXIMUM_GALAXY_ZOOM_HALF;

    inline int16_t Clamp_Map_Offset_(int32_t offset, int16_t map_extent, int32_t viewport_extent) {
        const int32_t max_offset = static_cast<int32_t>(map_extent) - viewport_extent;
        // A viewport larger than the galaxy must not introduce a negative origin.
        if (max_offset <= 0 || offset < 0) {
            return 0;
        }
        return static_cast<int16_t>(offset > max_offset ? max_offset : offset);
    }

    inline bool Uses_Extended_Scaling_(int16_t star_count, int16_t original_max_stars) {
        return star_count > original_max_stars;
    }

    inline int16_t Extended_Scale_For_Zoom_Level_(
        int16_t max_map_scale,
        int16_t max_zoom_level,
        int16_t zoom_level,
        uint8_t zoom_mode) {
        if (zoom_level < 0) {
            zoom_level = 0;
        }
        if (zoom_level >= max_zoom_level) {
            return max_map_scale;
        }

        if (zoom_mode == MAXIMUM_GALAXY_ZOOM_CLASSIC && max_zoom_level == 3) {
            constexpr int16_t classic_scales_without_closest[3] = {15, 20, 30};
            const int16_t scale = classic_scales_without_closest[zoom_level];
            return scale < max_map_scale ? scale : max_map_scale;
        }

        int32_t scale = max_map_scale;
        for (int16_t level = max_zoom_level; level > zoom_level; --level) {
            scale = (scale + 1) / 2;
        }
        return static_cast<int16_t>(scale);
    }

    inline int16_t Extended_Zoom_Level_For_Scale_(
        int16_t map_scale,
        int16_t max_map_scale,
        int16_t max_zoom_level,
        uint8_t zoom_mode) {
        for (int16_t zoom_level = max_zoom_level; zoom_level > 0; --zoom_level) {
            if (map_scale >= Extended_Scale_For_Zoom_Level_(
                                 max_map_scale, max_zoom_level, zoom_level, zoom_mode)) {
                return zoom_level;
            }
        }
        return 0;
    }

    inline int16_t Classic_Zoom_Level_For_Scale_(int16_t map_scale, int16_t max_zoom_level) {
        if (map_scale == 10 && max_zoom_level >= 0) {
            return 0;
        }
        if (map_scale == 15 && max_zoom_level >= 1) {
            return 1;
        }
        if (map_scale == 20 && max_zoom_level >= 2) {
            return 2;
        }
        if (max_zoom_level >= 3) {
            return 3;
        }
        return max_zoom_level;
    }

    inline int16_t Star_Scale_Percent_(int16_t star_count, int16_t original_max_stars, int16_t map_scale) {
        if (!Uses_Extended_Scaling_(star_count, original_max_stars) || map_scale <= 30) {
            return 100;
        }
        return static_cast<int16_t>(3000 / map_scale);
    }

    inline int16_t Scale_Star_Dimension_(int16_t dimension, int16_t scale_percent) {
        if (scale_percent >= 100) {
            return dimension;
        }

        dimension = static_cast<int16_t>((static_cast<int32_t>(dimension) * scale_percent) / 100);
        return dimension < 3 ? 3 : dimension;
    }

    inline bool Is_Extended_Max_Map_View_(
        int16_t star_count,
        int16_t original_max_stars,
        int16_t map_scale,
        int16_t max_map_scale) {
        return Uses_Extended_Scaling_(star_count, original_max_stars) && map_scale == max_map_scale;
    }

    inline bool Use_Main_Map_Overview_Markers_(
        bool setting_enabled,
        bool is_maximum_galaxy,
        int16_t star_count,
        int16_t original_max_stars,
        int16_t map_scale,
        int16_t max_map_scale) {
        return setting_enabled &&
            is_maximum_galaxy &&
            Is_Extended_Max_Map_View_(
                star_count, original_max_stars, map_scale, max_map_scale);
    }

    inline uint16_t Overview_Star_Color_Index_(
        bool is_black_hole,
        bool use_colony_palette,
        int16_t owner,
        int16_t max_players,
        uint8_t owner_color) {
        if (is_black_hole) {
            return use_colony_palette ? 9 : 10;
        }
        if (owner >= 0 && owner < max_players) {
            return owner_color;
        }
        return 8;
    }

    inline bool Uses_Visited_Neutral_Overview_Color_(
        bool is_black_hole,
        int16_t owner,
        int16_t max_players,
        bool visited) {
        return !is_black_hole &&
            (owner < 0 || owner >= max_players) &&
            (visited || owner == -2);
    }

    inline bool Uses_Visited_Uncolonized_Overview_Marker_(
        bool is_extended_max_map_view,
        bool is_black_hole,
        int16_t owner,
        bool visited) {
        return is_extended_max_map_view && !is_black_hole && owner == -1 && visited;
    }

    inline int16_t Influence_Overlay_Range_(
        int16_t ship_range,
        int16_t max_non_thorium_range,
        bool use_max_non_thorium_range) {
        return use_max_non_thorium_range && ship_range > max_non_thorium_range
            ? max_non_thorium_range
            : ship_range;
    }

    inline int16_t Influence_Overlay_Map_Radius_(int16_t ship_range) {
        constexpr int16_t pre_warp_radius = 15;
        return ship_range > 0
            ? static_cast<int16_t>(ship_range * 30)
            : pre_warp_radius;
    }

    inline int16_t Influence_Overlay_Screen_Radius_(
        int16_t map_radius,
        int16_t map_scale,
        int16_t star_dimension,
        bool is_pre_warp) {
        int16_t screen_radius = static_cast<int16_t>(
            (static_cast<int32_t>(map_radius) * 10) / map_scale);
        if (is_pre_warp) {
            screen_radius = static_cast<int16_t>(screen_radius + star_dimension / 2);
        }
        return screen_radius;
    }

    inline bool Advance_Black_Hole_Animation_(int16_t star_scale_percent) {
        return star_scale_percent >= 100;
    }
} // namespace MAP_SCALE