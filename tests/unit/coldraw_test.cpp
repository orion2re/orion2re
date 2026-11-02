#include "pch.h"

#include "game/coldraw.h"
#include "game/colony_read.h"
#include "doctest/doctest.h"
#include "fakes/fake_coldraw_runtime.h"

#include <algorithm>

namespace {
    // Arithmetic and packed-bit ordering from coldraw.cpp before bdbfc0b.
    colony_read::s_resource_row Baseline_Resource_Row(const s_colony* colony, int16_t type) {
        colony_read::s_resource_row row = {};
        if (type != ECON_INDUSTRY) {
            const int32_t value = abs((int32_t)colony->imports[type]);
            row.import_tens = (int16_t)(value / 10);
            row.import_units = (int16_t)(value % 10);
        }
        if (type == ECON_FOOD) {
            const int32_t value = abs((int32_t)colony->imports[ECON_INDUSTRY]);
            row.secondary_tens = (int16_t)(value / 10);
            row.secondary_units = (int16_t)(value % 10);
        } else if (type == ECON_INDUSTRY) {
            row.secondary_tens = colony->pollution / 10;
            row.secondary_units = colony->pollution % 10;
        }
        const int16_t production = colony->production[type];
        int16_t shortage = colony->maintenance[type] - colony->imports[type] - production;
        if (shortage < 1) shortage = 0;
        row.shortage_tens = (int16_t)(abs((int32_t)shortage) / 10);
        row.shortage_units = (int16_t)(abs((int32_t)shortage) % 10);

        int32_t net;
        if ((int8_t)colony->imports[type] < 0) {
            if (type == ECON_INDUSTRY) {
                net = (int32_t)production - colony->maintenance[type];
                if (net < 0) net = 0;
            } else {
                net = (int32_t)production - abs((int32_t)colony->imports[type]);
            }
        } else {
            if (colony->maintenance[ECON_INDUSTRY] == 0 || type != ECON_INDUSTRY) {
                net = production;
            } else {
                net = (int32_t)production - colony->maintenance[type];
                if (net < 0) net = 0;
            }
        }
        row.net_tens = (int16_t)(net / 10);
        row.net_units = (int16_t)(net % 10);
        return row;
    }

    int16_t Baseline_Population_Order(int16_t job, int16_t* out) {
        const auto& colony = MOX::colony[0];
        const int16_t races[] = {9, 0, 1, 2, 3, 4, 5, 6, 7, 8};
        int16_t count = 0;
        for (int state = 0; state < 7; ++state) {
            for (int conquered = 0; conquered < 2; ++conquered) {
                for (int profession = job == -1 ? 0 : job; profession < (job == -1 ? 3 : job + 1); ++profession) {
                    for (int race : races) {
                        for (int pop = 0; pop < colony.n_pops; ++pop) {
                            const uint32_t value = colony.pop[pop];
                            const int pop_state = (value & 0xf) == 9 ? 3 : ((value & 0xf) == 8 ? 4 : 2);
                            if ((value & 0xf) == race && pop_state == state && ((value & 0x180) >> 7) == profession &&
                                (value & 0x200) != 0 && ((value & 0x400) >> 10) == conquered) {
                                out[count++] = pop;
                            }
                        }
                    }
                }
            }
        }
        return count;
    }

    int16_t Population_Mode(int16_t mode, int16_t* slots, int16_t* last, int16_t* scroll, int16_t* field, uint8_t shadows = 0) {
        return COLDRAW::Do_Colony_Info_Pop_Stuff_For_Pop(0, ECON_FOOD, mode, 10, 20, 50, shadows, slots, last, scroll, field);
    }
} // namespace

DOCTEST_TEST_CASE("shared resource groups retain native shortage and signed byte import branches") {
    TEST_COLDRAW::Reset_();
    auto& colony = MOX::colony[0];
    colony.production[ECON_FOOD] = 0;
    colony.maintenance[ECON_FOOD] = 3;
    colony.imports[ECON_FOOD] = 0;
    colony_read::s_resource_row row;
    colony_read::Resource_Row(&colony, ECON_FOOD, &row);
    DOCTEST_CHECK(row.shortage_units == 3);
    COLDRAW::Draw_Colony_Prod_Both(0, ECON_FOOD, 0, 20, 300, 30, 0);
    DOCTEST_CHECK(TEST_COLDRAW::Short_Anim_Calls_() == row.shortage_units);
    colony.production[ECON_FOOD] = 140;
    colony.imports[ECON_FOOD] = 128;
    colony_read::Resource_Row(&colony, ECON_FOOD, &row);
    DOCTEST_CHECK(row.net_tens == 1);
    DOCTEST_CHECK(row.net_units == 2);
    colony.production[ECON_INDUSTRY] = 1;
    colony.maintenance[ECON_INDUSTRY] = 2;
    colony_read::Resource_Row(&colony, ECON_INDUSTRY, &row);
    DOCTEST_CHECK(row.net_units == 0);
}

