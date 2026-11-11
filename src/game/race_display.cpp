#include "pch.h"

namespace raceopt {
    void Format_Numeric_Trait(char* output, size_t output_size, int16_t trait_idx, int16_t value) {
        const char* sign = value < 0 ? "-" : "+";
        const char* units = trait_idx == TRAIT_TAX_BONUS ? " BC" : "";
        const int magnitude = value < 0 ? -(int)value : value;

        if (trait_idx == TRAIT_FOOD_BONUS || trait_idx == TRAIT_TAX_BONUS) {
            snprintf(
                output,
                output_size,
                "%s%i%s%s",
                sign,
                magnitude / 2,
                magnitude % 2 != 0 ? ".5" : "",
                units);
        } else {
            snprintf(output, output_size, "%s%i%s", sign, magnitude, units);
        }
    }

    s_race_stat_layout Race_Stat_Layout(const int8_t* traits) {
        int trait_count = 1;

        for (int trait_idx = TRAIT_POP_GROWTH_BONUS;
             trait_idx < TRAIT_COUNT;
             ++trait_idx) {
            if (traits[trait_idx] != 0) {
                ++trait_count;
            }
        }

        if (trait_count <= 11) {
            return {2, 13};
        }
        if (trait_count <= 13) {
            return {1, 11};
        }
        return {0, 7};
    }
} // namespace raceopt

namespace RACESEL {
    int16_t custom_flag;
    int16_t displayed_race;
    int16_t old_race;
    int16_t race_button_status[14];
    uint8_t* race_specials[14] = {
        reinterpret_cast<uint8_t*>(&stock_race[0]),
        reinterpret_cast<uint8_t*>(&stock_race[1]),
        reinterpret_cast<uint8_t*>(&stock_race[2]),
        reinterpret_cast<uint8_t*>(&stock_race[3]),
        reinterpret_cast<uint8_t*>(&stock_race[4]),
        reinterpret_cast<uint8_t*>(&stock_race[5]),
        reinterpret_cast<uint8_t*>(&stock_race[6]),
        reinterpret_cast<uint8_t*>(&stock_race[7]),
        reinterpret_cast<uint8_t*>(&stock_race[8]),
        reinterpret_cast<uint8_t*>(&stock_race[9]),
        reinterpret_cast<uint8_t*>(&stock_race[10]),
        reinterpret_cast<uint8_t*>(&stock_race[11]),
        reinterpret_cast<uint8_t*>(&stock_race[12]),
        nullptr,
    };
    char last_race_name[15];
    uint8_t race_flag[14];

    void __cdecl Message_Build(int16_t* message_count, int16_t split_at_count, char* output_text, uint32_t output_text_size) {
        int16_t next_count = static_cast<int16_t>(*message_count + 1);
        *message_count = next_count;

        if (split_at_count == next_count) {
            if (next_count > 1) {
                char separator_text[80];

                KEN::Ken_Get_Text_Message(0x68, separator_text, sizeof(separator_text));
                strlcat(output_text, separator_text, output_text_size);
            }
        } else if (next_count > 1) {
            strlcat(output_text, DESIGN::s_comma_space, output_text_size);
        }
    }

