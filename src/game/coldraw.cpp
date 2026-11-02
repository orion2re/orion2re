#include "pch.h"
#include "colony_read.h"

namespace colony_read {
    int16_t Population_Order(int16_t colony_idx, int16_t job, int16_t* out) {
        const auto& colony = MOX::colony[colony_idx];
        const uint8_t population_count = colony.n_pops;
        const int16_t order[] = {9, 0, 1, 2, 3, 4, 5, 6, 7, 8};
        const int16_t first_job = job == -1 ? 0 : job;
        const int16_t end_job = job == -1 ? 3 : job + 1;
        int16_t count = 0;
        for (int16_t state = 0; state < 7; ++state) {
            for (int16_t conquered = 0; conquered < 2; ++conquered) {
                for (int16_t profession = first_job; profession < end_job; ++profession) {
                    for (int16_t race : order) {
                        for (int16_t pop = 0; pop < population_count; ++pop) {
                            const auto value = colony.pop[pop];
                            if (POP::Get_Race(value) == race && COLONY::Pop_To_Pop_State(colony_idx, pop) == state &&
                                POP::Get_Prof(value) == profession && POP::Is_Assigned(value) && POP::Is_Conquered(value) == (conquered != 0)) {
                                out[count++] = pop;
                            }
                        }
                    }
                }
            }
        }
        return count;
    }

    void Resource_Row(const s_colony* colony, int16_t prod_type, s_resource_row* out) {
        *out = {};
        if (prod_type != ECON_INDUSTRY) {
            const int32_t value = abs((int32_t)colony->imports[prod_type]);
            out->import_tens = value / 10;
            out->import_units = value % 10;
        }
        if (prod_type == ECON_FOOD) {
            const int32_t value = abs((int32_t)colony->imports[ECON_INDUSTRY]);
            out->secondary_tens = value / 10;
            out->secondary_units = value % 10;
        } else if (prod_type == ECON_INDUSTRY) {
            out->secondary_tens = colony->pollution / 10;
            out->secondary_units = colony->pollution % 10;
        }
        const int16_t production = colony->production[prod_type];
        int16_t shortage = colony->maintenance[prod_type] - colony->imports[prod_type] - production;
        if (shortage < 1) shortage = 0;
        out->shortage_tens = abs((int32_t)shortage) / 10;
        out->shortage_units = abs((int32_t)shortage) % 10;
        int32_t net = production;
        if ((int8_t)colony->imports[prod_type] < 0) {
            if (prod_type == ECON_INDUSTRY) {
                net = production - colony->maintenance[prod_type];
                if (net < 0) net = 0;
            } else {
                net = production - abs((int32_t)colony->imports[prod_type]);
            }
        } else if (colony->maintenance[ECON_INDUSTRY] != 0 && prod_type == ECON_INDUSTRY) {
            net = production - colony->maintenance[prod_type];
            if (net < 0) net = 0;
        }
        out->net_tens = net / 10;
        out->net_units = net % 10;
    }

    int16_t Morale_Icons(const s_colony* colony, bool* suppressed) {
        *suppressed = MOX::player[colony->owner].traits[TRAIT_CURRENT_GOVERNMENT] >= GOVERNMENT_UNIFICATION;
        const int16_t value = (int8_t)colony->morale / 2;
        return *suppressed ? 0 : value;
    }
} // namespace colony_read

namespace COLDRAW {

    s_coldraw_road road_list[7][7];

    s_coldraw_veggie_cell veggie_list[6][6];

    uint8_t DAT_5a7760;
    uint8_t DAT_5a778a;

    void __cdecl Calculate_Squish_Step(int16_t start_x, int16_t available_width, int16_t icon_count, int16_t icon_spacing) {
        int16_t divisor = icon_count;
        int16_t base_val = icon_spacing;

        int16_t intermediate = (int16_t)(base_val / -3);
        intermediate = (intermediate - start_x) + available_width;

        if (divisor <= 1) {
            divisor = 1;
        }

        int16_t step_calc = intermediate / divisor;

        if (step_calc <= 1) {
            step_calc = 1;
        }

        if (step_calc < base_val) {
            COLONY::step_squish = base_val - step_calc;
        } else {
            COLONY::step_squish = 0;
        }
    }

