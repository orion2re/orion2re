#include "pch.h"

#include "fleet_movement.h"

namespace SHIPMOVE {
    namespace {
        bool Valid_Fleet_Request(std::span<int16_t> ship_list, int16_t destination, std::optional<int16_t> origin) {
            if (ship_list.size() < 2 || ship_list.size() > (size_t)INT16_MAX + 1 || ship_list.back() != -1 ||
                MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS ||
                MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS) return false;
            // NUM_STARS itself is the legacy Antares/portal destination.
            if (destination < 0 || destination > MOX::NUM_STARS ||
                (origin && (*origin < 0 || *origin >= MOX::NUM_STARS))) return false;
            for (int16_t index : ship_list.first(ship_list.size() - 1)) {
                if (index < 0 || index >= MOX::NUM_SHIPS) return false;
            }
            return true;
        }
    } // namespace

    e_fleet_evaluation Evaluate_Fleet_Move(std::span<int16_t> ship_list, int16_t destination, s_ship_move_info* result, std::optional<int16_t> origin) {
        *result = {};
        if (!Valid_Fleet_Request(ship_list, destination, origin)) return e_fleet_evaluation::invalid_input;
        if (origin) {
            SHIPMOVE::Evaluate_Fleet_Move_From_Star(ship_list.data(), destination, *origin, result);
        } else {
            SHIPMOVE::Ships_Try_To_Move_To(ship_list.data(), destination, result);
        }
        return result->moving ? e_fleet_evaluation::can_move : e_fleet_evaluation::blocked;
    }

    bool Order_Fleet_Move(std::span<int16_t> ship_list, int16_t destination, s_ship_move_info* result) {
        if (!Valid_Fleet_Request(ship_list, destination, std::nullopt)) return false;
        SHIPMOVE::Make_Ships_Move_To(ship_list.data(), destination, result);
        return true;
    }
} // namespace SHIPMOVE
