#include "pch.h"

#include "ai_legacy.h"

namespace AI_LEGACY {
    namespace {
        struct s_position {
            int16_t location;
            int16_t x;
            int16_t y;
        };

        void Query_(void*, std::span<const uint32_t> ships, uint32_t destination, std::optional<uint32_t> origin, s_ship_move_info* result) {
            *result = {};
            auto encoded = Ship_List(ships);
            auto target = Index(destination);
            auto source = origin ? Index(*origin) : std::optional<int16_t>{};
            if (!encoded || !target || (origin && !source)) return;

            std::vector<s_position> positions;
            if (source) {
                positions.reserve(ships.size());
                for (uint32_t id : ships) {
                    auto& ship = MOX::ship[id];
                    positions.push_back({ship.location, ship.x, ship.y});
                    ship.location = *source;
                    ship.x = MOX::star[*source].x;
                    ship.y = MOX::star[*source].y;
                }
            }

            SHIPMOVE::Ships_Try_To_Move_To(encoded->data(), *target, result);

            // Reverse restoration also handles repeated ship IDs in a legacy fleet.
            for (uint32_t i = (uint32_t)positions.size(); i-- > 0;) {
                auto& ship = MOX::ship[ships[i]];
                ship.location = positions[i].location;
                ship.x = positions[i].x;
                ship.y = positions[i].y;
            }
        }

        void Execute_(void*, std::span<const uint32_t> ships, uint32_t destination, s_ship_move_info* result) {
            auto encoded = Ship_List(ships);
            auto target = Index(destination);
            if (encoded && target) SHIPMOVE::Make_Ships_Move_To(encoded->data(), *target, result);
        }
    } // namespace

    AIMOVE::s_movement_source Movement() {
        return {nullptr, Query_, Execute_};
    }
} // namespace AI_LEGACY