    void __cdecl Draw_Colony_Prod_Both(int16_t colony_idx, int16_t prod_type, int16_t x, int16_t y, int16_t width, int16_t spacing, int8_t frame) {
        int16_t drawn_count = 0;

        s_colony* colony = &MOX::colony[colony_idx];
        colony_read::s_resource_row row;
        colony_read::Resource_Row(colony, prod_type, &row);
        const int16_t net_tens = row.net_tens;
        const int16_t net_units = row.net_units;
        const int16_t import_tens = row.import_tens;
        const int16_t import_units = row.import_units;
        const int16_t secondary_tens = row.secondary_tens;
        const int16_t secondary_units = row.secondary_units;
        const int16_t shortage_tens = row.shortage_tens;
        const int16_t shortage_units = row.shortage_units;

        int16_t total_tens = import_tens + secondary_tens + net_tens + shortage_tens;
        int16_t total_units = import_units + secondary_units + net_units + shortage_units;

        if (colony->imports[prod_type] != 0 && prod_type != ECON_INDUSTRY) {
            total_units++;
        }
        if (secondary_tens != 0 || secondary_units != 0) {
            total_units++;
        }

        int16_t total_icons = total_tens + total_units;
        COLDRAW::Calculate_Squish_Step(x, width, total_icons, spacing);

        if (spacing <= COLONY::step_squish + 1) {
            COLONY::step_squish = spacing - 2;
            graphics::Set_Window(x, y - 20, width, y + 20);
            video::Clip_On();
        }

        int16_t step = spacing - COLONY::step_squish;

        for (int i = 0; i < net_tens; ++i) {
            animate::Draw(step * drawn_count + x, y, COLONY::Prod_Anims(prod_type, 1, frame));
            drawn_count++;
        }

        for (int i = 0; i < net_units; ++i) {
            animate::Draw(step * drawn_count + x, y, COLONY::Prod_Anims(prod_type, 0, frame));
            drawn_count++;
        }

        if (secondary_tens != 0 || secondary_units != 0) {
            if (prod_type == ECON_INDUSTRY) {
                for (int i = 0; i < secondary_tens; ++i) {
                    animate::Draw(step * drawn_count + x, y, COLONY::Import_Anims(1, 1, frame));
                    drawn_count++;
                }
                for (int i = 0; i < secondary_units; ++i) {
                    animate::Draw(step * drawn_count + x, y, COLONY::Import_Anims(1, 0, frame));
                    drawn_count++;
                }
            } else if (prod_type == ECON_FOOD) {
                for (int i = 0; i < secondary_tens; ++i) {
                    animate::Draw(step * drawn_count + x, y, COLONY::Prod_Anims(1, 1, frame));
                    drawn_count++;
                }
                for (int i = 0; i < secondary_units; ++i) {
                    animate::Draw(step * drawn_count + x, y, COLONY::Prod_Anims(1, 0, frame));
                    drawn_count++;
                }
            }
            drawn_count++;
        }

        drawn_count++;

        if (colony->imports[prod_type] < 0 || prod_type == ECON_INDUSTRY) {
            for (int i = 0; i < import_tens; ++i) {
                animate::Draw(step * drawn_count + x, y, COLONY::Prod_Anims(prod_type, 1, frame));
                drawn_count++;
            }
            for (int i = 0; i < import_units; ++i) {
                animate::Draw(step * drawn_count + x, y, COLONY::Prod_Anims(prod_type, 0, frame));
                drawn_count++;
            }
        } else {
            for (int i = 0; i < import_tens; ++i) {
                animate::Draw(step * drawn_count + x, y, COLONY::Import_Anims(prod_type, 1, frame));
                drawn_count++;
            }
            for (int i = 0; i < import_units; ++i) {
                animate::Draw(step * drawn_count + x, y, COLONY::Import_Anims(prod_type, 0, frame));
                drawn_count++;
            }
            for (int i = 0; i < shortage_tens; ++i) {
                animate::Draw(step * drawn_count + x, y, COLONY::Short_Anims(prod_type, 1, frame));
                drawn_count++;
            }
            for (int i = 0; i < shortage_units; ++i) {
                animate::Draw(step * drawn_count + x, y, COLONY::Short_Anims(prod_type, 0, frame));
                drawn_count++;
            }
        }

        video::Clip_Off();
    }

