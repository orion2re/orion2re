#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "consts.h"
#include "object_ids.h"
#include "player_bitmap.h"
#include "orion2_consts.h"
#include "settler_data.h"
#include "ship_data.h"
#include "types.h"

struct s_player {
    int8_t picture;
    char name[20];
    char race_name[15];
    uint8_t eliminated;
    uint8_t race;
    uint8_t color;
    uint8_t personality;
    uint8_t objectives;
    game::id_planet home_planet_id = game::NO_PLANET;
    int16_t network_player_id;
    int8_t player_done_flags;
    int16_t dead_field;
    int8_t research_breakthrough;
    uint8_t tax_rate;
    int32_t bc;
    int16_t n_freighters;
    int16_t surplus_freighters;
    int16_t command_points;
    int16_t command_points_used;
    int16_t food_freighted;
    int16_t settlers_freighted;
    int16_t total_pop;
    int16_t food_produced;
    int16_t industry_produced;
    int16_t research_produced;
    int16_t bc_produced;
    int16_t surplus_food;
    int16_t surplus_bc;
    int32_t total_maintenance;
    int16_t maintenance[6];
    uint8_t tech_fields[TECH_FIELD_COUNT];
    uint8_t tech_applications[TECH_APP_COUNT];
    int32_t research_accumulated;
    int8_t captured_orion;
    int8_t captured_antares;
    int8_t won_council_vote;
    uint16_t player_defeated[MAX_PLAYERS];
    int16_t captured_population;
    game::owner_slot last_attacker;
    uint8_t ship_design_theme;
    int8_t ship_special_theme;
    int8_t tech_apps_learned[4];
    int8_t tech_modifiers[17];
    uint8_t hyper_advanced_tech[MAX_PLAYERS];
    int32_t total_known_tech_cost;
    int8_t knows_racial_stats;
    int32_t last_tech_report_date[MAX_PLAYERS];
    int8_t player_tech_applications[MAX_PLAYERS][27];
    int8_t current_research_field;
    int8_t current_research_application;
    int8_t got_new_app;
    int16_t ship_range;
    struct s_ship_design ship_designs[6];
    int16_t ships_built[6];
    sbyte contact[MAX_PLAYERS];
    game::player_bitmap just_established_contact;
    int8_t n_times_established_contact[MAX_PLAYERS];
    sbyte within_range[MAX_PLAYERS];
    int8_t do_not_research;
    int16_t offensive_stagepoint;
    int8_t best_ftl_drive;
    uint8_t ship_speed;
    int8_t officer_for_hire;
    int16_t n_trade_pops;
    int16_t current_trade_agreement_level[MAX_PLAYERS];
    int16_t trade_agreement_goal[MAX_PLAYERS];
    int16_t n_research_pops;
    int16_t current_research_agreement_level[MAX_PLAYERS];
    int16_t research_agreement_goal[MAX_PLAYERS];
    int8_t fighter_beam_type;
    int8_t fighter_bomb_type;
    int16_t total_research;
    int16_t total_ships;
    int32_t total_ship_strength[MAX_PLAYERS];
    int16_t total_colonies;
    sbyte failed_to_expand;
    int8_t first_contact[MAX_PLAYERS];
    sbyte relations[MAX_PLAYERS];
    int8_t base_relations[MAX_PLAYERS];
    sbyte treaty[MAX_PLAYERS];
    int8_t trade_treaty[MAX_PLAYERS];
    int8_t research_treaty[MAX_PLAYERS];
    int16_t tribute_treaty[MAX_PLAYERS];
    uint8_t diplomacy_incident[MAX_PLAYERS];
    int8_t diplomacy_message[MAX_PLAYERS];
    int16_t diplomacy_severity[MAX_PLAYERS];
    int16_t diplomacy_value[MAX_PLAYERS];
    int16_t diplomacy_system[MAX_PLAYERS];
    int16_t treaty_modifier[MAX_PLAYERS];
    int16_t trade_modifier[MAX_PLAYERS];
    int16_t tech_exchange_modifier[MAX_PLAYERS];
    int16_t peace_modifier[MAX_PLAYERS];
    sbyte last_bad_diplomatic_incident[MAX_PLAYERS];
    int8_t broken_treaties_modifier[MAX_PLAYERS];
    int8_t last_broken_treaty[MAX_PLAYERS];
    int8_t last_gift[MAX_PLAYERS];
    int16_t ship_war_losses[MAX_PLAYERS];
    int16_t colony_war_losses[MAX_PLAYERS];
    int8_t biological_weapons_flag[MAX_PLAYERS];
    int8_t time_since_last_attack[MAX_PLAYERS];
    int8_t threats[MAX_PLAYERS];
    int8_t dishonored_flag[MAX_PLAYERS];
    int8_t peace_duration[MAX_PLAYERS];
    int16_t last_turn_diplomacy_severity[MAX_PLAYERS];
    int8_t last_broken_treaty_type[MAX_PLAYERS];
    sbyte last_turn_treaty[MAX_PLAYERS];
    int8_t diplomacy_proposal_request[MAX_PLAYERS];
    int8_t diplomacy_proposal_tribute_bribe[MAX_PLAYERS];
    uint8_t diplomacy_proposal_tech_bribe1[MAX_PLAYERS];
    uint8_t diplomacy_proposal_tech_bribe2[MAX_PLAYERS];
    int16_t diplomacy_proposal_gold_bribe[MAX_PLAYERS];
    game::id_player diplomacy_proposal_war_player[MAX_PLAYERS];
    int32_t diplomacy_proposal_exchange_max_value[MAX_PLAYERS];
    uint8_t diplomacy_proposal_exchange_tech[MAX_PLAYERS];
    int16_t diplomacy_proposal_system_bribe[MAX_PLAYERS];
    game::id_planet sneak_attack_planet = game::NO_PLANET;
    int8_t sneak_attack_message;
    game::id_player sneak_attack_player;
    game::id_player council_threat;
    int16_t reward_amount[MAX_PLAYERS];
    uint8_t reward_tech[MAX_PLAYERS];
    game::id_player reward_attack_player[MAX_PLAYERS];
    int16_t food_shortage_duration;
    int8_t trust_worthiness[MAX_PLAYERS];
    game::id_player trust_breaker_player[MAX_PLAYERS];
    int8_t trust_breaker_treaty[MAX_PLAYERS];
    int8_t stop_spying_duration[MAX_PLAYERS];
    int8_t stop_blockading_duration[MAX_PLAYERS];
    int8_t war_buildup_duration;
    int16_t last_diplomacy_proposal_turn[MAX_PLAYERS];
    int16_t diplomacy_proposal_rejection[MAX_PLAYERS];
    game::id_planet opportunity_planet_id[MAX_PLAYERS];
    int16_t opportunity_planet_value[MAX_PLAYERS];
    int32_t opportunity_actual_attacker_ratio[MAX_PLAYERS];
    int16_t temp_diplomacy_dummy1[MAX_PLAYERS];
    int8_t diplomacy_demand_rejection_message[MAX_PLAYERS];
    uint8_t last_diplomacy_threat_turn[MAX_PLAYERS];
    int8_t delayed_diplomacy_orders[MAX_PLAYERS];
    int8_t traits[TRAIT_COUNT];
    int8_t evolutionary_upgrade_traits[TRAIT_COUNT];
    int8_t picked_evolutionary_upgrade;
    int8_t applied_evolutionary_upgrade;
    int8_t fleet_history[350];
    int8_t tech_history[350];
    int8_t population_history[350];
    int8_t production_history[350];
    uint8_t spies[MAX_PLAYERS];
    int8_t history_btns;
    game::player_bitmap ignoring;
    int8_t peace_flags[MAX_PLAYERS];
    int8_t cheated;
    uint16_t stardate_last_offered_hero;
    int8_t current_zoom_level;
    int16_t cur_map_x;
    int16_t cur_map_y;
    game::id_player council_voted_for;
    game::id_player surrender_to;
    int16_t time_since_last_lucky_event;
    int8_t has_galactic_lore;
    game::player_bitmap warned_about_cheating_player;
    int8_t cheated_this_turn;
    int32_t tribute_income;
    int8_t filler[45];

    s_settler_info settlers[MAX_FREIGHTED_SETTLERS];
    game::id_player id = game::NO_PLAYER;

    bool operator==(const s_player&) const = default;
};
static_assert(alignof(s_player) >= alignof(s_settler_info));

static_assert(std::is_trivially_copyable_v<s_player>);
static_assert(alignof(s_player) >= alignof(game::id_player));

namespace player_roster {
    inline auto Make_Roster() {
        std::array<s_player, MAX_PLAYERS> roster{};
        for (uint32_t slot = 0; slot < roster.size(); ++slot) {
            auto& player = roster[slot];
            player.id = game::id_player{slot};
            // Preserve the old zero-initialized setup slots and unused save tail.
            player.home_planet_id = game::id_planet{0};
            player.sneak_attack_planet = game::id_planet{0};
        }
        return roster;
    }
} // namespace player_roster
