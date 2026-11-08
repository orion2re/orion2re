#include "pch.h"

#include "ai_legacy.h"

namespace AI_LEGACY {
    AIDATA::s_world_view World() {
        AIDATA::s_world_view world{
            MOX::player,
            MOX::ship,
            MOX::star,
            MOX::colony,
            MOX::planet,
            MOX::leaders,
            &MOX::settings,
            &MOX::NUM_PLAYERS,
            &MOX::NUM_SHIPS,
            &MOX::NUM_STARS,
            &MOX::NUM_COLONIES,
            &MOX::planet_count,
            &MOX::stardate,
            &MOX::cur_combat_x,
            {&game_random::random_seed, game_random::Random},
        };
        world.rules = {
            .build = &AIBUILD::rules,
            .movement = &AIMOVE::rules,
            .design = &aidesign::rules,
            .research = &aitech::rules,
            .valuation = &AIDATA::planet_valuation_rules,
            .technology = {
                .applications = TECHDATA::technology_applications,
                .fields = TECHDATA::technology_fields,
                .armor = TECHDATA::armor,
                .computers = TECHDATA::computers,
                .drives = TECHDATA::drives,
                .fuel = TECHDATA::fuel,
                .hulls = TECHDATA::hull_data,
                .shields = TECHDATA::shields,
                .specials = TECHDATA::specials,
                .weapons = TECHDATA::weapons,
                .weapon_modifications = TECHDATA::weapon_modifications,
                .rifles = TECHDATA::rifles,
                .personal_shields = TECHDATA::personal_shields,
            },
            .colony = {
                .race_picks = &raceopt::pick_rules,
                .population_bonus = &COLCALC::population_bonus,
                .food_farmer_bonus = &COLCALC::food_farmer_bonus,
                .minerals_per_mine = COLCALC::minerals_per_mine,
                .climate_maintenance_modifiers = COLCALC::climate_maintenance_modifiers,
            },
            .combat = {
                .satellite_space_allocation = &COMBINIT::satellite_space_allocation,
                .ground_defense = &COMBINIT::ground_defense,
                .satellite_ammo_basis = &COMBINIT::satellite_ammo_basis,
                .missile_base_ammo_basis = &COMBINIT::missile_base_ammo_basis,
            },
            .engine_design = {
                .missile_x3_shot = &DESIGN::missile_x3_shot,
            },
            .refit = {
                .hulls = TECHDATA::hull_data,
                .governments = COLCALC::government_bonuses,
                .alternative_refit = &DESIGN::alternative_refit,
            },
        };
        world.movement = Movement();
        return world;
    }
} // namespace AI_LEGACY
