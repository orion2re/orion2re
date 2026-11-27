#include "pch.h"
#include "string_overrides.h"

namespace EVENTS {
    game::s_planning_context Planning_Context_For(const game::s_game_state& world) {
        game::s_planning_context result{GEO::Session_Nebulas_For(world)};
        if (&world != &game::current_state) return result;
        const auto active = [](const auto& event) { return event.status == 2 || event.status == 4 || event.status == 6; };
        result.hyperspace_flux = active(event_data[EVENT_HYPERSPACE_FLUX]);
        if (active(event_data[EVENT_TIME_SPACE_ANOMALY])) result.anomaly_star = event_data[EVENT_TIME_SPACE_ANOMALY].star_target.get();
        if (active(event_data[EVENT_PLAGUE])) result.plague_colony = event_data[EVENT_PLAGUE].colony_target.get();
        return result;
    }
    namespace {
        int16_t deferred_monster_event_target = -1;

        s_ship_data* Find_Event_Ship(const s_event_data& event) {
            return [&] { const auto entry = game::current_state.ships.find(event.ship_target.get()); return entry == game::current_state.ships.end() ? nullptr : &entry->second; }();
        }

        void Set_Event_Ship(s_event_data& event, game::id_ship ship) {
            event.target_id = game::Ship_Slot(game::current_state, ship); // Historical report projection.
            event.ship_target = ship;
        }

        uint16_t System_Colony_Research(int16_t star_idx) {
            uint16_t research = 0;
            for (int16_t i = 0; i < 5; ++i) {
                const auto planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[i];
                if (planet_idx != game::NO_PLANET) {
                    const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                    if (colony_id != game::NO_COLONY) {
                        research = (uint16_t)(research + Colony_Research_Points(colony_id));
                    }
                }
            }
            return research;
        }
    } // namespace

    uint8_t current_event;
    s_animation_header* event_display_seg;
    s_event_data event_data[EVENT_COUNT];
    MOX2::action_func_t jim_saved_autofunction;
    uint32_t last_event_year;
    int16_t event_delay_count;
    int8_t event_good_array[EVENT_COUNT] = {
        1,
        1,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
    };
    uint8_t event_status_flag;
    uint8_t* event_mouse_bitmap;
    char event_message[300];
    s_colors event_normal_colors = {{248, 254, 253, 251, 252, 253, 254, 255}};
    s_mouse_list event_blank_mouse = {0, 0, 0, 0, 639, 479};

    game::id_colony __cdecl Get_Event_Mineral_Depletion_Colony(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto found_colony_idx = game::NO_COLONY;
        int16_t attempts = 0;

        while (attempts < 200) {
            const auto pick = Pick_Random_Colony_No_Capitol(player_key);

            if (pick != game::NO_COLONY) {
                const auto planet_idx = game::current_state.colonies.at(pick).planet_id;
                // Mineral class richer than MINERAL_RESOURCE_RICH required for depletion event
                if (game::current_state.planets.at(planet_idx).mineral_class > MINERAL_RESOURCE_RICH) {
                    found_colony_idx = pick;
                }
            }

            attempts++;

            if (found_colony_idx != game::NO_COLONY) {
                break;
            }
        }

        return found_colony_idx;
    }

    /**
     * Checks if a comet event is currently active at the specified colony.
     */
    int16_t __cdecl Event_Check_Comet(game::id_colony colony_id) {
        // Event statuses: 2: Active, 4: (Unknown Active State), 6: Just Started
        const int8_t status = event_data[EVENT_COMET].status;

        if ((status == 2 || status == 4 || status == 6) &&
            colony_id != game::NO_COLONY && colony_id == event_data[EVENT_COMET].colony_target.get()) {
            return 1;
        }

        return 0;
    }

    void __cdecl Init_Events() {
        event_delay_count = 0;
        last_event_year = MOX::stardate;
        deferred_monster_event_target = -1;

        for (int i = 0; i < EVENT_COUNT; ++i) {
            event_data[i].player = legacy_world::Decode_Player_Slot(-1);
            event_data[i].status = 0;
            event_data[i].turn_count = 0;
            event_data[i].target_id = -1;
            event_data[i].planet_target = game::NO_PLANET;
            event_data[i].colony_target = game::NO_COLONY;
            event_data[i].star_target = game::NO_STAR;
            event_data[i].ship_target = game::NO_SHIP;
            event_data[i].amount = -1;
            event_data[i].max_amount = -1;
        }
    }

