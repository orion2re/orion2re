#include "pch.h"

namespace COLDRAW {
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
        int16_t import_tens = 0;
        int16_t import_units = 0;
        int16_t secondary_tens = 0;
        int16_t secondary_units = 0;
        int16_t drawn_count = 0;

        s_colony* colony = &MOX::colony[colony_idx];

        if (prod_type != ECON_INDUSTRY) {
            int32_t imp_val = abs((int32_t)colony->imports[prod_type]);
            import_tens = (int16_t)(imp_val / 10);
            import_units = (int16_t)(imp_val % 10);
        }

        if (prod_type == ECON_FOOD) {
            int32_t imp_val_1 = abs((int32_t)colony->imports[ECON_INDUSTRY]);
            secondary_tens = (int16_t)(imp_val_1 / 10);
            secondary_units = (int16_t)(imp_val_1 % 10);
        } else if (prod_type == ECON_INDUSTRY) {
            secondary_tens = colony->pollution / 10;
            secondary_units = colony->pollution % 10;
        }

        int16_t prod_val = colony->production[prod_type];
        int16_t shortage = colony->maintenance[prod_type] - colony->imports[prod_type] - prod_val;
        if (shortage < 1) {
            shortage = 0;
        }

        int32_t abs_shortage = abs((int32_t)shortage);
        int16_t shortage_tens = (int16_t)(abs_shortage / 10);
        int16_t shortage_units = (int16_t)(abs_shortage % 10);

        int16_t net_tens;
        int16_t net_units;

        if ((int8_t)colony->imports[prod_type] < 0) {
            if (prod_type == ECON_INDUSTRY) {
                int32_t adj_prod = (int32_t)prod_val - colony->maintenance[prod_type];
                if (adj_prod < 0) adj_prod = 0;
                net_tens = (int16_t)(adj_prod / 10);
                net_units = (int16_t)(adj_prod % 10);
            } else {
                int32_t adj_prod = (int32_t)prod_val - abs((int32_t)colony->imports[prod_type]);
                net_tens = (int16_t)(adj_prod / 10);
                net_units = (int16_t)(adj_prod % 10);
            }
        } else {
            if (colony->maintenance[ECON_INDUSTRY] == 0 || prod_type != ECON_INDUSTRY) {
                net_tens = prod_val / 10;
                net_units = prod_val % 10;
            } else {
                int32_t adj_prod = (int32_t)prod_val - colony->maintenance[prod_type];
                if (adj_prod < 0) adj_prod = 0;
                net_tens = (int16_t)(adj_prod / 10);
                net_units = (int16_t)(adj_prod % 10);
            }
        }

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

    void __cdecl Draw_Colony_Wee_Prod(int16_t colony_idx, int16_t prod_idx, int16_t x, int16_t y, int16_t width, int16_t height) {
        Draw_Colony_Prod_Both(colony_idx, prod_idx, x, y, width, height, 1);
    }
} // namespace COLDRAW
