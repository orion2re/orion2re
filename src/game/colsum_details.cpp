#include "pch.h"

namespace COLSUM {
    int16_t g_sort_index;

    const char s_colony_scan_info_en[] =
        "%s%s %s \n%sravity\nMineral %s\nPopulation (%d/%d)\n"
        "Enroute (%s%d)\n%dk %+dk (%d)\n%d/%d +%d";
    const char s_colony_scan_info_de[] =
        "%s%s %s \n%s.\nMineralien %s\nBev}lkerung (%d/%d)\n"
        "Unterwegs (%s%d)\n%dk %+dk (%d)";
    const char s_colony_scan_info_fr[] =
        "%s%s, %s \n%s\nMinerais %s\nPopulation (%d/%d)\n"
        "En route (%s%d)\n%dk %+dk (%d)";
    const char s_colony_scan_info_es[] =
        "%s%s %s \n%s\nMineral %s\nPoblaci}n (%d/%d)\n"
        "En camino (%s%d)\n%dk %+dk (%d)";
    const char s_colony_scan_info_it[] =
        "%s%s, %s \n%s \nMinerali: %s\nPopolazione (%d/%d)\n"
        "In viaggio (%s%d)\n%dk %+dk (%d)";

    const char* const s_colony_scan_info_formats[] = {
        s_colony_scan_info_en,
        s_colony_scan_info_de,
        s_colony_scan_info_fr,
        s_colony_scan_info_es,
        s_colony_scan_info_it,
    };

    const char* const s_empire_population_formats[2][5] = {
        {
            "%sPopulation: %s%d+%d",
            "%sBev}lkr.: %s%d+%d",
            "%sPopulation: %s%d+%d",
            "%sPoblaci}n: %s%d+%d",
            "%sPopolaz: %s%d+%d",
        },
        {
            "%sPop: %s%d+%d",
            "%sBev.: %s%d+%d",
            "%sPop : %s%d+%d",
            "%sPob: %s%d+%d",
            "%sPop: %s%d+%d",
        },
    };

    static const char* Empire_Population_Format(int16_t total_population, int16_t freighted_population) {
        bool use_short_format = false;
        if (freighted_population >= 10 || total_population >= 1000) {
            use_short_format = freighted_population >= 100 || total_population >= 100;
        }

        const char* format = ESTRINGS::E_Strings(use_short_format ? 359 : 358);
        if (strstr(format, "%d+%d") != nullptr) {
            return format;
        }

        uint8_t language = MOX::settings.language;
        return s_empire_population_formats[use_short_format ? 1 : 0][language < 5 ? language : 0];
    }

    void __cdecl Sort_Col_List() {
        int16_t outer_count = MAX_COLONIES;
        do {
            for (int16_t i = 0; i < MAX_COLONIES - 1; ++i) {
                int16_t val1 = COLXPORT::g_colony_list_ptr[i];
                int16_t val2 = COLXPORT::g_colony_list_ptr[i + 1];

                if (Switched_cmp(val1, val2) > 0) {
                    COLXPORT::g_colony_list_ptr[i] = val2;
                    COLXPORT::g_colony_list_ptr[i + 1] = val1;
                }
            }
            outer_count--;
        } while (outer_count != 0);
    }

    int16_t __cdecl Switched_cmp(int16_t colony_idx_1, int16_t colony_idx_2) {
        if (colony_idx_1 == -1 || colony_idx_2 == -1) {
            return 0;
        }

        switch (g_sort_index) {
            case 0:
                return cmp_Alpha(colony_idx_1, colony_idx_2);
            case 1:
                return -cmp_Pops(colony_idx_1, colony_idx_2);
            case 2:
                return -cmp_Food(colony_idx_1, colony_idx_2);
            case 3:
                return -cmp_Industry(colony_idx_1, colony_idx_2);
            case 4:
                return -cmp_Research(colony_idx_1, colony_idx_2);
            case 5:
                return cmp_Prod(colony_idx_1, colony_idx_2);
            case 6:
                return -cmp_BC(colony_idx_1, colony_idx_2);
            case 7:
                return -cmp(MOX::colony[colony_idx_1].production_spent,
                    MOX::colony[colony_idx_2].production_spent);
            default:
                return 0;
        }
    }

