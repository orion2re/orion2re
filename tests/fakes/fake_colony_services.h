#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct s_MSG;

namespace TEST_COLONY_POP_GROWTH {
    struct s_services {
        int settings_checks = 0;
        int contact_updates = 0;
        int range_updates = 0;
        int icon_updates = 0;
        int screen_updates = 0;
        int16_t maintenance_summary_player = -1;
        int16_t industry_summary_colony = -1;
        int16_t research_summary_colony = -1;
        int16_t anomaly_result = 0;
        bool in_normal_range = true;
        bool in_range = true;
        int16_t spies[MAX_PLAYERS][MAX_PLAYERS] = {};
        std::vector<int16_t> homeworlds, designs, queues, population_limits;
        std::vector<int16_t> fleet_etas, pooled, deassigned, eliminated;
        std::vector<int16_t> maintenance, stats, killed, anomalies;
        std::vector<std::array<int16_t, 2>> spy_reads, spies_home, normal_ranges;
        std::vector<std::array<int16_t, 3>> spy_writes, ranges, creations;
        std::vector<std::array<int16_t, 6>> relations;
        std::vector<s_MSG> messages;
        std::vector<int16_t> message_players;
        std::vector<std::string> titles, texts;
    };

    extern s_services services;
    void Reset_Services_();
}