    void __cdecl Draw_Colony_Prod(int16_t colony_idx, int16_t prod_type, int16_t x, int16_t y, int16_t width, int16_t spacing) {
        Draw_Colony_Prod_Both(colony_idx, prod_type, x, y, width, spacing, 0);
    }

    void __cdecl Draw_Info_Morale_Both(int16_t colony_idx, int16_t x, int16_t y, int32_t width, uint8_t is_mini) {
        s_colony* colony = &MOX::colony[colony_idx];
        bool suppressed;
        int16_t morale_div2 = colony_read::Morale_Icons(colony, &suppressed);

        int32_t morale_count_abs = abs((int32_t)morale_div2);
        int32_t morale_count = morale_count_abs;

        s_player* player = &MOX::player[(int8_t)colony->owner];
        int8_t govt_trait = player->traits[TRAIT_CURRENT_GOVERNMENT];

        if (suppressed) {
            morale_count = 0;
        }

        if (is_mini == 0) {
            s_animation_header* govt_anim = COLONY::Govt_Anim((int16_t)govt_trait);
            animate::Draw(x, y - 1, govt_anim);
            x += 30;
        }

        Calculate_Squish_Step(x, (int16_t)width, (int16_t)morale_count, 30);

        if (!suppressed) {
            int32_t loop_limit = morale_count_abs;
            if (loop_limit > 20) {
                loop_limit = 20;
            }

            if (loop_limit > 0) {
                for (int32_t i = 0; i < loop_limit; ++i) {
                    s_animation_header* anim;
                    int16_t draw_y;

                    if (morale_div2 > 0) {
                        if (is_mini != 0) {
                            anim = COLONY::C_Anims(8);
                            draw_y = y + 1;
                        } else {
                            anim = COLONY::C_Anims(5);
                            draw_y = y + 2;
                        }
                    } else {
                        if (is_mini != 0) {
                            anim = COLONY::C_Anims(9);
                            draw_y = y + 1;
                        } else {
                            anim = COLONY::C_Anims(6);
                            draw_y = y + 2;
                        }
                    }

                    int16_t draw_x = x + (int16_t)i * (30 - COLONY::step_squish);
                    animate::Draw(draw_x, draw_y, anim);
                }
            }
        }
    }

    void __cdecl Draw_Info_Morale(int16_t colony_idx, int16_t x, int16_t y, int32_t width) {
        COLDRAW::Draw_Info_Morale_Both(colony_idx, x, y, width, 0);
    }

    void __cdecl Draw_Colony_PC_Bar(int16_t x, int16_t y, int16_t total, int16_t current) {
        int32_t max_div = (total > 1000) ? (int32_t)total : 1000;
        int32_t capped_cur = (current < total) ? (int32_t)current : (int32_t)total;

        int16_t frame1 = (int16_t)((capped_cur * 50) / max_div);
        int32_t frame2_full = ((capped_cur - (int32_t)total) * 50) / max_div + 50;
        int16_t frame2 = (int16_t)frame2_full;

        int32_t base_y_offset = ((capped_cur - max_div) * 10) / max_div;
        int16_t adjusted_y_offset = (int16_t)base_y_offset;
        adjusted_y_offset += (int16_t)(((int32_t)total - capped_cur) * 64 / max_div);
        adjusted_y_offset -= (int16_t)(((int32_t)total * 64) / max_div);
        int32_t combined_y_offset = (base_y_offset & 0xFFFF0000) | (uint16_t)adjusted_y_offset;

        graphics::Set_Window(0, 0, 639 /* 0x27F */, 479 /* 0x1DF */);
        video::Clip_On();

        s_animation_header* anim_35 = COLONY::C_Anims(35 /* 0x23 */);
        animate::Set_Animation_Frame(anim_35, (int32_t)frame1);
        anim_35 = COLONY::C_Anims(35 /* 0x23 */);
        animate::Draw(x, y, anim_35);

        s_animation_header* anim_34 = COLONY::C_Anims(34 /* 0x22 */);
        animate::Set_Animation_Frame(anim_34, (int32_t)frame2);
        anim_34 = COLONY::C_Anims(34 /* 0x22 */);

        int16_t y_final = (int16_t)(combined_y_offset + (int32_t)y + 10);
        animate::Draw(x, y_final, anim_34);

        video::Clip_Off();
    }

