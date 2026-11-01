#pragma once

#include <cstdint>

namespace relations_read {
    struct s_relation {
        bool available = false;
        char relationship[15] = {};
        int16_t relationship_offset = 0;
        char treaty[200] = {};
        bool ignoring = false;
        bool research = false;
        int16_t research_level = 0;
        bool trade = false;
        int16_t trade_level = 0;
        int16_t tribute_percent = 0;
        bool giving = false;
    };

    struct s_assignment {
        bool available = false;
        int16_t count = 0;
        int16_t mission = -1;
    };

    struct s_organization {
        bool available = false;
        int16_t agents = 0;
        bool bonuses_available = false;
        int16_t spy_bonus = 0;
        int16_t agent_bonus = 0;
    };

    struct s_report_entry {
        int16_t player = -1;
        bool alliance = false;
        char name[128] = {};
    };

    struct s_disclosure {
        int16_t viewer = -1;
        int16_t target = -1;
        bool report = false;
        bool incoming_available = false;
        int16_t incoming = 0;
        char personality[128] = {};
        char objective[128] = {};
        bool objective_empty = false;
        s_report_entry entries[8];
        int16_t count = 0;
    };

    bool Is_Contact(int16_t viewer, int16_t target);
    int16_t Relationship_Bucket(int16_t viewer, int16_t target);
    int16_t Relationship_Indicator_Offset(int16_t viewer, int16_t target);
    void Read_Relation(int16_t viewer, int16_t target, s_relation* out);
    void Copy_Treaty_Summary(int16_t viewer, int16_t target, char* out, uint32_t size);
    void Read_Assignment(int16_t viewer, int16_t target, s_assignment* out);
    void Read_Organization(int16_t viewer, s_organization* out);
    bool Read_Overlay(int16_t viewer, int16_t target, s_disclosure* out);
    bool Read_Report(int16_t viewer, int16_t target, s_disclosure* out);
} // namespace relations_read
