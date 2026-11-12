#include "pch.h"

namespace SYS {
    int __cdecl qsort_sys_disp(const void* a, const void* b) {
        const s_sys_disp* p1 = (const s_sys_disp*)a;
        const s_sys_disp* p2 = (const s_sys_disp*)b;
        return (int)p1->y - (int)p2->y;
    }

    void __cdecl N_Ships_By_Type_In_Stack(int16_t ship_node_idx, int16_t* total_count, int16_t* colony_ships_count, int16_t* outposts_count, int16_t* transports_count, int16_t* ship_sizes) {
        *total_count = 0;
        *colony_ships_count = 0;
        *outposts_count = 0;
        *transports_count = 0;
        memset(ship_sizes, 0, (SHIP_SIZE_DOOM_STAR + 1) * sizeof(int16_t));

        while (ship_node_idx != -1) {
            const int16_t ship_idx = MOX::ship_node[ship_node_idx].ship_idx;
            s_ship_data* const ship = &MOX::ship[ship_idx];

            int16_t size = (int16_t)ship->d.size;
            if (size > SHIP_SIZE_DOOM_STAR) {
                size = SHIP_SIZE_DOOM_STAR;
            }

            switch (ship->d.ship_type) {
                case SHIP_TYPE_COMBAT:
                    ship_sizes[size]++;
                    break;
                case SHIP_TYPE_COLONY:
                    (*colony_ships_count)++;
                    break;
                case SHIP_TYPE_TRANSPORT:
                    (*transports_count)++;
                    break;
                case SHIP_TYPE_OUTPOST:
                    (*outposts_count)++;
                    break;
                default:
                    break;
            }

            (*total_count)++;
            ship_node_idx = MOX::ship_node[ship_node_idx].next_node;
        }
    }

    int16_t __cdecl Print_Special_Building_Data(
        int16_t colony_idx,
        int16_t x,
        int16_t* line_pos,
        int16_t* max_text_width,
        int16_t* total_height,
        int16_t line_spacing,
        bool draw) {
        int16_t visible_count = 0;
        const int16_t special_building_ids[7] = {
            BUILDING_STAR_FORTRESS,
            BUILDING_STAR_BASE,
            BUILDING_BATTLESTATION,
            BUILDING_MISSILE_BASE,
            BUILDING_GROUND_BATTERIES,
            BUILDING_STELLAR_CONVERTER,
            BUILDING_FIGHTER_GARRISON,
        };

        s_colony* colony = &MOX::colony[colony_idx];

        for (int16_t i = 0; i < 7; ++i) {
            const int16_t building_id = special_building_ids[i];

            if (colony->buildings[building_id] != 0) {
                if (visible_count == 0) {
                    *line_pos = static_cast<int16_t>(*line_pos + MISC::Get_Font_Style() + 3);
                    *total_height = static_cast<int16_t>(*total_height + MISC::Get_Font_Style() + 3);
                }

                *line_pos = static_cast<int16_t>(*line_pos + line_spacing);

                if (draw) {
                    fonts::Print(x, *line_pos, TECHDATA::buildings[building_id].name);
                } else {
                    *total_height = static_cast<int16_t>(*total_height + line_spacing);

                    const int16_t text_width =
                        fonts::Get_String_Width(TECHDATA::buildings[building_id].name);

                    if (*max_text_width < text_width) {
                        *max_text_width = text_width;
                    }
                }

                ++visible_count;
            }
        }

        return visible_count;
    }
} // namespace SYS
