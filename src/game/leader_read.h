#pragma once

#include <cstdint>
#include <cstring>

struct s_invasion_side_info;

namespace leader_read {
    enum e_kind {
        RESULT,
        TROOP_SUMMARY,
        GROUND_SUMMARY,
        MESSAGE_NAME,
    };

    struct s_skill {
        char name[128] = {};
        char value[80] = {};
    };

    // A presentation projection, not permission to inspect the backing record.
    struct s_leader {
        int16_t viewer = -1;
        int16_t slot = -1;
        e_kind kind = MESSAGE_NAME;
        char name[512] = {};
        char title[20] = {};
        char rank[128] = {};
        int16_t role = -1;
        int16_t bonus = 0;
        int16_t eta = -1;
        int16_t skill_count = 0;
        s_skill skills[54];
    };

    bool Read_Result(int16_t viewer, int16_t leader, s_leader* output);
    bool Read_Ground(int16_t viewer, const s_invasion_side_info* side, e_kind kind, s_leader* output);
    bool Read_Message_Name(int16_t viewer, int16_t leader, s_leader* output);
    inline bool Read_Message_Name(int16_t viewer, int16_t leader, const char* name, uint32_t capacity, s_leader* output) {
        if (output == nullptr) return false;
        *output = {};
        if (name == nullptr || viewer < 0 || leader < 0) return false;
        uint32_t length = 0;
        while (length < capacity && name[length] != '\0') ++length;
        if (length == capacity || length >= sizeof(output->name)) return false;
        output->viewer = viewer;
        output->slot = leader;
        std::memcpy(output->name, name, length + 1);
        return true;
    }
} // namespace leader_read
