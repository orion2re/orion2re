#include "pch.h"
#include "fleet_movement.h"

namespace REPORT {
    uint8_t no_outpost_prompt;
    int16_t n_reports;
    s_report reports[MAX_REPORTS];
    game::player_bitmap done_player_reports;
    uint8_t something_interesting_happened[MAX_PLAYERS];
    uint8_t done_colonize_check[BITMAP(MAX_STARS)];
    game::player_bitmap displayed_msgs;
    game::player_bitmap displayed_gnn;

    char* your_spy_destroys_fmt;
    char* your_spy_steals_fmt;
    char* you_framed_fmt;
    char* colonize_preselected_planet_fmt;
    char* enemy_spy_destroys_fmt;
    char* really_trash_colony_base_fmt;
    char* enemy_spy_steals_fmt;
    char* outpost_preselected_planet_fmt;

    namespace {
        s_ship_data* Find_Eligible_Founding_Ship(game::s_game_state& state, game::id_ship key,
            int owner, int type, game::id_star star) {
            const auto ship_entry = state.ships.find(key);
            auto* ship = ship_entry == state.ships.end() ? nullptr : &ship_entry->second;
            if (!ship || factions::Legacy_Code(ship->owner) != owner || ship->d.ship_type != type || ship->status >= 5 || ship->just_built ||
                ship->position.kind != game::e_ship_location::at_star ||
                ship->position.target != game::e_ship_target::star || ship->position.star_id.get() != star) return nullptr;
            return ship;
        }
    }

