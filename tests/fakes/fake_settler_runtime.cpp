#include "pch.h"
#include "../fixtures/planet_roster.h"

static s_colony colonies[2];

static s_player players[2];
static s_star_data stars[2];
static int16_t pop_limits[10];

namespace TEST_SETTLER {
    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        test_planets::Reset(2);
        memset(players, 0, sizeof(players));
        memset(stars, 0, sizeof(stars));
        memset(pop_limits, 0, sizeof(pop_limits));
        SETTLER::allow_last_population_transfer = false;
        SETTLER::max_pop_freighted = ORIGINAL_MAX_FREIGHTED_SETTLERS;
        MOX::NUM_PLAYERS = 1;
        MOX::NUM_COLONIES = 2;
        MOX::NUM_STARS = 2;
        test_planets::Resize_Dense(2);
    }

    void Set_Pop_Limit_(int32_t race_idx, int16_t pop_limit) {
        pop_limits[race_idx] = pop_limit;
    }
}

namespace MOX {
    int16_t NUM_PLAYERS;
    int16_t NUM_COLONIES;
    int16_t NUM_STARS;

    s_colony* colony = colonies;

    s_player* player = players;
    s_star_data* star = stars;
}

namespace COLCALC {
    int16_t __cdecl Colony_Race_Pop_Limit(s_colony*, int16_t race_idx) {
        return pop_limits[race_idx];
    }
}

namespace EVENTS {
    int16_t __cdecl Event_Check_Plague(int16_t) {
        return 0;
    }

    int16_t __cdecl Event_Check_Space_Anomaly(int16_t) {
        return 0;
    }

    int16_t __cdecl Event_Check_Space_Anomaly_At_Star(int16_t) {
        return 0;
    }
}

namespace GEO {
    int32_t __cdecl Parsecs_Between_Stars(int16_t, int16_t) {
        return 0;
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, int16_t) {
    }
}
