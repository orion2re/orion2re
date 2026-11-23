#pragma once

#include <type_traits>

#include "settler_serialization.h"
#include "ship_serialization.h"

namespace NETPROTO {
    inline constexpr uint32_t PLAYER_TAG = 0x3550324f; // O2P5
    inline constexpr size_t PLAYER_BYTES = sizeof(s_net_player_wire);

    // The order here is the O2P5 order, independent of s_player's layout.
    template<class Player, class Field>
    constexpr void Player_Wire_Fields(Player& player, Field field) {
        field(player.picture);
        field(player.name);
        field(player.race_name);
        field(player.eliminated);
        field(player.race);
        field(player.color);
        field(player.personality);
        field(player.objectives);
        field(player.home_planet_id);
        field(player.network_player_id);
        field(player.player_done_flags);
        field(player.dead_field);
        field(player.research_breakthrough);
        field(player.tax_rate);
        field(player.bc);
        field(player.n_freighters);
        field(player.surplus_freighters);
        field(player.command_points);
        field(player.command_points_used);
        field(player.food_freighted);
        field(player.settlers_freighted);
        field(player.settlers);
        field(player.total_pop);
        field(player.food_produced);
        field(player.industry_produced);
        field(player.research_produced);
        field(player.bc_produced);
        field(player.surplus_food);
        field(player.surplus_bc);
        field(player.total_maintenance);
        field(player.maintenance);
        field(player.tech_fields);
        field(player.tech_applications);
        field(player.research_accumulated);
        field(player.captured_orion);
        field(player.captured_antares);
        field(player.won_council_vote);
        field(player.player_defeated);
        field(player.captured_population);
        field(player.last_attacker);
        field(player.ship_design_theme);
        field(player.ship_special_theme);
        field(player.tech_apps_learned);
        field(player.tech_modifiers);
        field(player.hyper_advanced_tech);
        field(player.total_known_tech_cost);
        field(player.knows_racial_stats);
        field(player.last_tech_report_date);
        field(player.player_tech_applications);
        field(player.current_research_field);
        field(player.current_research_application);
        field(player.got_new_app);
        field(player.ship_range);
        field(player.ship_designs);
        field(player.ships_built);
        field(player.contact);
        field(player.just_established_contact);
        field(player.n_times_established_contact);
        field(player.within_range);
        field(player.do_not_research);
        field(player.offensive_stagepoint);
        field(player.best_ftl_drive);
        field(player.ship_speed);
        field(player.officer_for_hire);
        field(player.n_trade_pops);
        field(player.current_trade_agreement_level);
        field(player.trade_agreement_goal);
        field(player.n_research_pops);
        field(player.current_research_agreement_level);
        field(player.research_agreement_goal);
        field(player.fighter_beam_type);
        field(player.fighter_bomb_type);
        field(player.total_research);
        field(player.total_ships);
        field(player.total_ship_strength);
        field(player.total_colonies);
        field(player.failed_to_expand);
        field(player.first_contact);
        field(player.relations);
        field(player.base_relations);
        field(player.treaty);
        field(player.trade_treaty);
        field(player.research_treaty);
        field(player.tribute_treaty);
        field(player.diplomacy_incident);
        field(player.diplomacy_message);
        field(player.diplomacy_severity);
        field(player.diplomacy_value);
        field(player.diplomacy_system);
        field(player.treaty_modifier);
        field(player.trade_modifier);
        field(player.tech_exchange_modifier);
        field(player.peace_modifier);
        field(player.last_bad_diplomatic_incident);
        field(player.broken_treaties_modifier);
        field(player.last_broken_treaty);
        field(player.last_gift);
        field(player.ship_war_losses);
        field(player.colony_war_losses);
        field(player.biological_weapons_flag);
        field(player.time_since_last_attack);
        field(player.threats);
        field(player.dishonored_flag);
        field(player.peace_duration);
        field(player.last_turn_diplomacy_severity);
        field(player.last_broken_treaty_type);
        field(player.last_turn_treaty);
        field(player.diplomacy_proposal_request);
        field(player.diplomacy_proposal_tribute_bribe);
        field(player.diplomacy_proposal_tech_bribe1);
        field(player.diplomacy_proposal_tech_bribe2);
        field(player.diplomacy_proposal_gold_bribe);
        field(player.diplomacy_proposal_war_player);
        field(player.diplomacy_proposal_exchange_max_value);
        field(player.diplomacy_proposal_exchange_tech);
        field(player.diplomacy_proposal_system_bribe);
        field(player.sneak_attack_planet);
        field(player.sneak_attack_message);
        field(player.sneak_attack_player);
        field(player.council_threat);
        field(player.reward_amount);
        field(player.reward_tech);
        field(player.reward_attack_player);
        field(player.food_shortage_duration);
        field(player.trust_worthiness);
        field(player.trust_breaker_player);
        field(player.trust_breaker_treaty);
        field(player.stop_spying_duration);
        field(player.stop_blockading_duration);
        field(player.war_buildup_duration);
        field(player.last_diplomacy_proposal_turn);
        field(player.diplomacy_proposal_rejection);
        field(player.opportunity_planet_id);
        field(player.opportunity_planet_value);
        field(player.opportunity_actual_attacker_ratio);
        field(player.temp_diplomacy_dummy1);
        field(player.diplomacy_demand_rejection_message);
        field(player.last_diplomacy_threat_turn);
        field(player.delayed_diplomacy_orders);
        field(player.traits);
        field(player.evolutionary_upgrade_traits);
        field(player.picked_evolutionary_upgrade);
        field(player.applied_evolutionary_upgrade);
        field(player.fleet_history);
        field(player.tech_history);
        field(player.population_history);
        field(player.production_history);
        field(player.spies);
        field(player.history_btns);
        field(player.ignoring);
        field(player.peace_flags);
        field(player.cheated);
        field(player.stardate_last_offered_hero);
        field(player.current_zoom_level);
        field(player.cur_map_x);
        field(player.cur_map_y);
        field(player.council_voted_for);
        field(player.surrender_to);
        field(player.time_since_last_lucky_event);
        field(player.has_galactic_lore);
        field(player.warned_about_cheating_player);
        field(player.cheated_this_turn);
        field(player.tribute_income);
        field(player.filler);
    }

