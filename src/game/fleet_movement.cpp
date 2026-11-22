#include "pch.h"

#include "fleet_movement.h"

namespace SHIPMOVE {
    namespace {
        bool Valid_Fleet_Request(std::span<int16_t> ship_list, int16_t destination, std::optional<int16_t> origin) {
            if (ship_list.size() < 2 || ship_list.size() > (size_t)INT16_MAX + 1 || ship_list.back() != -1 ||
                game::current_state.ship_count < 0 || game::current_state.ship_count > MAX_SHIPS ||
                std::ssize(game::current_state.stars) < 0 || std::ssize(game::current_state.stars) > MAX_STARS) return false;
            // NUM_STARS itself is the legacy Antares/portal destination.
            if (destination < 0 || (destination >= std::ssize(game::current_state.stars) && destination != game::ANTARES_POSITION) ||
                (origin && (*origin < 0 || *origin >= std::ssize(game::current_state.stars)))) return false;
            for (int16_t index : ship_list.first(ship_list.size() - 1)) {
                if (index < 0 || index >= game::current_state.ship_count) return false;
            }
            return true;
        }
    } // namespace

    e_fleet_evaluation Evaluate_Fleet_Move(std::span<int16_t> ship_list, int16_t destination, s_ship_move_info* result, std::optional<int16_t> origin) {
        if (!result) return e_fleet_evaluation::invalid_input;
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
        if (!result || !Valid_Fleet_Request(ship_list, destination, std::nullopt)) return false;
        SHIPMOVE::Make_Ships_Move_To(ship_list.data(), destination, result);
        return true;
    }
} // namespace SHIPMOVE
