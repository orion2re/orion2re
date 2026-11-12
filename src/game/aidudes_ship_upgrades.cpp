#include "pch.h"

#include "legacy_world.h"

namespace aidudes {
    void __cdecl Keep_Ship_Designs_Up_To_Date(s_ai_colony_context* ctx) {
        const auto legacy_player = legacy_world::Index(ctx->i_ai);
        if (!legacy_player) return;
        const auto* world = ctx->world;
        s_ship_design doom_star_design;
        doom_star_design.cost = 0;

        for (uint32_t i = 0; i < ctx->work->colonies.size(); ++i) {
            const uint32_t colony_idx = legacy_world::Slot(ctx->work->colonies[i].colony_id);
            short producing_id = world->colonies[colony_idx].producing[0];

            if (COLBLDG::Colony_Production_Is_Queued_Ship(producing_id)) {
                int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(producing_id);
                s_ship_data& ship = world->ships[ship_idx];

                if (ship.d.ship_type == SHIP_TYPE_COMBAT) {
                    const s_ship_design* source_design;
                    if (ship.d.size < SHIP_SIZE_DOOM_STAR) {
                        source_design = &ctx->p_ai->ship_designs[(int)ship.d.size];
                    } else {
                        if (doom_star_design.cost == 0) {
                            INITSHIP::Load_Doom_Star_Ship_Design(
                                *legacy_player, &doom_star_design, world->settings->strategic_combat_flag != 0);
                        }
                        source_design = &doom_star_design;
                    }
                    memcpy(&ship.d, source_design, sizeof(s_ship_design));
                }
            }
        }
    }

    void __cdecl Do_Ship_Upgrade_Cheats(AIDATA::s_state* ai, s_ai_colony_context* ctx) {
        const auto legacy_player = legacy_world::Index(ctx->i_ai);
        if (!legacy_player) return;
        const auto* world = ctx->world;
        auto& refits = ai->players[legacy_world::Slot(ctx->i_ai)].refits;
        const uint32_t num_entries = (uint32_t)refits.size();

        if (world->Stardate() <= 35035 /* 0x88db */) {
            return;
        }

        if (num_entries == 0) {
            return;
        }

        // Determine if AI is eligible for an upgrade cheat based on difficulty and last refit entry desirability
        int32_t desirability = refits[num_entries - 1].desirability;
        uint8_t difficulty_idx = (uint8_t)world->settings->game_difficulty;
        int32_t difficulty_bonus = (int32_t)AIBUILD::rules.ship_upgrade_bonus[difficulty_idx];
        int32_t upgrade_threshold = (difficulty_bonus + desirability) * 2;
        int upgrade_count = 0;

        if ((int32_t)game_random::Random_From_Seed(world->random_seed, 900) <= upgrade_threshold) {
            upgrade_count = 1;
        }

        // AI must have a stable empire (at least 2 colonies and sufficient population) to cheat
        if (ai->players[legacy_world::Slot(ctx->i_ai)].empire.colony_count < 2) {
            upgrade_count = 0;
        } else {
            int32_t total_pop = ctx->p_ai->total_pop;
            if ((int32_t)game_random::Random_From_Seed(world->random_seed, (uint32_t)total_pop) <= 8) {
                upgrade_count = 0;
            }
        }

        // Iterate backwards through refit entries and perform the upgrade if conditions met
        for (uint32_t i = num_entries; i > 0; i--) {
            if (upgrade_count == 0) {
                break;
            }

            const auto* entry = &refits[i - 1];
            s_ship_data* ship = &world->ships[legacy_world::Slot(entry->ship_id)];
            auto orbit = legacy_world::Orbit_Star(ship);
            if (!orbit) continue;
            const game::id_star location = *orbit;

            // Skip ships in systems with incoming human activity
            if (ai->stars[location].enroute_human_ships[3] != 0) {
                continue;
            }

            // Only upgrade if the system is either uncolonized or owned strictly by this AI
            uint8_t has_colony_mask = world->stars[legacy_world::Slot(location)].has_colony;
            uint32_t other_players_mask = ~(1 << legacy_world::Slot(ctx->i_ai));
            if ((has_colony_mask & other_players_mask) != 0) {
                continue;
            }

            // Apply design upgrade
            if (ship->d.size < SHIP_SIZE_DOOM_STAR) {
                ship->d = ctx->p_ai->ship_designs[(int)ship->d.size];
            } else {
                INITSHIP::Load_Doom_Star_Ship_Design(
                    *legacy_player, &ship->d, world->settings->strategic_combat_flag != 0);
            }

            upgrade_count--;

            // Remove the entry from the refit list
            refits.erase(refits.begin() + i - 1);
        }
    }
} // namespace aidudes
