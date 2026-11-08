#include "pch.h"

#include "legacy_world.h"
#include "fleet_movement.h"

namespace SHIPMOVE {
    namespace {
        struct s_native_fleet_request {
            std::vector<int16_t> ships;
            int16_t destination;
            std::optional<int16_t> origin;
        };

        std::optional<s_native_fleet_request> Prepare_Fleet_Request(std::span<const uint32_t> ships, uint32_t destination, std::optional<uint32_t> origin) {
            if (ships.empty() || MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS ||
                MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS) return std::nullopt;
            auto target = legacy_world::Index(destination);
            auto source = origin ? legacy_world::Index(*origin) : std::optional<int16_t>{};
            // NUM_STARS itself is the legacy Antares/portal destination.
            if (!target || (origin && !source) || destination > (uint32_t)MOX::NUM_STARS ||
                (origin && *origin >= (uint32_t)MOX::NUM_STARS)) return std::nullopt;
            for (uint32_t id : ships) {
                if (id >= (uint32_t)MOX::NUM_SHIPS) return std::nullopt;
            }
            auto encoded = legacy_world::Ship_List(ships);
            if (!encoded) return std::nullopt;
            return s_native_fleet_request{std::move(*encoded), *target, source};
        }
    } // namespace

    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin) {
        *result = {};
        auto request = Prepare_Fleet_Request(ships, destination, origin);
        if (!request) return e_fleet_evaluation::invalid_input;
        if (request->origin) {
            SHIPMOVE::Evaluate_Fleet_Move_From_Star(request->ships.data(), request->destination, *request->origin, result);
        } else {
            SHIPMOVE::Ships_Try_To_Move_To(request->ships.data(), request->destination, result);
        }
        return result->moving ? e_fleet_evaluation::can_move : e_fleet_evaluation::blocked;
    }

    bool Order_Fleet_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result) {
        auto request = Prepare_Fleet_Request(ships, destination, std::nullopt);
        if (!request) return false;
        SHIPMOVE::Make_Ships_Move_To(request->ships.data(), request->destination, result);
        return true;
    }
} // namespace SHIPMOVE
