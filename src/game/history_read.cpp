#include "pch.h"
#include "history_read.h"

namespace history_read {
    int16_t Copy_Players(int16_t viewer, uint8_t* players) {
        if (players == nullptr || MOX::player == nullptr || viewer < 0 || viewer >= MOX::NUM_PLAYERS || MOX::NUM_PLAYERS > MAX_PLAYERS) return 0;
        int16_t count = 0;
        players[count++] = (uint8_t)viewer;
        for (int16_t player = 0; player < MOX::NUM_PLAYERS; ++player) {
            if (player != viewer && !MOX::player[player].eliminated && MOX::player[viewer].contact[player]) players[count++] = (uint8_t)player;
        }
        const bool omniscient = HAROLD::Player_Is_Omniscient(viewer);
        for (int16_t player = 0; player < MOX::NUM_PLAYERS; ++player) {
            if (player == viewer || (!omniscient && !MOX::player[player].eliminated)) continue;
            bool found = false;
            for (int16_t index = 0; index < count; ++index) found |= players[index] == player;
            if (!found) players[count++] = (uint8_t)player;
        }
        return count;
    }

    void Read_Axis(s_axis* out) {
        *out = {};
        const uint16_t elapsed = (uint16_t)(MOX::stardate + 30536);
        int32_t date = 35000;
        int16_t step = 50;
        if (elapsed <= 35) {
            out->x_step = 10;
            step = 5;
        } else if (elapsed <= 70) {
            out->x_step = 5;
            step = 10;
        } else if (elapsed <= 175) {
            out->x_step = 2;
            step = 25;
        } else if (elapsed > 350) {
            date = ((MOX::stardate - 345) / 10) * 10;
        }
        for (int16_t index = 0; index < 8; ++index, date += step) {
            const int32_t fraction = date % 10;
            if (date % 5 == 0 && fraction != 0)
                snprintf(out->labels[index], sizeof(out->labels[index]), MOX::settings.language == 0 ? "%i.%i" : "%i,%i", date / 10, fraction);
            else
                snprintf(out->labels[index], sizeof(out->labels[index]), "%i", date / 10);
        }
    }

    bool Read_Chart(int16_t viewer, uint8_t series, s_chart* out) {
        if (out == nullptr) return false;
        *out = {};
        uint8_t players[MAX_PLAYERS];
        out->count = Copy_Players(viewer, players);
        if (out->count == 0 || (series & 15) == 0 || series > 15) return false;
        for (int index = 2; index < 6; ++index) {
            if (MOX::bill_savegame[index] == 0) return false;
        }
        const int16_t start = BILL::Get_History_Params(&out->samples);
        if (start < 0 || start >= 350 || out->samples < 0 || out->samples > 350) return false;
        int32_t maximum = 0;
        for (int16_t row = 0; row < out->count; ++row) {
            auto& curve = out->curves[row];
            curve.player = players[row];
            const auto& player = MOX::player[curve.player];
            for (int16_t sample = 0; sample < out->samples; ++sample) {
                const int16_t index = (start + sample) % 350;
                int32_t value = 0;
                if (series & 1) value += (uint8_t)player.population_history[index] * MOX::bill_savegame[2];
                if (series & 2) value += (uint8_t)player.production_history[index] * MOX::bill_savegame[3];
                if (series & 4) value += (uint8_t)player.fleet_history[index] * MOX::bill_savegame[4];
                if (series & 8) value += (uint8_t)player.tech_history[index] * MOX::bill_savegame[5];
                curve.height[sample] = value;
            }
            if (INFO::history_graph_smoothing) {
                for (int pass = 0; pass < 10; ++pass) {
                    for (int16_t sample = 0; sample < out->samples; ++sample) INFO::Smooth_History_Data(curve.height, sample, out->samples);
                }
            }
            for (int16_t sample = 0; sample < out->samples; ++sample) maximum = std::max(maximum, curve.height[sample]);
        }
        while (maximum >= out->top_scale) {
            switch (out->top_scale) {
                case 25: out->top_scale = 50; break;
                case 50: out->top_scale = 100; break;
                case 100: out->top_scale = 250; break;
                case 250: out->top_scale = 500; break;
                case 500: out->top_scale = 1000; break;
                case 1000: out->top_scale = 2000; break;
                default: out->top_scale += 2000; break;
            }
        }
        const int32_t divisor = (int32_t)((uint32_t)out->top_scale * 100) / 250;
        for (int16_t row = 0; row < out->count; ++row) {
            for (int16_t sample = 0; sample < out->samples; ++sample) out->curves[row].height[sample] = (int32_t)((uint32_t)out->curves[row].height[sample] * 100) / divisor;
        }
        return true;
    }
} // namespace history_read
