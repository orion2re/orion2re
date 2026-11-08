#pragma once

#include <cstdint>

struct s_tech_app_data;
struct s_tech_armor_data;
struct s_tech_computer_data;
struct s_tech_drive_data;
struct s_tech_field_data;
struct s_tech_fuel_data;
struct s_tech_hull_data;
struct s_tech_shield_data;
struct s_tech_specials_data;
struct s_tech_weapon_data;
struct s_tech_weapon_mod_data;
struct s_tech_data_rifle;
struct s_tech_data_personal_shield;

namespace AIBUILD {
    struct s_rules;
}
namespace AIMOVE {
    struct s_rules;
}
namespace aidesign {
    struct s_rules;
}
namespace aitech {
    struct s_rules;
}
namespace raceopt {
    struct s_pick_rules;
}
namespace COLCALC {
    struct s_government_bonus;
    struct s_population_bonus;
    struct s_food_farmer_bonus;
} // namespace COLCALC
namespace COMBINIT {
    struct s_satellite_space_allocation;
    struct s_ground_defense;
} // namespace COMBINIT

namespace AIDATA {
    struct s_planet_valuation_rules;

    struct s_technology_view {
        const s_tech_app_data* applications = nullptr;
        const s_tech_field_data* fields = nullptr;
        const s_tech_armor_data* armor = nullptr;
        const s_tech_computer_data* computers = nullptr;
        const s_tech_drive_data* drives = nullptr;
        const s_tech_fuel_data* fuel = nullptr;
        const s_tech_hull_data* hulls = nullptr;
        const s_tech_shield_data* shields = nullptr;
        const s_tech_specials_data* specials = nullptr;
        const s_tech_weapon_data* weapons = nullptr;
        const s_tech_weapon_mod_data* weapon_modifications = nullptr;
        const s_tech_data_rifle* rifles = nullptr;
        const s_tech_data_personal_shield* personal_shields = nullptr;
    };

    struct s_colony_rules_view {
        const raceopt::s_pick_rules* race_picks = nullptr;
        const COLCALC::s_population_bonus* population_bonus = nullptr;
        const COLCALC::s_food_farmer_bonus* food_farmer_bonus = nullptr;
        const uint8_t* minerals_per_mine = nullptr;
        const uint8_t* climate_maintenance_modifiers = nullptr;
    };

    struct s_combat_rules_view {
        const COMBINIT::s_satellite_space_allocation* satellite_space_allocation = nullptr;
        const COMBINIT::s_ground_defense* ground_defense = nullptr;
        const uint8_t* satellite_ammo_basis = nullptr;
        const uint8_t* missile_base_ammo_basis = nullptr;
    };

    struct s_engine_design_rules_view {
        const uint8_t* missile_x3_shot = nullptr;
    };

    struct s_refit_rule_inputs {
        const s_tech_hull_data* hulls = nullptr;
        const COLCALC::s_government_bonus* governments = nullptr;
        const uint8_t* alternative_refit = nullptr;
    };

    // Read-only references to canonical active rules, never turn-owned copies.
    struct s_rules_view {
        const AIBUILD::s_rules* build = nullptr;
        const AIMOVE::s_rules* movement = nullptr;
        const aidesign::s_rules* design = nullptr;
        const aitech::s_rules* research = nullptr;
        const s_planet_valuation_rules* valuation = nullptr;
        s_technology_view technology;
        s_colony_rules_view colony;
        s_combat_rules_view combat;
        s_engine_design_rules_view engine_design;
        s_refit_rule_inputs refit;
    };
} // namespace AIDATA