    void __cdecl Draw_Empire_Info() {
        char buffer[200];

        COLONY::Set_Colony_Font_To(2);

        int16_t player_num = MOX::PLAYER_NUM;
        s_player* player = &MOX::player[player_num];

        // Part 0: BC
        ERIC::String_Builder2(buffer, sizeof(buffer), 0, ESTRINGS::E_Strings(0x76 /* 118 */), ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, player->bc);

        // Part 1: Surplus BC (with color formatting)
        const char* red_fmt = ERIC::Red_If_Negative_Fmt_String(player->surplus_bc);
        ERIC::String_Builder2(buffer, sizeof(buffer), 1, ESTRINGS::E_Strings(0x6a /* 106 */), ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, red_fmt, player->surplus_bc);

        // Part 2: Total Population
        if (player->settlers_freighted == 0) {
            ERIC::String_Builder2(buffer, sizeof(buffer), 2, ESTRINGS::E_Strings(0x72 /* 114 */), ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, player->total_pop);
        } else {
            const char* format = Empire_Population_Format(player->total_pop, player->settlers_freighted);
            ERIC::String_Builder2(buffer, sizeof(buffer), 2, format, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, player->total_pop, player->settlers_freighted);
        }

        // Part 3: Surplus Freighters
        ERIC::String_Builder2(buffer, sizeof(buffer), 3, ESTRINGS::E_Strings(0x67 /* 103 */), ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, player->surplus_freighters);

        // Part 4: Surplus Food
        ERIC::String_Builder2(buffer, sizeof(buffer), 4, ESTRINGS::E_Strings(0x66 /* 102 ESTR_SFOOD_SD */), ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, player->surplus_food);

        int32_t research_value = player->research_produced;
        int16_t research_string = 0x75;
        const int64_t accumulated_research =
            (int64_t)player->research_accumulated +
            player->research_produced;

        switch (print_research_value) {
            case RESEARCH_VALUE_ACCUMULATED:
                research_value = (int32_t)accumulated_research;
                break;
            case RESEARCH_VALUE_REMAINING: {
                const int64_t research_cost = COLCALC::Player_Research_Cost(
                    player, (uint8_t)player->current_research_field);
                research_value = (int32_t)(research_cost * 2 - accumulated_research);
                break;
            }
            case RESEARCH_VALUE_PERCENTAGE_1:
            case RESEARCH_VALUE_PERCENTAGE_2: {
                const int32_t breakthrough_chance =
                    COLCALC::Chance_For_Research_Breakthrough(player);
                if (breakthrough_chance > 0) {
                    research_value = breakthrough_chance;
                    research_string = 0x7E;
                } else if (print_research_value == RESEARCH_VALUE_PERCENTAGE_2) {
                    const int64_t research_cost = COLCALC::Player_Research_Cost(
                        player, (uint8_t)player->current_research_field);
                    research_value = (int32_t)(research_cost - accumulated_research);
                }
                break;
            }
        }

        ERIC::String_Builder2(
            buffer, sizeof(buffer), 5, ESTRINGS::E_Strings(research_string), ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, research_value);

        // Render the built strings
        FMTPARA::Print_Formatted_Paragraph(520 /* 0x208 */, 354 /* 0x162 */, 104 /* 0x68 */, buffer, 3);

        COLONY::Set_Colony_Font();
    }

    int16_t __cdecl cmp_Alpha(int16_t colony_idx_1, int16_t colony_idx_2) {
        char name1[52];
        char name2[52];
        char* planet_name_ptr;

        planet_name_ptr = HACCESS::Get_Planet_Name_No_Prime(MOX::colony[colony_idx_1].planet);
        strlcpy(name1, planet_name_ptr, sizeof(name1));

        planet_name_ptr = HACCESS::Get_Planet_Name_No_Prime(MOX::colony[colony_idx_2].planet);
        strlcpy(name2, planet_name_ptr, sizeof(name2));

        return (int16_t)strcasecmp(name1, name2);
    }

