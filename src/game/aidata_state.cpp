#include "pch.h"

namespace AIDATA {
    s_state state;

    void Begin_Turn(s_state* ai, int32_t stardate) {
        *ai = s_state{};
        ai->stardate = stardate;
        ai->initialized = true;
    }

    void Complete_Turn(s_state* ai) {
        ai->complete = ai->initialized;
    }

    void Reset(s_state* ai) {
        *ai = s_state{};
    }
} // namespace AIDATA
