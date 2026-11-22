#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

static test_colonies::Slots<2> colonies;

static s_player players[2];
static test_stars::Slots<2> stars;
static int16_t pop_limits[10];

namespace TEST_SETTLER {
    void Reset_() {
        colonies.Reset();
        test_planets::Reset(2);
        memset(players, 0, sizeof(players));
        stars.Reset();
        memset(pop_limits, 0, sizeof(pop_limits));
        SETTLER::allow_last_population_transfer = false;
        SETTLER::max_pop_freighted = ORIGINAL_MAX_FREIGHTED_SETTLERS;
        MOX::NUM_PLAYERS = 1;
        test_stars::Resize(2);
        test_planets::Resize_Dense(2);
    }

    void Set_Pop_Limit_(int32_t race_idx, int16_t pop_limit) {
        pop_limits[race_idx] = pop_limit;
    }
}

namespace MOX {
    int16_t NUM_PLAYERS;



    s_player* player = players;
}

namespace COLCALC {
    int16_t __cdecl Colony_Race_Pop_Limit(const s_colony*, int16_t race_idx) {
        return pop_limits[race_idx];
    }
}

namespace EVENTS {
    int16_t __cdecl Event_Check_Plague(game::id_colony) {
        return 0;
    }

    int16_t __cdecl Event_Check_Space_Anomaly(game::id_colony) {
        return 0;
    }

    int16_t __cdecl Event_Check_Space_Anomaly_At_Star(int16_t) {
        return 0;
    }
}

namespace GEO {
    int32_t __cdecl Parsecs_Between_Points(int16_t, int16_t, int16_t, int16_t) {
        return 0;
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, int16_t) {
    }
}