    int16_t __cdecl Do_Colony_Info_Pop_Stuff_For_Pop(
        int16_t colony_idx, int16_t job_filter, int16_t mode, int16_t left_x, int16_t top_y, int16_t right_x, uint8_t show_farm_shadows, int16_t* pop_index_by_slot, int16_t* last_slot_idx_ptr, int16_t* scroll_value_ptr, int16_t* scroll_field_id_ptr) {
        int16_t pop_draw_index = 0;

        if (mode != 2) {
            Do_Colony_Info_Pop_Stuff_For_Pop(colony_idx, job_filter, 2, left_x, top_y, right_x, show_farm_shadows, pop_index_by_slot, last_slot_idx_ptr, scroll_value_ptr, scroll_field_id_ptr);
        }

        if (mode == 0 && job_filter == ECON_FOOD && MOX::colony[colony_idx].max_farms == 0) {
            COLONY::Set_Colony_Font_To(3);
            if (MOX::settings.language == 1 && MOX::current_screen == SCREEN_COLONY_SUMMARY) {
                BILL::Squeeze_Print_Paragraph(left_x, top_y - 1, right_x - left_x, 34, ESTRINGS::E_Strings(387 /* ESTR_NO_FARMING */), 2);
            } else {
                BILL::Squeeze_Print_Paragraph(left_x, top_y + 5, right_x - left_x, 28, ESTRINGS::E_Strings(387 /* ESTR_NO_FARMING */), 2);
            }
        }

        bool found_match = false;

        int16_t population_order[MAX_COLONY_POPULATION];
        const int16_t count = colony_read::Population_Order(colony_idx, job_filter, population_order);
        for (int16_t ordered = 0; ordered < count; ++ordered) {
            const int16_t pop_idx = population_order[ordered];
            switch (mode) {
                case 0:
                    COLONY::Set_Colony_Font_To(2);
                    if (pop_idx == COLONY::scanned_pop && COLONY::g_colony_n == colony_idx) {
                        if (ERIC::Global_Cycler(30, 2) != 0) {
                            COLONY::Draw_Pop_Dark((30 - COLONY::step_squish) * pop_draw_index + left_x, top_y, colony_idx, pop_idx);
                            break;
                        }
                    }
                    {
                        s_animation_header* anim = COLONY::Colony_Pop_Anim(colony_idx, pop_idx);
                        animate::Draw((30 - COLONY::step_squish) * pop_draw_index + left_x, top_y, anim);
                    }
                    break;

                case 1:
                    pop_index_by_slot[pop_draw_index] = pop_idx;
                    break;

                case 2:
                    break;

                case 3:
                    if (*scroll_value_ptr <= (30 - COLONY::step_squish) * (pop_draw_index + 1) + left_x || *last_slot_idx_ptr == pop_draw_index) {
                        found_match = true;
                    }
                    break;

                case 4:
                    if (mouse::Pointer_X() <= (30 - COLONY::step_squish) * (pop_draw_index + 1) + left_x || *last_slot_idx_ptr == pop_draw_index) {
                        found_match = true;
                    }
                    break;
            }

            if (found_match) {
                break;
            }

            pop_draw_index++;
        }

        switch (mode) {
            case 0:
                if (show_farm_shadows != 0 && job_filter == ECON_FOOD) {
                    const uint8_t max_farms = MOX::colony[colony_idx].max_farms;
                    if (max_farms != 0xff) {
                        if (pop_draw_index < (int16_t)max_farms) {
                            COLONY::Create_Shadow_Farmer(MOX::PLAYER_NUM);
                            while (pop_draw_index < (int16_t)max_farms) {
                                s_animation_header* anim = COLONY::Shadow_Farmer_Bitm();
                                animate::Draw((30 - COLONY::step_squish) * pop_draw_index + left_x, top_y, anim);
                                pop_draw_index++;
                            }
                        }
                    }
                }
                break;

            case 1:
                *last_slot_idx_ptr = pop_draw_index - 1;
                *scroll_field_id_ptr = fields::Add_Scroll_Field(left_x, top_y, left_x, right_x + 8, left_x, right_x, right_x - left_x + 8, 30, scroll_value_ptr, ESTRINGS::E_Strings(12 /* ESTR_SCROLL_BAR_EMPTY */), 0);
                break;

            case 2: {
                int16_t squish_count = pop_draw_index;
                const uint8_t max_farms = MOX::colony[colony_idx].max_farms;
                if (show_farm_shadows != 0 && job_filter == 0 && max_farms != 0xff) {
                    squish_count = (int16_t)max_farms;
                }
                COLDRAW::Calculate_Squish_Step(left_x, right_x, squish_count, 30);
                return -1;
            }

            case 3:
                if (found_match) {
                    if (COLONY::debug_flag != 0) {
                        key::Wait_For_Key();
                    }
                    return pop_index_by_slot[pop_draw_index];
                }
                break;

            case 4:
                if (found_match) {
                    return pop_index_by_slot[pop_draw_index];
                }
                break;

            default:
                return -1;
        }

        COLONY::Reset_Step_Squish();
        return -1;
    }

