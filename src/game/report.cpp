#include "pch.h"

namespace REPORT {
    uint8_t no_outpost_prompt;
    int16_t n_reports;
    s_report reports[MAX_REPORTS];
    uint8_t done_player_reports;
    uint8_t something_interesting_happened[MAX_PLAYERS];
    uint8_t done_colonize_check[BITMAP(MAX_STARS)];
    uint8_t displayed_msgs;
    uint8_t displayed_gnn;

    char* your_spy_destroys_fmt;
    char* your_spy_steals_fmt;
    char* you_framed_fmt;
    char* colonize_preselected_planet_fmt;
    char* enemy_spy_destroys_fmt;
    char* really_trash_colony_base_fmt;
    char* enemy_spy_steals_fmt;
    char* outpost_preselected_planet_fmt;

    s_colony* __cdecl Player_Colony_Base_At_Star(int16_t player_idx, int16_t star_idx) {
        for (int i = 0; i < 5; ++i) {
            int16_t planet_idx = MOX::star[star_idx].planet_index[i];

            if (planet_idx != -1) {
                int16_t colony_idx = MOX::planet[planet_idx].colony_index;

                if (colony_idx != -1) {
                    s_colony* colony = &MOX::colony[colony_idx];

                    if (colony->owner == player_idx && colony->buildings[BUILDING_COLONY_BASE] > 0) {
                        return colony;
                    }
                }
            }
        }

        return nullptr;
    }