    int16_t __cdecl Event_Check_Space_Anomaly(game::id_colony colony_id) {
        auto& event = event_data[EVENT_TIME_SPACE_ANOMALY];

        if (event.status == 2 || event.status == 4 || event.status == 6) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::current_state.planets.at(planet_idx).star_id == event.star_target.get()) {
                return 1;
            }
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Space_Anomaly_At_Star(int16_t star_idx) {
        auto& event = event_data[EVENT_TIME_SPACE_ANOMALY];

        if ((event.status == 2 || event.status == 4 || event.status == 6) &&
            (star_idx >= 0 && game::Star_Key_At_Row(game::current_state, star_idx) == event.star_target.get())) {
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Pirate_Activity(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        uint8_t event_status = event_data[EVENT_PIRATE_ACTIVITY].status;

        if (event_status == 2 || event_status == 4 || event_status == 6) {
            const auto* star = &game::current_state.stars.at(event_data[EVENT_PIRATE_ACTIVITY].star_target.get());

            if (star->has_colony.test_slot(player_idx & 0x1F)) {
                return 1;
            }
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Colony_Researching(game::id_colony colony_id) {
        if ((event_data[EVENT_PLAGUE].status == 2 || event_data[EVENT_PLAGUE].status == 6) &&
            (colony_id != game::NO_COLONY && event_data[EVENT_PLAGUE].colony_target.get() == colony_id)) {
            return 1;
        }

        if ((event_data[EVENT_SUPERNOVA].status == 2 || event_data[EVENT_SUPERNOVA].status == 6)) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::current_state.planets.at(planet_idx).star_id == event_data[EVENT_SUPERNOVA].star_target.get()) {
                return 1;
            }
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Population_Boom(game::id_colony colony_id) {
        int8_t status = event_data[EVENT_POPULATION_BOOM].status;

        if ((status == 2 || status == 4 || status == 6) &&
            (colony_id != game::NO_COLONY && event_data[EVENT_POPULATION_BOOM].colony_target.get() == colony_id)) {
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Plague(game::id_colony colony_id) {
        const auto& event = event_data[EVENT_PLAGUE];

        if ((event.status == 2 || event.status == 4 || event.status == 6) &&
            colony_id != game::NO_COLONY && event.colony_target.get() == colony_id) {
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Hyperspace_Flux() {
        int8_t state = event_data[EVENT_HYPERSPACE_FLUX].status;

        if (state == 2 || state == 4 || state == 6) {
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Event_Check_Warp_Beast() {
        int8_t status = event_data[EVENT_WARP_BEAST].status;

        if (status == 2 /* Active */ || status == 4 || status == 6 /* Just Started */) {
            return 1;
        }

        return 0;
    }

    void __cdecl Event_Surrender(game::id_player player_key, game::id_player target_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int target_id = legacy_world::Encode_Player_Slot(target_key);
        event_data[EVENT_EMPIRE_SURRENDERS].target_id = target_id;
        event_data[EVENT_EMPIRE_SURRENDERS].player = player_key;
        event_data[EVENT_EMPIRE_SURRENDERS].status = 1 /* Triggered */;
    }

    /**
     * @brief Marks the "Orion Discovered" event as triggered for a specific player.
     * @param player_idx The index of the player who discovered Orion.
     */
    void __cdecl Event_Orion_Discovered(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        event_data[EVENT_ORION_VISITED].status = 1 /* Triggered */;
        event_data[EVENT_ORION_VISITED].player = player_key;
    }

    void __cdecl Allocate_Comet_Hit(game::id_colony colony_id) {
        s_combat_bomb_result result;
        // Zero out the full result structure.
        memset(&result, 0, sizeof(s_combat_bomb_result));

        result.attacker = factions::From_Legacy(-1);
        result.colony_id = colony_id;
        result.is_planetary_hit = 1;

        int16_t building_count = 0;
        s_colony* colony = &game::current_state.colonies.at(colony_id);

        const int16_t building_slot_count = (int16_t)(sizeof(colony->buildings) / sizeof(colony->buildings[0]));
        for (int16_t i = 0; i < building_slot_count; ++i) {
            if (colony->buildings[i] != 0) {
                building_count++;
            }
        }

        // Potential hits based on sum of buildings and population
        uint8_t population = (uint8_t)colony->n_pops;
        int32_t random_val1 = game_random::Random(3);
        int32_t random_val2 = game_random::Random(3);

        int16_t hits = (int16_t)((random_val1 + random_val2) * (building_count + population) / 10);
        if (hits == 0) {
            hits = 1;
        }

        result.bomb_hits_remaining = hits;
        result.is_bio_weapon = 0;
        result.bio_hits_remaining = 0;

        BOMB::Apply_Bomb_Hits(&result);
    }

    void __cdecl Event_Race_Eliminated(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        event_data[EVENT_EMPIRE_DESTROYED].status = 1 /* Triggered */;
        event_data[EVENT_EMPIRE_DESTROYED].player = player_key;
    }

    game::id_player __cdecl Determine_Lucky_Players_Events() {
        int16_t lucky_divisor;
        auto lucky_player = game::NO_PLAYER;

        // Determine the divisor based on game settings
        if (MOX::settings.random_events == 0) {
            lucky_divisor = (MOX::settings.xenons_exist != 0) ? 12 : 10;
        } else {
            lucky_divisor = (MOX::settings.xenons_exist != 0) ? 8 : 6;
        }

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (lucky_player != game::NO_PLAYER) {
                break;
            }

            s_player& player = game::current_state.players[i];

            // Check if player is active and has the 'Lucky' trait (index 26)
            if (player.eliminated == 0 && player.traits[TRAIT_LUCKY] == 1) {
                int16_t chance = player.time_since_last_lucky_event / lucky_divisor;

                if ((int32_t)game_random::Random(event_roll.lucky_player) <= chance) {
                    lucky_player = player.id;
                    player.time_since_last_lucky_event = 0;
                }
            }
        }

        return lucky_player;
    }

    int16_t __cdecl Donation_Amount(int32_t game_turn) {
        return (int16_t)((int64_t)(game_turn / donation_bc.turn_divisor) *
                donation_bc.multiplier +
            donation_bc.base);
    }

    int16_t __cdecl Pirate_Raid_Amount(int32_t bc) {
        const uint32_t percent = game_random::Random(pirate_raid.random_percent) +
            pirate_raid.fixed_percent;
        int64_t amount = (int64_t)bc * percent / 100;
        if (amount > INT16_MAX) {
            amount = INT16_MAX;
        }
        return (int16_t)amount;
    }

    int16_t __cdecl Supernova_Research_Requirement(int16_t star_idx,
        int16_t turn_count) {
        const uint16_t research_per_turn = (uint16_t)(supernova.minimum_research_per_turn +
            System_Colony_Research(star_idx));
        return (int16_t)((int32_t)turn_count * (int16_t)research_per_turn);
    }

    void __cdecl Event_Twiddle() {
        for (int16_t i = 0; i < EVENT_COUNT; ++i) {
            if (event_data[i].status == 5 /* Finishing */) {
                event_data[i].status = 3 /* Expired */;
            }

            if (event_data[i].status == 4 /* Triggered */ || event_data[i].status == 6 /* Just Started */) {
                event_data[i].status = 2 /* Active */;
            }

            if (event_data[i].status == 1 /* Triggered New */) {
                event_data[i].status = 6 /* Just Started */;
            } else if (event_data[i].status != 2 /* Active */) {
                continue;
            }

            switch (i) {
                case EVENT_DERELICT:
                    COLCALC::Player_Gets_Tech_App(&game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())], event_data[i].target_id);
                    if (event_data[i].amount != 0) {
                        COLCALC::Player_Gets_Tech_App(&game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())], event_data[i].amount);
                    }
                    event_data[i].status = 5;
                    break;

                case EVENT_CLIMATIC_CHANGE: {
                    const auto planet_idx = event_data[i].planet_target.get();
                    int8_t old_climate = game::current_state.planets.at(planet_idx).climate;
                    game::current_state.planets.at(planet_idx).climate = PLANET_CLIMATE_TERRAN;
                    game::current_state.planets.at(planet_idx).food_per_farmer += (MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] - MOX::food_per_farmer_table[old_climate]);
                    const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                    if (colony_id != game::NO_COLONY) {
                        COLCALC::Colony_Calculation(colony_id);
                        COLBLDG::Delete_Prod_From_Colony_Queue(game::current_state, colony_id, BUILDING_TERRAFORMING);
                    }
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_COMET:
                    if (event_data[i].status != 6) {
                        int16_t star_idx = game::Star_Row(game::current_state, game::current_state.planets.at(game::current_state.colonies.at(event_data[i].colony_target.get()).planet_id).star_id);
                        event_data[i].amount -= Ship_Size_Points_At_Star(star_idx);
                        if (event_data[i].amount <= 0) {
                            event_data[i].amount = 0;
                            event_data[i].status = 5;
                        } else {
                            event_data[i].turn_count--;
                            if (event_data[i].turn_count < 1) {
                                Allocate_Comet_Hit(event_data[i].colony_target.get());
                                event_data[i].status = 5;
                            }
                        }
                        if (game_random::Random(20) == 1 && event_data[i].status != 5 &&
                            event_data[i].amount != event_data[i].max_amount) {
                            event_data[i].status = 4;
                        }
                    }
                    break;

                case EVENT_COMPUTER_VIRUS: {
                    s_player& player = game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())];
                    int16_t loss = (int16_t)(game_random::Random(computer_virus.random_research) +
                        computer_virus.fixed_research);
                    if (player.research_accumulated < loss) {
                        loss = (int16_t)player.research_accumulated;
                    }
                    event_data[i].target_id = loss;
                    event_data[i].amount = (int16_t)player.current_research_field;
                    player.research_accumulated -= loss;
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_DIPLOMATIC_BLUNDER:
                    diplomac::Change_Relations(-100, legacy_world::Decode_Player_Slot(event_data[i].target_id), event_data[i].player.get(), 0, 0, 0);
                    event_data[i].status = 5;
                    break;

                case EVENT_DIPLOMATIC_MARRIAGE:
                    diplomac::Change_Relations(100, legacy_world::Decode_Player_Slot(event_data[i].target_id), event_data[i].player.get(), 0, 0, 0);
                    event_data[i].status = 5;
                    break;

                case EVENT_DONATION:
                    game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())].bc +=
                        event_data[i].target_id;
                    event_data[i].status = 5;
                    break;

                case EVENT_EARTHQUAKE:
                    event_data[i].status = 5;
                    Allocate_Earthquake_Hit(event_data[i].colony_target.get(), &event_data[i].amount, &event_data[i].max_amount);
                    if (event_data[i].amount < 1 && event_data[i].max_amount < 1) {
                        event_data[i].status = 0;
                        event_data[i].player = legacy_world::Decode_Player_Slot(-1);
                    }
                    COLCALC::Colony_Calculation(event_data[i].colony_target.get());
                    break;

                case EVENT_FREAK_ACCIDENT: {
                    auto* ship = Find_Event_Ship(event_data[i]);
                    if (!ship || ship->status == 5) {
                        event_data[i].status = 3;
                        break;
                    }
                    const auto officer = ship->officer_id;
                    if (officer != game::NO_LEADER) {
                        event_data[i].amount = leader_slots::Encode(officer);
                        OFFICER::Kill_Officer(officer);
                    }
                    SHIPS::Kill_Ship(*ship);
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_HYPERSPACE_FLUX:
                    if (event_data[i].status == 2) {
                        if (event_data[i].turn_count > duration.flux_min &&
                            game_random::Random(duration.flux_chance) == 1) {
                            event_data[i].status = 5;
                        }
                        if (event_data[i].turn_count > 20) {
                            event_data[i].status = 5;
                        }
                        event_data[i].turn_count++;
                    }
                    break;

                case EVENT_INDUSTRIAL_ACCIDENT: {
                    const auto colony_id = game::current_state.planets.at(event_data[i].planet_target.get()).colony_id;
                    if (colony_id != game::NO_COLONY) {
                        Allocate_Industrial_Accident_Hit(colony_id);
                        COLCALC::Colony_Calculation(colony_id);
                    }
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_MINERAL_DEPLETION: {
                    int8_t mineral = game::current_state.planets.at(event_data[i].planet_target.get()).mineral_class - 1;
                    if (mineral <= MINERAL_RESOURCE_ULTRA_POOR) mineral = MINERAL_RESOURCE_ULTRA_POOR;
                    game::current_state.planets.at(event_data[i].planet_target.get()).mineral_class = mineral;
                    const auto colony_id = game::current_state.planets.at(event_data[i].planet_target.get()).colony_id;
                    if (colony_id != game::NO_COLONY) {
                        COLCALC::Colony_Calculation(colony_id);
                    }
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_MINERAL_DEPOSIT: {
                    int8_t mineral = game::current_state.planets.at(event_data[i].planet_target.get()).mineral_class + 2;
                    if (mineral >= MINERAL_RESOURCE_ULTRA_RICH) mineral = MINERAL_RESOURCE_ULTRA_RICH;
                    game::current_state.planets.at(event_data[i].planet_target.get()).mineral_class = mineral;
                    const auto colony_id = game::current_state.planets.at(event_data[i].planet_target.get()).colony_id;
                    if (colony_id != game::NO_COLONY) {
                        COLCALC::Colony_Calculation(colony_id);
                    }
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_PIRATE_ACTIVITY:
                    if (event_data[i].status != 6) {
                        if ((int32_t)game_random::Random(100) <= (int)event_data[i].amount * 100 / (int)event_data[i].max_amount) {
                            for (int16_t j = 0; j < game::current_state.player_count; ++j) {
                                if (game::current_state.stars.at(event_data[i].star_target.get()).has_colony.test_slot(j)) {
                                    if (game::current_state.players[j].n_freighters > 0) {
                                        game::current_state.players[j].n_freighters--;
                                    }
                                }
                            }
                        }
                        event_data[i].amount -= Ship_Size_Points_At_Star(game::Star_Row(game::current_state, event_data[i].star_target.get()));
                        if (event_data[i].amount <= 0) {
                            event_data[i].status = 5;
                        }
                    }
                    break;

                case EVENT_PIRATE_RAID:
                    game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())].bc -= (int)event_data[i].amount;
                    if (game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())].bc < 0) {
                        game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())].bc = 0;
                    }
                    event_data[i].status = 5;
                    break;

                case EVENT_PLAGUE:
                    if (event_data[i].status != 6) {
                        event_data[i].amount -= Colony_Research_Points(event_data[i].colony_target.get());
                        COLCALC::Colony_Calculation(event_data[i].colony_target.get());
                        if (event_data[i].amount <= 0) {
                            event_data[i].status = 5;
                        } else if (event_data[i].turn_count > 4 && game_random::Random(20) == 1) {
                            event_data[i].status = 4;
                        }
                        event_data[i].turn_count++;
                    }
                    break;

                case EVENT_POPULATION_BOOM:
                    if (event_data[i].status == 2) {
                        COLCALC::Colony_Calculation(event_data[i].colony_target.get());
                        if (event_data[i].turn_count > 4 && game_random::Random(20) == 1) {
                            event_data[i].status = 5;
                        }
                        if (event_data[i].turn_count > 20) {
                            event_data[i].status = 5;
                        }
                        event_data[i].turn_count++;
                    }
                    break;

                case EVENT_SERENDIPITY: {
                    s_player* player = &game::current_state.players[legacy_world::Encode_Player_Slot(event_data[i].player.get())];
                    int8_t field = player->current_research_field;
                    event_data[i].target_id = (int16_t)field;
                    COLCALC::Give_Player_Field(player, (int)field, 1);
                    bool keep_points = COLCALC::keep_research_on_breakthrough != 0 &&
                        player->research_breakthrough != 0;
                    COLCALC::Resolve_Research_Completion(player, keep_points, 0);
                    event_data[i].status = 5;
                    break;
                }

                case EVENT_SPACE_AMOEBA:
                    if (event_data[i].status == 6) {
                        SHIPS::Build_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_AMOEBA, game::Star_Row(game::current_state, event_data[i].star_target.get()), 1);
                    } else if (event_data[i].planet_target.get() != game::NO_PLANET && event_data[i].colony_target.get() != game::NO_COLONY) {
                        event_data[i].status = 4;
                    } else {
                        if (event_data[i].turn_count > 4) event_data[i].status = 5;
                        if (event_data[i].amount != -1) event_data[i].status = 5;
                    }
                    break;

                case EVENT_SPACE_CRYSTAL:
                    if (event_data[i].status == 6) {
                        SHIPS::Build_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_CRYSTAL, game::Star_Row(game::current_state, event_data[i].star_target.get()), 1);
                    } else if (event_data[i].planet_target.get() != game::NO_PLANET && event_data[i].colony_target.get() != game::NO_COLONY) {
                        event_data[i].status = 4;
                    } else {
                        if (event_data[i].turn_count > 4) event_data[i].status = 5;
                        if (event_data[i].amount != -1) event_data[i].status = 5;
                    }
                    break;

                case EVENT_SPACE_DRAGON:
                    if (event_data[i].status == 6) {
                        SHIPS::Build_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_DRAGON, game::Star_Row(game::current_state, event_data[i].star_target.get()), 1);
                    } else if (event_data[i].planet_target.get() != game::NO_PLANET && event_data[i].colony_target.get() != game::NO_COLONY) {
                        event_data[i].status = 4;
                    } else {
                        if (event_data[i].turn_count > 4) event_data[i].status = 5;
                        if (event_data[i].amount != -1) event_data[i].status = 5;
                    }
                    break;