    void __cdecl Draw_Simplest_Path(int16_t previous_row, int16_t previous_col, int16_t row, int16_t col, uint8_t* first_segment_out, uint8_t* second_segment_out) {
    }

    void __cdecl Road_Back_To_Front(int16_t index, int16_t* row, int16_t* col) {
        uint8_t values[98] = {
            6, 6, 5, 6, 6, 5, 4, 6, 5, 5, 6, 4, 3, 6, 4, 3, 5, 4, 6, 3, 2, 6, 3, 5, 4, 4, 5, 3, 6, 2, 1, 6, 2, 5, 3, 4, 4, 3, 5, 2, 6, 1, 0, 6, 1, 5, 2, 4, 3, 3, 4, 2, 5, 1, 6, 0, 0, 5, 1, 4, 2, 3, 3, 2, 4, 1, 5, 0, 0, 4, 1, 3, 2, 2, 3, 1, 4, 0, 0, 3, 1, 2, 2, 1, 3, 0, 0, 2, 1, 1, 2, 0, 0, 1, 1, 0, 0, 0};

        *row = values[index * 2];
        *col = values[index * 2 + 1];
    }

    void __cdecl Draw_Road_List() {
        int16_t y;
        int16_t x;
        uint8_t draw_order[4] = {3, 2, 0, 1};

        for (int16_t i = 0; i < 49; ++i) {
            Road_Back_To_Front(i, &y, &x);

            for (uint8_t j = 0; j < 4; ++j) {
                uint8_t dir = draw_order[j];

                if (road_list[y][x].connections[dir] != 0) {
                    s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload(
                        COLONY::COLROADS_LBX,
                        road_list[y][x].anims[dir],
                        MOX::global_cache_seg);

                    animate::Draw(0, 0, anim);
                }
            }
        }
    }

    void __cdecl Load_Road_List_Anims() {
        memset(road_list, 0, sizeof(road_list));

        int16_t anim_id = 0;
        for (uint8_t c = 0; c < 4; c++) {
            for (int16_t i = 0; i < 7; i++) {
                for (int16_t j = 0; j < 7; j++) {
                    if ((i == 6 && c != 0) || (j == 6 && c != 1)) {
                        road_list[i][j].anims[c] = -1;
                        road_list[i][j].connections[c] = 0;
                    } else {
                        road_list[i][j].anims[c] = anim_id++;
                        road_list[i][j].connections[c] = 0;
                    }
                }
            }
        }
    }

