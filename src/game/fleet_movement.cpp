#include "pch.h"

#include "legacy_world.h"
#include "fleet_movement.h"

namespace SHIPMOVE {
    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin) {
        *result = {};
        if (ships.empty() || MOX::NUM_SHIPS < 0 || MOX::NUM_STARS < 0) return e_fleet_evaluation::invalid_input;
        auto encoded = legacy_world::Ship_List(ships);
        auto target = legacy_world::Index(destination);
        auto source = origin ? legacy_world::Index(*origin) : std::optional<int16_t>{};
        if (!encoded || !target || (origin && !source) || destination > (uint32_t)MOX::NUM_STARS ||
            (origin && *origin >= (uint32_t)MOX::NUM_STARS)) return e_fleet_evaluation::invalid_input;
        for (uint32_t id : ships) {
            if (id >= (uint32_t)MOX::NUM_SHIPS) return e_fleet_evaluation::invalid_input;
        }
        if (source) {
            SHIPMOVE::Evaluate_Fleet_Move_From_Star(encoded->data(), *target, *source, result);
        } else {
            SHIPMOVE::Ships_Try_To_Move_To(encoded->data(), *target, result);
        }
        return result->moving ? e_fleet_evaluation::can_move : e_fleet_evaluation::blocked;
    }

    bool Order_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result) {
        if (ships.empty() || MOX::NUM_SHIPS < 0 || MOX::NUM_STARS < 0) return false;
        auto encoded = legacy_world::Ship_List(ships);
        auto target = legacy_world::Index(destination);
        if (!encoded || !target || destination > (uint32_t)MOX::NUM_STARS) return false;
        for (uint32_t id : ships) {
            if (id >= (uint32_t)MOX::NUM_SHIPS) return false;
        }
        SHIPMOVE::Make_Ships_Move_To(encoded->data(), *target, result);
        return true;
    }
} // namespace SHIPMOVE
