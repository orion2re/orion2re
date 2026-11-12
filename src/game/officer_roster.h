#pragma once

#include <cstdint>

struct s_leader_data;

namespace OFFICER {
    constexpr int16_t LEADERS_PER_PAGE = 4;
    extern int16_t leader_page;

    int16_t Leader_Type_Limit();
    int16_t Total_Leader_Limit();
    bool Leader_Is_Displayed(const s_leader_data& leader);
}