    template<class T>
    constexpr size_t Player_Field_Bytes() {
        using Value = std::remove_cv_t<T>;
        if constexpr (std::is_array_v<Value>) {
            return std::extent_v<Value> * Player_Field_Bytes<std::remove_extent_t<Value>>();
        } else if constexpr (std::is_same_v<Value, s_settler_info>) {
            return SERIAL::SETTLER_BYTES;
        } else if constexpr (std::is_same_v<Value, s_ship_design>) {
            return SERIAL::LEGACY_SHIP_DESIGN_BYTES;
        } else if constexpr (std::is_same_v<Value, game::id_player>) {
            return 1;
        } else if constexpr (std::is_same_v<Value, game::id_planet>) {
            return sizeof(uint32_t);
        } else {
            static_assert(std::is_integral_v<Value> && sizeof(Value) <= 4);
            return sizeof(Value);
        }
    }

    static_assert([] {
        size_t bytes = sizeof(PLAYER_TAG);
        const s_player player{};
        Player_Wire_Fields(player, [&]<class T>(const T&) { bytes += Player_Field_Bytes<T>(); });
        return bytes == PLAYER_BYTES;
    }(), "O2P5 player fields must retain their fixed wire size");

    template<class T>
    void Write_Player_Field(SERIAL::Writer_& writer, const T& field) {
        if constexpr (std::is_array_v<T>) {
            if constexpr (std::is_integral_v<std::remove_all_extents_t<T>> && sizeof(std::remove_all_extents_t<T>) == 1) {
                writer.Write_Bytes(field, sizeof(field));
            } else {
                for (const auto& element : field) Write_Player_Field(writer, element);
            }
        } else if constexpr (std::is_same_v<T, s_settler_info>) {
            SERIAL::Write_Settler(writer, field);
        } else if constexpr (std::is_same_v<T, s_ship_design>) {
            SERIAL::Write_Ship_Design(&writer, &field);
        } else if constexpr (std::is_same_v<T, game::id_player>) {
            writer.Write_Uint8(static_cast<uint8_t>(field.id));
        } else if constexpr (std::is_same_v<T, game::id_planet>) {
            writer.Write_Uint32(field.id);
        } else if constexpr (sizeof(T) == 1) {
            writer.Write_Uint8(static_cast<uint8_t>(field));
        } else if constexpr (sizeof(T) == 2) {
            writer.Write_Uint16(static_cast<uint16_t>(field));
        } else {
            writer.Write_Uint32(static_cast<uint32_t>(field));
        }
    }

    template<class T>
    void Read_Player_Field(SERIAL::Reader_& reader, T& field) {
        if constexpr (std::is_array_v<T>) {
            if constexpr (std::is_integral_v<std::remove_all_extents_t<T>> && sizeof(std::remove_all_extents_t<T>) == 1) {
                reader.Read_Bytes(field, sizeof(field));
            } else {
                for (auto& element : field) Read_Player_Field(reader, element);
            }
        } else if constexpr (std::is_same_v<T, s_settler_info>) {
            field = SERIAL::Read_Settler(reader);
        } else if constexpr (std::is_same_v<T, s_ship_design>) {
            SERIAL::Read_Ship_Design(&reader, &field);
        } else if constexpr (std::is_same_v<T, game::id_player>) {
            field = game::id_player{static_cast<uint32_t>(reader.Read_Int8())};
        } else if constexpr (std::is_same_v<T, game::id_planet>) {
            field = game::id_planet{reader.Read_Uint32()};
        } else if constexpr (sizeof(T) == 1) {
            field = static_cast<T>(reader.Read_Uint8());
        } else if constexpr (sizeof(T) == 2) {
            field = static_cast<T>(reader.Read_Uint16());
        } else {
            field = static_cast<T>(reader.Read_Uint32());
        }
    }

    inline void Write_Player(SERIAL::Writer_& writer, const s_player& player) {
        writer.Write_Uint32(PLAYER_TAG);
        Player_Wire_Fields(player, [&](const auto& field) { Write_Player_Field(writer, field); });
    }

    inline bool Read_Player(SERIAL::Reader_& reader, s_player& player) {
        if (!reader.Require(PLAYER_BYTES) || reader.Read_Uint32() != PLAYER_TAG) return false;
        Player_Wire_Fields(player, [&](auto& field) { Read_Player_Field(reader, field); });
        return !reader.Failed();
    }

    inline s_net_player_wire Encode_Player(const s_player& player) {
        s_net_player_wire wire;
        SERIAL::Writer_ writer(wire.bytes, PLAYER_BYTES, "player packet overflow");
        Write_Player(writer, player);
        return wire;
    }

    inline bool Decode_Player(const void* bytes, uint32_t length, s_player& player) {
        if (bytes == nullptr || length != PLAYER_BYTES) return false;
        SERIAL::Reader_ reader(bytes, length, "invalid player packet", true);
        return Read_Player(reader, player);
    }
} // namespace NETPROTO