    s_ship_data* __cdecl Player_Ship_Type_At_Star(int16_t owner, int16_t ship_type, int16_t location, int8_t include_just_built) {
        s_ship_data* ship = MOX::ship;
        int num_ships = (int)MOX::NUM_SHIPS;
        s_ship_data* end = &ship[num_ships];

        while (ship < end) {
            if ((short)(int8_t)ship->d.ship_type == ship_type &&
                (short)ship->owner == owner &&
                ship->location == location &&
                ship->status < 5) {
                if (include_just_built != 0 || ship->just_built == 0) {
                    return ship;
                }
            }
            ship++;
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

    int8_t __cdecl Has_Msg(int16_t player_idx, int8_t show_summary) {
        int8_t result = 0;

        // Check if the summary for this player hasn't been displayed yet
        if ((((uint8_t)REPORT::displayed_msgs >> (uint8_t)player_idx) & 1) == 0) {
            int16_t num_msgs = MSG_::N_Msgs_For_Player(player_idx);

            if (num_msgs > 0 && MOX::settings.end_of_turn_summary != 0) {
                int16_t first_idx = MSG_::First_Msg_For_Player(player_idx);
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
                            if ((uint8_t)msg->u.diplomacy.plr_a == (uint8_t)MOX::PLAYER_NUM) {
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
                        uint8_t mask = 1;
                        mask = (uint8_t)(mask << (uint8_t)player_idx);
                        REPORT::displayed_msgs |= mask;
                    }
                }
            }
        }

        return result;
    }

    int8_t __cdecl Colonization(int16_t player_idx, int8_t show_dialog) {
        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (MOX::star[star_idx].colonize_player == player_idx) {
                if (struct_::Test_Bit_Field(done_colonize_check, star_idx) == 0) {
                    s_colony* colony_base = Player_Colony_Base_At_Star(player_idx, star_idx);
                    s_ship_data* colony_ship = Player_Ship_Type_At_Star(player_idx, SHIP_TYPE_COLONY, star_idx, 0);
                    s_ship_data* colony_prompt_ship = colony_ship;
                    s_ship_data* outpost_ship = Player_Ship_Type_At_Star(player_idx, SHIP_TYPE_OUTPOST, star_idx, 0);

                    if (no_outpost_prompt != 0) {
                        outpost_ship = nullptr;
                        if (colony_prompt_ship != nullptr &&
                            COLONIZE::Star_N_Colonizable_Planets_For_Player(
                                &MOX::star[star_idx], player_idx) == 0) {
                            colony_prompt_ship = nullptr;
                        }
                    }

                    bool has_ships = false;
                    if (colony_prompt_ship != nullptr || outpost_ship != nullptr) {
                        has_ships = true;
                    }

                    if (colony_base != nullptr || (has_ships && ((MOX::star[star_idx].ignore_colony_ships >> player_idx) & 1) == 0)) {
                        if (show_dialog != 0) {
                            do {
                                bool is_colony = false;
                                MOVEBOX::Remove_Box_Check();

                                int16_t planet_idx;
                                if (colony_base == nullptr && (colony_prompt_ship == nullptr || outpost_ship != nullptr)) {
                                    planet_idx = PLNTSUM::Colony_Ship_Auto_Colonization(outpost_ship);
                                    if (planet_idx == PLANET_INDEX_NONE) {
                                        planet_idx = MAINPUPS::New_Outpost_Selection_Popup(star_idx);
                                    } else if (MOX::planet[planet_idx].colony_index == -1) {
                                        char msg[200];
                                        snprintf(msg, sizeof(msg), outpost_preselected_planet_fmt, HACCESS::Get_Planet_Name(planet_idx));
                                        if (GENDRAW::Do_Confirmation_Box(msg) == 0) {
                                            planet_idx = PLANET_INDEX_NONE;
                                        } else {
                                            if (namestar::Can_Change_Star_Name(star_idx) != 0 && MOX::settings.animations_on == 0) {
                                                int16_t y = namestar::Star_Name_Popup_Screen_Center_Y();
                                                int16_t x = namestar::Star_Name_Popup_Screen_Center_X();
                                                namestar::Change_Star_Name_Popup(x, y, star_idx, 0);
                                            }
                                        }
                                    } else {
                                        planet_idx = MAINPUPS::New_Outpost_Selection_Popup(star_idx);
                                    }
                                } else {
                                    is_colony = true;
                                    bool auto_colonized = false;

                                    if (colony_base == nullptr && PLNTSUM::Colony_Ship_Auto_Colonization(colony_prompt_ship) != PLANET_INDEX_NONE) {
                                        planet_idx = PLNTSUM::Colony_Ship_Auto_Colonization(colony_prompt_ship);
                                        if (MOX::planet[planet_idx].colony_index == -1) {
                                            auto_colonized = true;
                                            char msg[200];
                                            snprintf(msg, sizeof(msg), colonize_preselected_planet_fmt, HACCESS::Get_Planet_Name(planet_idx));
                                            if (GENDRAW::Do_Confirmation_Box(msg) == 0) {
                                                planet_idx = PLANET_INDEX_NONE;
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
                                        planet_idx = MAINPUPS::New_Colony_Selection_Popup(star_idx, colony_base == nullptr);
                                    }

                                    while (colony_base != nullptr && planet_idx < 0) {
                                        int refund = TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                                        char msg[200];
                                        snprintf(msg, sizeof(msg), really_trash_colony_base_fmt, refund);
                                        if (GENDRAW::Do_Confirmation_Box(msg) == 0) {
                                            planet_idx = MAINPUPS::New_Colony_Selection_Popup(star_idx, colony_base == nullptr);
                                        } else {
                                            do {
                                                MOX::player[player_idx].bc += refund;
                                                colony_base->buildings[BUILDING_COLONY_BASE] = 0;
                                                colony_base = Player_Colony_Base_At_Star(player_idx, star_idx);
                                            } while (colony_base != nullptr);
                                            break;
                                        }
                                    }
                                }

                                if (colony_ship != nullptr) {
                                    SHIPS::Clear_Auto_Colonization_Flag(colony_ship);
                                }
                                if (outpost_ship != nullptr) {
                                    SHIPS::Clear_Auto_Colonization_Flag(outpost_ship);
                                }

                                if (planet_idx < 0) {
                                    struct_::Set_Bit_Field(done_colonize_check, star_idx);
                                    MOX::star[star_idx].ignore_colony_ships |= (1 << player_idx);
                                } else {
                                    if (is_colony) {
                                        COLONIZE::Make_New_Colony(planet_idx, player_idx);
                                        if (colony_base == nullptr) {
                                            SHIPS::Kill_Ship((int16_t)(colony_ship - MOX::ship));
                                            COLCALC::Update_Player_Stats(&MOX::player[player_idx]);
                                            MAINSCR::Update_Ship_Icons_In_Between_Turns();
                                            colony_base = nullptr;
                                        } else {
                                            ERICNET::Remove_Building((int16_t)(colony_base - MOX::colony), BUILDING_COLONY_BASE);
                                            colony_base = Player_Colony_Base_At_Star(player_idx, star_idx);
                                        }

                                        if (COLONIZE::Star_N_Colonizable_Planets_For_Player(&MOX::star[star_idx], player_idx) == 0) {
                                            struct_::Set_Bit_Field(done_colonize_check, star_idx);
                                            if (colony_base != nullptr) {
                                                do {
                                                    MOX::player[player_idx].bc += TECHDATA::buildings[BUILDING_COLONY_BASE].cost / 2;
                                                    colony_base->buildings[BUILDING_COLONY_BASE] = 0;
                                                    colony_base = Player_Colony_Base_At_Star(player_idx, star_idx);
                                                } while (colony_base != nullptr);
                                            }
                                        }

                                        if (MOX::settings.animations_on != 0) {
                                            MOX::current_screen = 0x21; /* SCR_COLONIZE */
                                            MOX::return_screen = SCREEN_REPORTS;
                                            MOX::screen_data = MOX::planet[planet_idx].star_index;
                                            MOX::global_esc = 1;
                                            COLONY::orbit_temp = MOX::planet[planet_idx].orbit;
                                            return 3;
                                        }
                                        return 1;
                                    } else {
                                        COLONIZE::Make_New_Outpost(planet_idx, player_idx);
                                        SHIPS::Kill_Ship((int16_t)(outpost_ship - MOX::ship));
                                        COLCALC::Update_Player_Stats(&MOX::player[player_idx]);
                                        MAINSCR::Update_Ship_Icons_In_Between_Turns();
                                    }
                                }
                            } while (colony_base != nullptr);
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

    int8_t __cdecl Display_Report_Aux(int16_t player_idx, uint8_t show_ui) {
        int8_t status;

        if (((done_player_reports >> (uint8_t)player_idx) & 1) != 0) {
            return 0;
        }

        if (MOX::stardate == 35000) {
            return 0;
        }

        if ((status = Antarans(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_GNN((uint8_t)player_idx, show_ui != 0)) != 0) return status;
        if ((status = Set_Initial_Tech(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Report(player_idx, show_ui != 0)) != 0) return status;
        if ((status = Has_Research_Breakthrough(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Evolutionary_Upgrade(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Explored_New_Star(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Colonization(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Officer_For_Hire(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = OFFICER::Officer_Made_Level_Report(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Has_Diplomacy_Messages(player_idx, show_ui != 0)) != 0) return status;
        if ((status = Has_Msg(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Empty_Colony_Queue(player_idx, (int8_t)show_ui)) != 0) return status;
        if ((status = Need_To_Set_Occupation_Policy(player_idx, (int8_t)show_ui)) != 0) return status;

        done_player_reports |= (1 << (uint8_t)player_idx);

        return 0;
    }

    uint8_t __cdecl Display_Report(int16_t player_idx) {
        return (uint8_t)REPORT::Display_Report_Aux(player_idx, 1);
    }

    void __cdecl sprintf_report(char* buffer, uint32_t buffer_size, s_report* report) {
        if (buffer == nullptr || buffer_size == 0 || report == nullptr) {
            return;
        }

        buffer[0] = '\0';

        if (report->type == 1 /* MSG_TYPE_REPORT_YOUR_SPY */) {
            if (report->action == 0 /* SPY_ACTION_STEAL */) {
                snprintf(buffer, buffer_size, REPORT::your_spy_steals_fmt, TECHDATA::technology_applications[report->d.steal.tech_app_id].name, MOX::player[report->d.steal.victim_player_idx].race_name);
            } else if (report->action == 1 /* SPY_ACTION_SABOTAGE */) {
                int16_t planet_idx = report->d.sabotage.planet_idx;
                char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
                snprintf(buffer, buffer_size, REPORT::your_spy_destroys_fmt, MOX::player[report->d.sabotage.victim_player_idx].race_name, TECHDATA::buildings[report->d.sabotage.building_id].name, planet_name);
            }

            if (report->d.steal.framed_player_idx != -1) {
                const size_t used_length = strlen(buffer);
                if (used_length < buffer_size) {
                    snprintf(buffer + used_length, buffer_size - used_length, REPORT::you_framed_fmt, MOX::player[report->d.steal.framed_player_idx].race_name);
                }
            }
        } else if (report->type == 2 /* MSG_TYPE_REPORT_ENEMY_SPY */) {
            if (report->action == 0 /* SPY_ACTION_STEAL */) {
                snprintf(buffer, buffer_size, REPORT::enemy_spy_steals_fmt, MOX::player[report->d.enemy_steal.enemy_player_idx].race_name, TECHDATA::technology_applications[report->d.enemy_steal.tech_app_id].name);
            } else if (report->action == 1 /* SPY_ACTION_SABOTAGE */) {
                int16_t planet_idx = report->d.enemy_sabotage.planet_idx;
                char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
                snprintf(buffer, buffer_size, REPORT::enemy_spy_destroys_fmt, MOX::player[report->d.enemy_sabotage.enemy_player_idx].race_name, TECHDATA::buildings[report->d.enemy_sabotage.building_id].name, planet_name);
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
        done_player_reports = 0;
        displayed_msgs = 0;
        displayed_gnn = 0;

        // Reset colonization checks for all tracked stars.
        memset(done_colonize_check, 0, sizeof(done_colonize_check));

        MOX::antarans.attack_notified_mask = 0;
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
    int8_t __cdecl Has_GNN(uint8_t player_idx, bool mark_as_displayed) {
        int8_t status = 0;
        uint8_t displayed_mask = REPORT::displayed_gnn;

        if ((((uint8_t)(displayed_mask >> player_idx)) & 1u) == 0) {
            if (EVENTS::Check_For_Event_Message() != 0) {
                status = 7;
                if (mark_as_displayed) {
                    EVENTS::Check_For_Event();
                    REPORT::displayed_gnn = (uint8_t)(REPORT::displayed_gnn | (uint8_t)(1u << player_idx));
                }
            }
        }

        return status;
    }

    uint8_t __cdecl Must_Display_Reports(int16_t player_idx) {
        int8_t displayed = 0;

        if (MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            displayed = REPORT::Display_Report_Aux(player_idx, 0);
        }

        if (displayed == 0) {
            REPORT::Redirect_Newly_Built_Ships(player_idx);

            if (displayed == 0) {
                if (MOX::game_type >= 2 && MOX::game_type <= 3) {
                    NEXTTURN::Do_Autosave();
                }
            }
        }

        return displayed;
    }

    int8_t __cdecl Set_Initial_Tech(int16_t player_idx, int8_t select_tech) {
        if (initial_tech_prompt != 0 && MOX::stardate > 35000) {
            uint8_t field_idx = (uint8_t)MOX::player[player_idx].current_research_field;

            // Check if current research field progress is at state 3 and field is 0
            if (MOX::player[player_idx].tech_fields[field_idx] == 3 && field_idx == TECH_FIELD_STARTING_TECH) {
                if (select_tech != 0) {
                    TECH::Tech_Select();
                }
                return 2;
            }
        }
        return 0;
    }

    int8_t __cdecl Has_Officer_For_Hire(int16_t player_idx, int8_t hire_flag) {
        int8_t has_officer = 0;
        int8_t officer_id = MOX::player[player_idx].officer_for_hire;

        if (officer_id != -1) {
            has_officer = 1;

            if (hire_flag != 0) {
                bool accepted = MAINPUPS::Random_New_Officer_Popup(player_idx);
                int32_t cost = OFFICER::Calculate_Officer_Cost(player_idx, officer_id);

                OFFICER::Set_Officer_To_Player(officer_id, player_idx, (uint8_t)accepted);

                if (accepted) {
                    MOX::player[player_idx].bc -= cost;
                    COLCALC::Do_Player_Colony_Calculations(player_idx);
                    if (OFFICER::rejected_offer_keeps_timer != 0) {
                        // The offer was generated on the preceding turn.
                        MOX::player[player_idx].stardate_last_offered_hero =
                            static_cast<uint16_t>(MOX::stardate - 1);
                    }
                }

                MOX::player[player_idx].officer_for_hire = -1;
            }
        }

        return has_officer;
    }

    int8_t __cdecl Has_Research_Breakthrough(int16_t player_idx, int8_t do_select) {
        int8_t status = 0;

        if (MOX::player[player_idx].research_breakthrough != 0) {
            status = 1;
            if (do_select != 0) {
                TECH::Tech_Select();
                MOX::player[player_idx].research_breakthrough = 0;
                status = 2;
            }
        }

        return status;
    }

    int8_t __cdecl Has_Evolutionary_Upgrade(int16_t player_idx, int8_t do_setup) {
        int8_t result = 0;
        s_player* plr = &MOX::player[player_idx];

        // Check if player has researched Evolutionary Mutation (Tech App 62)
        // and hasn't picked the upgrade yet.
        if (plr->tech_applications[TECH_APP_EVOLUTIONARY_MUTATION] == TECH_RESEARCH_STATUS_RESEARCHED &&
            plr->picked_evolutionary_upgrade == 0) {
            result = 1;

            if (do_setup != 0) {
                raceopt::Setup_Evolutionary_Upgrade(player_idx);
            }
        }

        return result;
    }

    int8_t __cdecl Draw_Special(s_star_data* star, int16_t player_idx) {
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

    int8_t __cdecl Star_Has_Special_For_Player(s_star_data* star, int player_idx) {
        int8_t has_special = 0;

        if (star->system_special >= 50) {
            if ((star->system_special % 10) == player_idx) {
                has_special = 1;
            }
        }

        return has_special;
    }

    int8_t __cdecl Explored_New_Star(int16_t player_idx, int8_t show_popup) {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];
            int8_t has_special = REPORT::Star_Has_Special_For_Player(star, (int)player_idx);

            // Check if player just visited this star or it contains a relevant special
            if (((star->just_visited >> (uint8_t)player_idx) & 1) != 0 || has_special != 0) {
                // If telepathic and the star discovery is not a special, clear discovery flag and skip report
                if (MOX::player[player_idx].traits[27] != 0 && has_special == 0) {
                    star->just_visited &= ~(1 << (uint8_t)player_idx);
                    return 0;
                }

                if (show_popup != 0) {
                    MOVEBOX::Remove_Box_Check();
                    char special_type = REPORT::Draw_Special(star, player_idx);
                    int16_t star_idx = (int16_t)(star - MOX::star);
                    MAINPUPS::New_System_Discovery_Popup(star_idx, (int16_t)special_type);

                    // Mark as no longer "just visited" after reporting
                    star->just_visited &= ~(1 << (uint8_t)player_idx);

                    if (has_special != 0) {
                        star->system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    }
                }
                return 1;
            }
        }
        return 0;
    }

    int8_t __cdecl Has_Diplomacy_Messages(int16_t receiver_idx, bool show_messages) {
        int8_t has_messages = 0;

        for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
            // A player doesn't send messages to themselves
            if (player_idx == receiver_idx) {
                continue;
            }

            // Check if the receiver is ignoring the sender
            if (!((MOX::player[receiver_idx].ignoring >> (player_idx & 0x1F)) & 1)) {
                // Check if player_idx has a message for receiver_idx
                uint8_t msg_id = MOX::player[player_idx].diplomacy_message[receiver_idx];
                if (msg_id != 0) {
                    has_messages = 5;

                    if (show_messages) {
                        // Check for multiplayer/interactive diplomacy requirements
                        if ((MOX::game_type == 2 || MOX::game_type == 3) &&
                            DIP_SCRN::Diplomacy_Message_Is_Interactive((uint16_t)msg_id)) {
                            NETMOX::Net_Request_AI_Diplomacy((uint8_t)player_idx);

                            while (!NETMOX::Net_May_Begin_AI_Diplomacy()) {
                                RUSS::Mox_Update();
                            }

                            DIP_SCRN::Npc_Diplomacy_Screen(player_idx, 0);
                            MOX::player[player_idx].diplomacy_message[receiver_idx] = 0;
                            NETMOX::Net_End_AI_Diplomacy();
                        } else {
                            // Standard single-player or non-interactive message display
                            DIP_SCRN::Npc_Diplomacy_Screen(player_idx, 0);
                            MOX::player[player_idx].diplomacy_message[receiver_idx] = 0;
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
    int8_t __cdecl Antarans(int16_t unused_player_idx, int8_t show_notification) {
        int8_t result = 0;
        (void)unused_player_idx;

        // Check if stardate is exactly one day past invasion date
        if (((int32_t)MOX::antarans.invasion_stardate - (int32_t)MOX::stardate) == -1) {
            // Check if current player was already notified
            if (!((MOX::antarans.attack_notified_mask >> (uint8_t)MOX::PLAYER_NUM) & 1)) {
                result = 6;
                if (show_notification != 0) {
                    int16_t target_star_idx = -1;
                    uint8_t max_turns_left = 0;

                    // Locate the Antaran ship with the longest remaining travel time to determine target
                    for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                        s_ship_data* ship = &MOX::ship[i];
                        if (ship->owner == 8 /* Antaran */ && ship->status == 1 /* Active */) {
                            if (max_turns_left < (uint8_t)ship->turns_left) {
                                target_star_idx = ship->location - SHIP_LOCATION_MOVING_OFFSET;
                                max_turns_left = ship->turns_left;
                            }
                        }
                    }

                    bool room_screen_shown = false;
                    if (target_star_idx != -1) {
                        // Check if player has a colony at the target system
                        if ((MOX::star[target_star_idx].has_colony >> (uint8_t)MOX::PLAYER_NUM) & 1) {
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
                    MOX::antarans.attack_notified_mask |= (1 << (uint8_t)MOX::PLAYER_NUM);
                }
            }
        }

        return result;
    }

    int8_t __cdecl Need_To_Set_Occupation_Policy(int16_t player_idx, int8_t show_screen) {
        int16_t num_colonies = MOX::NUM_COLONIES;
        s_colony* colonies = MOX::colony;

        for (int16_t i = 0; i < num_colonies; ++i) {
            s_colony* colony = &colonies[i];

            if (colony->owner == (int8_t)player_idx &&
                COLONY::Colony_Needs_Occupation_Policy(i)) {
                if (show_screen != 0) {
                    MOVEBOX::Remove_Box_Check();
                    MOX::current_screen = SCREEN_COLONY;
                    MOX::return_screen = SCREEN_REPORTS;

                    int16_t planet_idx = colony->planet;
                    s_planet_data* planet = &MOX::planet[planet_idx];

                    MOX::screen_data = planet->star_index;
                    COLONY::orbit_temp = (int16_t)planet->orbit;

                    COLONY::Colony_Screen();
                }
                return 1;
            }
        }

        return 0;
    }

    void __cdecl Redirect_Newly_Built_Ships(int16_t player_idx) {
        bool any_ships_redirected = false;
        s_ship_data* ship = MOX::ship;
        s_ship_data* end_ship = &MOX::ship[MOX::NUM_SHIPS];

        if (ship < end_ship) {
            do {
                if (ship->owner == (int8_t)player_idx && ship->just_built != 0) {
                    any_ships_redirected = true;
                    ship->just_built = 0;

                    // If ship is active (not destroyed/refitting)
                    if (ship->status == 0) {
                        int16_t star_idx = ship->location;
                        if (HACCESS::Star_Has_Relocation(star_idx, player_idx)) {
                            int16_t target_star_idx = HACCESS::Relocation(star_idx, player_idx);

                            int16_t ship_indices[2];
                            s_ship_move_info move_info;

                            ship_indices[0] = (int16_t)(ship - MOX::ship);
                            ship_indices[1] = -1; // Terminator

                            SHIPMOVE::Ships_Try_To_Move_To(ship_indices, target_star_idx, &move_info);
                            if (move_info.moving != 0) {
                                SHIPMOVE::Make_Ships_Move_To(ship_indices, target_star_idx, &move_info);
                            }
                        }
                    }
                }
                ship++;
            } while (ship < end_ship);
        }

        if (any_ships_redirected) {
            COLCALC::Do_Player_Colony_Calculations(player_idx);
        }
    }

    void __cdecl Add_Report(s_report* report) {
        if (n_reports < MAX_REPORTS) {
            reports[n_reports] = *report;
            n_reports++;
        }
    }

    int8_t __cdecl Has_Report(int16_t player_idx, bool show_messages) {
        int8_t status = 0;
        char buf[200];

        if (REPORT::n_reports > 0) {
            for (int16_t i = 0; i < REPORT::n_reports; ++i) {
                if (REPORT::reports[i].player_idx == player_idx) {
                    status = 1;
                    if (show_messages) {
                        bool report_handled = false;

                        // Check for special report type (steal tech)
                        if (REPORT::reports[i].type == 1 && REPORT::reports[i].action == 0) {
                            status = 2;
                            MOVEBOX::Remove_Box_Check();
                            SCIENCE::Show_Off_Stolen_Tech(
                                REPORT::reports[i].d.steal.tech_app_id,
                                REPORT::reports[i].d.steal.victim_player_idx);
                            report_handled = true;
                        } else if (REPORT::reports[i].type == 3) {
                            status = 2;
                            MOVEBOX::Remove_Box_Check();
                            SCIENCE::Show_Off_Artifact_Tech(
                                REPORT::reports[i].d.artifacts.tech_app_id,
                                REPORT::reports[i].d.artifacts.planet_idx);
                            report_handled = true;
                        }

                        if (!report_handled) {
                            REPORT::sprintf_report(buf, sizeof(buf), &REPORT::reports[i]);
                            GENDRAW::Message_Box(buf);
                        }

                        REPORT::reports[i].player_idx = -1;
                    }
                }
            }
        }

        return status;
    }

    int8_t __cdecl Empty_Colony_Queue(int16_t player_idx, int8_t do_jump) {
        s_colony* target_colony = nullptr;
        int16_t num_colonies = MOX::NUM_COLONIES;

        for (int16_t i = 0; i < num_colonies; ++i) {
            s_colony* colony = &MOX::colony[i];
            // Check if colony belongs to player, is not an outpost, and its production queue is empty.
            if (colony->owner == player_idx && colony->outpost_flag == 0 && colony->producing[0] == COLONY_PRODUCTION_NONE) {
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
            MOX::screen_data = MOX::planet[target_colony->planet].star_index;
            COLONY::orbit_temp = (int16_t)MOX::planet[target_colony->planet].orbit;
            MOX::global_esc = 1;
        }

        return 3;
    }

} // namespace REPORT