    void __cdecl Draw_Race_Selection_Screen(void) {
        s_colors shadow_colors = {{0x78, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c}};
        int16_t line_index = 0;
        int16_t total_lines = 0;
        char bonus_text[16];
        char paragraph[1024];
        char text_message[100];
        s_animation_header* animation;

        paragraph[0] = '\0';

        if (RACESEL::custom_flag == 0) {
            animation = (s_animation_header*)buffer::Buffer_Reload("RACESEL.LBX", 0x21, MOX::global_cache_seg);
            animate::Draw(0x16e, 0x34, animation);
        } else {
            animation = (s_animation_header*)buffer::Buffer_Reload("RACESEL.LBX", 0x1d, MOX::global_cache_seg);
            animate::Draw(0x16f, 0x30, animation);
        }

        if (RACESEL::old_race != RACESEL::displayed_race) {
            RACESEL::race_button_status[static_cast<int32_t>(RACESEL::old_race)] = 0;
            RACESEL::race_button_status[static_cast<int32_t>(RACESEL::displayed_race)] = 1;
        }

        if (RACESEL::race_flag[static_cast<int32_t>(RACESEL::displayed_race)] == 0) {
            animation = (s_animation_header*)buffer::Buffer_Reload(
                "RACESEL.LBX",
                static_cast<int32_t>(RACESEL::displayed_race) + 0x0f,
                MOX::global_cache_seg);
            animate::Draw(0x36, 0x3f, animation);
        } else {
            fonts::Set_Font_Style_Shadow_Down(4, &shadow_colors);
            KEN::Ken_Get_Text_Message(0x48, text_message, sizeof(text_message));
            BILL::_Print_Paragraph(0x3c, 0xaf, 0x118, text_message, 2);
        }

        RACESEL::old_race = RACESEL::displayed_race;

        if ((((RACESEL::displayed_race == 13) && (RACESEL::race_specials[13] != nullptr)) ||
                ((RACESEL::displayed_race != 13) &&
                    (RACESEL::race_flag[static_cast<int32_t>(RACESEL::displayed_race)] == 0))) &&
            (((RACESEL::custom_flag == 0) && (RACESEL::displayed_race != 13)) ||
                ((RACESEL::custom_flag == 1) && (RACESEL::displayed_race == 13)))) {
            uint8_t* const race_specials = RACESEL::race_specials[static_cast<int32_t>(RACESEL::displayed_race)];

            line_index = 0;
            total_lines = 0;

            for (int16_t ability_id = TRAIT_POP_GROWTH_BONUS; ability_id < TRAIT_LOW_G_WORLD; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    ++total_lines;
                }
            }

            for (int16_t ability_id = TRAIT_LOW_G_WORLD; ability_id < TRAIT_COUNT; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    ++total_lines;
                }
            }

            ++total_lines;

            for (int16_t ability_id = TRAIT_POP_GROWTH_BONUS; ability_id < TRAIT_LOW_G_WORLD; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    RACESEL::Message_Build(&line_index, total_lines, paragraph, sizeof(paragraph));
                    strlcat(paragraph, MOX::special_ability_names[ability_id], sizeof(paragraph));

                    const int32_t special_level = static_cast<int8_t>(race_specials[ability_id]);
                    const int primary_pick = ability_id - TRAIT_POP_GROWTH_BONUS;
                    const int8_t bonus = raceopt::pick_rules.primary[primary_pick]
                                                               [special_level - 1]
                                                                   .value;
                    strlcat(
                        paragraph,
                        ability_id == TRAIT_PRODUCTION_BONUS ? ": " : " ",
                        sizeof(paragraph));
                    raceopt::Format_Numeric_Trait(
                        bonus_text, sizeof(bonus_text), ability_id, bonus);

                    strlcat(paragraph, bonus_text, sizeof(paragraph));
                }
            }

            for (int16_t ability_id = TRAIT_LOW_G_WORLD; ability_id < TRAIT_COUNT; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    RACESEL::Message_Build(&line_index, total_lines, paragraph, sizeof(paragraph));
                    const int name_id = ability_id == TRAIT_RICH_HOME_WORLD &&
                            (int8_t)race_specials[ability_id] < 0
                        ? TRAIT_COUNT
                        : ability_id;
                    strlcat(paragraph, MOX::special_ability_names[name_id], sizeof(paragraph));
                }
            }

            RACESEL::Message_Build(&line_index, total_lines, paragraph, sizeof(paragraph));
            strlcat(paragraph, MOX::government_labels[static_cast<int8_t>(race_specials[0])], sizeof(paragraph));

            fonts::Set_Font_Style_Shadow_Down(2, &shadow_colors);
            fonts::Set_Outline_Color(0, 0);
            BILL::_Print_Paragraph(0x32, 0x187, 0x127, paragraph, 0);

            if (RACESEL::displayed_race == 13) {
                fonts::Set_Font_Style_Shadow_Down(4, &shadow_colors);
                fonts::Set_Outline_Color(0, 0);
                fonts::Print_Centered(0xc5, 0x2a, RACESEL::last_race_name);
            }
        }

        animation = (s_animation_header*)buffer::Buffer_Reload("RACESEL.LBX", 0, MOX::global_cache_seg);
        animate::Draw(0x30, 0x3a, animation);
    }
} // namespace RACESEL