    int16_t __cdecl cmp(int16_t lhs, int16_t rhs) {
        if (lhs < rhs) {
            return -1;
        }

        return (int16_t)(rhs < lhs);
    }

    int16_t __cdecl cmp_Pops(int16_t colony_idx1, int16_t colony_idx2) {
        uint16_t pops1 = static_cast<uint8_t>(MOX::colony[colony_idx1].n_pops);
        uint16_t pops2 = static_cast<uint8_t>(MOX::colony[colony_idx2].n_pops);

        return cmp(pops1, pops2);
    }

    int16_t __cdecl cmp_Food(int16_t colony_idx1, int16_t colony_idx2) {
        return cmp(MOX::colony[colony_idx1].production[ECON_FOOD],
            MOX::colony[colony_idx2].production[ECON_FOOD]);
    }

    int16_t __cdecl cmp_Industry(int16_t colony_idx_1, int16_t colony_idx_2) {
        return cmp(MOX::colony[colony_idx_1].production[ECON_INDUSTRY],
            MOX::colony[colony_idx_2].production[ECON_INDUSTRY]);
    }

    int16_t __cdecl cmp_Research(int16_t colony_idx_1, int16_t colony_idx_2) {
        return cmp(MOX::colony[colony_idx_1].production[ECON_RESEARCH],
            MOX::colony[colony_idx_2].production[ECON_RESEARCH]);
    }

    int16_t __cdecl cmp_BC(int16_t colony_idx_1, int16_t colony_idx_2) {
        return cmp(MOX::colony[colony_idx_1].production[ECON_BC],
            MOX::colony[colony_idx_2].production[ECON_BC]);
    }

    int16_t __cdecl cmp_Prod(int16_t colony_idx_1, int16_t colony_idx_2) {
        int16_t prod1 = MOX::colony[colony_idx_1].producing[0];
        int16_t prod2 = MOX::colony[colony_idx_2].producing[0];

        int16_t sort_type1 = Prod_To_Sort_Type(prod1);
        int16_t sort_type2 = Prod_To_Sort_Type(prod2);

        int16_t sort_res = cmp(sort_type1, sort_type2);
        if (sort_res != 0) {
            return -sort_res;
        }

        const char* name1 = COLBLDG::Selection_Name(prod1);
        const char* name2 = COLBLDG::Selection_Name(prod2);

        int name_diff = strcmp(name1, name2);
        if (name_diff != 0) {
            int16_t normalized_diff = (name_diff > 0) ? 1 : -1;
            return -normalized_diff;
        }

        int16_t turns1 = COLONY::Calculate_Current_Production_Turn_Count(colony_idx_1);
        int16_t turns2 = COLONY::Calculate_Current_Production_Turn_Count(colony_idx_2);

        return cmp(turns1, turns2);
    }