    void __cdecl Link_Buildings() {
        int16_t prev_y = -1;
        int16_t prev_x = -1;

        for (int16_t i = 35; i >= 0; --i) {
            int16_t local_y;
            int16_t local_x;

            COLONY::Bldg_Back_To_Front(i, &local_y, &local_x);

            if (COLONY::colony_bldgs[local_y].buildings[local_x] != 0) {
                if (prev_y == -1) {
                    prev_y = local_y;
                    prev_x = local_x;
                } else {
                    Draw_Simplest_Path(prev_y, prev_x, local_y, local_x, &DAT_5a7760, &DAT_5a778a);
                }
            }
        }
    }

    void __cdecl Build_Road_List_Based_On_Bldg_List() {
        Link_Buildings();

        for (int16_t x = 0; x < 6; ++x) {
            for (int16_t y = 0; y < 6; ++y) {
                if (COLONY::colony_bldgs[x].buildings[y] != 0) {
                    if (game_random::Random(2) == 1) {
                        road_list[x][y].connections[0] = 1;
                    } else {
                        road_list[x][y].connections[1] = 1;
                    }

                    if (game_random::Random(2) != 1) {
                        road_list[x + 1][y].connections[0] = 1;
                    }

                    if (game_random::Random(2) != 1) {
                        road_list[x][y + 1].connections[1] = 1;
                    }

                    road_list[x][y + 1].connections[2] = 0;
                    road_list[x][y + 1].connections[3] = 0;
                } else {
                    if (road_list[x][y + 1].connections[2] != 0 &&
                        road_list[x][y + 1].connections[3] != 0) {
                        if (game_random::Random(2) == 1) {
                            road_list[x][y + 1].connections[3] = 0;
                        } else {
                            road_list[x][y + 1].connections[2] = 0;
                        }
                    }

                    if (N_Roads_Around_CR(x, y) == 4) {
                        if (road_list[x][y].connections[2] == 1 ||
                            road_list[x][y].connections[3] == 1) {
                            int16_t rand_x = x;
                            int16_t rand_y = y;
                            int16_t rand_dir = 0;

                            if (Do_Pick_Random_Road_At_And_Around_CR(&rand_x, &rand_y, &rand_dir) != 0) {
                                road_list[rand_x][rand_y].connections[rand_dir] = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    int16_t __cdecl Pick_Random_Road_At_And_Around_CR(int16_t* row, int16_t* col, int16_t* connection_idx, int16_t search_count) {
        int16_t best_row = *row;
        int16_t best_col = *col;
        int16_t best_connection = *connection_idx;
        int16_t match_count = 0;

        const uint8_t offsets[12] = {
            0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1};

        for (int16_t i = 0; i < search_count; ++i) {
            int32_t base_idx = i * 3;
            uint8_t dx = offsets[base_idx];
            uint8_t dy = offsets[base_idx + 1];
            uint8_t dconn = offsets[base_idx + 2];

            int16_t new_row = *row + dx;
            int16_t new_col = *col + dy;

            if (road_list[new_row][new_col].connections[dconn] == *connection_idx) {
                match_count++;
                if (game_random::Random(match_count) == 1) {
                    best_row = new_row;
                    best_col = new_col;
                    best_connection = dconn;
                }
            }
        }

        *row = best_row;
        *col = best_col;
        *connection_idx = best_connection;

        return match_count;
    }

    uint8_t __cdecl Do_Pick_Random_Road_At_And_Around_CR(int16_t* row, int16_t* col, int16_t* connection_idx) {
        return (uint8_t)Pick_Random_Road_At_And_Around_CR(row, col, connection_idx, 4);
    }

    int16_t __cdecl N_Roads_Around_CR(int16_t row, int16_t col) {
        uint8_t offsets[4][3] = {
            {0, 0, 0},
            {0, 0, 1},
            {0, 1, 0},
            {1, 0, 1}};

        int16_t count = 0;
        for (int16_t i = 0; i < 4; i++) {
            int16_t check_row = row + offsets[i][0];
            int16_t check_col = col + offsets[i][1];
            uint8_t dir = offsets[i][2];

            if (road_list[check_row][check_col].connections[dir] != 0) {
                count++;
            }
        }

        return count;
    }

    void __cdecl Build_Veggie_List_Based_On_Bldg_List() {
        const int16_t initial_planet_index =
            HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
        (void)COLONY::N_Bldgs(MOX::planet[initial_planet_index].colony_index);

        COLDRAW::Clear_Veggie_Draw_Flags();

        for (int16_t row = 0; row < 6; ++row) {
            for (int16_t col = 0; col < 6; ++col) {
                if (COLONY::colony_bldgs[row].buildings[col] != 0) {
                    continue;
                }

                const int16_t road_count = COLDRAW::N_Roads_Around_CR(row, col);

                const int16_t planet_index =
                    HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
                const int16_t building_count =
                    COLONY::N_Bldgs(MOX::planet[planet_index].colony_index);

                bool should_process_cell = false;
                if ((int32_t)game_random::Random(7) - 2 < (int32_t)road_count) {
                    should_process_cell = true;
                } else if (road_count == 0) {
                    if (game_random::Random(building_count + 2) != 0) {
                        should_process_cell = true;
                    }
                }

                if (!should_process_cell) {
                    continue;
                }

                if (COLDRAW::road_list[row][col].connections[2] != 0) {
                    continue;
                }

                if (COLDRAW::road_list[row][col].connections[3] != 0) {
                    continue;
                }

                for (int16_t sprite_index = 0; sprite_index < 2; ++sprite_index) {
                    COLDRAW::veggie_list[row][col].sprites[sprite_index].draw_flag =
                        (uint16_t)(game_random::Random(3) == 1);

                    if (COLDRAW::veggie_list[row][col].sprites[sprite_index].draw_flag == 0) {
                        continue;
                    }

                    const int16_t veggie_planet_index =
                        HAROLD::Planet_Index(MOX::screen_data, COLONY::orbit_temp);
                    const int16_t anim_entry =
                        COLDRAW::Pick_Random_Veggie_Anim_Entry_For_Colony_CR(
                            MOX::planet[veggie_planet_index].colony_index, row, col);

                    COLDRAW::veggie_list[row][col].anim_entries[sprite_index] = anim_entry;

                    s_animation_header* const animation =
                        (s_animation_header*)buffer::Buffer_Reload(COLONY::COLVEGGIE_LBX, anim_entry, MOX::global_cache_seg);

                    int32_t screen_x = game_random::Random(animate::Get_Width(animation));
                    screen_x -= animate::Get_Width(animation) / 2;
                    screen_x += COLONY::Bldg_Coords_To_Centered_Screen_Coord(row, col, 0);
                    COLDRAW::veggie_list[row][col].sprites[sprite_index].screen_x =
                        (int16_t)screen_x;

                    int32_t screen_y = game_random::Random(animate::Get_Height(animation));
                    screen_y -= animate::Get_Height(animation) / 4;
                    screen_y += COLONY::Bldg_Coords_To_Centered_Screen_Coord(row, col, 1);
                    COLDRAW::veggie_list[row][col].sprites[sprite_index].screen_y =
                        (int16_t)screen_y;
                }
            }
        }
    }

    void __cdecl Draw_Colony_Wee_Prod(int16_t colony_idx, int16_t prod_idx, int16_t x, int16_t y, int16_t width, int16_t height) {
        Draw_Colony_Prod_Both(colony_idx, prod_idx, x, y, width, height, 1);
    }

    void __cdecl Draw_Info_Wee_Morale(int16_t colony_idx, int16_t x, int16_t y, int32_t width) {
        Draw_Info_Morale_Both(colony_idx, x, y, width, 1);
    }

    void __cdecl Clear_Veggie_Draw_Flags() {
        for (int16_t row = 0; row < 6; ++row) {
            for (int16_t column = 0; column < 6; ++column) {
                for (int16_t sprite_index = 0; sprite_index < 2; ++sprite_index) {
                    veggie_list[row][column].sprites[sprite_index].draw_flag = 0;
                }
            }
        }
    }

    void __cdecl Draw_Veggie_CR(int16_t row_idx, int16_t col_idx, uint8_t draw_pass_count) {
        s_coldraw_veggie_cell& veggie_cell = COLDRAW::veggie_list[row_idx][col_idx];
        const uint16_t initial_pass_count = draw_pass_count;

        for (int16_t remaining_passes = (int16_t)initial_pass_count;
             remaining_passes >= 0;
             --remaining_passes) {
            for (int16_t sprite_idx = 0; sprite_idx < 2; ++sprite_idx) {
                if (veggie_cell.sprites[sprite_idx].draw_flag != 0) {
                    graphics::Set_Window(0, 0, 0x27F, 0x1DF);
                    video::Clip_On();

                    if (draw_pass_count != 0 &&
                        initial_pass_count == (uint16_t)remaining_passes) {
                        s_animation_header* animation =
                            (s_animation_header*)buffer::Buffer_Reload(COLONY::COLVEGGIE_LBX,
                                veggie_cell.anim_entries[sprite_idx],
                                MOX::global_cache_seg);
                        ERIC::Draw_Bottom_Centered(veggie_cell.sprites[sprite_idx].screen_x,
                            veggie_cell.sprites[sprite_idx].screen_y,
                            animation);
                    } else {
                        s_animation_header* animation =
                            (s_animation_header*)buffer::Buffer_Reload(COLONY::COLVEGGIE_LBX,
                                veggie_cell.anim_entries[sprite_idx],
                                MOX::global_cache_seg);
                        ERIC::Draw_Bottom_Centered_No_Glass(veggie_cell.sprites[sprite_idx].screen_x,
                            veggie_cell.sprites[sprite_idx].screen_y,
                            animation);
                    }

                    video::Clip_Off();
                }
            }
        }
    }

    int16_t __cdecl Pick_Random_Veggie_Anim_Entry_For_Colony_CR(int16_t colony_idx, int16_t row, int16_t col) {
        int32_t random_entry = (int16_t)(game_random::Random(8) - 1);
        int32_t veggie_type = (int16_t)COLDRAW::Veggie_Type_For_Colony(colony_idx);

        random_entry -= ((int32_t)row + (int32_t)col) / 2;
        if (random_entry <= 0) {
            random_entry = 0;
        }

        return (int16_t)(random_entry + veggie_type * 8);
    }

    int16_t __cdecl Veggie_Type_For_Colony(int16_t colony_idx) {
        int16_t veggie_type = 4;
        const int16_t planet_idx = MOX::colony[colony_idx].planet;
        const int32_t climate = MOX::planet[planet_idx].climate;

        switch (climate) {
            case PLANET_CLIMATE_TOXIC:
                return (game_random::Random(2) == 1) ? 5 : 9;

            case PLANET_CLIMATE_RADIATED:
            case PLANET_CLIMATE_BARREN:
                return (game_random::Random(2) == 1) ? 6 : 4;

            case PLANET_CLIMATE_DESERT:
                return 8;

            case PLANET_CLIMATE_TUNDRA:
                return 4;

            case PLANET_CLIMATE_OCEAN:
                switch (game_random::Random(4)) {
                    case 1:
                        return 7;
                    case 2:
                        return 9;
                    case 3:
                        return 11;
                    case 4:
                        return 4;
                    default:
                        return veggie_type;
                }

            case PLANET_CLIMATE_SWAMP:
                return 10;

            case PLANET_CLIMATE_ARID:
                return 1;

            case PLANET_CLIMATE_TERRAN:
            case PLANET_CLIMATE_GAIA:
                switch (game_random::Random(4)) {
                    case 1:
                        return 12;
                    case 2:
                        return 3;
                    case 3:
                        return 0;
                    case 4:
                        veggie_type = 2;
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }

        return veggie_type;
    }

} // namespace COLDRAW