#include "pch.h"
#include "turn_review_rpc.h"
#include "rpc_game.h"
#include "game/history_read.h"
#include "config/text_encoding.h"

namespace turn_review_rpc {
    bool Read_Turn_Review(rpc::s_map_observation* observation) {
        if (observation == nullptr || !observation->players_captured || observation->viewer != MOX::PLAYER_NUM ||
            MOX::player == nullptr || observation->viewer < 0 || observation->viewer >= MOX::NUM_PLAYERS || MOX::NUM_PLAYERS > MAX_PLAYERS ||
            RUSS::all_ai_game != 0 || MOX::player[observation->viewer].eliminated != 0 || MOX::player[observation->viewer].objectives != PLAYER_OBJECTIVE_HUMAN) return false;
        const int16_t viewer = (int16_t)observation->viewer;
        observation->history_captured = false;
        observation->history = {};
        history_read::s_axis axis;
        history_read::Read_Axis(&axis);
        observation->history.x_step = axis.x_step;
        observation->history.smoothing = INFO::history_graph_smoothing;
        for (const auto& label : axis.labels) observation->history.labels.emplace_back(label);
        uint8_t selected = MOX::player[viewer].history_btns & 15;
        if (selected == 0) selected = 15;
        bool history_ready = true;
        for (uint8_t series : {uint8_t(1), uint8_t(2), uint8_t(4), uint8_t(8), selected}) {
            history_read::s_chart native;
            if (!history_read::Read_Chart(viewer, series, &native)) {
                history_ready = false;
                break;
            }
            rpc::s_history_chart chart;
            chart.series = series;
            chart.top_scale = native.top_scale;
            for (int16_t row = 0; row < native.count; ++row) {
                const auto& curve = native.curves[row];
                rpc::s_history_curve item;
                item.slot = curve.player;
                for (int16_t sample = 0; sample < native.samples; ++sample) item.y.push_back((int16_t)(390 - (int16_t)curve.height[sample]));
                chart.curves.push_back(std::move(item));
            }
            observation->history.charts.push_back(std::move(chart));
        }
        observation->history_captured = history_ready;
        if (!history_ready) observation->history = {};
        observation->messages_captured = false;
        observation->messages.clear();
        const int16_t count = MSG_::N_Msgs_For_Player(viewer);
        const int16_t first = MSG_::First_Msg_For_Player(viewer);
        if (count > MSG_::max_msgs[viewer] || first + count > MAX_MESSAGES || MSG_::native_msg == nullptr) return true;
        uint8_t order[MAX_MESSAGES];
        REPORT::Order_Turn_Summary_Messages(order, first, count, REPORT::sort_serious_turn_summary_first);
        for (int16_t row = 0; row < count; ++row) {
            char text[256];
            int16_t colony = -1;
            if (!MSG_::Read_Message(viewer, order[row] - first, text, sizeof(text), &colony)) return true;
            const auto& native = MSG_::msgs[order[row]];
            rpc::s_message_observation item;
            item.text_available = native.msg_type >= MSG_TYPE_COLONY && native.msg_type <= MSG_TYPE_SCRAP_LEADER && native.msg_type != MSG_TYPE_UNKNOWN_0x3 &&
                (native.msg_type != MSG_TYPE_COLONY || (native.u.colony.subtype >= 0 && native.u.colony.subtype < MSG_COLONY_SUBTYPE_COUNT));
            if (item.text_available) config::Decode_Cp850_To_Utf8(text, strlen(text), &item.text);
            item.serious = REPORT::Is_Serious_Msg(native);
            if (colony >= 0) {
                const int16_t planet = MOX::colony[colony].planet;
                if (planet >= 0 && planet < MOX::planet_count) {
                    item.colony_system = MOX::planet[planet].star_index;
                    item.colony_orbit = MOX::planet[planet].orbit + 1;
                }
            }
            observation->messages.push_back(std::move(item));
        }
        observation->messages_captured = true;
        return true;
    }
} // namespace turn_review_rpc
