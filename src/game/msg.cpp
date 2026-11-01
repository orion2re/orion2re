#include "pch.h"
#include "game_events.h"

namespace MSG_ {
    namespace {
        constexpr int16_t DIPLOMACY_MSG_WAR = 0;
        constexpr int16_t DIPLOMACY_MSG_PEACE = 1;
        constexpr int16_t DIPLOMACY_MSG_ALLIANCE = 2;
        constexpr int16_t DIPLOMACY_MSG_NONAGGRESSION = 3;
        constexpr int16_t DIPLOMACY_MSG_PEACE_ENDED = 4;
    } // namespace

    uint8_t first_msg[MAX_PLAYERS];
    uint8_t n_msgs[MAX_PLAYERS];
    s_MSG msgs[MAX_MESSAGES];
    uint8_t max_msgs[MAX_PLAYERS];

    char* msg_artifacts_fmt;
    char artifacts_and[20];
    char* msg_colony_your_rebellion_lost_fmt;
    char* msg_colony_starvation_food_fmt;
    char* msg_colony_1_rebel_won_fmt;
    char* msg_colony_assimilation_fmt;
    char* msg_scrap_leader_fmt;
    char* msg_colony_genocide_fmt;
    char* msg_hyperspace_beast_hungry_fmt;
    char* msg_lost_ship_fmt;
    char* s_and_s_declare_war_fmt;
    char* msg_colony_built_freighter_fmt;
    char* msg_lost_attacking_spy_fmt;
    char* msg_1_ship_killed_retreating_fmt;
    char* msg_colony_built_next_queue_fmt;
    char* lost_colony_reason_strings[4];
    char* msg_lost_defending_spies_fmt;
    char* s_and_s_declare_alliance_fmt;
    char* msg_colony_d_rebels_won_fmt;
    char* msg_hyperspace_flux_fmt;
    char* msg_financial_fmt;
    char* msg_lost_colony_leader_fmt;
    char* msg_colony_built_android_farmer_fmt;
    char* native_msg;
    char* msg_scrap_ship_hyperspace_fmt;
    char* msg_offensive_spy_fmt;
    char* msg_colony_built_android_scientist_fmt;
    char* msg_lost_defending_spy_fmt;
    char* msg_april1_fmt;
    char* msg_lost_colony_fmt;
    char* msg_colony_boom_fmt;
    char* msg_defensive_spy_fmt;
    char* msg_eel_vanished_fmt;
    char* msg_colony_built_ship_fmt;
    char* leader_fate_strings[3];
    char* msg_colony_built_auto_build_fmt;
    char* msg_colony_android_killed_organic_fmt;
    char* msg_colony_settlers_arrived_blockaded_fmt;
    char* msg_colony_comet_fmt;
    char* msg_colony_d_settlers_arrived_fmt;
    char* msg_colony_starvation_ind_fmt;
    char* msg_d_ships_killed_retreating_fmt;
    char* msg_failed_retreat_leader_fmt;
    char* msg_colony_or_outpost[2];
    char* msg_colony_1_settler_arrived_over_limit_fmt;
    char* msg_spy_assassinated_fmt;
    char* msg_spy_assassinated_victim_fmt;
    char* msg_colony_no_room_for_android_fmt;
    char* failed_retreat_reason_strings[3];
    char* msg_eel_spawned_fmt;
    char* msg_scrap_ship_bc_fmt;
    char* msg_colony_d_rebels_lost_fmt;
    char* msg_colony_scrap_building_fmt;
    char* msg_hyperspace_beast_ate_other_ship_fmt;
    char* msg_colony_built_android_worker_fmt;
    char* msg_colony_built_refit_fmt;
    char* msg_artemis_leader_fmt;
    char* msg_colony_your_rebellion_won_fmt;
    char* msg_colony_plague_fmt;
    char* msg_anomaly_fmt;
    char* msg_diplomacy_strings[5];
    char* msg_colony_starvation_both_fmt;
    char* msg_colony_built_spy_fmt;
    char* msg_colony_built_building_fmt;
    char* msg_artemis_fmt;
    char* msg_colony_1_rebel_lost_fmt;
    char* msg_hyperspace_beast_ate_your_ship_fmt;
    char* msg_colony_1_settler_arrived_fmt;
    char* msg_colony_d_settlers_arrived_over_limit_fmt;

    int16_t __cdecl N_Msgs_For_Player(int16_t player_idx) {
        return (int16_t)n_msgs[player_idx];
    }