    void __cdecl Draw_Colony_Scan_Info(void) {
        int16_t total_roundoff;
        int16_t total_growth;
        int16_t planet_idx;
        uint8_t climate_idx;
        char buffer[1000];

        total_roundoff = 0;
        total_growth = 0;

        if (COLONY::g_colony_n != -1) {
            s_colony* colony = &MOX::colony[COLONY::g_colony_n];
            climate_idx = (uint8_t)colony->climate;
            planet_idx = colony->planet;

            int16_t y_pos = 349 /* 0x15d */;
            for (int16_t i = 0; i < ECON_COUNT; ++i) {
                COLDRAW::Draw_Colony_Wee_Prod(COLONY::g_colony_n, i, 106, y_pos, 366, 20);
                y_pos += 18;
            }

            COLDRAW::Draw_Info_Wee_Morale(COLONY::g_colony_n, 106, 421, 366);
            COLONY::Set_Colony_Font_To(1);

            for (int16_t i = 0; i < 10; ++i) {
                total_roundoff += colony->pop_roundoff[i];
                total_growth += colony->pop_growth[i];
            }

            int32_t inbound_population = 0;
            s_player* owner = &MOX::player[colony->owner];
            for (int16_t i = 0; i < owner->settlers_freighted; ++i) {
                if (SETTLER::Get_Dest_Planet(owner->settlers[i].val) == colony->planet) {
                    inbound_population++;
                }
            }

            int32_t population_change = (total_roundoff + total_growth) / 1000;
            if (total_roundoff + total_growth < 0) {
                population_change--;
            }

            int32_t net_production = colony->production[ECON_INDUSTRY] - colony->maintenance[ECON_INDUSTRY];
            if (net_production < 0) {
                net_production = 0;
            }

            COLONY::Sum_Colonists(COLONY::g_colony_n, -1, -1, -1, -1, -1);

            const char* sign_str = "";
            if (total_growth < 0) {
                /* Fmtpara: "\x1B2" colors negative growth before the numeric value is rendered. */
                sign_str = "\x1B"
                           "2";
            }

            s_planet_data* planet = &MOX::planet[planet_idx];
            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(planet, MOX::PLAYER_NUM);
            const char* format = ESTRINGS::E_Strings(74 /* 0x4a */);
            if (strstr(format, "%s%d") == nullptr) {
                uint8_t language = MOX::settings.language;
                format = s_colony_scan_info_formats[language < 5 ? language : 0];
            }

            snprintf(buffer, sizeof(buffer), format, sign_str, MOX::planet_size_string[planet->size], MOX::planet_climate_string[climate_idx], MOX::planet_gravity_string[planet->gravity_class], MOX::mineral_class_string[planet->mineral_class], (uint32_t)(uint8_t)colony->n_pops, (int32_t)max_pop, sign_str, inbound_population, (int32_t)total_roundoff, (int32_t)total_growth, population_change, (int32_t)colony->production_spent + colony->bought_outright, COLCALC::Colony_Producing_Product_Cost(colony), net_production, "\x1B"
                "0");

            BILL::Squeeze_Print_Formatted_Paragraph(13, 354, 80, 88, buffer, 0);
        }
    }

    int16_t __cdecl Prod_To_Sort_Type(int16_t production_id) {
        if (COLBLDG::Colony_Production_Is_Queued_Ship(production_id)) {
            // Queued ship production maps through the queued-ship helper to the ship index.
            return static_cast<int16_t>(MOX::ship[COLBLDG::Colony_Production_Ship_Index(production_id)].d.size) + 10000;
        }

        if (production_id == COLONY_PRODUCTION_FREIGHTERS) {
            return 9999;
        }

        // Handle specific building indices (1-48)
        if (COLBLDG::Colony_Production_Is_Building(production_id)) {
            return static_cast<int16_t>(TECHDATA::buildings[production_id].cost) + 10;
        }

        // Special categories of items/projects
        if (production_id == COLONY_PRODUCTION_SCIENTIST ||
            production_id == COLONY_PRODUCTION_FARMER ||
            production_id == COLONY_PRODUCTION_WORKER ||
            production_id == COLONY_PRODUCTION_SPY) {
            return -1;
        }

        if (production_id == COLONY_PRODUCTION_HOUSING ||
            production_id == COLONY_PRODUCTION_TRADE_GOODS ||
            production_id == COLONY_PRODUCTION_REPEAT) {
            return -2;
        }

        // Ship design pseudo-ids sort separately from queued ship instances.
        if (COLBLDG::Colony_Production_Is_Ship_Design(production_id)) {
            return 0;
        }

        // Specific indices returning 0
        if (production_id == COLONY_PRODUCTION_TRANSPORT_SHIP ||
            production_id == COLONY_PRODUCTION_COLONY_SHIP ||
            production_id == COLONY_PRODUCTION_CUSTOM_DESIGN ||
            production_id == COLONY_PRODUCTION_OUTPOST_SHIP ||
            production_id == COLONY_PRODUCTION_DOOM_STAR ||
            production_id == COLONY_PRODUCTION_REFIT ||
            production_id == COLONY_PRODUCTION_SEPARATOR ||
            production_id >= BUILDING_COUNT) {
            return 0;
        }

        // Optimized fallback for production_id == no selection
        if (production_id == COLONY_PRODUCTION_NONE) {
            return -1;
        }

        return 0;
    }
} // namespace COLSUM
