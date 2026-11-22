#include "pch.h"

#include "legacy_world.h"

namespace AIDATA {
    using game::e_ship_location;

    // Preserve the existing travel-search budget independently of result availability.
    constexpr uint16_t MAX_CACHED_TRAVEL_TURNS = 254;

    std::optional<s_route> Player_N_Turns_From_Star_To_Star_With_Intermediate(
        s_state* ai, const game::s_view* world, id_player player_id, id_star source_star, id_star destination_star, bool use_extended_range) {
        const auto& state = *world->state;
        const auto source = state.stars.find(source_star);
        const auto destination = state.stars.find(destination_star);
        if (source == state.stars.end() || destination == state.stars.end()) return std::nullopt;
        const uint32_t player_index = legacy_world::Slot(player_id);
        const s_route_key key{player_id, source_star, destination_star};
        auto cached = ai->routes.intermediate.find(key);
        if (cached != ai->routes.intermediate.end()) {
            return use_extended_range ? cached->second.extended : cached->second.normal;
        }

        s_route_choices choices;
        const s_player* player = &world->players[player_index];
        const auto& direct_routes = ai->routes.direct;
        const auto source_row = direct_routes.Find_Row_Index(source_star, player_id);
        const auto destination_offset = direct_routes.Find_Destination(destination_star);
        const auto direct = direct_routes.Find_Cell(source_row, destination_offset);
        bool direct_is_best = false;
        if (player->ship_speed != 0 &&
            player->tech_applications[TECH_APP_JUMP_GATE] != TECH_RESEARCH_STATUS_RESEARCHED &&
            player->tech_applications[TECH_APP_STAR_GATE] != TECH_RESEARCH_STATUS_RESEARCHED &&
            !state.stars.contains(source->second.wormhole_star_id) &&
            !state.stars.contains(destination->second.wormhole_star_id) && direct) {
            const auto& from = source->second;
            const auto& to = destination->second;
            const auto distance = GEO::Parsecs_Between_Points(from.x, from.y, to.x, to.y);
            const auto turns_at_speed = (distance + player->ship_speed - 1) / player->ship_speed;
            if (*direct <= turns_at_speed) {
                const uint16_t turns = static_cast<uint8_t>(turns_at_speed);
                const auto destination_assessment = ai->stars.find(destination_star);
                if (destination_assessment != ai->stars.end() && ((destination_assessment->second.in_range >> player_index) & 1)) {
                    choices.normal = s_route{destination_star, turns};
                }
                choices.extended = s_route{destination_star, turns};
                direct_is_best = true;
            }
        }

        if (!direct_is_best) {
            for (int32_t index = world->Star_Count() - 1; index >= 0; --index) {
                const id_star star_id = game::Star_Key_At_Row(*world->state, index);
                const auto assessment = ai->stars.find(star_id);
                if (assessment == ai->stars.end()) continue;
                const auto& star_assessment = assessment->second;
                if ((star_assessment.in_extended_range >> player_index) & 1) {
                    const s_star_data* star = &state.stars.at(star_id);
                    if (star_id == destination_star ||
                        (star->spectral_class != STAR_CLASS_BLACK_HOLE &&
                            star->system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                            (((star->has_colony >> player_index) & 1) || star->has_colony == 0))) {
                        const auto intermediate_offset = direct_routes.Find_Destination(star_id);
                        const auto intermediate_row = direct_routes.Find_Row_Index(star_id, player_id);
                        const auto first_leg = direct_routes.Find_Cell(source_row, intermediate_offset);
                        const auto second_leg = direct_routes.Find_Cell(intermediate_row, destination_offset);
                        if (first_leg && second_leg) {
                            const uint16_t turns = *first_leg + *second_leg;
                            const id_star next_star = star_id == source_star ? destination_star : star_id;
                            if (turns <= MAX_CACHED_TRAVEL_TURNS) {
                                if (((star_assessment.in_range >> player_index) & 1) &&
                                    (!choices.normal || turns < choices.normal->turns)) {
                                    choices.normal = s_route{next_star, turns};
                                }
                                if (!choices.extended || turns < choices.extended->turns) {
                                    choices.extended = s_route{next_star, turns};
                                }
                            }
                        }
                    }
                }
                if (choices.normal && choices.normal->turns <= 1) break;
            }
        }
        ai->routes.intermediate.emplace(key, choices);
        return use_extended_range ? choices.extended : choices.normal;
    }

    std::optional<s_route> Ship_N_Turns_To_Star_With_Intermediate(s_state* ai, const game::s_view* world, id_ship ship_id, id_star destination_star) {
        const uint64_t key = Ship_Route_Key(ship_id, destination_star);
        auto cached = ai->routes.ship_intermediate.find(key);
        if (cached != ai->routes.ship_intermediate.end()) return cached->second;

        std::optional<s_route> result;
        const s_ship_data* ship = &world->ships[legacy_world::Slot(ship_id)];
        const bool has_stargate = struct_::Read_Bit_Field(ship->d.special_device_flags, 11);
        const auto location = legacy_world::Read_Ship_Location(ship);
        if (location && location->kind == e_ship_location::in_flight) {
            const id_star destination = location->star_id;
            const uint32_t owner = (uint32_t)ship->owner;
            for (int32_t index = world->Star_Count() - 1; index >= 0; --index) {
                const id_star star_id = game::Star_Key_At_Row(*world->state, index);
                const auto assessment = ai->stars.find(star_id);
                if (assessment == ai->stars.end()) continue;
                const auto& star_assessment = assessment->second;
                const bool in_range = (star_assessment.in_range >> owner) & 1;
                const bool in_extended_range = (star_assessment.in_extended_range >> owner) & 1;
                const auto* star = &world->state->stars.at(star_id);
                if ((in_range || (in_extended_range && has_stargate)) &&
                    (star_id == destination_star || star_id == destination ||
                        (star->spectral_class != STAR_CLASS_BLACK_HOLE &&
                            star->system_special != SYSTEM_SPECIAL_SPACE_MONSTER &&
                            (((star->has_colony >> owner) & 1) || star->has_colony == 0)))) {
                    if (!GEO::Black_Hole_Blocks_Points(*world->state, ship->x, ship->y, star->x, star->y)) {
                        const auto first_leg = Ship_N_Turns_To(&ai->routes, ship_id, star_id);
                        const auto second_leg = N_Turns_From_To(&ai->routes, star_id, destination_star, legacy_world::Player_Id(owner));
                        if (first_leg && second_leg) {
                            const uint16_t turns = *first_leg + *second_leg;
                            if (turns <= MAX_CACHED_TRAVEL_TURNS && (!result || turns < result->turns)) {
                                result = s_route{star_id, turns};
                            }
                        }
                    }
                }
                if (result && result->turns <= 1) break;
            }
        } else {
            auto source = location && location->kind == e_ship_location::in_wormhole
                ? game::Find_Star_At_Coordinates(*world->state, ship->x, ship->y)
                : location ? std::optional<id_star>{location->star_id} : std::nullopt;
            if (source) {
                result = Player_N_Turns_From_Star_To_Star_With_Intermediate(
                    ai, world, legacy_world::Player_Id(ship->owner), *source, destination_star, has_stargate);
            }
        }
        ai->routes.ship_intermediate.emplace(key, result);
        return result;
    }
} // namespace AIDATA