    void __cdecl Add_Msg(s_MSG msg, int16_t plr_idx) {
        if (plr_idx < 0 || plr_idx >= MAX_PLAYERS) {
            return;
        }

        const int16_t first_msg = MSG_::first_msg[plr_idx];
        if (first_msg < 0 || first_msg >= MAX_MESSAGES) {
            return;
        }

        int16_t capacity = max_msgs[plr_idx];
        const int16_t remaining_capacity = MAX_MESSAGES - first_msg;
        if (capacity > remaining_capacity) {
            capacity = remaining_capacity;
        }
        if (capacity <= 0) {
            return;
        }

        if (MSG_QUEUE::Insertion_Index(&msgs[first_msg], capacity, n_msgs[plr_idx], msg.priority) >= 0) game_events::Publish_Messages_Replacing(plr_idx);
        MSG_QUEUE::Add(&msgs[first_msg], capacity, &n_msgs[plr_idx], msg);
    }

    int16_t __cdecl First_Msg_For_Player(int16_t player_idx) {
        return (int16_t)first_msg[player_idx];
    }

    /** Calculates and allocates message pool slices for all active players. */
    void __cdecl Allocate_Msg_Slots() {
        for (int16_t player = 0; player < MAX_PLAYERS; ++player) {
            if (n_msgs[player] != 0) game_events::Publish_Messages_Replacing(player);
        }
        int active_players = 0;

        // First pass: Count active players
        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            // s_player structure size is 0xea9 (3753 bytes)
            if (MOX::player[i].eliminated == 0) {
                active_players++;
            }
        }

