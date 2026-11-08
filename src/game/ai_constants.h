#pragma once

#include <cstdint>

namespace AIDATA {
    // Forecast offsets include the current turn at zero.
    inline constexpr int32_t FORECAST_TURN_COUNT = 10;
    inline constexpr int32_t LAST_FORECAST_TURN = FORECAST_TURN_COUNT - 1;
} // namespace AIDATA
