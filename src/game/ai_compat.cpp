#include "pch.h"

#include "ai_compat.h"
#include "legacy_world.h"

namespace ai_compat {
    SHIPMOVE::e_fleet_evaluation Evaluate_Fleet_Move(std::span<const uint32_t> ships,
        uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin) {
        *result = {};
        const auto target = legacy_world::Index(destination);
        const auto source = origin ? legacy_world::Index(*origin) : std::optional<int16_t>{};
        if (!target || (origin && !source)) return SHIPMOVE::e_fleet_evaluation::invalid_input;
        auto ship_list = legacy_world::Ship_List(ships);
        if (!ship_list) return SHIPMOVE::e_fleet_evaluation::invalid_input;
        return SHIPMOVE::Evaluate_Fleet_Move(*ship_list, *target, result, source);
    }

    bool Order_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result) {
        const auto target = legacy_world::Index(destination);
        if (!target) return false;
        auto ship_list = legacy_world::Ship_List(ships);
        return ship_list && SHIPMOVE::Order_Fleet_Move(*ship_list, *target, result);
    }
} // namespace ai_compat