    game::id_colony __cdecl Player_Colony_Base_At_Star(game::id_player player_key, int16_t star_idx) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        for (int i = 0; i < 5; ++i) {
            const auto planet_idx = game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, star_idx)).orbit_planet_ids[i];

            if (planet_idx != game::NO_PLANET) {
                const auto colony_id = game::current_state.planets.at(planet_idx).colony_id;

                if (colony_id != game::NO_COLONY) {
                    const auto& colony = game::current_state.colonies.at(colony_id);

                    if (legacy_world::Encode_Player_Slot(colony.owner) == player_idx && colony.buildings[BUILDING_COLONY_BASE] > 0) {
                        return colony_id;
                    }
                }
            }
        }

        return game::NO_COLONY;
    }

    s_ship_data* __cdecl Player_Ship_Type_At_Star(game::id_player owner_key, int16_t ship_type, int16_t location, int8_t include_just_built) {
        int owner = legacy_world::Encode_Player_Slot(owner_key);
        for (int row = 0; row < static_cast<int16_t>(game::current_state.ship_slots.size()); ++row) {
            if (game::current_state.ship_slots[row] == ::game::NO_SHIP) continue;
            auto* ship = &ship_slots::At(game::current_state, row);
            if ((short)(int8_t)ship->d.ship_type == ship_type &&
                (short)factions::Legacy_Code(ship->owner) == owner &&
                game::Ship_Orbit_Row(game::current_state, ship->position) == location &&
                ship->status < 5) {
                if (include_just_built != 0 || ship->just_built == 0) {
                    return ship;
                }
            }
        }

        return nullptr;
    }

    bool Is_Serious_Msg(const s_MSG& msg) {
        switch (msg.msg_type) {
            case MSG_TYPE_COLONY:
                return msg.u.colony.subtype == MSG_COLONY_SUBTYPE_STARVATION ||
                    msg.u.colony.subtype == MSG_COLONY_SUBTYPE_REBELLION;
            case MSG_TYPE_FINANCIAL:
            case MSG_TYPE_LOST_SHIP:
            case MSG_TYPE_LOST_COLONY:
            case MSG_TYPE_LOST_ATTACKING_SPY:
            case MSG_TYPE_LOST_DEFENDING_SPY:
                return true;
            case MSG_TYPE_HYPERSPACE_BEAST:
                return msg.u.hyperspace_beast.ship_idx != -1 && msg.u.hyperspace_beast.fate != 0;
            default:
                return false;
        }
    }

    void Order_Turn_Summary_Messages(uint8_t* indices, int16_t first_idx, int16_t count, bool serious_first) {
        if (indices == nullptr || count <= 0) {
            return;
        }

        int16_t output = 0;
        if (serious_first) {
            for (int16_t i = 0; i < count; ++i) {
                if (Is_Serious_Msg(MSG_::msgs[first_idx + i])) {
                    indices[output++] = (uint8_t)(first_idx + i);
                }
            }
        }
        for (int16_t i = 0; i < count; ++i) {
            if (!serious_first || !Is_Serious_Msg(MSG_::msgs[first_idx + i])) {
                indices[output++] = (uint8_t)(first_idx + i);
            }
        }
    }

    int8_t __cdecl Has_Msg(game::id_player player_key, int8_t show_summary) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t result = 0;

        // Check if the summary for this player hasn't been displayed yet
        if (!REPORT::displayed_msgs.test(player_key)) {
            int16_t num_msgs = MSG_::N_Msgs_For_Player(player_key);

            if (num_msgs > 0 && MOX::settings.end_of_turn_summary != 0) {
                int16_t first_idx = MSG_::First_Msg_For_Player(player_key);
                bool should_show = false;

                // Scan messages backwards from newest to oldest
                for (int i = num_msgs - 1; i >= 0; --i) {
                    s_MSG* msg = &MSG_::msgs[first_idx + i];

                    switch (msg->msg_type) {
                        case MSG_TYPE_COLONY:
                            switch (msg->u.colony.subtype) {
                                case MSG_COLONY_SUBTYPE_STARVATION:
                                case MSG_COLONY_SUBTYPE_REBELLION:
                                    should_show = Is_Serious_Msg(*msg);
                                    break;
                                case MSG_COLONY_SUBTYPE_PLAGUE:
                                case MSG_COLONY_SUBTYPE_POPULATION_BOOM:
                                case MSG_COLONY_SUBTYPE_COMET:
                                    should_show = false;
                                    break;
                                default:
                                    should_show = (MOX::settings.only_show_serious_turn_summary_reports == 0);
                                    break;
                            }
                            break;

                        case MSG_TYPE_FINANCIAL:
                        case MSG_TYPE_LOST_SHIP:
                        case MSG_TYPE_LOST_COLONY:
                        case MSG_TYPE_LOST_ATTACKING_SPY:
                        case MSG_TYPE_LOST_DEFENDING_SPY:
                            should_show = Is_Serious_Msg(*msg);
                            break;

                        case MSG_TYPE_HYPERSPACE_FLUX:
                        case MSG_TYPE_ANOMALY:
                            should_show = false;
                            break;

                        case MSG_TYPE_HYPERSPACE_BEAST:
                            should_show = Is_Serious_Msg(*msg);
                            break;

                        case MSG_TYPE_DIPLOMACY:
                            if (msg->u.diplomacy.plr_a.get() == MOX::local_player) {
                                should_show = false;
                            } else {
                                should_show = (MOX::settings.only_show_serious_turn_summary_reports == 0);
                            }
                            break;

                        default:
                            should_show = (MOX::settings.only_show_serious_turn_summary_reports == 0);
                            break;
                    }

                    if (should_show) {
                        break;
                    }
                }

                if (should_show) {
                    result = 4;
                    if (show_summary != 0) {
                        MOX::current_screen = SCREEN_TURN_SUMMARY;
                        MOX::return_screen = SCREEN_REPORTS;
                        MOX::global_esc = 1;
                        REPORT::displayed_msgs.set(player_key);
                    }
                }
            }
        }

        return result;
    }

    int8_t __cdecl Colonization(game::id_player player_key, int8_t show_dialog) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto& state = game::current_state;
        for (int16_t star_idx = 0; star_idx < std::ssize(state.star_order); ++star_idx) {
            const auto star_id_ = state.star_order[star_idx];
            auto& star_record = state.stars.at(star_id_);
            if (factions::Legacy_Code(star_record.colonize_player) == player_idx) {
                if (struct_::Test_Bit_Field(done_colonize_check, star_idx) == 0) {
                    auto colony_base_id = Player_Colony_Base_At_Star(player_key, star_idx);
                    const auto select_ship_key = [&](int16_t type) {
                        const auto* ship = Player_Ship_Type_At_Star(player_key, type, star_idx, 0);
                        return ship ? ship->id : game::NO_SHIP;
                    };
                    const auto colony_key = select_ship_key(SHIP_TYPE_COLONY);
                    const auto outpost_key = no_outpost_prompt ? game::NO_SHIP : select_ship_key(SHIP_TYPE_OUTPOST);
                    const bool prompt_colony = Find_Eligible_Founding_Ship(state, colony_key, player_idx, SHIP_TYPE_COLONY, star_id_) &&
                            (!no_outpost_prompt || COLONIZE::Star_N_Colonizable_Planets_For_Player(&star_record, player_key) != 0);
                    const bool has_ships = prompt_colony || Find_Eligible_Founding_Ship(state, outpost_key, player_idx, SHIP_TYPE_OUTPOST, star_id_);

                    if (colony_base_id != game::NO_COLONY || (has_ships && (star_record.ignore_colony_ships.test_slot(player_idx)) == 0)) {
                        if (show_dialog != 0) {
                            do {
                                bool is_colony = false;
                                MOVEBOX::Remove_Box_Check();
                                auto* outpost_ship = Find_Eligible_Founding_Ship(state, outpost_key, player_idx, SHIP_TYPE_OUTPOST, star_id_);
                                auto* colony_prompt_ship = prompt_colony
                                    ? Find_Eligible_Founding_Ship(state, colony_key, player_idx, SHIP_TYPE_COLONY, star_id_) : nullptr;
                                if (colony_base_id == game::NO_COLONY && !outpost_ship && !colony_prompt_ship) break;

                                auto planet_id = game::NO_PLANET;
                                if (colony_base_id == game::NO_COLONY && (colony_prompt_ship == nullptr || outpost_ship != nullptr)) {
                                    planet_id = PLNTSUM::Colony_Ship_Auto_Colonization(outpost_ship);
                                    if (planet_id == game::NO_PLANET) {
                                        planet_id = MAINPUPS::New_Outpost_Selection_Popup(star_idx);
                                    } else if (const auto planet = game::current_state.planets.find(planet_id);
                                        planet != game::current_state.planets.end() && planet->second.colony_id == game::NO_COLONY) {
                                        char msg[200];
                                        snprintf(msg, sizeof(msg), outpost_preselected_planet_fmt, HACCESS::Get_Planet_Name(game::current_state, planet_id));
                                        if (GENDRAW::Do_Confirmation_Box(msg) == 0) {
                                            planet_id = game::NO_PLANET;
                                        } else {
                                            if (namestar::Can_Change_Star_Name(star_idx) != 0 && MOX::settings.animations_on == 0) {
                                                int16_t y = namestar::Star_Name_Popup_Screen_Center_Y();
                                                int16_t x = namestar::Star_Name_Popup_Screen_Center_X();
                                                namestar::Change_Star_Name_Popup(x, y, star_idx, 0);
                                            }
                                        }
                                    } else {
                                        planet_id = MAINPUPS::New_Outpost_Selection_Popup(star_idx);
                                    }
                                } else {
                                    is_colony = true;
                                    bool auto_colonized = false;

                                    if (colony_base_id == game::NO_COLONY && PLNTSUM::Colony_Ship_Auto_Colonization(colony_prompt_ship) != game::NO_PLANET) {
                                        planet_id = PLNTSUM::Colony_Ship_Auto_Colonization(colony_prompt_ship);
                                        if (const auto planet = game::current_state.planets.find(planet_id);
                                            planet != game::current_state.planets.end() && planet->second.colony_id == game::NO_COLONY) {
                                            auto_colonized = true;
                                            char msg[200];
                                            snprintf(msg, sizeof(msg), colonize_preselected_planet_fmt, HACCESS::Get_Planet_Name(game::current_state, planet_id));
                                            if (GENDRAW::Do_Confirmation_Box(msg) == 0) {
                                                planet_id = game::NO_PLANET;
                                            } else {
                                                if (namestar::Can_Change_Star_Name(star_idx) != 0 && MOX::settings.animations_on == 0) {
                                                    int16_t y = namestar::Star_Name_Popup_Screen_Center_Y();
                                                    int16_t x = namestar::Star_Name_Popup_Screen_Center_X();
                                                    namestar::Change_Star_Name_Popup(x, y, star_idx, 0);
                                                }
                                            }
                                        }
                                    }

                                    if (!auto_colonized) {
                                        planet_id = MAINPUPS::New_Colony_Selection_Popup(star_idx, colony_base_id == game::NO_COLONY);
                                    }

                                    while (colony_base_id != game::NO_COLONY && planet_id == game::NO_PLANET) {
                                        int refund = TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                                        char msg[200];
                                        snprintf(msg, sizeof(msg), really_trash_colony_base_fmt, refund);
                                        if (GENDRAW::Do_Confirmation_Box(msg) == 0) {
                                            planet_id = MAINPUPS::New_Colony_Selection_Popup(star_idx, colony_base_id == game::NO_COLONY);
                                        } else {
                                            do {
                                                game::current_state.players[player_idx].bc += refund;
                                                game::current_state.colonies.at(colony_base_id).buildings[BUILDING_COLONY_BASE] = 0;
                                                colony_base_id = Player_Colony_Base_At_Star(player_key, star_idx);
                                            } while (colony_base_id != game::NO_COLONY);
                                            break;
                                        }
                                    }
                                }

                                const auto live_star = state.stars.find(star_id_);
                                if (live_star == state.stars.end() || game::Star_Key_At_Row(state, star_idx) != star_id_) break;
                                if (planet_id != game::NO_PLANET && colony_base_id == game::NO_COLONY &&
                                    !Find_Eligible_Founding_Ship(state, is_colony ? colony_key : outpost_key,
                                        player_idx, is_colony ? SHIP_TYPE_COLONY : SHIP_TYPE_OUTPOST, star_id_)) break;
                                if (planet_id != game::NO_PLANET &&
                !(is_colony ? COLONIZE::Make_New_Colony(planet_id, player_key)
                            : COLONIZE::Make_New_Outpost(planet_id, player_key))) {
                                    struct_::Set_Bit_Field(done_colonize_check, star_idx);
                                    break;
                                }
                                auto* colony_ship = Find_Eligible_Founding_Ship(state, colony_key, player_idx, SHIP_TYPE_COLONY, star_id_);
                                outpost_ship = Find_Eligible_Founding_Ship(state, outpost_key, player_idx, SHIP_TYPE_OUTPOST, star_id_);
                                if (colony_ship != nullptr) {
                                    SHIPS::Clear_Auto_Colonization_Flag(colony_ship);
                                }
                                if (outpost_ship != nullptr) {
                                    SHIPS::Clear_Auto_Colonization_Flag(outpost_ship);
                                }

                                if (planet_id == game::NO_PLANET) {
                                    struct_::Set_Bit_Field(done_colonize_check, star_idx);
                                    live_star->second.ignore_colony_ships.set_slot(player_idx);
                                } else {
                                    if (is_colony) {
                                        if (colony_base_id == game::NO_COLONY) {
                                            if (colony_ship) SHIPS::Kill_Ship(*colony_ship);
                                            COLCALC::Update_Player_Stats(&game::current_state.players[player_idx]);
                                            MAINSCR::Update_Ship_Icons_In_Between_Turns();
                                            colony_base_id = game::NO_COLONY;
                                        } else {
                                            ERICNET::Remove_Building(game::current_state, colony_base_id, BUILDING_COLONY_BASE);
                                            colony_base_id = Player_Colony_Base_At_Star(player_key, star_idx);
                                        }

            if (COLONIZE::Star_N_Colonizable_Planets_For_Player(&live_star->second, player_key) == 0) {
                                            struct_::Set_Bit_Field(done_colonize_check, star_idx);
                                            if (colony_base_id != game::NO_COLONY) {
                                                do {
                                                    game::current_state.players[player_idx].bc += TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                                                    game::current_state.colonies.at(colony_base_id).buildings[BUILDING_COLONY_BASE] = 0;
                                                    colony_base_id = Player_Colony_Base_At_Star(player_key, star_idx);
                                                } while (colony_base_id != game::NO_COLONY);
                                            }
                                        }

                                        if (MOX::settings.animations_on != 0) {
                                            MOX::current_screen = 0x21; /* SCR_COLONIZE */
                                            MOX::return_screen = SCREEN_REPORTS;
                                            MOX::screen_star_id = game::current_state.planets.at(planet_id).star_id;
                                            MOX::global_esc = 1;
                                            COLONY::screen_colony_id = game::current_state.planets.at(planet_id).colony_id;
                                            return 3;
                                        }
                                        return 1;
                                    } else {
                                        if (outpost_ship) SHIPS::Kill_Ship(*outpost_ship);
                                        COLCALC::Update_Player_Stats(&game::current_state.players[player_idx]);
                                        MAINSCR::Update_Ship_Icons_In_Between_Turns();
                                    }
                                }
                            } while (colony_base_id != game::NO_COLONY);
                        }
                        return 1;
                    } else {
                        struct_::Set_Bit_Field(done_colonize_check, star_idx);
                    }
                }
            }
        }
        return 0;
    }

    int8_t __cdecl Display_Report_Aux(game::id_player player_key, uint8_t show_ui) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t status;

        if (done_player_reports.test(player_key)) {
            return 0;
        }

        if (MOX::stardate == 35000) {
            return 0;
        }

        if ((status = Antarans(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_GNN(player_key, show_ui != 0)) != 0) return status;
        if ((status = Set_Initial_Tech(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Report(player_key, show_ui != 0)) != 0) return status;
        if ((status = Has_Research_Breakthrough(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Evolutionary_Upgrade(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Explored_New_Star(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Colonization(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Officer_For_Hire(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = OFFICER::Officer_Made_Level_Report(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Diplomacy_Messages(player_key, show_ui != 0)) != 0) return status;
        if ((status = Has_Msg(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Empty_Colony_Queue(player_key, (int8_t)show_ui)) != 0) return status;
        if ((status = Need_To_Set_Occupation_Policy(player_key, (int8_t)show_ui)) != 0) return status;

        done_player_reports.set(player_key);

        return 0;
    }

    uint8_t __cdecl Display_Report(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        return (uint8_t)REPORT::Display_Report_Aux(player_key, 1);
    }

    void __cdecl sprintf_report(char* buffer, uint32_t buffer_size, s_report* report) {
        if (buffer == nullptr || buffer_size == 0 || report == nullptr) {
            return;
        }

        buffer[0] = '\0';

        if (report->type == 1 /* MSG_TYPE_REPORT_YOUR_SPY */) {
            if (report->action == 0 /* SPY_ACTION_STEAL */) {
                snprintf(buffer, buffer_size, REPORT::your_spy_steals_fmt, TECHDATA::technology_applications[report->d.steal.tech_app_id].name, game::current_state.players.at(report->d.steal.victim.get().id).race_name);
            } else if (report->action == 1 /* SPY_ACTION_SABOTAGE */) {
                char* planet_name = HACCESS::Get_Planet_Name(game::current_state, report->d.sabotage.planet_id.get());
                snprintf(buffer, buffer_size, REPORT::your_spy_destroys_fmt, game::current_state.players.at(report->d.sabotage.victim.get().id).race_name, TECHDATA::buildings[report->d.sabotage.building_id].name, planet_name);
            }

            const auto framed = report->action == 0 ? report->d.steal.framed.get() : report->d.sabotage.framed.get();
            if (framed.id < MAX_PLAYERS) {
                const size_t used_length = strlen(buffer);
                if (used_length < buffer_size) {
                    snprintf(buffer + used_length, buffer_size - used_length, REPORT::you_framed_fmt, game::current_state.players.at(framed.id).race_name);
                }
            }
        } else if (report->type == 2 /* MSG_TYPE_REPORT_ENEMY_SPY */) {
            if (report->action == 0 /* SPY_ACTION_STEAL */) {
                snprintf(buffer, buffer_size, REPORT::enemy_spy_steals_fmt, game::current_state.players.at(report->d.enemy_steal.enemy.get().id).race_name, TECHDATA::technology_applications[report->d.enemy_steal.tech_app_id].name);
            } else if (report->action == 1 /* SPY_ACTION_SABOTAGE */) {
                char* planet_name = HACCESS::Get_Planet_Name(game::current_state, report->d.enemy_sabotage.planet_id.get());
                snprintf(buffer, buffer_size, REPORT::enemy_spy_destroys_fmt, game::current_state.players.at(report->d.enemy_sabotage.enemy.get().id).race_name, TECHDATA::buildings[report->d.enemy_sabotage.building_id].name, planet_name);
            }
        }
    }

    char* __cdecl Load_Report_Strings(char* data) {
        your_spy_steals_fmt = data;
        data += strlen(data) + 1;

        your_spy_destroys_fmt = data;
        data += strlen(data) + 1;

        you_framed_fmt = data;
        data += strlen(data) + 1;

        enemy_spy_steals_fmt = data;
        data += strlen(data) + 1;

        enemy_spy_destroys_fmt = data;
        data += strlen(data) + 1;

        really_trash_colony_base_fmt = data;
        data += strlen(data) + 1;

        colonize_preselected_planet_fmt = data;
        data += strlen(data) + 1;

        outpost_preselected_planet_fmt = data;
        data += strlen(data) + 1;

        return data;
    }

    void __cdecl Client_Initialize_Reports() {
        done_player_reports.reset();
        displayed_msgs.reset();
        displayed_gnn.reset();

        // Reset colonization checks for all tracked stars.
        memset(done_colonize_check, 0, sizeof(done_colonize_check));

        MOX::antarans.attack_notified_mask.reset();
    }

    void __cdecl Initialize_Reports() {
        n_reports = 0;

        // Clear per-player "something interesting happened" flags.
        memset(something_interesting_happened, 0, sizeof(something_interesting_happened));

        Client_Initialize_Reports();
    }

    /**
     * @brief Checks if a GNN (Galactic News Network) event is available.
     * 
     * @param player_idx The bit index in the displayed GNN mask to check.
     * @param mark_as_displayed If true, marks the event as displayed and triggers event processing.
     * @return int8_t Returns 7 if a GNN event message is available, 0 otherwise.
     */
    int8_t __cdecl Has_GNN(game::id_player player_key, bool mark_as_displayed) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t status = 0;

        if (!REPORT::displayed_gnn.test_slot(player_idx)) {
            if (EVENTS::Check_For_Event_Message() != 0) {
                status = 7;
                if (mark_as_displayed) {
                    EVENTS::Check_For_Event();
                    REPORT::displayed_gnn.set_slot(player_idx);
                }
            }
        }

        return status;
    }

    uint8_t __cdecl Must_Display_Reports(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t displayed = 0;

        if (game::current_state.players[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            displayed = REPORT::Display_Report_Aux(player_key, 0);
        }

        if (displayed == 0) {
            REPORT::Redirect_Newly_Built_Ships(player_key);

            if (displayed == 0) {
                if (MOX::game_type >= 2 && MOX::game_type <= 3) {
                    NEXTTURN::Do_Autosave();
                }
            }
        }

        return displayed;
    }

    int8_t __cdecl Set_Initial_Tech(game::id_player player_key, int8_t select_tech) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        if (initial_tech_prompt != 0 && MOX::stardate > 35000) {
            uint8_t field_idx = (uint8_t)game::current_state.players[player_idx].current_research_field;

            // Check if current research field progress is at state 3 and field is 0
            if (game::current_state.players[player_idx].tech_fields[field_idx] == 3 && field_idx == TECH_FIELD_STARTING_TECH) {
                if (select_tech != 0) {
                    TECH::Tech_Select();
                }
                return 2;
            }
        }
        return 0;
    }

    int8_t __cdecl Has_Officer_For_Hire(game::id_player player_key, int8_t hire_flag) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t has_officer = 0;
        int8_t officer_id = game::current_state.players[player_idx].officer_for_hire;

        if (officer_id != -1) {
            has_officer = 1;

            if (hire_flag != 0) {
                bool accepted = MAINPUPS::Random_New_Officer_Popup(player_key);
                int32_t cost = OFFICER::Calculate_Officer_Cost(player_key, officer_id);

                OFFICER::Set_Officer_To_Player(officer_id, player_key, (uint8_t)accepted);

                if (accepted) {
                    game::current_state.players[player_idx].bc -= cost;
                    COLCALC::Do_Player_Colony_Calculations(player_key);
                    if (OFFICER::rejected_offer_keeps_timer != 0) {
                        // The offer was generated on the preceding turn.
                        game::current_state.players[player_idx].stardate_last_offered_hero =
                            static_cast<uint16_t>(MOX::stardate - 1);
                    }
                }

                game::current_state.players[player_idx].officer_for_hire = -1;
            }
        }

        return has_officer;
    }

    int8_t __cdecl Has_Research_Breakthrough(game::id_player player_key, int8_t do_select) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t status = 0;

        if (game::current_state.players[player_idx].research_breakthrough != 0) {
            status = 1;
            if (do_select != 0) {
                TECH::Tech_Select();
                game::current_state.players[player_idx].research_breakthrough = 0;
                status = 2;
            }
        }

        return status;
    }

    int8_t __cdecl Has_Evolutionary_Upgrade(game::id_player player_key, int8_t do_setup) {
        if (player_key.id >= MAX_PLAYERS) return 0;
        int8_t result = 0;
        s_player* plr = &game::current_state.players[player_key.id];

        // Check if player has researched Evolutionary Mutation (Tech App 62)
        // and hasn't picked the upgrade yet.
        if (plr->tech_applications[TECH_APP_EVOLUTIONARY_MUTATION] == TECH_RESEARCH_STATUS_RESEARCHED &&
            plr->picked_evolutionary_upgrade == 0) {
            result = 1;

            if (do_setup != 0) {
                raceopt::Setup_Evolutionary_Upgrade(*plr);
            }
        }

        return result;
    }

    int8_t __cdecl Draw_Special(s_star_data* star, game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t special = star->system_special;

        if (special < 50) {
            switch (special) {
                case SYSTEM_SPECIAL_NO_SPECIAL:
                case SYSTEM_SPECIAL_GOLD_DEPOSITS:
                case SYSTEM_SPECIAL_GEM_DEPOSITS:
                case SYSTEM_SPECIAL_NATIVES:
                case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS:
                case SYSTEM_SPECIAL_ORION_STAR:
                    return special;
                default:
                    return 0;
            }
        } else {
            int8_t result;
            if (special >= 90) {
                result = 10;
            } else if (special >= 80) {
                result = 8;
            } else if (special >= 70) {
                result = 7;
            } else {
                result = (special >= 60) + 2;
            }

            int16_t remainder = special % 10;
            if (remainder == player_idx || result == 10) {
                return result;
            }

            return 0;
        }
    }

    int8_t __cdecl Star_Has_Special_For_Player(s_star_data* star, game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t has_special = 0;

        if (star->system_special >= 50) {
            if ((star->system_special % 10) == player_idx) {
                has_special = 1;
            }
        }

        return has_special;
    }

    int8_t __cdecl Explored_New_Star(game::id_player player_key, int8_t show_popup) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        for (const auto star_id : game::current_state.star_order) {
            s_star_data* star = &game::current_state.stars.at(star_id);
            int8_t has_special = REPORT::Star_Has_Special_For_Player(star, player_key);

            // Check if player just visited this star or it contains a relevant special
            if ((star->just_visited.test_slot(player_idx)) != 0 || has_special != 0) {
                // If telepathic and the star discovery is not a special, clear discovery flag and skip report
                if (game::current_state.players[player_idx].traits[27] != 0 && has_special == 0) {
                    star->just_visited.reset_slot(player_idx);
                    return 0;
                }

                if (show_popup != 0) {
                    MOVEBOX::Remove_Box_Check();
                    char special_type = REPORT::Draw_Special(star, player_key);
                    int16_t star_idx = game::Star_Row(game::current_state, star->id);
                    MAINPUPS::New_System_Discovery_Popup(star_idx, (int16_t)special_type);

                    // Mark as no longer "just visited" after reporting
                    star->just_visited.reset_slot(player_idx);

                    if (has_special != 0) {
                        star->system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    }
                }
                return 1;
            }
        }
        return 0;
    }

    int8_t __cdecl Has_Diplomacy_Messages(game::id_player receiver, bool show_messages) {
        if (receiver.id >= MAX_PLAYERS) return 0;
        const auto receiver_idx = receiver.id;
        int8_t has_messages = 0;

        for (int16_t player_idx = 0; player_idx < game::current_state.player_count; ++player_idx) {
            // A player doesn't send messages to themselves
            const auto sender = game::current_state.players[player_idx].id;
            if (sender == receiver) {
                continue;
            }

            // Check if the receiver is ignoring the sender
            if (!game::current_state.players[receiver_idx].ignoring.test(sender)) {
                // Check if player_idx has a message for receiver_idx
                uint8_t msg_id = game::current_state.players[player_idx].diplomacy_message[receiver_idx];
                if (msg_id != 0) {
                    has_messages = 5;

                    if (show_messages) {
                        // Check for multiplayer/interactive diplomacy requirements
                        if ((MOX::game_type == 2 || MOX::game_type == 3) &&
                            DIP_SCRN::Diplomacy_Message_Is_Interactive((uint16_t)msg_id)) {
                            NETMOX::Net_Request_AI_Diplomacy(sender);

                            while (!NETMOX::Net_May_Begin_AI_Diplomacy()) {
                                RUSS::Mox_Update();
                            }

                            DIP_SCRN::Npc_Diplomacy_Screen(sender, 0);
                            game::current_state.players[player_idx].diplomacy_message[receiver_idx] = 0;
                            NETMOX::Net_End_AI_Diplomacy();
                        } else {
                            // Standard single-player or non-interactive message display
                            DIP_SCRN::Npc_Diplomacy_Screen(sender, 0);
                            game::current_state.players[player_idx].diplomacy_message[receiver_idx] = 0;
                        }
                    }
                }
            }
        }

        return has_messages;
    }

    /**
     * @brief Evaluates and triggers Antaran attack notifications.
     * 
     * @param unused_player_idx Unused caller-supplied player index.
     * @param show_notification Flag indicating if notification logic should proceed beyond basic state check.
     * @return int8_t 0 if no notification, 6 if stardate matches but logic skipped, 1 if attack movie played.
     */
    int8_t __cdecl Antarans(game::id_player unused_player_key, int8_t show_notification) {
        int unused_player_idx = legacy_world::Encode_Player_Slot(unused_player_key);
        int8_t result = 0;
        (void)unused_player_idx;

        // Check if stardate is exactly one day past invasion date
        if (((int32_t)MOX::antarans.invasion_stardate - (int32_t)MOX::stardate) == -1) {
            // Check if current player was already notified
            if (!MOX::antarans.attack_notified_mask.test(MOX::local_player)) {
                result = 6;
                if (show_notification != 0) {
                    int16_t target_star_idx = -1;
                    uint8_t max_turns_left = 0;

                    // Locate the Antaran ship with the longest remaining travel time to determine target
                    for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
                        if (game::current_state.ship_slots[i] == ::game::NO_SHIP) continue;
                        s_ship_data* ship = &ship_slots::At(game::current_state, i);
                        if (factions::Legacy_Code(ship->owner) == 8 /* Antaran */ && ship->status == 1 /* Active */) {
                            if (max_turns_left < (uint8_t)ship->turns_left) {
                                target_star_idx = game::Ship_Target_Row(game::current_state, ship->position);
                                max_turns_left = ship->turns_left;
                            }
                        }
                    }

                    bool room_screen_shown = false;
                    if (target_star_idx != -1) {
                        // Check if player has a colony at the target system
                        if (game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, target_star_idx)).has_colony.test(MOX::local_player)) {
                            if (game_random::Random(10) <= 4) {
                                ANTAROOM::Main_Antaran_Room_Screen(MOX::antarans.invasion_stardate < 35000 /* 0x88B8 */);
                                room_screen_shown = true;
                            }
                        }
                    }

                    if (!room_screen_shown) {
                        result = 1;
                        ERIC::Play_Antarans_Attack_Flic();
                    }

                    // Mark player as notified
                    MOX::antarans.attack_notified_mask.set(MOX::local_player);
                }
            }
        }

        return result;
    }

    int8_t __cdecl Need_To_Set_Occupation_Policy(game::id_player player_key, int8_t show_screen) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* colony = &roster_colony;

            if (legacy_world::Encode_Player_Slot(colony->owner) == (int8_t)player_idx &&
                COLONY::Colony_Needs_Occupation_Policy(roster_id)) {
                if (show_screen != 0) {
                    MOVEBOX::Remove_Box_Check();
                    MOX::current_screen = SCREEN_COLONY;
                    MOX::return_screen = SCREEN_REPORTS;

                    const auto planet_idx = colony->planet_id;
                    s_planet_data* planet = &game::current_state.planets.at(planet_idx);

                    MOX::screen_star_id = planet->star_id;
                    COLONY::screen_colony_id = roster_id;

                    COLONY::Colony_Screen();
                }
                return 1;
            }
        }

        return 0;
    }

    void __cdecl Redirect_Newly_Built_Ships(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        bool any_ships_redirected = false;
        for (int16_t row = 0; row < static_cast<int16_t>(game::current_state.ship_slots.size()); ++row) {
            if (game::current_state.ship_slots[row] == ::game::NO_SHIP) continue;
            auto* ship = &ship_slots::At(game::current_state, row);
                if (factions::Legacy_Code(ship->owner) == (int8_t)player_idx && ship->just_built != 0) {
                    any_ships_redirected = true;
                    ship->just_built = 0;

                    // If ship is active (not destroyed/refitting)
                    if (ship->status == 0) {
                        int16_t star_idx = game::Ship_Orbit_Row(game::current_state, ship->position);
                        if (HACCESS::Star_Has_Relocation(star_idx, player_key)) {
                            int16_t target_star_idx = HACCESS::Relocation(star_idx, player_key);

                            const game::id_ship keys[] = {ship->id};
                            const auto destination = game::Destination_At_Row(game::current_state, target_star_idx);
                            s_ship_move_info move_info{};
                            SHIPMOVE::Evaluate_Fleet_Move(game::current_state, keys, destination, &move_info);
                            if (move_info.moving != 0) {
                                SHIPMOVE::Order_Fleet_Move(game::current_state, keys, destination, &move_info);
                            }
                        }
                    }
                }
        }

        if (any_ships_redirected) {
            COLCALC::Do_Player_Colony_Calculations(player_key);
        }
    }

    void __cdecl Add_Report(s_report* report) {
        if (n_reports < MAX_REPORTS) {
            reports[n_reports] = *report;
            n_reports++;
        }
    }

    int8_t __cdecl Has_Report(game::id_player player_key, bool show_messages) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        int8_t status = 0;
        char buf[200];

        if (REPORT::n_reports > 0) {
            for (int16_t i = 0; i < REPORT::n_reports; ++i) {
                if (legacy_world::Encode_Player_Slot(REPORT::reports[i].recipient.get()) == player_idx) {
                    status = 1;
                    if (show_messages) {
                        bool report_handled = false;

                        // Check for special report type (steal tech)
                        if (REPORT::reports[i].type == 1 && REPORT::reports[i].action == 0) {
                            status = 2;
                            MOVEBOX::Remove_Box_Check();
                            SCIENCE::Show_Off_Stolen_Tech(
                                REPORT::reports[i].d.steal.tech_app_id,
                                REPORT::reports[i].d.steal.victim.get());
                            report_handled = true;
                        } else if (REPORT::reports[i].type == 3) {
                            status = 2;
                            MOVEBOX::Remove_Box_Check();
                            SCIENCE::Show_Off_Artifact_Tech(
                                REPORT::reports[i].d.artifacts.tech_app_id,
                                REPORT::reports[i].d.artifacts.planet_id.get());
                            report_handled = true;
                        }

                        if (!report_handled) {
                            REPORT::sprintf_report(buf, sizeof(buf), &REPORT::reports[i]);
                            GENDRAW::Message_Box(buf);
                        }

                        REPORT::reports[i].recipient = legacy_world::Decode_Player_Slot(-1);
                    }
                }
            }
        }

        return status;
    }

    int8_t __cdecl Empty_Colony_Queue(game::id_player player_key, int8_t do_jump) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        s_colony* target_colony = nullptr;

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony* colony = &roster_colony;
            // Check if colony belongs to player, is not an outpost, and its production queue is empty.
            if (legacy_world::Encode_Player_Slot(colony->owner) == player_idx && colony->outpost_flag == 0 && colony->producing[0] == COLONY_PRODUCTION_NONE) {
                target_colony = colony;
                break;
            }
        }

        if (target_colony == nullptr) {
            return 0;
        }

        if (do_jump != 0) {
            MOVEBOX::Remove_Box_Check();
            MOX::return_screen = SCREEN_REPORTS;
            MOX::current_screen = SCREEN_COLONY;

            // Set up planet specific data for the colony screen jump
            MOX::screen_star_id = game::current_state.planets.at(target_colony->planet_id).star_id;
            COLONY::screen_colony_id = target_colony->id;
            MOX::global_esc = 1;
        }

        return 3;
    }

} // namespace REPORT