DOCTEST_TEST_CASE("food shortage animations are shown with no production or imports") {
    TEST_COLDRAW::Reset_();

    MOX::colony[0].production[ECON_FOOD] = 0;
    MOX::colony[0].imports[ECON_FOOD] = 0;
    MOX::colony[0].maintenance[ECON_FOOD] = 3;

    COLDRAW::Draw_Colony_Prod_Both(0, ECON_FOOD, 0, 20, 300, 30, 0);

    DOCTEST_CHECK(TEST_COLDRAW::Short_Anim_Calls_() == 3);
    DOCTEST_CHECK(TEST_COLDRAW::Draw_Calls_() == 3);
}

DOCTEST_TEST_CASE("resource extraction matches pre-branch arithmetic at signed width boundaries") {
    TEST_COLDRAW::Reset_();
    const int16_t values[] = {-32768, -32767, -257, -256, -129, -128, -127, -11, -1, 0, 1, 9, 10, 11, 127, 128, 129, 255, 256, 32767};
    auto& colony = MOX::colony[0];
    for (int16_t type = 0; type < ECON_COUNT; ++type) {
        for (int16_t production : values) {
            for (int16_t imports : values) {
                for (int16_t maintenance : values) {
                    colony = {};
                    colony.production[type] = production;
                    colony.imports[ECON_INDUSTRY] = maintenance;
                    colony.imports[type] = imports;
                    colony.maintenance[ECON_INDUSTRY] = imports;
                    colony.maintenance[type] = maintenance;
                    colony.pollution = production;
                    const auto expected = Baseline_Resource_Row(&colony, type);
                    colony_read::s_resource_row actual;
                    colony_read::Resource_Row(&colony, type, &actual);
                    DOCTEST_CHECK(actual.net_tens == expected.net_tens);
                    DOCTEST_CHECK(actual.net_units == expected.net_units);
                    DOCTEST_CHECK(actual.import_tens == expected.import_tens);
                    DOCTEST_CHECK(actual.import_units == expected.import_units);
                    DOCTEST_CHECK(actual.secondary_tens == expected.secondary_tens);
                    DOCTEST_CHECK(actual.secondary_units == expected.secondary_units);
                    DOCTEST_CHECK(actual.shortage_tens == expected.shortage_tens);
                    DOCTEST_CHECK(actual.shortage_units == expected.shortage_units);
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("resource drawing retains icon groups separators and export shortage suppression") {
    for (int16_t imports : {12, -12}) {
        TEST_COLDRAW::Reset_();
        auto& colony = MOX::colony[0];
        colony.production[ECON_FOOD] = 23;
        colony.imports[ECON_FOOD] = imports;
        colony.imports[ECON_INDUSTRY] = -15;
        colony.maintenance[ECON_FOOD] = 40;
        COLDRAW::Draw_Colony_Prod_Both(0, ECON_FOOD, 0, 20, 300, 30, 0);
        const auto& runtime = TEST_COLDRAW::runtime;
        // Baseline: net, secondary industry, two empty slots, imports, then shortage (imports only).
        const int net_icons = imports > 0 ? 5 : 2;
        const int step = imports > 0 ? 13 : 12;
        DOCTEST_REQUIRE(runtime.draw_count == (imports > 0 ? 19 : 11));
        for (int i = 0; i < runtime.draw_count; ++i) {
            const auto& draw = runtime.draws[i];
            DOCTEST_CHECK(draw.x == (i + (i >= net_icons + 6 ? 2 : 0)) * step);
            DOCTEST_CHECK(draw.y == 20);
            if (i < net_icons) {
                DOCTEST_CHECK(draw.kind == TEST_COLDRAW::PRODUCTION);
                DOCTEST_CHECK(draw.id == (i < (imports > 0 ? 2 : 1) ? 1 : 0));
            } else if (i < net_icons + 6) {
                DOCTEST_CHECK(draw.kind == TEST_COLDRAW::PRODUCTION);
                DOCTEST_CHECK(draw.id == (i == net_icons ? 3 : 2));
            } else if (i < net_icons + 9) {
                DOCTEST_CHECK(draw.kind == (imports > 0 ? TEST_COLDRAW::IMPORT : TEST_COLDRAW::PRODUCTION));
                DOCTEST_CHECK(draw.id == (i == net_icons + 6 ? 1 : 0));
            } else {
                DOCTEST_CHECK(draw.kind == TEST_COLDRAW::SHORTAGE);
                DOCTEST_CHECK(draw.id == 0);
            }
        }
        DOCTEST_CHECK(COLONY::step_squish == 30 - step);
    }
}

DOCTEST_TEST_CASE("population extraction preserves packed-bit ordering and filters at full capacity") {
    TEST_COLDRAW::Reset_();
    auto& colony = MOX::colony[0];
    // An odd multiplier permutes every combination of the eleven population bits.
    for (int batch = 0; batch < 2048; batch += MAX_COLONY_POPULATION) {
        colony.n_pops = MAX_COLONY_POPULATION;
        for (int i = 0; i < colony.n_pops; ++i) colony.pop[i] = ((batch + i) * 37) & 0x7ff;
        for (int16_t job = -1; job <= 3; ++job) {
            int16_t expected[MAX_COLONY_POPULATION];
            int16_t actual[MAX_COLONY_POPULATION];
            const auto expected_count = Baseline_Population_Order(job, expected);
            const auto actual_count = colony_read::Population_Order(0, job, actual);
            DOCTEST_REQUIRE(actual_count == expected_count);
            for (int i = 0; i < actual_count; ++i) DOCTEST_CHECK(actual[i] == expected[i]);
        }
    }
    colony.n_pops = 0;
    int16_t unused = -99;
    DOCTEST_CHECK(colony_read::Population_Order(0, -1, &unused) == 0);
    DOCTEST_CHECK(unused == -99);
}

DOCTEST_TEST_CASE("population modes preserve selection boundaries and squish lifetime") {
    TEST_COLDRAW::Reset_();
    auto& colony = MOX::colony[0];
    colony.n_pops = 3;
    colony.pop[0] = 0x202;
    colony.pop[1] = 0x200;
    colony.pop[2] = 0x209;
    int16_t slots[MAX_COLONY_POPULATION] = {};
    int16_t last = -1, scroll = 0, field = -1;
    DOCTEST_CHECK(Population_Mode(2, slots, &last, &scroll, &field) == -1);
    DOCTEST_CHECK(COLONY::step_squish == 20);
    DOCTEST_CHECK(TEST_COLDRAW::runtime.reset_count == 0);
    DOCTEST_CHECK(Population_Mode(1, slots, &last, &scroll, &field) == -1);
    DOCTEST_CHECK(slots[0] == 1);
    DOCTEST_CHECK(slots[1] == 0);
    DOCTEST_CHECK(slots[2] == 2);
    DOCTEST_CHECK(last == 2);
    DOCTEST_CHECK(field == 42);
    DOCTEST_CHECK(COLONY::step_squish == 0);

    for (int16_t mode : {3, 4}) {
        for (int16_t position : {0, 20, 21, 30, 31, 100}) {
            scroll = position;
            TEST_COLDRAW::runtime.pointer_x = position;
            COLONY::debug_flag = 1;
            const int resets = TEST_COLDRAW::runtime.reset_count;
            const int waits = TEST_COLDRAW::runtime.wait_count;
            const int slot = position <= 20 ? 0 : (position <= 30 ? 1 : 2);
            DOCTEST_CHECK(Population_Mode(mode, slots, &last, &scroll, &field) == slots[slot]);
            DOCTEST_CHECK(COLONY::step_squish == 20);
            DOCTEST_CHECK(TEST_COLDRAW::runtime.reset_count == resets);
            DOCTEST_CHECK(TEST_COLDRAW::runtime.wait_count == waits + (mode == 3 ? 1 : 0));
        }
        last = -1;
        scroll = TEST_COLDRAW::runtime.pointer_x = 100;
        const int resets = TEST_COLDRAW::runtime.reset_count;
        DOCTEST_CHECK(Population_Mode(mode, slots, &last, &scroll, &field) == -1);
        DOCTEST_CHECK(COLONY::step_squish == 0);
        DOCTEST_CHECK(TEST_COLDRAW::runtime.reset_count == resets + 1);
        last = 2;
    }
    const int resets = TEST_COLDRAW::runtime.reset_count;
    DOCTEST_CHECK(Population_Mode(5, slots, &last, &scroll, &field) == -1);
    DOCTEST_CHECK(COLONY::step_squish == 20);
    DOCTEST_CHECK(TEST_COLDRAW::runtime.reset_count == resets);

    colony.n_pops = 0;
    DOCTEST_CHECK(Population_Mode(1, slots, &last, &scroll, &field) == -1);
    DOCTEST_CHECK(last == -1);
    for (int16_t mode : {3, 4}) DOCTEST_CHECK(Population_Mode(mode, slots, &last, &scroll, &field) == -1);
}

DOCTEST_TEST_CASE("population drawing preserves scanned icons farmer shadows and resets") {
    for (int max_farms : {0, 5, 255}) {
        TEST_COLDRAW::Reset_();
        auto& colony = MOX::colony[0];
        colony.n_pops = 2;
        colony.pop[0] = 0x201;
        colony.pop[1] = 0x200;
        colony.max_farms = max_farms;
        COLONY::scanned_pop = 1;
        COLONY::g_colony_n = 0;
        TEST_COLDRAW::runtime.cycler = 1;
        DOCTEST_CHECK(Population_Mode(0, nullptr, nullptr, nullptr, nullptr, 1) == -1);
        const auto& runtime = TEST_COLDRAW::runtime;
        DOCTEST_REQUIRE(runtime.draw_count == (max_farms == 5 ? 5 : 2));
        DOCTEST_CHECK(runtime.draws[0].kind == TEST_COLDRAW::DARK_POPULATION);
        DOCTEST_CHECK(runtime.draws[0].id == 1);
        DOCTEST_CHECK(runtime.draws[1].kind == TEST_COLDRAW::POPULATION);
        DOCTEST_CHECK(runtime.draws[1].id == 0);
        const int step = max_farms == 5 ? 6 : (max_farms == 0 ? 30 : 15);
        for (int i = 0; i < runtime.draw_count; ++i) {
            DOCTEST_CHECK(runtime.draws[i].x == 10 + i * step);
            DOCTEST_CHECK(runtime.draws[i].y == 20);
            if (i >= 2) DOCTEST_CHECK(runtime.draws[i].kind == TEST_COLDRAW::SHADOW);
        }
        DOCTEST_CHECK(runtime.shadow_count == (max_farms == 5 ? 1 : 0));
        DOCTEST_CHECK(runtime.paragraph_count == (max_farms == 0 ? 1 : 0));
        DOCTEST_CHECK(runtime.reset_count == 1);
        DOCTEST_CHECK(COLONY::step_squish == 0);
    }
}

DOCTEST_TEST_CASE("morale extraction and drawing match baseline for every signed byte and government") {
    for (int government = 0; government < 8; ++government) {
        for (int morale = -128; morale <= 127; ++morale) {
            for (uint8_t mini : {0, 1}) {
                TEST_COLDRAW::Reset_();
                MOX::colony[0].morale = morale;
                MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = government;
                const bool expected_suppressed = government >= GOVERNMENT_UNIFICATION;
                bool suppressed;
                DOCTEST_CHECK(colony_read::Morale_Icons(&MOX::colony[0], &suppressed) == (expected_suppressed ? 0 : morale / 2));
                DOCTEST_CHECK(suppressed == expected_suppressed);
                COLDRAW::Draw_Info_Morale_Both(0, 10, 20, 120, mini);
                const int count = expected_suppressed ? 0 : abs(morale / 2);
                const int icons = std::min(count, 20);
                const int x = mini ? 10 : 40;
                const int step = std::clamp((120 - x - 10) / std::max(count, 1), 1, 30);
                const auto& runtime = TEST_COLDRAW::runtime;
                DOCTEST_REQUIRE(runtime.draw_count == icons + (mini ? 0 : 1));
                DOCTEST_CHECK(COLONY::step_squish == 30 - step);
                if (!mini) {
                    DOCTEST_CHECK(runtime.draws[0].kind == TEST_COLDRAW::GOVERNMENT);
                    DOCTEST_CHECK(runtime.draws[0].id == government);
                    DOCTEST_CHECK(runtime.draws[0].x == 10);
                    DOCTEST_CHECK(runtime.draws[0].y == 19);
                }
                for (int i = 0; i < icons; ++i) {
                    const auto& draw = runtime.draws[i + (mini ? 0 : 1)];
                    DOCTEST_CHECK(draw.kind == TEST_COLDRAW::MORALE);
                    DOCTEST_CHECK(draw.id == (morale / 2 > 0 ? (mini ? 8 : 5) : (mini ? 9 : 6)));
                    DOCTEST_CHECK(draw.x == x + i * step);
                    DOCTEST_CHECK(draw.y == (mini ? 21 : 22));
                }
            }
        }
    }
}