                case EVENT_SPACE_EEL:
                    if (event_data[i].status == 6) {
                        SHIPS::Build_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_EEL, game::Star_Row(game::current_state, event_data[i].star_target.get()), 1);
                    } else if (event_data[i].planet_target.get() != game::NO_PLANET && event_data[i].colony_target.get() != game::NO_COLONY) {
                        event_data[i].status = 4;
                    } else {
                        if (event_data[i].turn_count > 4) event_data[i].status = 5;
                        if (event_data[i].amount != -1) event_data[i].status = 5;
                    }
                    break;

                case EVENT_SPACE_HYDRA:
                    if (event_data[i].status == 6) {
                        SHIPS::Build_Nonplayer_Ship(NONPLAYER_SHIP_TYPE_HYDRA, game::Star_Row(game::current_state, event_data[i].star_target.get()), 1);
                    } else if (event_data[i].planet_target.get() != game::NO_PLANET && event_data[i].colony_target.get() != game::NO_COLONY) {
                        event_data[i].status = 4;
                    } else {
                        if (event_data[i].turn_count > 4) event_data[i].status = 5;
                        if (event_data[i].amount != -1) event_data[i].status = 5;
                    }
                    break;

                case EVENT_SUPERNOVA:
                    if (event_data[i].status != 6) {
                        for (int j = 0; j < 5; ++j) {
                            const auto planet_idx = game::current_state.stars.at(event_data[i].star_target.get()).orbit_planet_ids[j];
                            if (planet_idx != game::NO_PLANET) {
                                const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                                if (colony_id != game::NO_COLONY) {
                                    const int16_t research = Colony_Research_Points(colony_id);
                                    event_data[i].amount -= research;
                                    COLCALC::Colony_Calculation(colony_id);
                                }
                            }
                        }
                        if (event_data[i].amount <= 0) {
                            event_data[i].status = 5;
                        } else {
                            event_data[i].turn_count--;
                            if (event_data[i].turn_count <= 0) {
                                for (int j = 0; j < 5; ++j) {
                                    const auto planet_idx = game::current_state.stars.at(event_data[i].star_target.get()).orbit_planet_ids[j];
                                    if (planet_idx != game::NO_PLANET) {
                                        game::current_state.planets.at(planet_idx).climate = PLANET_CLIMATE_RADIATED;
                                        game::current_state.planets.at(planet_idx).environmental_alterations = 0;
                                        game::current_state.planets.at(planet_idx).food_per_farmer =
                                            MOX::food_per_farmer_table[PLANET_CLIMATE_RADIATED];
                                        const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                                        if (colony_id != game::NO_COLONY) {
                                            BOMB::Destroy_Colony(game::current_state, colony_id, -1);
                                        }
                                    }
                                }
                                event_data[i].status = 5;
                            }
                        }
                        if (game_random::Random(20) == 1 && event_data[i].status != 5) {
                            event_data[i].status = 4;
                        }
                    }
                    break;

                case EVENT_TIME_SPACE_ANOMALY:
                    if (event_data[i].status == 2) {
                        for (int j = 0; j < 5; ++j) {
                            const auto planet_idx = game::current_state.stars.at(event_data[i].star_target.get()).orbit_planet_ids[j];
                            if (planet_idx != game::NO_PLANET) {
                                const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                                if (colony_id != game::NO_COLONY) {
                                    COLCALC::Colony_Calculation(colony_id);
                                }
                            }
                        }
                        if (event_data[i].turn_count > duration.anomaly_min &&
                            game_random::Random(duration.anomaly_chance) == 1) {
                            event_data[i].status = 5;
                        }
                        if (event_data[i].turn_count > 20) {
                            event_data[i].status = 5;
                        }
                        event_data[i].turn_count++;
                    }
                    break;

                case EVENT_WARP_BEAST:
                    if (event_data[i].status == 2) {
                        if (event_data[i].turn_count > duration.beast_min &&
                            game_random::Random(duration.beast_chance) == 1) {
                            event_data[i].status = 5;
                        }
                        if (event_data[i].turn_count > 20) {
                            event_data[i].status = 5;
                        }
                        if (event_data[i].status != 5) {
                            SHIPMOVE::Do_Hyperspace_Beast(event_data[i].player.get());
                            event_data[i].player = Get_Event_Victim(event_good_array[i]);
                        }
                        event_data[i].turn_count++;
                    }
                    break;

                case EVENT_WARP_FUNNEL:
                    if (event_data[i].status == 2) {
                        if (event_data[i].turn_count > 4 && game_random::Random(20) == 1) {
                            event_data[i].status = 5;
                        }
                        if (event_data[i].turn_count > 20) {
                            event_data[i].status = 5;
                        }
                        event_data[i].turn_count++;
                    }
                    break;

                case EVENT_UNSTABLE_WORMHOLE:
                case EVENT_EMPIRE_DESTROYED:
                case EVENT_EMPIRE_GROWN_IN_STRENGTH:
                case EVENT_EMPIRES_RANKING:
                case EVENT_ORION_VISITED:
                case EVENT_ANTARANS_DEFEATED:
                case EVENT_EMPIRE_SURRENDERS:
                case EVENT_REBEL_ASSIMILATION:
                    event_data[i].status = 5;
                    break;

                default:
                    break;
            }
        }
    }

    int16_t __cdecl Colony_Has_Event(game::id_colony colony_id) {
        if (Event_Check_Comet(colony_id) == 1) {
            return EVENT_COMET;
        }

        if (Event_Check_Plague(colony_id) == 1) {
            return EVENT_PLAGUE;
        }

        if (Event_Check_Population_Boom(colony_id) == 1) {
            return EVENT_POPULATION_BOOM;
        }

        if (Event_Check_Space_Anomaly(colony_id) == 1) {
            return EVENT_TIME_SPACE_ANOMALY;
        }

        int8_t status14 = event_data[EVENT_PIRATE_ACTIVITY].status;
        if (status14 == 2 || status14 == 4 || status14 == 6) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::current_state.planets.at(planet_idx).star_id == event_data[EVENT_PIRATE_ACTIVITY].star_target.get()) {
                return EVENT_PIRATE_ACTIVITY;
            }
        }

        int8_t status24 = event_data[EVENT_SUPERNOVA].status;
        if (status24 == 2 || status24 == 4 || status24 == 6) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::current_state.planets.at(planet_idx).star_id == event_data[EVENT_SUPERNOVA].star_target.get()) {
                return EVENT_SUPERNOVA;
            }
        }

        return -1;
    }

    static bool Hyperspace_Flux_Blocks_Event(EVENT event_type) {
        if (EVENTS::Event_Check_Hyperspace_Flux() == 0) {
            return false;
        }

        switch (event_type) {
            case EVENT_COMET:
            case EVENT_SPACE_AMOEBA:
            case EVENT_SPACE_CRYSTAL:
            case EVENT_SPACE_DRAGON:
            case EVENT_SPACE_EEL:
            case EVENT_SPACE_HYDRA:
                return true;
            default:
                return false;
        }
    }

    void __cdecl Determine_Event() {
        bool is_random_event_due = false;

        EVENTS::Update_Lucky_Players_Events();
        const auto lucky_player_event = EVENTS::Determine_Lucky_Players_Events();

        if (lucky_player_event != game::NO_PLAYER) {
            is_random_event_due = true;
            if ((int32_t)(MOX::stardate - 35000) < all_events_delay) {
                is_random_event_due = false;
            }
        }

        if (MOX::settings.random_events == 1 && !is_random_event_due) {
            int32_t delay = 0;
            if ((int32_t)MOX::stardate >= (int32_t)EVENTS::last_event_year &&
                (int32_t)(MOX::stardate - 35000) >= all_events_delay) {
                delay = (int32_t)(MOX::stardate - EVENTS::last_event_year);
                if (delay < 0) {
                    delay = 0;
                }
            }

            if (EVENTS::event_delay_count < between_events_delay) {
                EVENTS::event_delay_count++;
                delay = 0;
            }

            int32_t probability = delay;
            switch (MOX::settings.game_difficulty) {
                case GAME_DIFFICULTY_TUTOR: probability = delay / 2; break;
                case GAME_DIFFICULTY_EASY: probability = (delay * 2) / 3; break;
                case GAME_DIFFICULTY_AVERAGE: probability = (delay * 3) / 4; break;
                case GAME_DIFFICULTY_HARD: probability = (delay * 4) / 5; break;
                case GAME_DIFFICULTY_IMPOSSIBLE: probability = (delay * 5) / 6; break;
            }

            if ((int32_t)game_random::Random(event_roll.all_events) <= probability) {
                is_random_event_due = true;
            }
        }

        if (!is_random_event_due) {
            return;
        }

        int16_t derelict_count = 0;
        uint16_t derelict_apps[2] = {0, 0};
        s_wormhole_data wormhole_data;

        for (int16_t loop_idx = 0; loop_idx < 5; ++loop_idx) {
            int16_t event_type = game_random::Random(EVENT_RANDOM_COUNT) - 1;

            const EVENT candidate = (EVENT)event_type;
            const int32_t game_turn = (int32_t)(MOX::stardate - 35000);
            if (game_turn < EVENTS::event_delay[candidate] ||
                Hyperspace_Flux_Blocks_Event(candidate)) {
                event_type = -1;
            }

            if (EVENTS::force_event_type != 0) {
                event_type = EVENTS::force_event_type - 1;
            }

            if (event_type == -1) {
                continue;
            }

            if (MOX::settings.game_difficulty == GAME_DIFFICULTY_TUTOR && EVENTS::event_good_array[event_type] == 0) {
                continue;
            }

            int16_t target_id = -1;
            auto target_colony = game::NO_COLONY;
            auto player = game::NO_PLAYER;
            auto contact = game::NO_PLAYER;
            auto ship_id = game::NO_SHIP;

            if (EVENTS::event_data[event_type].status != 0) {
                event_type = -1;
            } else {
                if (lucky_player_event != game::NO_PLAYER) {
                    if (EVENTS::event_good_array[event_type] != 1) {
                        event_type = -1;
                    } else {
                        player = lucky_player_event;
                    }
                } else {
                    if (event_type != EVENT_HYPERSPACE_FLUX && event_type != EVENT_SUPERNOVA) {
                        player = EVENTS::Get_Event_Victim(EVENTS::event_good_array[event_type]);
                        if (player == game::NO_PLAYER) {
                            event_type = -1;
                        }
                    }
                }
            }

            if (event_type == EVENT_DERELICT) {
                KEN::Get_Derelict_Applications(player, derelict_apps, &derelict_count);
                if (derelict_count < 1) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_PIRATE_RAID && game::current_state.players[player.id].bc < 100) {
                event_type = -1;
            }

            if (event_type == EVENT_CLIMATIC_CHANGE) {
                target_colony = EVENTS::Get_Event_Climate_Change_Colony(player);
                if (target_colony == game::NO_COLONY) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_DIPLOMATIC_BLUNDER || event_type == EVENT_DIPLOMATIC_MARRIAGE) {
                contact = EVENTS::Pick_Random_Player_In_Contact(player);
                if (contact == game::NO_PLAYER || game::current_state.players[player.id].treaty[contact.id] >= 4) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_FREAK_ACCIDENT) {
                ship_id = EVENTS::Pick_Random_Ship(player);
                if (ship_id == game::NO_SHIP) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_INDUSTRIAL_ACCIDENT) {
                if (game::current_state.players[player.id].traits[23] == 1) {
                    event_type = -1;
                } else {
                    target_colony = EVENTS::Get_Event_Industrial_Accident_Colony(player);
                    if (target_colony == game::NO_COLONY) {
                        event_type = -1;
                    }
                }
            }

            if (event_type == EVENT_MINERAL_DEPLETION) {
                target_colony = EVENTS::Get_Event_Mineral_Depletion_Colony(player);
                if (target_colony == game::NO_COLONY) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_MINERAL_DEPOSIT) {
                target_colony = EVENTS::Get_Event_Mineral_Deposit_Colony(player);
                if (target_colony == game::NO_COLONY) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_PIRATE_ACTIVITY) {
                if (game::current_state.players[player.id].n_freighters < 4) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_MUTINY) {
                event_type = -1;
            }

            if (event_type == EVENT_PLAGUE) {
                target_colony = EVENTS::Pick_Random_Colony_No_Capitol(player);
                if (target_colony != game::NO_COLONY) {
                    if (EVENTS::event_data[EVENT_POPULATION_BOOM].status == 2 &&
                        EVENTS::event_data[EVENT_POPULATION_BOOM].colony_target.get() == target_colony) {
                        event_type = -1;
                    }
                    if (EVENTS::Colony_Has_Event(target_colony) != -1) {
                        event_type = -1;
                    }
                } else {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_POPULATION_BOOM) {
                target_colony = EVENTS::Pick_Random_Colony_No_Outpost(player);
                if (target_colony != game::NO_COLONY) {
                    if (EVENTS::event_data[EVENT_PLAGUE].status == 2 &&
                        EVENTS::event_data[EVENT_PLAGUE].colony_target.get() == target_colony) {
                        event_type = -1;
                    }
                    if (EVENTS::Colony_Has_Event(target_colony) != -1) {
                        event_type = -1;
                    }
                    if (COLCALC::Planet_Max_Population_For_Player(&game::current_state.planets.at(game::current_state.colonies.at(target_colony).planet_id), player) == game::current_state.colonies.at(target_colony).n_pops) {
                        event_type = -1;
                    }
                } else {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_UNSTABLE_WORMHOLE) {
                SHIPMOVE::Do_Wormhole_Event(player, &wormhole_data);
                if (wormhole_data.success == 0) {
                    event_type = -1;
                }
            }

            if (event_type == EVENT_WARP_BEAST || event_type == EVENT_WARP_FUNNEL) {
                ship_id = EVENTS::Pick_Random_Ship(player);
                if (ship_id == game::NO_SHIP) {
                    event_type = -1;
                }
            }

            if (event_type == -1) continue;

            EVENTS::last_event_year = MOX::stardate;
            EVENTS::event_delay_count = 0;

            EVENTS::event_data[event_type].player = player;
            EVENTS::event_data[event_type].status = 1;
            EVENTS::event_data[event_type].turn_count = -1;
            EVENTS::event_data[event_type].target_id = -1;
            EVENTS::event_data[event_type].planet_target = game::NO_PLANET;
            EVENTS::event_data[event_type].colony_target = game::NO_COLONY;
            EVENTS::event_data[event_type].star_target = game::NO_STAR;
            EVENTS::event_data[event_type].ship_target = game::NO_SHIP;
            EVENTS::event_data[event_type].amount = -1;
            EVENTS::event_data[event_type].max_amount = -1;

            switch (event_type) {
                case EVENT_DERELICT:
                    EVENTS::event_data[event_type].target_id = derelict_apps[0];
                    EVENTS::event_data[event_type].amount = 0;
                    if (derelict_count > 1) {
                        EVENTS::event_data[event_type].amount = derelict_apps[1];
                    }
                    break;
                case EVENT_CLIMATIC_CHANGE:
                    EVENTS::event_data[event_type].planet_target = game::current_state.colonies.at(target_colony).planet_id;
                    break;
                case EVENT_COMET:
                    target_colony = EVENTS::Pick_Random_Colony_No_Capitol(player);
                    EVENTS::event_data[event_type].colony_target = target_colony;
                    if (target_colony == game::NO_COLONY || EVENTS::Colony_Has_Event(target_colony) != -1) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    } else {
                        int16_t amount = (game_random::Random(5) + MOX::settings.game_difficulty + 10) * 10;
                        EVENTS::event_data[event_type].amount = amount;
                        EVENTS::event_data[event_type].max_amount = amount;
                        EVENTS::event_data[event_type].turn_count = game_random::Random(5) - MOX::settings.game_difficulty + 10;
                    }
                    break;
                case EVENT_COMPUTER_VIRUS:
                    if (game::current_state.players[player.id].research_accumulated < 10) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    }
                    break;
                case EVENT_DIPLOMATIC_BLUNDER:
                case EVENT_DIPLOMATIC_MARRIAGE:
                    EVENTS::event_data[event_type].target_id = legacy_world::Encode_Player_Slot(contact);
                    break;
                case EVENT_DONATION:
                    EVENTS::event_data[event_type].target_id = Donation_Amount(
                        MOX::stardate - 35000);
                    break;
                case EVENT_EARTHQUAKE:
                    target_colony = EVENTS::Pick_Random_Colony_No_Capitol(player);
                    EVENTS::event_data[event_type].colony_target = target_colony;
                    if (target_colony == game::NO_COLONY) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    }
                    break;
                case EVENT_FREAK_ACCIDENT:
                    Set_Event_Ship(EVENTS::event_data[event_type], ship_id);
                    break;
                case EVENT_HYPERSPACE_FLUX:
                    EVENTS::event_data[event_type].turn_count = 0;
                    break;
                case EVENT_INDUSTRIAL_ACCIDENT:
                case EVENT_MINERAL_DEPLETION:
                case EVENT_MINERAL_DEPOSIT:
                    EVENTS::event_data[event_type].planet_target = game::current_state.colonies.at(target_colony).planet_id;
                    break;
                case EVENT_MUTINY:
                    Set_Event_Ship(EVENTS::event_data[event_type], ship_id);
                    EVENTS::event_data[event_type].amount = legacy_world::Encode_Player_Slot(contact);
                    break;
                case EVENT_PIRATE_ACTIVITY:
                    target_id = EVENTS::Pick_Random_Star(player);
                    EVENTS::event_data[event_type].star_target = game::Star_Key_At_Row(game::current_state, target_id);
                    if (target_id == -1 || EVENTS::Star_Has_Event(target_id) != -1) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    } else {
                        int16_t str = EVENTS::Event_Pirate_Activity_Strength(target_id);
                        EVENTS::event_data[event_type].amount = str;
                        EVENTS::event_data[event_type].max_amount = str;
                        if (str < 1) {
                            EVENTS::event_data[event_type].status = 0;
                            EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                        }
                    }
                    break;
                case EVENT_PIRATE_RAID:
                    target_id = EVENTS::Pick_Random_Star(player);
                    EVENTS::event_data[event_type].star_target = game::Star_Key_At_Row(game::current_state, target_id);
                    if (target_id == -1) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    } else {
                        EVENTS::event_data[event_type].amount =
                            Pirate_Raid_Amount(game::current_state.players[player.id].bc);
                    }
                    break;
                case EVENT_PLAGUE: {
                    EVENTS::event_data[event_type].colony_target = target_colony;
                    int16_t multiplier = game_random::Random(8) + MOX::settings.game_difficulty * 2;
                    int16_t pts = multiplier * EVENTS::Colony_Research_Points(target_colony);
                    EVENTS::event_data[event_type].amount = pts;
                    EVENTS::event_data[event_type].max_amount = pts;
                    if (pts < 1) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    }
                    break;
                }
                case EVENT_POPULATION_BOOM:
                    EVENTS::event_data[event_type].colony_target = target_colony;
                    EVENTS::event_data[event_type].turn_count = 0;
                    break;
                case EVENT_SPACE_AMOEBA:
                case EVENT_SPACE_CRYSTAL:
                case EVENT_SPACE_DRAGON:
                case EVENT_SPACE_EEL:
                case EVENT_SPACE_HYDRA:
                    target_id = EVENTS::Pick_Random_Star(player);
                    EVENTS::event_data[event_type].star_target = game::Star_Key_At_Row(game::current_state, target_id);
                    if (target_id == -1) {
                        EVENTS::event_data[event_type].status = 0;
                    }
                    EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    break;
                case EVENT_SUPERNOVA: {
                    target_id = EVENTS::Pick_Completely_Random_Star();
                    EVENTS::event_data[event_type].star_target = game::Star_Key_At_Row(game::current_state, target_id);
                    if (target_id == -1 || EVENTS::Star_Has_Event(target_id) != -1) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    } else {
                        EVENTS::event_data[event_type].turn_count = game_random::Random(5) - MOX::settings.game_difficulty + 10;
                        int16_t total = Supernova_Research_Requirement(
                            target_id, EVENTS::event_data[event_type].turn_count);
                        EVENTS::event_data[event_type].amount = total;
                        EVENTS::event_data[event_type].max_amount = total;
                    }
                    break;
                }
                case EVENT_TIME_SPACE_ANOMALY:
                    target_id = EVENTS::Pick_Random_Star(player);
                    EVENTS::event_data[event_type].star_target = game::Star_Key_At_Row(game::current_state, target_id);
                    EVENTS::event_data[event_type].turn_count = 0;
                    if (target_id == -1 || EVENTS::Star_Has_Event(target_id) != -1) {
                        EVENTS::event_data[event_type].status = 0;
                        EVENTS::event_data[event_type].player = legacy_world::Decode_Player_Slot(-1);
                    }
                    break;
                case EVENT_WARP_BEAST:
                    EVENTS::event_data[event_type].turn_count = 0;
                    break;
                case EVENT_WARP_FUNNEL:
                    EVENTS::event_data[event_type].status = 3;
                    Set_Event_Ship(EVENTS::event_data[event_type], ship_id);
                    EVENTS::event_data[event_type].turn_count = 0;
                    break;
                case EVENT_UNSTABLE_WORMHOLE:
                    EVENTS::event_data[event_type].target_id = wormhole_data.ships_moved;
                    EVENTS::event_data[event_type].star_target = wormhole_data.destination.star_id;
                    if (wormhole_data.destination.kind == game::e_star_destination::antares) {
                        EVENTS::event_data[event_type].amount = UNSTABLE_WORMHOLE_ANTARES;
                    }
                    break;
            }

            break;
        }

        EVENTS::Event_Check_Planet_Warning();
        EVENTS::Event_Check_General_Status();
    }

    game::id_player __cdecl Get_Antaran_Victim() {
        uint32_t weights[MAX_PLAYERS];
        int32_t candidate_weights[MAX_PLAYERS];
        game::id_player candidates[MAX_PLAYERS];
        int16_t count = 0;

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            weights[i] = 0;
            candidate_weights[i] = 0;
            candidates[i] = game::NO_PLAYER;
        }

        if (game::current_state.player_count > 0) {
            for (int i = 0; i < (int)game::current_state.player_count; ++i) {
                s_player* plr = &game::current_state.players[i];

                if (plr->eliminated == 0) {
                    int32_t weight = plr->total_pop;
                    candidates[count] = plr->id;

                    if (plr->objectives == PLAYER_OBJECTIVE_HUMAN) {
                        switch (MOX::settings.game_difficulty) {
                            case GAME_DIFFICULTY_TUTOR:
                                weight /= 10;
                                break;
                            case GAME_DIFFICULTY_EASY:
                                weight /= 5;
                                break;
                            case GAME_DIFFICULTY_AVERAGE:
                                break;
                            case GAME_DIFFICULTY_HARD:
                                weight = weight + weight / 2;
                                break;
                            case GAME_DIFFICULTY_IMPOSSIBLE:
                                weight *= antarans::fleet_config.attack
                                              .impossible_human_weight_multiplier;
                                break;
                        }
                    }

                    if (plr->traits[TRAIT_LUCKY] == 1) {
                        weight /= antarans::fleet_config.attack.lucky_victim_weight_divisor;
                    }

                    candidate_weights[count] = weight;
                    count++;
                }
            }
        }

        int16_t min_pop = 32000;
        int16_t min_idx = 0;

        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                int16_t current_pop = game::current_state.players[candidates[i].id].total_pop;
                if (current_pop < min_pop) {
                    min_pop = current_pop;
                    min_idx = i;
                }
            }
        }

        count--;

        if (min_idx < count) {
            for (int j = min_idx; j < count; ++j) {
                candidates[j] = candidates[j + 1];
                candidate_weights[j] = candidate_weights[j + 1];
            }
        }

        for (int i = 0; i < count; ++i) {
            int32_t diff = candidate_weights[i] - min_pop;
            weights[i] = (uint32_t)((int64_t)diff * diff);
        }

        int16_t choice_idx = RUSS::Get_Weighted_Choice_Int(weights, count);
        if (choice_idx < 0) {
            choice_idx = 0;
        }
        return candidates[choice_idx];
    }

    /**
     * Terminate events associated with a colony that is no longer valid (e.g. destroyed or abandoned).
     * Finished reports can still name the planet after the colony is erased.
     */
    void __cdecl Kill_All_Events_At_Colony(game::id_colony colony_id) {
        for (const auto type : {EVENT_COMET, EVENT_EARTHQUAKE, EVENT_PLAGUE, EVENT_POPULATION_BOOM}) {
            auto& event = event_data[type];
            if (event.colony_target.get() != colony_id || event.status == 0 || event.status == 3) continue;
            if (event.status == 5) {
                event.planet_target = game::current_state.colonies.at(colony_id).planet_id;
            } else {
                event.status = 3; // Expired.
                event.player = legacy_world::Decode_Player_Slot(-1);
            }
            event.colony_target = game::NO_COLONY;
        }

        if (EVENTS::Event_Check_Space_Anomaly(colony_id) == 1) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            int16_t presence_count = 0;
            const auto has_colony_mask = game::current_state.stars.at(game::current_state.planets.at(planet_idx).star_id).has_colony;

            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                if (has_colony_mask.test_slot(i)) {
                    presence_count++;
                }
            }

            if (presence_count < 2) {
                event_data[EVENT_TIME_SPACE_ANOMALY].status = 3 /* Expired */;
                event_data[EVENT_TIME_SPACE_ANOMALY].player = legacy_world::Decode_Player_Slot(-1);
            }
        }

        int8_t status14 = event_data[EVENT_PIRATE_ACTIVITY].status;
        if (status14 == 2 || status14 == 4 || status14 == 6) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            const auto parent_star_id = game::current_state.planets.at(planet_idx).star_id;

            if (event_data[EVENT_PIRATE_ACTIVITY].star_target.get() == parent_star_id) {
                int16_t presence_count = 0;
                const auto has_colony_mask = game::current_state.stars.at(parent_star_id).has_colony;

                for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                    if (has_colony_mask.test_slot(i)) {
                        presence_count++;
                    }
                }

                if (presence_count < 2) {
                    event_data[EVENT_PIRATE_ACTIVITY].status = 3 /* Expired */;
                    event_data[EVENT_PIRATE_ACTIVITY].player = legacy_world::Decode_Player_Slot(-1);
                }
            }
        }

        int8_t status24 = event_data[EVENT_SUPERNOVA].status;
        if (status24 == 2 || status24 == 4 || status24 == 6) {
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            const auto parent_star_id = game::current_state.planets.at(planet_idx).star_id;

            if (event_data[EVENT_SUPERNOVA].star_target.get() == parent_star_id) {
                int16_t presence_count = 0;
                const auto has_colony_mask = game::current_state.stars.at(parent_star_id).has_colony;

                for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                    if (has_colony_mask.test_slot(i)) {
                        presence_count++;
                    }
                }

                if (presence_count < 2) {
                    event_data[EVENT_SUPERNOVA].status = 3 /* Expired */;
                    event_data[EVENT_SUPERNOVA].player = legacy_world::Decode_Player_Slot(-1);
                }
            }
        }
    }

    void __cdecl Update_Lucky_Players_Events() {
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            // Index 0x1a corresponds to the 'Lucky' racial trait
            if (game::current_state.players[i].eliminated == 0 && game::current_state.players[i].traits[TRAIT_LUCKY] == 1) {
                game::current_state.players[i].time_since_last_lucky_event++;
            }
        }
    }

    /**
     * Attempts to find a colony belonging to the specified player with a climate value less than 8.
     * It first tries to pick a colony randomly up to 200 times. If that fails, it performs a 
     * sequential search through all colonies.
     */
    game::id_colony __cdecl Get_Event_Climate_Change_Colony(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto colony_idx = game::NO_COLONY;

        // Attempt to pick a random colony that satisfies the climate condition
        for (int i = 0; i < 200; ++i) {
            const auto picked = Pick_Random_Colony_No_Outpost(player_key);
            if (picked != game::NO_COLONY) {
                const auto planet_idx = game::current_state.colonies.at(picked).planet_id;
                if (game::current_state.planets.at(planet_idx).climate < PLANET_CLIMATE_TERRAN) {
                    colony_idx = picked;
                    break;
                }
            }

            if (colony_idx != game::NO_COLONY) {
                break;
            }
        }

        // Fallback: sequential search through all colonies
        if (colony_idx == game::NO_COLONY) {
            for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
                if (colony_idx != game::NO_COLONY) {
                    break;
                }

                if (legacy_world::Encode_Player_Slot(roster_colony.owner) == player_idx) {
                    const auto planet_idx = roster_colony.planet_id;
                    if (game::current_state.planets.at(planet_idx).climate < PLANET_CLIMATE_TERRAN) {
                        colony_idx = roster_id;
                    }
                }
            }
        }

        return colony_idx;
    }
    game::id_player __cdecl Pick_Random_Player_In_Contact(game::id_player player) {
        if (player.id >= MAX_PLAYERS) return game::NO_PLAYER;
        auto selected_player = game::NO_PLAYER;
        int16_t eligible_count = 0;
        // The legacy event picker reads the first contact value as a bitmap.
        const auto contacts = game::player_bitmap::From_Byte(static_cast<uint8_t>(game::current_state.players[player.id].contact[0]));

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (game::current_state.players[i].eliminated == 0) {
                if (contacts.test_slot(i)) {
                    if (game::current_state.players[i].id != player) {
                        eligible_count++;

                        // Reservoir sampling: pick one of N elements with 1/N probability
                        if (game_random::Random(eligible_count) == 1) {
                            selected_player = game::current_state.players[i].id;
                        }
                    }
                }
            }
        }

        return selected_player;
    }

    /**
     * Picks a random ship belonging to the specified player that is currently active.
     * Uses reservoir sampling to ensure uniform distribution in a single pass.
     */
    game::id_ship __cdecl Pick_Random_Ship(game::id_player owner_key) {
        const auto owner = factions::From_Player(owner_key);
        if (owner == game::NO_OWNER) return game::NO_SHIP;
        int16_t match_count = 0;
        auto selected_ship = game::NO_SHIP;

        for (int16_t i = 0; i < std::ssize(game::current_state.ship_slots); ++i) {
            if (game::current_state.ship_slots[i] == game::NO_SHIP) continue;
            s_ship_data* ship = &ship_slots::At(game::current_state, i);

            if (ship->owner == owner) {
                // status 0 or 1 usually indicates active ships (e.g., normal or in transit)
                if (ship->status == 0 || ship->status == 1) {
                    match_count++;

                    // Reservoir sampling: pick the nth item with probability 1/n
                    if (game_random::Random((int32_t)match_count) == 1) {
                        selected_ship = ship->id;
                    }
                }
            }
        }

        return selected_ship;
    }

    /**
     * Attempts to find a suitable colony for an industrial accident event.
     * Searches randomly first, then falls back to a linear scan of player colonies.
     * Only selects colonies on planets with climate > 1.
     */
    game::id_colony __cdecl Get_Event_Industrial_Accident_Colony(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto result_colony_idx = game::NO_COLONY;
        int16_t attempts = 0;

        // Attempt to pick a random colony that isn't the capitol
        while (attempts < 200) {
            const auto random_idx = Pick_Random_Colony_No_Capitol(player_key);

            if (random_idx != game::NO_COLONY) {
                const auto planet_idx = game::current_state.colonies.at(random_idx).planet_id;
                if (game::current_state.planets.at(planet_idx).climate > PLANET_CLIMATE_RADIATED) {
                    result_colony_idx = random_idx;
                }
            }

            attempts++;
            if (result_colony_idx != game::NO_COLONY) {
                break;
            }
        }

        // Fallback: if no colony found randomly, scan all colonies linearly
        if (result_colony_idx == game::NO_COLONY) {
            for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
                if (result_colony_idx != game::NO_COLONY) {
                    break;
                }

                if (legacy_world::Encode_Player_Slot(roster_colony.owner) == player_idx) {
                    const auto planet_idx = roster_colony.planet_id;
                    if (game::current_state.planets.at(planet_idx).climate > PLANET_CLIMATE_RADIATED) {
                        result_colony_idx = roster_id;
                    }
                }
            }
        }

        return result_colony_idx;
    }

    game::id_colony __cdecl Get_Event_Mineral_Deposit_Colony(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto colony_idx = game::NO_COLONY;
        int16_t attempts = 0;

        while (attempts < 200) {
            const auto random_idx = Pick_Random_Colony_No_Outpost(player_key);

            if (random_idx != game::NO_COLONY) {
                const auto planet_idx = game::current_state.colonies.at(random_idx).planet_id;

                // Mineral classes: 0=Ultra Poor, 1=Poor, 2=Abundant, 3=Rich, 4=Ultra Rich
                // The check is for mineral_class < 4.
                if (game::current_state.planets.at(planet_idx).mineral_class < MINERAL_RESOURCE_ULTRA_RICH) {
                    colony_idx = random_idx;
                }
            }

            attempts++;

            if (colony_idx != game::NO_COLONY) {
                break;
            }
        }

        return colony_idx;
    }

    game::id_colony __cdecl Pick_Random_Colony_No_Outpost(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto selected_colony_idx = game::NO_COLONY;
        int16_t match_count = 0;

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            // Check if the colony is owned by the specified player and is not an outpost (outpost_flag == 0)
            if (legacy_world::Encode_Player_Slot(roster_colony.owner) == player_idx && roster_colony.outpost_flag == 0) {
                match_count++;

                // Using reservoir sampling: for the n-th match found, the probability of selecting it is 1/n.
                if (game_random::Random((int32_t)match_count) == 1) {
                    selected_colony_idx = roster_id;
                }
            }
        }

        return selected_colony_idx;
    }

    game::id_colony __cdecl Pick_Random_Colony_No_Capitol(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto selected_colony_idx = game::NO_COLONY;
        int16_t match_count = 0;

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* colony = &roster_colony;

            if (legacy_world::Encode_Player_Slot(colony->owner) == player_idx &&
                colony->buildings[BUILDING_CAPITOL] == 0 &&
                colony->outpost_flag == 0) {
                match_count++;

                // Reservoir sampling: replace the current selection with 1/match_count probability
                if (game_random::Random((int32_t)match_count) == 1) {
                    selected_colony_idx = roster_id;
                }
            }
        }

        return selected_colony_idx;
    }

    int16_t __cdecl Pick_Random_Star(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int16_t result_star_idx = -1;
        int16_t star_idx = 0;
        int16_t count = 0;

        if (star_idx < std::ssize(game::current_state.stars)) {
            do {
                // Check if the player owns the star system
                if (factions::Legacy_Code(game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).owner) == (int8_t)player_idx) {
                    for (int16_t p = 0; p < 5; ++p) {
                        const auto planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[p];
                        if (planet_idx != game::NO_PLANET) {
                            const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                            if (colony_id != game::NO_COLONY) {
                                const auto* colony = &game::current_state.colonies.at(colony_id);
                                // Only consider colonies (not outposts) belonging to the player
                                if (colony->outpost_flag == 0 && legacy_world::Encode_Player_Slot(colony->owner) == (int8_t)player_idx) {
                                    count++;
                                    // Reservoir sampling: pick the current star with 1/count probability.
                                    // The check 'count == 0' handles the unlikely case of a 16-bit counter overflow.
                                    if (count == 0 || game_random::Random((int32_t)count) == 1) {
                                        result_star_idx = star_idx;
                                    }
                                }
                            }
                        }
                    }
                }
                star_idx++;
            } while (star_idx < std::ssize(game::current_state.stars));
        }

        return result_star_idx;
    }

    int16_t __cdecl Star_Has_Event(int16_t star_idx) {
        if ((event_data[EVENT_COMET].status == 2 || event_data[EVENT_COMET].status == 4 ||
                event_data[EVENT_COMET].status == 6)) {
            const auto colony_id = event_data[EVENT_COMET].colony_target.get();
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::Star_Row(game::current_state, game::current_state.planets.at(planet_idx).star_id) == star_idx) {
                return EVENT_COMET;
            }
        }

        if ((event_data[EVENT_PLAGUE].status == 2 || event_data[EVENT_PLAGUE].status == 4 ||
                event_data[EVENT_PLAGUE].status == 6)) {
            const auto colony_id = event_data[EVENT_PLAGUE].colony_target.get();
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::Star_Row(game::current_state, game::current_state.planets.at(planet_idx).star_id) == star_idx) {
                return EVENT_PLAGUE;
            }
        }

        if ((event_data[EVENT_POPULATION_BOOM].status == 2 ||
                event_data[EVENT_POPULATION_BOOM].status == 4 ||
                event_data[EVENT_POPULATION_BOOM].status == 6)) {
            const auto colony_id = event_data[EVENT_POPULATION_BOOM].colony_target.get();
            const auto planet_idx = game::current_state.colonies.at(colony_id).planet_id;
            if (game::Star_Row(game::current_state, game::current_state.planets.at(planet_idx).star_id) == star_idx) {
                return EVENT_POPULATION_BOOM;
            }
        }

        if ((event_data[EVENT_TIME_SPACE_ANOMALY].status == 2 ||
                event_data[EVENT_TIME_SPACE_ANOMALY].status == 4 ||
                event_data[EVENT_TIME_SPACE_ANOMALY].status == 6)) {
            if (event_data[EVENT_TIME_SPACE_ANOMALY].star_target.get() == game::Star_Key_At_Row(game::current_state, star_idx)) {
                return EVENT_TIME_SPACE_ANOMALY;
            }
        }

        if ((event_data[EVENT_PIRATE_ACTIVITY].status == 2 ||
                event_data[EVENT_PIRATE_ACTIVITY].status == 4 ||
                event_data[EVENT_PIRATE_ACTIVITY].status == 6)) {
            if (event_data[EVENT_PIRATE_ACTIVITY].star_target.get() == game::Star_Key_At_Row(game::current_state, star_idx)) {
                return EVENT_PIRATE_ACTIVITY;
            }
        }

        if ((event_data[EVENT_SUPERNOVA].status == 2 || event_data[EVENT_SUPERNOVA].status == 4 ||
                event_data[EVENT_SUPERNOVA].status == 6)) {
            if (event_data[EVENT_SUPERNOVA].star_target.get() == game::Star_Key_At_Row(game::current_state, star_idx)) {
                return EVENT_SUPERNOVA;
            }
        }

        return -1;
    }

    int16_t __cdecl Event_Pirate_Activity_Strength(int16_t star_idx) {
        int16_t total_cp = 0;

        // Sum command points used by all players who have a colony in this system
        for (int16_t player_idx = 0; player_idx < game::current_state.player_count; ++player_idx) {
            // has_colony is treated as a bitfield where each bit corresponds to a player index
            if (game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).has_colony.test_slot(player_idx & 0x1f)) {
                total_cp += game::current_state.players[player_idx].command_points_used;
            }
        }

        int32_t scaled_cp;
        switch (MOX::settings.game_difficulty) {
            case GAME_DIFFICULTY_TUTOR:
                scaled_cp = total_cp / 5;
                break;
            case GAME_DIFFICULTY_EASY:
                scaled_cp = (total_cp * 2) / 5;
                break;
            case GAME_DIFFICULTY_AVERAGE:
                scaled_cp = (total_cp * 3) / 5;
                break;
            case GAME_DIFFICULTY_IMPOSSIBLE:
                scaled_cp = (total_cp * 4) / 5;
                break;
            case GAME_DIFFICULTY_HARD:
            default:
                scaled_cp = (int32_t)total_cp;
                break;
        }

        // If the calculated strength is too low, no pirate activity occurs
        if (scaled_cp < 5) {
            return 0;
        }

        return (int16_t)scaled_cp;
    }

    int16_t __cdecl Pick_Completely_Random_Star() {
        int16_t found = 0;
        int16_t counter = 0;
        int16_t star_idx;

        // Pick initial star
        star_idx = (int16_t)game_random::Random(std::ssize(game::current_state.stars)) - 1; /* INDEX_NARROWING */

        while (counter < 1000) {
            if (game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).has_colony.any()) {
                // If the star has a colony, check if any colony on its planets meets the criteria
                for (int i = 0; i < 5; ++i) {
                    const auto planet_ptr_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[i];
                    if (planet_ptr_idx != game::NO_PLANET) {
                        const auto colony_id = game::current_state.planets.at(planet_ptr_idx).colony_id;
                        if (colony_id != game::NO_COLONY) {
                            // Check if colony is not an outpost and does not have building index 9
                            const auto& colony = game::current_state.colonies.at(colony_id);
                            if (colony.outpost_flag == 0 && colony.buildings[BUILDING_CAPITOL] == 0) {
                                found = 1;
                            }
                        }
                    }
                }
            } else {
                // If no colony on star, pick a new random star
                star_idx = (int16_t)game_random::Random(std::ssize(game::current_state.stars)) - 1; /* INDEX_NARROWING */
            }

            counter++;

            // Exit the loop if a valid star was found
            if (found != 0) {
                break;
            }

            // Note: If has_colony was true but no planet satisfied the condition, star_idx is not updated.
            // This will cause the function to repeatedly check the same star until counter reaches 1000.
        }

        // Return -1 if the search timed out or took too many iterations
        if (counter > 990 /* 0x3de */) {
            return -1;
        }

        return star_idx;
    }

    void __cdecl Event_Check_General_Status() {
        if (game_random::Random(40 /* 0x28 */) == 1) {
            // Check if stardate is past 35050 and the Galactic Council hasn't been established.
            if (MOX::stardate > 35050 /* 35000 + 50 */ && diplodef::COUNCIL_FLAG == 0) {
                event_data[EVENT_EMPIRES_RANKING].status = 6; /* Just Started */
                event_data[EVENT_EMPIRES_RANKING].amount = (int16_t)(game_random::Random(4) - 1);
            }
        }
    }

    int16_t __cdecl Ship_Size_Points_At_Star(int16_t star_idx) {
        int16_t total_size_points = 0;

        auto& state = game::current_state;
        const auto destination = game::Destination_At_Row(state, star_idx);
        if (destination.kind == game::e_star_destination::none) return 0;
        const game::s_ship_location orbit{game::e_ship_location::at_star, destination.star_id.get(),
            destination.kind == game::e_star_destination::antares ? game::e_ship_target::antares : game::e_ship_target::star};
        ship_slots::Visit_Records(state, [&](const s_ship_data& ship) {
            if (ship.status == 0 && ship.position.Get() == orbit) total_size_points += ship.d.size + 1;
        });

        return total_size_points;
    }

    void __cdecl Allocate_Earthquake_Hit(game::id_colony colony_id, int16_t* pop_killed, int16_t* buildings_destroyed_count) {
        s_combat_bomb_result result;
        memset(&result, 0, sizeof(s_combat_bomb_result));

        result.attacker = factions::From_Legacy(-1);
        result.colony_id = colony_id;
        result.is_planetary_hit = 1;

        int16_t building_count = 0;
        const int16_t building_slot_count =
            (int16_t)(sizeof(game::current_state.colonies.at(colony_id).buildings) / sizeof(game::current_state.colonies.at(colony_id).buildings[0]));
        for (int16_t i = 0; i < building_slot_count; ++i) {
            if (game::current_state.colonies.at(colony_id).buildings[i] != 0) {
                building_count++;
            }
        }

        uint8_t population = game::current_state.colonies.at(colony_id).n_pops;
        int16_t total_targets = building_count + population;

        int32_t random1 = game_random::Random(2);
        int32_t random2 = game_random::Random(3);
        int16_t final_hits = (int16_t)(((random1 + random2) * total_targets) / 10);

        if (final_hits == 0) {
            final_hits = 1;
        }

        result.bomb_hits_remaining = final_hits;
        result.bio_hits_remaining = 0;
        result.is_bio_weapon = 0;

        BOMB::Apply_Bomb_Hits(&result);

        *pop_killed = result.population_killed;
        *buildings_destroyed_count = 0;

        for (int16_t i = 0; i < building_slot_count; ++i) {
            if (result.buildings_destroyed[i] == 1) {
                *buildings_destroyed_count += 1;
            }
        }
    }

    int16_t __cdecl Colony_Research_Points(game::id_colony colony_id) {
        return game::current_state.colonies.at(colony_id).production[ECON_RESEARCH];
    }

    /**
     * @brief Checks if the player has any "bad" events currently active.
     *
     * Iterates through the event data array to find an event that belongs to the
     * specified player, is active (status not Inactive/Expired), and is marked
     * as a bad event in the good_event_array.
     */
    bool __cdecl Player_Has_Bad_Event(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        bool found = false;

        for (int i = 0; i < EVENT_COUNT; ++i) {
            // Early exit if event found in previous iteration
            if (found) {
                break;
            }

            const s_event_data& event = event_data[i];

            // Status 0: Inactive, Status 3: Expired
            if (event.status != 0 /* Inactive */ && event.status != 3 /* Expired */) {
                if (legacy_world::Encode_Player_Slot(event.player.get()) == player_idx) {
                    if (event_good_array[i] == 0) {
                        found = true;
                    }
                }
            }
        }

        return found;
    }

    game::id_player __cdecl Get_Event_Victim(int8_t type) {
        uint16_t weights[MAX_PLAYERS];
        game::id_player candidates[MAX_PLAYERS];
        int16_t num_candidates = 0;

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            weights[i] = 0;
            candidates[i] = game::NO_PLAYER;
        }

        int16_t extreme_val;
        int16_t extreme_idx = 0;

        if (type == 0) {
            // "Bad" events: targets players with existing population but no pending bad events.
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                if (game::current_state.players[i].eliminated == 0) {
                    const auto player = game::current_state.players[i].id;
                    if (EVENTS::Player_Has_Bad_Event(player) == 0) {
                        candidates[num_candidates] = player;
                        weights[num_candidates] = game::current_state.players[i].total_pop;
                        num_candidates++;
                    }
                }
            }

            extreme_val = 32000 /* 0x7D00 */;
            for (int16_t i = 0; i < num_candidates; ++i) {
                if (game::current_state.players[candidates[i].id].total_pop < extreme_val) {
                    extreme_val = game::current_state.players[candidates[i].id].total_pop;
                    extreme_idx = i;
                }
            }

            // Exclude the candidate with the lowest population.
            num_candidates--;
            for (int16_t i = extreme_idx; i < num_candidates; ++i) {
                candidates[i] = candidates[i + 1];
                weights[i] = weights[i + 1];
            }

            for (int16_t i = 0; i < num_candidates; ++i) {
                int16_t diff = weights[i] - extreme_val;
                weights[i] = (uint16_t)(diff * diff);
            }
        } else {
            // Other events: targets all active players.
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                if (game::current_state.players[i].eliminated == 0) {
                    candidates[num_candidates] = game::current_state.players[i].id;
                    weights[num_candidates] = game::current_state.players[i].total_pop;
                    num_candidates++;
                }
            }

            extreme_val = 0;
            for (int16_t i = 0; i < num_candidates; ++i) {
                if (game::current_state.players[candidates[i].id].total_pop > extreme_val) {
                    extreme_val = game::current_state.players[candidates[i].id].total_pop;
                    extreme_idx = i;
                }
            }

            // Exclude the candidate with the highest population.
            num_candidates--;
            for (int16_t i = extreme_idx; i < num_candidates; ++i) {
                candidates[i] = candidates[i + 1];
                weights[i] = weights[i + 1];
            }

            for (int16_t i = 0; i < num_candidates; ++i) {
                int16_t diff = extreme_val - (int16_t)weights[i];
                if (diff < 0) {
                    diff = -diff;
                }
                weights[i] = (uint16_t)(diff * diff);
            }
        }

        int16_t selection_idx = (int16_t)KEN::Get_Weighted_Choice(weights, num_candidates);
        if (selection_idx < 0 || selection_idx >= MAX_PLAYERS) return game::NO_PLAYER;
        const auto selected_player = candidates[selection_idx];

        // The "Lucky" trait (index 26) protects from bad events.
        if (type == 0 && selected_player != game::NO_PLAYER && game::current_state.players[selected_player.id].traits[TRAIT_LUCKY] == 1) {
            return game::NO_PLAYER;
        }

        return selected_player;
    }

    void __cdecl Event_Check_Planet_Warning() {
        if (event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].amount <= 2) {
            int16_t half_stars = std::ssize(game::current_state.stars) / 2;
            int16_t colony_counts[MAX_PLAYERS];

            for (int i = 0; i < MAX_PLAYERS; ++i) {
                colony_counts[i] = 0;
            }

            for (const auto& [star_id, star_record] : game::current_state.stars) {
                int16_t player_idx = -1;
                for (int16_t slot = 0; slot < MAX_PLAYERS; ++slot) {
                    if (star_record.has_colony == game::player_bitmap::Only_Slot(slot)) {
                        player_idx = slot;
                        break;
                    }
                }

                if (player_idx != -1) {
                    bool has_real_colony = false;
                    for (int p = 0; p < 5; ++p) {
                        const auto planet_idx = star_record.orbit_planet_ids[p];
                        if (planet_idx != game::NO_PLANET) {
                            const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;
                            if (colony_id != game::NO_COLONY) {
                                if (game::current_state.colonies.at(colony_id).outpost_flag == 0) {
                                    has_real_colony = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (has_real_colony) {
                        colony_counts[player_idx]++;
                    }
                }
            }

            int16_t max_colonies = 0;
            int16_t top_player = 0;
            for (int p = 0; p < game::current_state.player_count; ++p) {
                if (colony_counts[p] > max_colonies) {
                    max_colonies = colony_counts[p];
                    top_player = p;
                }
            }

            int16_t current_amount = event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].amount;
            int16_t next_amount = current_amount + 1;
            int16_t threshold = ((next_amount + 1) * half_stars) / 4;

            if (max_colonies >= threshold ||
                (current_amount == 2 && max_colonies >= (half_stars - 2) &&
                    max_colonies < half_stars && diplodef::COUNCIL_FLAG == 0)) {
                event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].target_id = max_colonies;
                event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].player = game::current_state.players[top_player].id;
                event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].status = 6 /* EVENT_JUST_STARTED */;
                event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].amount = next_amount;
            }
        }
    }

    void __cdecl Allocate_Industrial_Accident_Hit(game::id_colony colony_id) {
        s_combat_bomb_result bomb_result;
        memset(&bomb_result, 0, sizeof(s_combat_bomb_result));

        bomb_result.attacker = factions::From_Legacy(-1);
        bomb_result.colony_id = colony_id;
        bomb_result.is_planetary_hit = 1;
        bomb_result.bomb_hits_remaining = 1;
        bomb_result.is_bio_weapon = 1;

        int rand_val1 = game_random::Random(3);
        int rand_val2 = game_random::Random(3);
        int rand_sum = rand_val1 + rand_val2;

        bomb_result.bio_hits_remaining = (int16_t)((rand_sum * (uint8_t)game::current_state.colonies.at(colony_id).n_pops) / 10);

        BOMB::Apply_Bomb_Hits(&bomb_result);
    }

    bool __cdecl Check_For_Event_Message() {
        for (int16_t event_idx = 0; event_idx < EVENT_COUNT; ++event_idx) {
            int8_t status = event_data[event_idx].status;
            if (status == 4 || status == 6 || status == 5) {
                return true;
            }
        }
        return false;
    }

    void __cdecl Event_Monster_Update(game::owner_slot monster, game::id_planet target_id, game::owner_slot victor) {
        int16_t event_index;

        switch (monster.code) {
            case NONPLAYER_SHIP_TYPE_AMOEBA:
                event_index = EVENT_SPACE_AMOEBA;
                break;
            case NONPLAYER_SHIP_TYPE_CRYSTAL:
                event_index = EVENT_SPACE_CRYSTAL;
                break;
            case NONPLAYER_SHIP_TYPE_DRAGON:
                event_index = EVENT_SPACE_DRAGON;
                break;
            case NONPLAYER_SHIP_TYPE_EEL:
                event_index = EVENT_SPACE_EEL;
                break;
            case NONPLAYER_SHIP_TYPE_HYDRA:
                event_index = EVENT_SPACE_HYDRA;
                break;
            default:
                return;
        }

        s_event_data& event = event_data[event_index];
        if (target_id != game::NO_PLANET) {
            const auto it = game::current_state.planets.find(target_id);
            if (it == game::current_state.planets.end()) return;
            const auto colony_id = it->second.colony_id;
            event.star_target = game::NO_STAR;
            event.planet_target = target_id;
            event.colony_target = colony_id;
            event.turn_count = 0;
            if (colony_id != game::NO_COLONY) {
                event.player = game::current_state.colonies.at(colony_id).owner;
            }
            event.status = 4;
            return;
        }

        event.player = factions::Player(victor);
        event.amount = static_cast<int16_t>(victor.code);
        if (victor == game::NO_OWNER) {
            event.turn_count = 20;
        }
        event.status = 4;
    }

    void __cdecl Clear_Deferred_Monster_Event_Target() {
        if (deferred_monster_event_target != -1) {
            event_data[deferred_monster_event_target].planet_target = game::NO_PLANET;
            deferred_monster_event_target = -1;
        }
    }

    void __cdecl Load_Event_Picture(int16_t event_id) {
        allocate::Mark_Block(MOX::screen_seg);
        event_display_seg = (s_animation_header*)farload::Far_Reload_Next("EVENTS.LBX", event_id + 2, MOX::screen_seg);
        allocate::Release_Block(MOX::screen_seg);
    }

    void __cdecl Check_For_Event() {
        int16_t event_idx;
        int8_t event_status;

        // Text-only reports also restore this callback during event cleanup.
        MOX2::Get_Auto_Function(&jim_saved_autofunction);
        EVENTS::current_event = 0;
        for (event_idx = 0; event_idx < EVENT_COUNT; ++event_idx) {
            event_status = EVENTS::event_data[event_idx].status;
            if ((event_status == 4) || (event_status == 6) || (event_status == 5)) {
                EVENTS::Setup_Next_Event(event_idx);
            }
        }

        if (EVENTS::current_event != 0) {
            fields::Deactivate_Auto_Function();
            fields::Clear_Fields();
            file_ani::Reset_File_Animation_Buffer();
            if (MOX::settings.show_gnn_report == 1) {
                ERIC::Set_Fade_Percent(0);
                ERIC::Fast_Fade_Out();
            }
            JIM::Play_Background_Music();
            MOX::auto_turn_exit = 1;
            RUSS::Release_All_Blocks(MOX::screen_seg);
            MOX2::Put_Auto_Function(&EVENTS::jim_saved_autofunction);
        }

        MOX::auto_turn_exit = 1;
        EVENTS::Reset_Events();
    }

    void __cdecl Reset_Events() {
        if ((int32_t)MOX::stardate % 20 == 0) {
            for (int16_t i = 0; i < EVENT_COUNT; ++i) {
                if (EVENTS::event_data[i].status == 0) {
                    return;
                }
            }
            EVENTS::Init_Events();
        }
    }

    void __cdecl Setup_Next_Event(int16_t event_id) {
        int16_t event_subtype = -1;
        auto p3 = game::NO_COLONY;
        auto planet_idx = game::NO_PLANET;
        int16_t p4 = 0;
        int16_t p5 = 0;
        const auto player = event_data[event_id].player.get();
        int16_t p7 = 0;
        game::id_player p8{0};
        int16_t p9 = 0;
        game::id_player p10{0};
        int16_t p11 = 0;
        int16_t p12 = 0;
        int16_t p13 = 0;
        char text_buffer[40];

        switch (event_id) {
            case EVENT_DERELICT:
                p12 = event_data[event_id].target_id;
                event_subtype = 0;
                p13 = 0;
                if (event_data[event_id].amount != 0) {
                    event_subtype = 1;
                    p13 = event_data[event_id].amount;
                }
                break;
            case EVENT_CLIMATIC_CHANGE:
                event_subtype = 0;
                p3 = game::current_state.planets.at(event_data[event_id].planet_target.get()).colony_id;
                break;
            case EVENT_COMET:
                p3 = event_data[event_id].colony_target.get();
                planet_idx = event_data[event_id].planet_target.get();
                p4 = event_data[event_id].turn_count;
                p5 = ((event_data[event_id].max_amount - event_data[event_id].amount) * 100) / event_data[event_id].max_amount;
                if (event_data[event_id].status == 4) {
                    event_subtype = 1;
                    if (event_data[event_id].amount == 0) {
                        event_subtype = -1;
                    }
                } else if (event_data[event_id].status == 5) {
                    if (event_data[event_id].amount <= 0) {
                        event_subtype = 2;
                    } else {
                        event_subtype = 3;
                    }
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 0;
                }
                break;
            case EVENT_COMPUTER_VIRUS:
                p4 = event_data[event_id].target_id;
                p7 = event_data[event_id].amount;
                event_subtype = 0;
                break;
            case EVENT_DIPLOMATIC_BLUNDER:
                p10 = legacy_world::Decode_Player_Slot(event_data[event_id].target_id);
                event_subtype = 0;
                break;
            case EVENT_DIPLOMATIC_MARRIAGE:
                p4 = 100;
                p10 = legacy_world::Decode_Player_Slot(event_data[event_id].target_id);
                event_subtype = 0;
                break;
            case EVENT_DONATION:
                p8 = player;
                p4 = event_data[event_id].target_id;
                event_subtype = 0;
                break;
            case EVENT_EARTHQUAKE:
                p3 = event_data[event_id].colony_target.get();
                planet_idx = event_data[event_id].planet_target.get();
                p4 = event_data[event_id].amount;
                p5 = event_data[event_id].max_amount;
                event_subtype = 0;
                if (p4 < 1) {
                    event_subtype = 1;
                    p4 = p5;
                }
                break;
            case EVENT_FREAK_ACCIDENT:
                event_subtype = 0;
                if (event_data[event_id].amount != -1) {
                    event_subtype = 1;
                    p9 = event_data[event_id].amount;
                }
                break;
            case EVENT_HYPERSPACE_FLUX:
                if (event_data[event_id].status == 5) {
                    event_subtype = 2;
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 3;
                }
                break;
            case EVENT_INDUSTRIAL_ACCIDENT:
                event_subtype = 0;
                planet_idx = event_data[event_id].planet_target.get();
                p11 = game::Star_Row(game::current_state, game::current_state.planets.at(event_data[event_id].planet_target.get()).star_id);
                break;
            case EVENT_MINERAL_DEPLETION:
                p8 = player;
                event_subtype = 0;
                planet_idx = event_data[event_id].planet_target.get();
                p11 = game::Star_Row(game::current_state, game::current_state.planets.at(event_data[event_id].planet_target.get()).star_id);
                p4 = game::current_state.planets.at(event_data[event_id].planet_target.get()).mineral_class;
                break;
            case EVENT_MINERAL_DEPOSIT:
                event_subtype = 0;
                planet_idx = event_data[event_id].planet_target.get();
                p11 = game::Star_Row(game::current_state, game::current_state.planets.at(event_data[event_id].planet_target.get()).star_id);
                p4 = game::current_state.planets.at(event_data[event_id].planet_target.get()).mineral_class;
                break;
            case EVENT_MUTINY:
                break;
            case EVENT_PIRATE_ACTIVITY:
                p11 = game::Star_Row(game::current_state, event_data[event_id].star_target.get());
                if (event_data[event_id].status == 5) {
                    event_subtype = 2;
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 0;
                }
                break;
            case EVENT_PIRATE_RAID:
                p11 = game::Star_Row(game::current_state, event_data[event_id].star_target.get());
                p4 = event_data[event_id].amount;
                event_subtype = 0;
                break;
            case EVENT_PLAGUE:
                p3 = event_data[event_id].colony_target.get();
                planet_idx = event_data[event_id].planet_target.get();
                p4 = event_data[event_id].amount;
                if (event_data[event_id].status == 4) {
                    event_subtype = 1;
                } else if (event_data[event_id].status == 5) {
                    event_subtype = 2;
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 0;
                }
                break;
            case EVENT_POPULATION_BOOM:
                p3 = event_data[event_id].colony_target.get();
                planet_idx = event_data[event_id].planet_target.get();
                if (event_data[event_id].status == 5) {
                    event_subtype = 2;
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 0;
                }
                break;
            case EVENT_SERENDIPITY:
                event_subtype = 0;
                p7 = event_data[event_id].target_id;
                break;
            case EVENT_SPACE_AMOEBA:
            case EVENT_SPACE_CRYSTAL:
            case EVENT_SPACE_DRAGON:
            case EVENT_SPACE_EEL:
            case EVENT_SPACE_HYDRA:
                if (event_data[event_id].status == 4) {
                    if (event_data[event_id].planet_target.get() != game::NO_PLANET) {
                        event_subtype = 3;
                        p3 = event_data[event_id].colony_target.get();
                        planet_idx = event_data[event_id].planet_target.get();
                        p11 = game::Star_Row(game::current_state, game::current_state.planets.at(planet_idx).star_id);

                        if (MOX::game_type == 1) {
                            deferred_monster_event_target = event_id;
                        } else {
                            event_data[event_id].planet_target = game::NO_PLANET;
                        }
                    }
                } else if (event_data[event_id].status == 5) {
                    if (event_data[event_id].amount == -1) {
                        event_subtype = 2;
                    } else {
                        event_subtype = 1;
                    }
                } else if (event_data[event_id].status == 6) {
                    event_data[event_id].star_target = game::NO_STAR;
                    event_subtype = 0;
                }
                break;
            case EVENT_SUPERNOVA:
                p11 = game::Star_Row(game::current_state, event_data[event_id].star_target.get());
                p4 = event_data[event_id].turn_count;
                p5 = event_data[event_id].amount;
                if (event_data[event_id].status == 4) {
                    event_subtype = 1;
                } else if (event_data[event_id].status == 5) {
                    if (p5 <= 0) {
                        event_subtype = 2;
                    } else {
                        event_subtype = 3;
                    }
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 0;
                }
                break;
            case EVENT_TIME_SPACE_ANOMALY:
                p11 = game::Star_Row(game::current_state, event_data[event_id].star_target.get());
                if (event_data[event_id].status == 5) {
                    event_subtype = 2;
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 3;
                }
                break;
            case EVENT_WARP_BEAST:
            case EVENT_WARP_FUNNEL:
                if (event_data[event_id].status == 5) {
                    event_subtype = 2;
                } else if (event_data[event_id].status == 6) {
                    event_subtype = 3;
                }
                break;
            case EVENT_UNSTABLE_WORMHOLE:
                event_subtype = 0;
                p4 = event_data[event_id].target_id;
                p11 = event_data[event_id].amount == UNSTABLE_WORMHOLE_ANTARES &&
                    event_data[event_id].star_target.get() == game::NO_STAR
                    ? game::ANTARES_POSITION : game::Star_Row(game::current_state, event_data[event_id].star_target.get());
                break;
            case EVENT_EMPIRE_DESTROYED:
                event_subtype = (int16_t)(game_random::Random(4) - 1);
                break;
            case EVENT_EMPIRE_GROWN_IN_STRENGTH:
                event_subtype = event_data[event_id].amount;
                if (event_subtype > 3) {
                    event_subtype = 3;
                }
                p4 = event_data[event_id].target_id;
                break;
            case EVENT_EMPIRES_RANKING:
                p4 = 0;
                event_subtype = event_data[event_id].amount;
                break;
            case EVENT_ORION_VISITED:
                event_subtype = (int16_t)(game_random::Random(4) - 1);
                p8 = player;
                break;
            case EVENT_ANTARANS_DEFEATED:
                event_subtype = (int16_t)(game_random::Random(4) - 1);
                break;
            case EVENT_EMPIRE_SURRENDERS:
                p10 = legacy_world::Decode_Player_Slot(event_data[event_id].target_id);
                event_subtype = (int16_t)(game_random::Random(4) - 1);
                break;
            case EVENT_REBEL_ASSIMILATION:
                event_subtype = (int16_t)(game_random::Random(4) - 1);
                p8 = legacy_world::Decode_Player_Slot(event_data[event_id].target_id);
                break;
            default:
                break;
        }

        if (event_subtype != -1) {
            if (MOX::settings.show_gnn_report == 1) {
                if (current_event == 0) {
                    Start_Main_Event();
                    event_display_seg = 0;
                    Get_Event_Message(0, (int16_t)(game_random::Random(4) - 1), game::NO_COLONY, game::NO_PLANET, 0, 0, game::NO_PLAYER, 0, game::NO_PLAYER, 0, game::NO_PLAYER, 0, 0, 0, event_message, sizeof(event_message));
                } else {
                    event_display_seg = 0;
                    Get_Event_Message(1, (int16_t)(game_random::Random(4) - 1), game::NO_COLONY, game::NO_PLANET, 0, 0, game::NO_PLAYER, 0, game::NO_PLAYER, 0, game::NO_PLAYER, 0, 0, 0, event_message, sizeof(event_message));
                    event_status_flag = 0;
                }
                Drive_Event_Screen();
            }

            current_event++;

            if (MOX::settings.show_gnn_report == 1) {
                Load_Event_Picture(event_id);
            }

            Get_Event_Message(event_id + 2, event_subtype, p3, planet_idx, p4, p5, player, p7, p8, p9, p10, p11, p12, p13, event_message, sizeof(event_message));
            event_status_flag = 0;

            if (MOX::settings.show_gnn_report == 1) {
                Drive_Event_Screen();
            } else {
                JIM::Get_Text_Message("jimtext.lbx", 29, text_buffer, sizeof(text_buffer));
                MAINSCR::Mini_Main_Screen_Text_Box(text_buffer, event_message);
                fields::Clear_Fields();
            }
        }
    }

    void __cdecl Get_Event_Message(
        int16_t event_type,
        int16_t event_subtype,
        game::id_colony colony_id,
        game::id_planet planet_idx,
        int16_t val1,
        int16_t val2,
        game::id_player first_player,
        int16_t tech_field_idx1,
        game::id_player second_player,
        int16_t leader_idx,
        game::id_player third_player,
        int16_t star_idx,
        int16_t tech_app_idx1,
        int16_t tech_app_idx2,
        char* out_str,
        uint32_t out_str_size) {
        if (out_str_size == 0) {
            return;
        }

        out_str[0] = '\0';
        const char* event_lbx;

        switch (MOX::settings.language) {
            case 0: event_lbx = "EVENTMSE.LBX"; break;
            case 1: event_lbx = "EVENTMSG.LBX"; break;
            case 2: event_lbx = "EVENTMSF.LBX"; break;
            case 3: event_lbx = "EVENTMSS.LBX"; break;
            case 4: event_lbx = "EVENTMSI.LBX"; break;
            case 5: event_lbx = "EVENTMSP.LBX"; break;
            default: event_lbx = "EVENTMSE.LBX"; break;
        }

        char lbx_name[13];
        strlcpy(lbx_name, event_lbx, sizeof(lbx_name));

        char buffer[300];
        int16_t index = event_subtype + (event_type * 4);
        string_overrides::Load_Message(lbx_name, index, 0, sizeof(buffer), index + 1, buffer, sizeof(buffer));

        int16_t out_len = 0;
        char temp_str[300];

        auto append_text = [&](const char* text) {
            if (text != nullptr) {
                strlcat(out_str, text, out_str_size);
                out_len = (int16_t)strlen(out_str);
            }
        };

        auto append_char = [&](char c) {
            if ((uint32_t)(out_len + 1) < out_str_size) {
                out_str[out_len++] = c;
                out_str[out_len] = '\0';
            }
        };

        for (int16_t i = 0; i < (int16_t)sizeof(buffer); ++i) {
            char c = buffer[i];
            if (c == '\0') {
                break;
            }

            const char* append_str = nullptr;
            auto fetch_player = game::NO_PLAYER;

            if (c == '\x80') {
                auto name_planet_idx = planet_idx;
                if (name_planet_idx == game::NO_PLANET && game::current_state.colonies.contains(colony_id)) {
                    name_planet_idx = game::current_state.colonies.at(colony_id).planet_id;
                }
                if (game::current_state.planets.contains(name_planet_idx)) {
                    append_str = HACCESS::Get_Planet_Name(game::current_state, name_planet_idx);
                }
            } else if (c == '\x81') {
                snprintf(temp_str, sizeof(temp_str), "%i", val1);
                append_str = temp_str;
            } else if (c == '\x82') {
                snprintf(temp_str, sizeof(temp_str), "%i", val2);
                append_str = temp_str;
            } else if (c == '\x83') {
                if ((val1 > 1 || val1 == 0) && MOX::settings.language == 0) {
                    append_str = "s";
                } else {
                    continue;
                }
            } else if (c == '\x84') {
                if ((val2 > 1 || val2 == 0) && MOX::settings.language == 0) {
                    append_str = "s";
                } else {
                    continue;
                }
            } else if (c == '\x85') {
                fetch_player = first_player;
            } else if (c == '\x86') {
                append_str = TECHDATA::technology_fields[tech_field_idx1].name;
            } else if (c == '\x87') {
                append_str = TECHDATA::technology_fields[val1].name;
            } else if (c == '\x88') {
                if (second_player.id < MAX_PLAYERS) append_str = game::current_state.players[second_player.id].name;
            } else if (c == '\x89') {
                const auto& state = game::current_state;
                append_str = state.leaders.at(leader_idx).name;
            } else if (c == '\x8A') {
                fetch_player = third_player;
            } else if (c == '\x8B') {
                append_str = star_idx == game::ANTARES_POSITION ? "Antares"
                    : game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).name;
            } else if (c == '\x8C') {
                append_str = MOX::mineral_class_string[val1];
            } else if (c == '\x8D') {
                append_str = TECHDATA::technology_applications[tech_app_idx1].name;
            } else if (c == '\x8E') {
                append_str = TECHDATA::technology_applications[tech_app_idx2].name;
            } else if (c == '\x8F') {
                if (MOX::local_player != first_player) {
                    JIM::Get_Text_Message("jimtext2.lbx", 0x3d, temp_str, 30);
                    append_str = temp_str;
                } else {
                    continue;
                }
            } else if (c == '\x90') {
                if (first_player != game::NO_PLAYER && MOX::local_player == first_player) {
                    JIM::Get_Text_Message("jimtext2.lbx", 0x3e, temp_str, 30);
                    append_str = temp_str;
                } else if (first_player != game::NO_PLAYER) {
                    fetch_player = first_player;
                } else {
                    continue;
                }
            } else if (c == '\x91') {
                if (MOX::local_player != third_player) {
                    JIM::Get_Text_Message("jimtext2.lbx", 0x3d, temp_str, 30);
                    append_str = temp_str;
                } else {
                    continue;
                }
            } else if (c == '\x92') {
                if (third_player != game::NO_PLAYER && MOX::local_player == third_player) {
                    JIM::Get_Text_Message("jimtext2.lbx", 0x3e, temp_str, 30);
                    append_str = temp_str;
                } else if (third_player != game::NO_PLAYER) {
                    fetch_player = third_player;
                } else {
                    continue;
                }
            } else {
                append_char(c);
                continue;
            }

            if (fetch_player.id < MAX_PLAYERS) {
                append_str = game::current_state.players[fetch_player.id].race_name;
            }

            const int16_t old_out_len = out_len;
            append_text(append_str);

            if (old_out_len == 0 && out_len > 0) {
                if (c == '\x8F') {
                    out_str[1] -= 0x20;
                } else if (c == '\x90') {
                    out_str[0] -= 0x20;
                }
            }
        }
        out_str[out_len] = '\0';

        if (event_type == 0x21) {
            strlcat(out_str, string::s_Newline, out_str_size);

            int32_t bldg_costs[MAX_PLAYERS];
            if (event_subtype == 0) {
                COLCALC::Compute_Total_Building_Costs(bldg_costs);
            }

            int16_t player_indices[MAX_PLAYERS];
            int32_t player_values[MAX_PLAYERS];
            int16_t num_active = 0;

            for (int16_t p = 0; p < game::current_state.player_count; ++p) {
                if (game::current_state.players[p].eliminated == 0) {
                    player_indices[num_active] = p;
                    if (event_subtype == 0) {
                        player_values[num_active] = bldg_costs[p];
                    } else if (event_subtype == 1) {
                        player_values[num_active] = game::current_state.players[p].total_pop;
                    } else if (event_subtype == 2) {
                        player_values[num_active] = game::current_state.players[p].total_known_tech_cost;
                    } else if (event_subtype == 3) {
                        player_values[num_active] = Event_Get_Fleet_Strength(game::current_state.players[p].id);
                    }
                    num_active++;
                }
            }

            if (num_active > 0) {
                int16_t passes = num_active;
                do {
                    for (int16_t j = 0; j < num_active - 1; ++j) {
                        if (player_values[j + 1] > player_values[j]) {
                            strings::Swap(&player_values[j], &player_values[j + 1]);
                            strings::Swap_Short(&player_indices[j], &player_indices[j + 1]);
                        }
                    }
                    passes--;
                } while (passes != 0);
            }

            /* fmtpara: \x1A0 switches the appended ranking block to left alignment. */
            strlcat(out_str, "\x1A"
                             "0",
                out_str_size);

            int16_t spacing = (MOX::settings.show_gnn_report == 1) ? 0x78 : 0x0A;
            int16_t initial_spacing = spacing;
            int16_t count = (num_active > 6) ? 6 : num_active;

            for (int16_t k = 0; k < count; ++k) {
                /* fmtpara: \aX repositions the text cursor before each ranked entry. */
                snprintf(temp_str, sizeof(temp_str), "\x07"
                                                     "X%d.%d. %s",
                    spacing,
                    k + 1,
                    game::current_state.players[player_indices[k]].race_name);
                strlcat(out_str, temp_str, out_str_size);

                if (((k + 1) % 3) == 0) {
                    strlcat(out_str, string::s_Newline, out_str_size);
                    spacing = initial_spacing;
                } else {
                    spacing += 0x7D;
                }
            }
        }
    }

    void __cdecl Drive_Event_Screen() {
        bool leave_screen = false;

        while (true) {
            timer::Mark_Time();
            fields::Clear_Fields();

            int16_t bg_field_id = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
            int16_t input_id = fields::Get_Input();

            if (input_id == bg_field_id) {
                leave_screen = true;
            }

            if (leave_screen) {
                break;
            }

            Draw_Event_Screen();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(2);
        }
    }

    int32_t __cdecl Event_Get_Fleet_Strength(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int32_t strength = 0;

        auto& state = game::current_state;
        ship_slots::Visit_Records(state, [&](const s_ship_data& ship) {
            if (factions::Legacy_Code(ship.owner) == player_idx && ship.status <= 1) strength += ship.d.size + 1;
        });

        return strength;
    }

    void __cdecl Start_Main_Event() {
        fields::Deactivate_Help_List();
        fields::Enable_Draw_All_Fields();
        graphics::Reset_Window();
        video::Clip_Off();
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        mouse::Set_Mouse_List(&event_blank_mouse, 1);
        JIM::Play_Streaming_Music(0x76, -1, 1);
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
        fields::Set_Input_Delay(1);
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();
        fields::Deactivate_Auto_Function();
        Event_Fade_In();
        fields::Assign_Auto_Function(Draw_Event_Screen, 2);
        fonts::Set_Outline_Color(0, 0xF8);
        fonts::Set_Font_Style_Outline(4, &event_normal_colors);
    }

    void __cdecl Draw_Event_Screen() {
        video::Set_Page_Off();

        if (MOX::settings.animations_on == 1) {
            file_ani::Draw_File_Animation_Stencil();
            if (file_ani::Get_File_Animation_Frame() == 0) {
                file_ani::Set_File_Animation_Frame(1);
            }
        }

        if (event_status_flag == 1) {
            if (event_display_seg != nullptr) {
                animate::Draw(0x140, 0xE, event_display_seg);
            }

            int16_t height = FMTPARA::Get_Formatted_Paragraph_Max_Height(0x226, event_message);
            if (height > 0x3C) {
                height += 0xE;
            } else if (height > 0x12) {
                height -= 0x4;
            }

            int16_t y = 0x1BD - (height / 2);
            FMTPARA::Print_Formatted_Paragraph(0x37, y, 0x226, event_message, 2);
        } else {
            event_status_flag = 1;
            if (event_display_seg != nullptr) {
                animate::Draw_Palette(event_display_seg);
            }
        }

        JIM::Play_Streaming_Music(0x76, -1, 1);
    }

    void __cdecl Event_Fade_In() {
        event_mouse_bitmap = (uint8_t*)allocate::Allocate_First_Block(MOX::screen_seg, 917 /* 0x395 */);
        s_animation_header* h2 = (s_animation_header*)farload::Far_Reload_Next("EVENTS.LBX", 0, MOX::screen_seg);

        bitmap::Clear_Mouse_Image(0);
        bitmap::Create_Blank_Bitmap(30, 30, event_mouse_bitmap);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)event_mouse_bitmap, h2);

        event_status_flag = 1;

        uint8_t* buffer = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, 320000);
        file_ani::Set_File_Animation_Buffer(buffer);

        video::Set_Page_Off();
        animate::Draw_Palette(h2);
        palette::Set_Palette_Changes(0, 255);
        graphics::Fill(0, 0, 639, 479, 0);

        file_ani::Open_File_Animation("EVENTS.LBX", 1);
        video::Set_Page_Off();
        file_ani::Draw_File_Animation();
        video::Copy_Off_To_Back();

        bitmap::Draw_Bitmap_To_Mouse(0, 0, (s_animation_header*)event_mouse_bitmap, 0);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
    }
} // namespace EVENTS