        // Reset message tracking arrays for all potential players
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            first_msg[i] = 0;
            n_msgs[i] = 0;
            max_msgs[i] = 0;
        }

        // Second pass: Distribute message slots among active players
        if (MOX::NUM_PLAYERS > 0 && active_players > 0) {
            int cumulative_pool = 0;
            for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                if (MOX::player[i].eliminated == 0) {
                    uint8_t start_slot = (uint8_t)(cumulative_pool / active_players);
                    cumulative_pool += MAX_MESSAGES;
                    uint8_t end_slot = (uint8_t)(cumulative_pool / active_players);

                    first_msg[i] = start_slot;
                    max_msgs[i] = end_slot - start_slot;
                }
            }
        }
    }

    void __cdecl sprintf_msg(char* out_text, uint32_t out_text_size, s_MSG* msg) {
        Copy_Message_Text(out_text, out_text_size, msg, MOX::PLAYER_NUM);
    }

    void Copy_Message_Text(char* out_text, uint32_t out_text_size, const s_MSG* msg, int16_t viewer) {
        char append_text[200];
        char details_text[200];

        if (out_text == nullptr || out_text_size == 0) {
            return;
        }

        out_text[0] = '\0';
        if (msg == nullptr || (msg->msg_type == MSG_TYPE_COLONY && (msg->u.colony.subtype < 0 || msg->u.colony.subtype >= MSG_COLONY_SUBTYPE_COUNT))) return;

        switch (msg->msg_type) {
            case MSG_TYPE_COLONY: {
                const auto& colony_msg = msg->u.colony;
                const int16_t planet_idx = colony_msg.planet_idx;
                char planet_name[31];
                HACCESS::Do_Get_Planet_Name(planet_idx, planet_name, 1);

                switch (colony_msg.subtype) {
                    case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED: {
                        const auto& scrap_msg = colony_msg.c.scrap;
                        snprintf(out_text, out_text_size, msg_colony_scrap_building_fmt, TECHDATA::buildings[scrap_msg.building_id].name, planet_name, (int)scrap_msg.bc);
                        return;
                    }
                    case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED: {
                        const auto& build_msg = colony_msg.c.build;
                        int16_t build_id = build_msg.build_id;
                        if (build_id < BUILDING_ALIEN_CONTROL_CENTER || build_id >= BUILDING_COUNT) {
                            if (COLBLDG::Colony_Production_Is_Queued_Ship(build_id)) {
                                const char* build_fmt = msg_colony_built_ship_fmt;
                                if (build_msg.is_refit != 0) {
                                    build_fmt = msg_colony_built_refit_fmt;
                                }
                                snprintf(out_text, out_text_size, build_fmt, planet_name, &MOX::ship[COLBLDG::Colony_Production_Ship_Index(build_id)]);
                            } else {
                                switch (build_id) {
                                    case COLONY_PRODUCTION_FREIGHTERS:
                                        snprintf(out_text, out_text_size, msg_colony_built_freighter_fmt, planet_name);
                                        break;
                                    case COLONY_PRODUCTION_SPY:
                                        snprintf(out_text, out_text_size, msg_colony_built_spy_fmt, planet_name);
                                        break;
                                    case COLONY_PRODUCTION_SCIENTIST:
                                        snprintf(out_text, out_text_size, msg_colony_built_android_scientist_fmt, planet_name);
                                        break;
                                    case COLONY_PRODUCTION_WORKER:
                                        snprintf(out_text, out_text_size, msg_colony_built_android_worker_fmt, planet_name);
                                        break;
                                    case COLONY_PRODUCTION_FARMER:
                                        snprintf(out_text, out_text_size, msg_colony_built_android_farmer_fmt, planet_name);
                                        break;
                                    default:
                                        break;
                                }
                            }
                        } else {
                            snprintf(out_text, out_text_size, msg_colony_built_building_fmt, planet_name, TECHDATA::buildings[build_id].name);
                        }

                        int16_t product_id = build_msg.product_id;
                        if (product_id != COLONY_PRODUCTION_NONE) {
                            const char* queue_fmt = msg_colony_built_next_queue_fmt;
                            if (build_msg.append_fmt != 0) {
                                queue_fmt = msg_colony_built_auto_build_fmt;
                            }
                            char product_name[800];
                            if (product_id == COLONY_PRODUCTION_AI_TRADE_GOODS)
                                strlcpy(product_name, "AI trade goods", sizeof(product_name));
                            else
                                COLBLDG::Copy_Selection_Name(product_id, product_name, sizeof(product_name), viewer);
                            snprintf(details_text, sizeof(details_text), queue_fmt, product_name);
                            strlcat(out_text, details_text, out_text_size);
                            return;
                        }
                        break;
                    }
                    case MSG_COLONY_SUBTYPE_GENOCIDE: {
                        uint32_t race_idx = (uint32_t)colony_msg.c.assimilation.val;
                        const char* victim_race_name = native_msg;
                        if ((int)race_idx < (int)MOX::NUM_PLAYERS) {
                            victim_race_name = MOX::player[race_idx].race_name;
                        }
                        snprintf(out_text, out_text_size, msg_colony_genocide_fmt, victim_race_name, planet_name);
                        return;
                    }
                    case MSG_COLONY_SUBTYPE_ASSIMILATION:
                        snprintf(out_text, out_text_size, msg_colony_assimilation_fmt, MOX::player[colony_msg.c.assimilation.val].race_name, planet_name);
                        return;
                    case MSG_COLONY_SUBTYPE_STARVATION: {
                        uint8_t food_loss = colony_msg.c.starvation.a;
                        if (food_loss == 0) {
                            snprintf(out_text, out_text_size, msg_colony_starvation_ind_fmt, planet_name, (uint32_t)(uint8_t)colony_msg.c.starvation.b);
                            return;
                        }
                        uint8_t industry_loss = colony_msg.c.starvation.b;
                        if (industry_loss != 0) {
                            snprintf(out_text, out_text_size, msg_colony_starvation_both_fmt, planet_name, (uint32_t)food_loss, (uint32_t)industry_loss);
                            return;
                        }
                        snprintf(out_text, out_text_size, msg_colony_starvation_food_fmt, planet_name, (uint32_t)food_loss);
                        return;
                    }
                    case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED: {
                        const auto& settlers_msg = colony_msg.c.settlers;
                        uint8_t settler_count = settlers_msg.settlers;
                        const char* settlers_fmt = msg_colony_d_settlers_arrived_fmt;
                        if (settler_count == 1) {
                            settlers_fmt = msg_colony_1_settler_arrived_fmt;
                        }
                        snprintf(out_text, out_text_size, settlers_fmt, (uint32_t)settler_count, planet_name);
                        if (settlers_msg.blockaded != 0) {
                            strlcat(out_text, msg_colony_settlers_arrived_blockaded_fmt, out_text_size);
                            return;
                        }
                        uint8_t over_limit = settlers_msg.over_limit;
                        if (over_limit != 0) {
                            const char* over_limit_fmt = msg_colony_d_settlers_arrived_over_limit_fmt;
                            if (over_limit == 1) {
                                over_limit_fmt = msg_colony_1_settler_arrived_over_limit_fmt;
                            }
                            snprintf(append_text, sizeof(append_text), over_limit_fmt, (uint32_t)over_limit);
                            strlcat(out_text, append_text, out_text_size);
                        }
                        return;
                    }
                    case MSG_COLONY_SUBTYPE_PLAGUE:
                        snprintf(out_text, out_text_size, msg_colony_plague_fmt, planet_name);
                        return;
                    case MSG_COLONY_SUBTYPE_POPULATION_BOOM:
                        snprintf(out_text, out_text_size, msg_colony_boom_fmt, planet_name);
                        return;
                    case MSG_COLONY_SUBTYPE_COMET:
                        snprintf(out_text, out_text_size, msg_colony_comet_fmt, planet_name);
                        return;
                    case MSG_COLONY_SUBTYPE_REBELLION: {
                        const auto& rebels_msg = colony_msg.c.rebels;
                        uint32_t owner_player_idx = (uint32_t)(uint8_t)rebels_msg.owner_player;
                        uint8_t rebel_count = rebels_msg.rebels;
                        if ((int)viewer != (int)owner_player_idx) {
                            if ((uint8_t)rebels_msg.lost < rebel_count) {
                                const char* rebellion_fmt = msg_colony_d_rebels_won_fmt;
                                if (rebel_count == 1) {
                                    rebellion_fmt = msg_colony_1_rebel_won_fmt;
                                }
                                snprintf(out_text, out_text_size, rebellion_fmt, (uint32_t)rebel_count, MOX::player[owner_player_idx].race_name, planet_name, (uint32_t)(uint8_t)rebels_msg.lost);
                                return;
                            }
                            const char* rebellion_fmt = msg_colony_d_rebels_lost_fmt;
                            if (rebel_count == 1) {
                                rebellion_fmt = msg_colony_1_rebel_lost_fmt;
                            }
                            snprintf(out_text, out_text_size, rebellion_fmt, (uint32_t)rebel_count, MOX::player[owner_player_idx].race_name, planet_name);
                            return;
                        }
                        uint8_t lost_rebels = rebels_msg.lost;
                        if (lost_rebels < rebel_count) {
                            snprintf(out_text, out_text_size, msg_colony_your_rebellion_won_fmt, (uint32_t)rebel_count, planet_name, (uint32_t)lost_rebels);
                            return;
                        }
                        snprintf(out_text, out_text_size, msg_colony_your_rebellion_lost_fmt, (uint32_t)rebel_count, planet_name);
                        return;
                    }
                    case MSG_COLONY_SUBTYPE_NO_ROOM_FOR_ANDROID:
                        snprintf(out_text, out_text_size, msg_colony_no_room_for_android_fmt, planet_name);
                        return;
                    case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC: {
                        uint8_t race_idx = colony_msg.c.android_killed.race_idx;
                        const char* victim_race_name = native_msg;
                        if (race_idx < MAX_PLAYERS) {
                            victim_race_name = MOX::player[race_idx].race_name;
                        }
                        snprintf(out_text, out_text_size, msg_colony_android_killed_organic_fmt, planet_name, victim_race_name);
                        return;
                    }
                    default: break;
                }
                break;
            }
            case MSG_TYPE_DECLARE_ALLIANCE:
                snprintf(out_text, out_text_size, s_and_s_declare_alliance_fmt, MOX::player[msg->u.players.player_a_idx].race_name, MOX::player[msg->u.players.player_b_idx].race_name);
                return;
            case MSG_TYPE_DECLARE_WAR:
                snprintf(out_text, out_text_size, s_and_s_declare_war_fmt, MOX::player[msg->u.players.player_a_idx].race_name, MOX::player[msg->u.players.player_b_idx].race_name);
                return;
            case MSG_TYPE_FINANCIAL:
                snprintf(out_text, out_text_size, msg_financial_fmt, (int)msg->u.financial.a, (int)msg->u.financial.b);
                return;
            case MSG_TYPE_LOST_SHIP: {
                const auto& lost_ship_msg = msg->u.lost_ship;
                s_ship_data* ship = &MOX::ship[lost_ship_msg.ship_idx];
                snprintf(out_text, out_text_size, msg_lost_ship_fmt, TECHDATA::hull_data[ship->d.size].name, ship, (int)ship->d.cost, &MOX::star[lost_ship_msg.star_idx]);
                uint8_t leader_idx = lost_ship_msg.leader_idx;
                if (leader_idx != 0xFF) {
                    snprintf(append_text, sizeof(append_text), leader_fate_strings[lost_ship_msg.fate], &MOX::leaders[leader_idx]);
                    strlcat(out_text, append_text, out_text_size);
                    return;
                }
                break;
            }
            case MSG_TYPE_SCRAP_SHIP: {
                const auto& scrap_ship_msg = msg->u.scrap_ship;
                int16_t location_idx = scrap_ship_msg.where;
                s_ship_data* ship = &MOX::ship[scrap_ship_msg.ship_idx];
                if (location_idx < MOX::NUM_STARS) {
                    snprintf(out_text, out_text_size, msg_scrap_ship_bc_fmt, TECHDATA::hull_data[ship->d.size].name, ship, (int)ship->d.cost, &MOX::star[location_idx], (int)scrap_ship_msg.bc);
                    return;
                }
                snprintf(out_text, out_text_size, msg_scrap_ship_hyperspace_fmt, TECHDATA::hull_data[ship->d.size].name, ship, (int)ship->d.cost);
                return;
            }
            case MSG_TYPE_LOST_COLONY: {
                const auto& lost_colony_msg = msg->u.lost_colony;
                const char* reason_text = lost_colony_reason_strings[lost_colony_msg.reason];
                char planet_name[31];
                HACCESS::Do_Get_Planet_Name(lost_colony_msg.planet_idx, planet_name, 1);
                snprintf(out_text, out_text_size, msg_lost_colony_fmt, msg_colony_or_outpost[(uint8_t)lost_colony_msg.colony_or_outpost], planet_name, reason_text);
                int16_t leader_idx = lost_colony_msg.leader_idx;
                if (leader_idx != -1) {
                    snprintf(append_text, sizeof(append_text), msg_lost_colony_leader_fmt, &MOX::leaders[leader_idx]);
                    strlcat(out_text, append_text, out_text_size);
                    return;
                }
                break;
            }
            case MSG_TYPE_HYPERSPACE_FLUX:
                strlcpy(out_text, msg_hyperspace_flux_fmt, out_text_size);
                return;
            case MSG_TYPE_HYPERSPACE_BEAST: {
                const auto& beast_msg = msg->u.hyperspace_beast;
                int16_t ship_idx = beast_msg.ship_idx;
                int16_t star_idx = beast_msg.star_idx;
                if (ship_idx == -1) {
                    strlcpy(out_text, msg_hyperspace_beast_hungry_fmt, out_text_size);
                    return;
                }
                if (beast_msg.fate == 0) {
                    snprintf(out_text, out_text_size, msg_hyperspace_beast_ate_other_ship_fmt, MOX::player[MOX::ship[ship_idx].owner].race_name);
                    return;
                }
                snprintf(out_text, out_text_size, msg_hyperspace_beast_ate_your_ship_fmt, &MOX::ship[ship_idx], &MOX::star[star_idx]);
                uint8_t officer_idx = beast_msg.officer_idx;
                if (officer_idx != 0xFF) {
                    snprintf(append_text, sizeof(append_text), leader_fate_strings[(uint8_t)beast_msg.leader_status], &MOX::leaders[officer_idx]);
                    strlcat(out_text, append_text, out_text_size);
                    return;
                }
                break;
            }
            case MSG_TYPE_LOST_ATTACKING_SPY:
                snprintf(out_text, out_text_size, msg_lost_attacking_spy_fmt, MOX::player[msg->u.lost_attacking_spy.player_idx].race_name);
                return;
            case MSG_TYPE_LOST_DEFENDING_SPY: {
                int16_t spy_count = msg->u.lost_defending_spy.count;
                if (spy_count != 1) {
                    snprintf(out_text, out_text_size, msg_lost_defending_spies_fmt, (int)spy_count);
                    return;
                }
                strlcpy(out_text, msg_lost_defending_spy_fmt, out_text_size);
                return;
            }
            case MSG_TYPE_ARTEMIS: {
                int16_t star_idx = msg->u.artemis.star_idx;
                snprintf(out_text, out_text_size, msg_artemis_fmt, &MOX::star[star_idx]);
                return;
            }
            case MSG_TYPE_ARTEMIS_LEADER: {
                const auto& artemis_leader_msg = msg->u.artemis_leader;
                int16_t star_idx = artemis_leader_msg.star_idx;
                snprintf(out_text, out_text_size, msg_artemis_leader_fmt, &MOX::star[star_idx]);
                snprintf(append_text, sizeof(append_text), leader_fate_strings[artemis_leader_msg.fate], &MOX::leaders[artemis_leader_msg.leader_idx]);
                strlcat(out_text, append_text, out_text_size);
                return;
            }
            case MSG_TYPE_FAILED_RETREAT: {
                const auto& retreat_msg = msg->u.retreat;
                int16_t star_idx = retreat_msg.star_idx;
                uint16_t ships_lost = retreat_msg.ships_lost;
                const char* retreat_fmt = msg_d_ships_killed_retreating_fmt;
                if (ships_lost == 1) {
                    retreat_fmt = msg_1_ship_killed_retreating_fmt;
                }
                snprintf(out_text, out_text_size, retreat_fmt, (uint32_t)ships_lost, &MOX::star[star_idx]);
                snprintf(details_text, sizeof(details_text), "  %s", failed_retreat_reason_strings[(uint8_t)retreat_msg.reason]);
                strlcat(out_text, details_text, out_text_size);
                uint8_t leader_count = retreat_msg.leader_count;
                if (leader_count != 0) {
                    snprintf(append_text, sizeof(append_text), msg_failed_retreat_leader_fmt, (uint32_t)leader_count);
                    strlcat(out_text, append_text, out_text_size);
                    return;
                }
                break;
            }
            case MSG_TYPE_SPY_ASSASSINATION: {
                const auto& assassination_msg = msg->u.spy_assasination;
                uint8_t defending_player_idx = assassination_msg.plr_b;
                uint8_t attacking_player_idx = assassination_msg.plr_a;
                if (attacking_player_idx == defending_player_idx) {
                    strlcpy(details_text, msg_defensive_spy_fmt, sizeof(details_text));
                } else {
                    snprintf(details_text, sizeof(details_text), msg_offensive_spy_fmt, MOX::player[defending_player_idx].race_name);
                }
                if (assassination_msg.victim_id != viewer) {
                    snprintf(out_text, out_text_size, msg_spy_assassinated_fmt, details_text);
                    return;
                }
                if (MOX::settings.language == 4) {
                    snprintf(out_text, out_text_size, msg_spy_assassinated_victim_fmt, &MOX::leaders[(uint8_t)assassination_msg.leader_id], details_text, MOX::player[attacking_player_idx].race_name);
                    return;
                }
                snprintf(out_text, out_text_size, msg_spy_assassinated_victim_fmt, &MOX::leaders[(uint8_t)assassination_msg.leader_id], MOX::player[attacking_player_idx].race_name, details_text);
                return;
            }
            case MSG_TYPE_ANOMALY: {
                int16_t star_idx = msg->u.anomaly.star_idx;
                snprintf(out_text, out_text_size, msg_anomaly_fmt, &MOX::star[star_idx]);
                return;
            }
            case MSG_TYPE_DIPLOMACY:
                snprintf(out_text, out_text_size, msg_diplomacy_strings[(uint8_t)msg->u.diplomacy.diplomacy_idx], MOX::player[(uint8_t)msg->u.diplomacy.plr_a].race_name, MOX::player[(uint8_t)msg->u.diplomacy.plr_b].race_name);
                return;
            case MSG_TYPE_EEL_SPAWNED: {
                int16_t star_idx = msg->u.eel.star_idx;
                snprintf(out_text, out_text_size, msg_eel_spawned_fmt, &MOX::star[star_idx]);
                return;
            }
            case MSG_TYPE_EEL_VANISHED: {
                int16_t star_idx = msg->u.eel.star_idx;
                snprintf(out_text, out_text_size, msg_eel_vanished_fmt, &MOX::star[star_idx]);
                return;
            }
            case MSG_TYPE_ARTIFACTS: {
                const auto& artifacts_msg = msg->u.artifacts;
                int16_t star_idx = artifacts_msg.star_idx;
                if (artifacts_msg.tech2 == '\0') {
                    snprintf(details_text, sizeof(details_text), MOX::s_fmt_s, TECHDATA::technology_applications[(uint8_t)artifacts_msg.tech1].name);
                } else {
                    snprintf(details_text, sizeof(details_text), "%s %s %s", TECHDATA::technology_applications[(uint8_t)artifacts_msg.tech1].name, artifacts_and, TECHDATA::technology_applications[(uint8_t)artifacts_msg.tech2].name);
                }
                snprintf(out_text, out_text_size, msg_artifacts_fmt, details_text, &MOX::star[star_idx]);
                return;
            }
            case MSG_TYPE_APRIL1:
                strlcpy(out_text, msg_april1_fmt, out_text_size);
                return;
            case MSG_TYPE_SCRAP_LEADER:
                snprintf(out_text, out_text_size, msg_scrap_leader_fmt, &MOX::leaders[(uint8_t)msg->u.scrap_leader.leader_idx]);
                return;
            default:
                break;
        }
        return;
    }

    void __cdecl Add_Ongoing_Event_Msgs() {
        s_MSG msg;

        if (EVENTS::Event_Check_Hyperspace_Flux() != 0) {
            memset(&msg, 0, sizeof(msg));
            msg.msg_type = MSG_TYPE_HYPERSPACE_FLUX;
            msg.priority = 0;

            for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                Add_Msg(msg, player_idx);
            }
        }

        for (int16_t colony_idx = 0; colony_idx < MOX::NUM_COLONIES; ++colony_idx) {
            int8_t owner = MOX::colony[colony_idx].owner;
            if (owner >= 0) {
                if (EVENTS::Event_Check_Plague(colony_idx) != 0) {
                    memset(&msg, 0, sizeof(msg));
                    msg.msg_type = MSG_TYPE_COLONY;
                    msg.u.colony.planet_idx = MOX::colony[colony_idx].planet;
                    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_PLAGUE;
                    Add_Msg(msg, owner);
                }

                if (EVENTS::Event_Check_Population_Boom(colony_idx) != 0) {
                    memset(&msg, 0, sizeof(msg));
                    msg.msg_type = MSG_TYPE_COLONY;
                    msg.u.colony.planet_idx = MOX::colony[colony_idx].planet;
                    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_POPULATION_BOOM;
                    Add_Msg(msg, owner);
                }

                if (EVENTS::Event_Check_Comet(colony_idx) != 0) {
                    memset(&msg, 0, sizeof(msg));
                    msg.msg_type = MSG_TYPE_COLONY;
                    msg.u.colony.planet_idx = MOX::colony[colony_idx].planet;
                    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_COMET;
                    Add_Msg(msg, owner);
                }
            }
        }

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (EVENTS::Event_Check_Space_Anomaly_At_Star(star_idx) != 0) {
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_TYPE_ANOMALY;
                msg.u.anomaly.star_idx = star_idx;

                for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                    Add_Msg(msg, player_idx);
                }
            }
        }

        if (MOX::stardate == 35023) {
            time_t current_time = time(NULL);
            tm* time_info = localtime(&current_time);

            if (time_info->tm_mon == 3 /* April */ && time_info->tm_mday == 1) {
                msg.msg_type = MSG_TYPE_APRIL1;
                Add_Msg_All(msg);
            }
        }
    }

    void __cdecl Add_Peace_Ended_Msg(int player_a_idx, int player_b_idx) {
        Add_Diplomacy_Msg(player_a_idx, player_b_idx, DIPLOMACY_MSG_PEACE_ENDED);
    }

    /**
     * Adds a diplomacy message indicating peace has been declared between two players.
     */
    void __cdecl Add_Peace_Msg(int32_t player_a_idx, int32_t player_b_idx) {
        Add_Diplomacy_Msg(player_a_idx, player_b_idx, DIPLOMACY_MSG_PEACE);
    }

    void __cdecl Add_Diplomacy_Msg(int16_t plr_a, int16_t plr_b, int16_t diplomacy_idx) {
        s_MSG msg = {};
        msg.msg_type = MSG_TYPE_DIPLOMACY;
        msg.u.diplomacy.diplomacy_idx = static_cast<int8_t>(diplomacy_idx);
        msg.u.diplomacy.plr_a = static_cast<int8_t>(plr_a);
        msg.u.diplomacy.plr_b = static_cast<int8_t>(plr_b);

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if (diplomacy_idx == DIPLOMACY_MSG_PEACE_ENDED) {
                    if (i == plr_a || i == plr_b) {
                        Add_Msg(msg, i);
                    }
                } else {
                    if (MOX::player[i].contact[plr_a] != 0 || MOX::player[i].contact[plr_b] != 0) {
                        Add_Msg(msg, i);
                    }
                }
            }
        }
    }

    void __cdecl Add_Nonaggression_Msg(int16_t plr_a, int16_t plr_b) {
        Add_Diplomacy_Msg(plr_a, plr_b, DIPLOMACY_MSG_NONAGGRESSION);
    }

    void __cdecl Add_Msg_All(s_MSG msg) {
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            Add_Msg(msg, i);
        }
    }

    void __cdecl Add_Alliance_Msg(uint32_t player_a_idx, uint32_t player_b_idx) {
        Add_Diplomacy_Msg(player_a_idx, player_b_idx, DIPLOMACY_MSG_ALLIANCE);
    }

    void __cdecl Add_War_Msg(int32_t player_a, int32_t player_b) {
        Add_Diplomacy_Msg(player_a, player_b, DIPLOMACY_MSG_WAR);
    }

    /**
     * @brief Transitions to the colony screen for a colony mentioned in a message.
     *
     * If the message is of type MSG_TYPE_COLONY and the player owns the colony,
     * this function updates the screen state to navigate to that colony's management view.
     */
    int8_t __cdecl Goto_Msg_Colony(s_MSG* msg) {
        const int16_t colony_idx = Message_Colony(msg, MOX::PLAYER_NUM);
        if (colony_idx == -1) return 0;
        MOX::return_screen = SCREEN_INFO;
        MOX::current_screen = SCREEN_COLONY;
        const s_planet_data* planet = &MOX::planet[MOX::colony[colony_idx].planet];
        MOX::screen_data = planet->star_index;
        COLONY::orbit_temp = (int16_t)planet->orbit;
        return 1;
    }

    int16_t Message_Colony(const s_MSG* msg, int16_t viewer) {
        if (msg == nullptr || msg->msg_type != MSG_TYPE_COLONY || MOX::planet == nullptr || MOX::colony == nullptr) return -1;
        const int16_t planet = msg->u.colony.planet_idx;
        if (planet < 0 || planet >= MOX::planet_count) return -1;
        const int16_t colony = MOX::planet[planet].colony_index;
        if (colony < 0 || colony >= MOX::NUM_COLONIES || MOX::colony[colony].owner != viewer) return -1;
        return colony;
    }

    bool Read_Message(int16_t viewer, int16_t position, char* out, uint32_t size, int16_t* colony) {
        if (viewer < 0 || viewer >= MOX::NUM_PLAYERS || viewer >= MAX_PLAYERS || position < 0 || position >= n_msgs[viewer]) return false;
        const int16_t first = first_msg[viewer];
        if (n_msgs[viewer] > max_msgs[viewer] || first + n_msgs[viewer] > MAX_MESSAGES) return false;
        const s_MSG* msg = &msgs[first + position];
        if (native_msg == nullptr) return false;
        Copy_Message_Text(out, size, msg, viewer);
        if (colony != nullptr) *colony = Message_Colony(msg, viewer);
        return true;
    }

    char* __cdecl Load_Msg_Strings(char* native_msg) {
        auto next_string = [](char* s) -> char* {
            return s + strlen(s) + 1;
        };

        char* cursor = native_msg;

        MSG_::native_msg = cursor;
        cursor = next_string(cursor);

        s_and_s_declare_alliance_fmt = cursor;
        cursor = next_string(cursor);

        s_and_s_declare_war_fmt = cursor;
        cursor = next_string(cursor);

        msg_financial_fmt = cursor;
        cursor = next_string(cursor);

        msg_lost_ship_fmt = cursor;
        cursor = next_string(cursor);

        msg_scrap_ship_bc_fmt = cursor;
        cursor = next_string(cursor);

        msg_scrap_ship_hyperspace_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_or_outpost[0] = cursor;
        cursor = next_string(cursor);

        msg_colony_or_outpost[1] = cursor;
        cursor = next_string(cursor);

        msg_lost_colony_fmt = cursor;
        cursor = next_string(cursor);

        msg_lost_colony_leader_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_scrap_building_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_building_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_ship_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_refit_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_spy_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_android_farmer_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_android_worker_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_android_scientist_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_freighter_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_next_queue_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_built_auto_build_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_genocide_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_assimilation_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_starvation_food_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_starvation_ind_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_starvation_both_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_1_settler_arrived_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_d_settlers_arrived_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_settlers_arrived_blockaded_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_1_settler_arrived_over_limit_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_d_settlers_arrived_over_limit_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_plague_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_boom_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_comet_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_your_rebellion_won_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_your_rebellion_lost_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_1_rebel_won_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_d_rebels_won_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_1_rebel_lost_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_d_rebels_lost_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_no_room_for_android_fmt = cursor;
        cursor = next_string(cursor);

        msg_colony_android_killed_organic_fmt = cursor;
        cursor = next_string(cursor);

        msg_hyperspace_flux_fmt = cursor;
        cursor = next_string(cursor);

        msg_hyperspace_beast_hungry_fmt = cursor;
        cursor = next_string(cursor);

        msg_hyperspace_beast_ate_your_ship_fmt = cursor;
        cursor = next_string(cursor);

        msg_hyperspace_beast_ate_other_ship_fmt = cursor;
        cursor = next_string(cursor);

        msg_lost_attacking_spy_fmt = cursor;
        cursor = next_string(cursor);

        msg_lost_defending_spy_fmt = cursor;
        cursor = next_string(cursor);

        msg_lost_defending_spies_fmt = cursor;
        cursor = next_string(cursor);

        msg_artemis_fmt = cursor;
        cursor = next_string(cursor);

        msg_artemis_leader_fmt = cursor;
        cursor = next_string(cursor);

        msg_1_ship_killed_retreating_fmt = cursor;
        cursor = next_string(cursor);

        msg_d_ships_killed_retreating_fmt = cursor;
        cursor = next_string(cursor);

        msg_defensive_spy_fmt = cursor;
        cursor = next_string(cursor);

        msg_offensive_spy_fmt = cursor;
        cursor = next_string(cursor);

        msg_spy_assassinated_victim_fmt = cursor;
        cursor = next_string(cursor);

        msg_spy_assassinated_fmt = cursor;
        cursor = next_string(cursor);

        msg_anomaly_fmt = cursor;
        cursor = next_string(cursor);

        msg_diplomacy_strings[0] = cursor;
        cursor = next_string(cursor);

        msg_diplomacy_strings[1] = cursor;
        cursor = next_string(cursor);

        msg_diplomacy_strings[2] = cursor;
        cursor = next_string(cursor);

        msg_diplomacy_strings[3] = cursor;
        cursor = next_string(cursor);

        msg_diplomacy_strings[4] = cursor;
        cursor = next_string(cursor);

        msg_eel_spawned_fmt = cursor;
        cursor = next_string(cursor);

        msg_eel_vanished_fmt = cursor;
        cursor = next_string(cursor);

        msg_artifacts_fmt = cursor;
        cursor = next_string(cursor);

        msg_april1_fmt = cursor;
        cursor = next_string(cursor);

        msg_scrap_leader_fmt = cursor;
        cursor = next_string(cursor);

        failed_retreat_reason_strings[0] = cursor;
        cursor = next_string(cursor);

        failed_retreat_reason_strings[1] = cursor;
        cursor = next_string(cursor);

        failed_retreat_reason_strings[2] = cursor;
        cursor = next_string(cursor);

        msg_failed_retreat_leader_fmt = cursor;
        cursor = next_string(cursor);

        leader_fate_strings[0] = cursor;
        cursor = next_string(cursor);

        leader_fate_strings[1] = cursor;
        cursor = next_string(cursor);

        leader_fate_strings[2] = cursor;
        cursor = next_string(cursor);

        lost_colony_reason_strings[0] = cursor;
        cursor = next_string(cursor);

        lost_colony_reason_strings[1] = cursor;
        cursor = next_string(cursor);

        lost_colony_reason_strings[2] = cursor;
        cursor = next_string(cursor);

        lost_colony_reason_strings[3] = cursor;
        cursor = next_string(cursor);

        return cursor;
    }
} // namespace MSG_