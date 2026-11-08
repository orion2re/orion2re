#pragma once

#include <cstdint>
#include <optional>
#include <span>

struct s_ship_move_info;

namespace AIMOVE {
    struct s_movement_source {
        void* context = nullptr;
        void (*query)(void*, std::span<const uint32_t>, uint32_t, std::optional<uint32_t>, s_ship_move_info*) = nullptr;
        void (*execute)(void*, std::span<const uint32_t>, uint32_t, s_ship_move_info*) = nullptr;

        void Try_Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result, std::optional<uint32_t> origin = std::nullopt) const {
            query(context, ships, destination, origin, result);
        }

        void Move(std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result) const {
            execute(context, ships, destination, result);
        }
    };
} // namespace AIMOVE
