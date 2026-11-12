#include "pch.h"

#include "strbuild.h"

namespace COLCALC {
    void __cdecl Colony_Morale(s_colony* colony, STRBUILD::s_builder* debug_out) {
        int8_t owner = colony->owner;
        s_player* player = &MOX::player[owner];
        int8_t govt = player->traits[TRAIT_CURRENT_GOVERNMENT];
        int8_t gov_type = govt / 2;

        int leader_bonus = 0;
        int no_capital_penalty = 0;
        int psychophysics_bonus = 0;
        int holo_simulator_bonus = 0;
        int vr_network_bonus = 0;
        int pleasure_dome_bonus = 0;
        int barracks_bonus = 0;
        int marine_barracks_bonus = 0;
        const char* msg_barracks = "";

        int mixed_race_penalty = Mixed_Race_Morale_Penalty(colony);
        colony->morale = morale_lookup_table[govt];

        if (gov_type != GOVERNMENT_UNIFICATION / 2) {
            int officer_idx = Colony_Officer(colony);
            if (officer_idx != -1) {
                s_leader_data* leader = &MOX::leaders[officer_idx];
                if (leader->special_skills & LEADER_COLONY_SKILL_SPIRITUAL_LEADER2) {
                    leader_bonus = (OFFICER::Owned_Officer_Level(officer_idx) * 3 + 4) / 2;
                } else if (leader->special_skills & LEADER_COLONY_SKILL_SPIRITUAL_LEADER) {
                    leader_bonus = OFFICER::Owned_Officer_Level(officer_idx) + 1;
                }
            }

            if (colony->buildings[BUILDING_HOLO_SIMULATOR]) holo_simulator_bonus = morale_bonus.holo_simulator;
            if (player->tech_applications[TECH_APP_VIRTUAL_REALITY_NETWORK] == TECH_RESEARCH_STATUS_RESEARCHED) vr_network_bonus = morale_bonus.virtual_reality_network;
            if (colony->buildings[BUILDING_PLEASURE_DOME]) pleasure_dome_bonus = morale_bonus.pleasure_dome;

            int8_t owner_trait0 = MOX::player[owner].traits[TRAIT_CURRENT_GOVERNMENT];
            int8_t owner_gov = owner_trait0 / 2;
            if (owner_gov != GOVERNMENT_UNIFICATION / 2) {
                int16_t home_id = player->home_planet_id;
                bool has_capitol = false;
                if (home_id != -1) {
                    int16_t cap_col_idx = MOX::planet[home_id].colony_index;
                    if (MOX::colony[cap_col_idx].buildings[BUILDING_CAPITOL]) has_capitol = true;
                }
                if (!has_capitol) no_capital_penalty = non_capital_morale_lookup[owner_gov];
            }

            if (govt <= static_cast<int8_t>(psionics_government_limit) &&
                player->tech_applications[TECH_APP_PSIONICS] ==
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                psychophysics_bonus = morale_bonus.psionics;
            }

            if (colony->buildings[BUILDING_MARINE_BARRACKS]) {
                marine_barracks_bonus = barracks_morale_lookup[govt];
            } else if (colony->buildings[BUILDING_ARMOR_BARRACKS]) {
                barracks_bonus = barracks_morale_lookup[govt];
            } else if (gov_type < GOVERNMENT_DEMOCRACY / 2) {
                msg_barracks = no_barracks_msg;
            }

            colony->morale += mixed_race_penalty + leader_bonus + holo_simulator_bonus +
                vr_network_bonus + pleasure_dome_bonus + psychophysics_bonus +
                barracks_bonus + marine_barracks_bonus + no_capital_penalty;
        }

        if (debug_out) {
            if (gov_type == GOVERNMENT_UNIFICATION / 2) {
                STRBUILD::Copy(debug_out, unification_has_no_morale_msg);
                return;
            }

            // fmtpara: \aT+15,+65. defines tab stops for the column layout, \x1A0 restores
            // left alignment, and the row formats below use \t/\r for column and paragraph breaks.
            STRBUILD::Sprintf(debug_out, "%s", "\x07"
                                               "T+15,+65.");
            STRBUILD::Sprintf(debug_out, "%s", "\x1A"
                                               "0");

            STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s%s\r", morale_lookup_table[govt] * 5, government_morale_msg, msg_barracks);

            if (no_capital_penalty != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", no_capital_penalty * 5, no_capital_msg);

            if (mixed_race_penalty != 0)
                STRBUILD::Sprintf(debug_out, "\t%d%%\t%s\r", mixed_race_penalty * 5, mixed_races_msg);

            if (barracks_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", barracks_bonus * 5, TECHDATA::buildings[BUILDING_ARMOR_BARRACKS].name);

            if (marine_barracks_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", marine_barracks_bonus * 5, TECHDATA::buildings[BUILDING_MARINE_BARRACKS].name);

            if (psychophysics_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", psychophysics_bonus * 5, TECHDATA::technology_applications[TECH_APP_PSIONICS].name);

            if (holo_simulator_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", holo_simulator_bonus * 5, TECHDATA::buildings[BUILDING_HOLO_SIMULATOR].name);

            if (vr_network_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", vr_network_bonus * 5, TECHDATA::technology_applications[TECH_APP_VIRTUAL_REALITY_NETWORK].name);

            if (pleasure_dome_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", pleasure_dome_bonus * 5, TECHDATA::buildings[BUILDING_PLEASURE_DOME].name);

            if (leader_bonus != 0)
                STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", leader_bonus * 5, leader_bonus_msg);

            STRBUILD::Sprintf(debug_out, "\r");
            STRBUILD::Sprintf(debug_out, "\t%+d%%\t%s\r", colony->morale * 5, total_morale_msg);
        }
    }

    void __cdecl Twiddle_Initial_Homeworld(s_colony* colony) {
        uint32_t* p_pop = (uint32_t*)colony->pop;
        uint32_t* p_pop_end = &p_pop[colony->n_pops];
        uint32_t* last_adjusted_pop = nullptr;

        RUSS::Mox_Update();

        // Convert all initial population to Scientists
        for (uint32_t* p = p_pop; p < p_pop_end; ++p) {
            POP::Set_Prof(p, POP::PROF_SCIENTIST);
        }

        int16_t star_idx = MOX::planet[colony->planet].star_index;
        MOX::star[star_idx].ignore_colony_ships |= (1 << (colony->owner & 0x1F));

        Recalculate_Colony(colony);

        // Convert Scientists to Workers to meet industry maintenance requirements
        while (p_pop < p_pop_end && colony->production[ECON_INDUSTRY] < (int16_t)colony->maintenance[ECON_INDUSTRY]) {
            POP::Set_Prof(p_pop, POP::PROF_WORKER);
            Recalculate_Colony(colony);
            last_adjusted_pop = p_pop;
            p_pop++;
        }

        // Convert Pops to Farmers if colony has food multipliers and needs to meet maintenance
        if (colony->food2_per_farmer != 0) {
            while (p_pop < p_pop_end && (int16_t)colony->maintenance[ECON_FOOD] > colony->production[ECON_FOOD]) {
                POP::Set_Prof(p_pop, POP::PROF_FARMER);
                Recalculate_Colony(colony);
                last_adjusted_pop = p_pop;
                p_pop++;
            }
        }

        // Convert Scientists to Workers if Research production exceeds Industry
        while (p_pop < p_pop_end && colony->production[ECON_RESEARCH] > colony->production[ECON_INDUSTRY]) {
            POP::Set_Prof(p_pop, POP::PROF_WORKER);
            Recalculate_Colony(colony);
            last_adjusted_pop = p_pop;
            p_pop++;
        }

        // If Industry is balanced or higher than Research, switch the last adjusted Pop back to Scientist
        if (last_adjusted_pop != nullptr &&
            colony->production[ECON_INDUSTRY] >= colony->production[ECON_RESEARCH]) {
            POP::Set_Prof(last_adjusted_pop, POP::PROF_SCIENTIST);
            Recalculate_Colony(colony);
        }

        // Handle unique system specials (Orion/Guardians/Monsters/Artifacts)
        switch (MOX::star[star_idx].system_special) {
            case SYSTEM_SPECIAL_SPACE_DEBRIS:
            case SYSTEM_SPECIAL_PIRATE_CACHE:
                MOX::star[star_idx].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                break;
            case SYSTEM_SPECIAL_NATIVES:
            case SYSTEM_SPECIAL_SPLINTER_COLONY:
                MOX::star[star_idx].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                for (int i = 0; i < 5; ++i) {
                    int16_t p_idx = MOX::star[star_idx].planet_index[i];
                    if (p_idx != -1) {
                        MOX::planet[p_idx].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    }
                }
                break;
            case SYSTEM_SPECIAL_MAROONED_HERO:
                // Deassign officers if the system special is removed (e.g., Orion conquered)
                for (int16_t i = 0; i < MAX_LEADERS; ++i) {
                    if (MOX::leaders[i].status == 5 && MOX::leaders[i].location == star_idx) {
                        OFFICER::Deassign_Officer(i);
                    }
                }
                MOX::star[star_idx].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                break;
            case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS:
                MOX::star[star_idx].artifacts_gave_app = 1;
                break;
        }
    }

    void __cdecl Pass_Out_Imports(int16_t player_idx, s_colony* colony) {
        s_player* player = &MOX::player[player_idx];
        int16_t needy_colony_indices[MAX_COLONIES];
        int16_t num_needy = 0;
        int16_t total_surplus = 0;
        int16_t total_deficit = 0;
        int16_t capacity = player->n_freighters - (player->settlers_freighted * 5);

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* c = &MOX::colony[i];
            if (c->owner == player_idx && c->outpost_flag == 0) {
                if (EVENTS::Event_Check_Space_Anomaly(i) == 0) {
                    int16_t balance = c->production[ECON_FOOD] - (uint8_t)c->maintenance[ECON_FOOD];
                    if (balance < 0) {
                        c->imports[ECON_FOOD] = 0;
                        if (!Colony_Is_Blockaded(c)) {
                            needy_colony_indices[num_needy++] = i;
                            total_deficit -= balance; // Make positive
                        }
                    } else {
                        c->imports[ECON_FOOD] = -balance; // Negative means exporting
                        if (!Colony_Is_Blockaded(c)) {
                            total_surplus += balance;
                        }
                    }
                }
            }
        }

        player->surplus_food = total_surplus - total_deficit;

        if (total_surplus < total_deficit || capacity < total_deficit) {
            int16_t remaining_capacity = capacity;
            int16_t remaining_food = total_surplus;
            int16_t consecutive_skips;
            int16_t needy_idx;

            // Pass 1: "Our Empire" food needs
            needy_idx = 0;
            consecutive_skips = 0;
            while (remaining_capacity > 0 && consecutive_skips < num_needy) {
                int16_t c_idx = needy_colony_indices[needy_idx];
                s_colony* c = &MOX::colony[c_idx];
                if (remaining_food > 0 && (uint8_t)c->food2_needed_for_our_empire > (c->production[ECON_FOOD] + c->imports[ECON_FOOD]) * 2) {
                    remaining_food--;
                    remaining_capacity--;
                    c->imports[ECON_FOOD]++;
                    consecutive_skips = 0;
                } else {
                    consecutive_skips++;
                }
                if (++needy_idx == num_needy) needy_idx = 0;
            }

            // Pass 2: "Our Empire" + "Assimilated" food needs
            needy_idx = 0;
            consecutive_skips = 0;
            while (remaining_capacity > 0 && consecutive_skips < num_needy) {
                int16_t c_idx = needy_colony_indices[needy_idx];
                s_colony* c = &MOX::colony[c_idx];
                int needs = (uint8_t)c->food2_needed_for_our_empire + (uint8_t)c->food2_needed_for_assimilated;
                if (remaining_food > 0 && needs > (c->production[ECON_FOOD] + c->imports[ECON_FOOD]) * 2) {
                    remaining_food--;
                    remaining_capacity--;
                    c->imports[ECON_FOOD]++;
                    consecutive_skips = 0;
                } else {
                    consecutive_skips++;
                }
                if (++needy_idx == num_needy) needy_idx = 0;
            }

            // Pass 3: "Our Empire" + "Assimilated" + "Conquered" food needs
            needy_idx = 0;
            consecutive_skips = 0;
            while (remaining_capacity > 0 && consecutive_skips < num_needy) {
                int16_t c_idx = needy_colony_indices[needy_idx];
                s_colony* c = &MOX::colony[c_idx];
                int needs = (uint8_t)c->food2_needed_for_our_empire + (uint8_t)c->food2_needed_for_assimilated + (uint8_t)c->food2_needed_for_conquered;
                if (remaining_food > 0 && needs > (c->production[ECON_FOOD] + c->imports[ECON_FOOD]) * 2) {
                    remaining_food--;
                    remaining_capacity--;
                    c->imports[ECON_FOOD]++;
                    consecutive_skips = 0;
                } else {
                    consecutive_skips++;
                }
                if (++needy_idx == num_needy) needy_idx = 0;
            }

            // Pass 4: Basic maintenance needs
            needy_idx = 0;
            consecutive_skips = 0;
            while (remaining_capacity > 0 && consecutive_skips < num_needy) {
                int16_t c_idx = needy_colony_indices[needy_idx];
                s_colony* c = &MOX::colony[c_idx];
                if (remaining_food > 0 && c->maintenance[ECON_FOOD] > (c->production[ECON_FOOD] + c->imports[ECON_FOOD])) {
                    remaining_food--;
                    remaining_capacity--;
                    c->imports[ECON_FOOD]++;
                    consecutive_skips = 0;
                } else {
                    consecutive_skips++;
                }
                if (++needy_idx == num_needy) needy_idx = 0;
            }

            int16_t freighted = total_surplus - remaining_food;
            player->food_freighted = freighted;
            if (remaining_capacity == 0) {
                int16_t min_freight = (total_deficit <= total_surplus) ? total_deficit : total_surplus;
                player->surplus_freighters = freighted - min_freight;
            } else {
                player->surplus_freighters = capacity - freighted;
            }
        } else {
            // Resources are sufficient to cover everything
            for (int16_t i = 0; i < num_needy; ++i) {
                int16_t c_idx = needy_colony_indices[i];
                s_colony* c = &MOX::colony[c_idx];
                c->imports[ECON_FOOD] = (uint8_t)c->maintenance[ECON_FOOD] - c->production[ECON_FOOD];
            }
            player->food_freighted = total_deficit;
            player->surplus_freighters = capacity - total_deficit;
        }

        if (colony != nullptr) {
            bool colony_was_updated = false;
            for (int16_t i = 0; i < num_needy; ++i) {
                s_colony* c = &MOX::colony[needy_colony_indices[i]];
                Post_Import_Computing(c);
                if (c == colony) {
                    colony_was_updated = true;
                }
            }
            if (!colony_was_updated) {
                Post_Import_Computing(colony);
            }
        }
    }

    void __cdecl Colony_Replicators(s_colony* colony) {
        colony->n_food_replicated = 0;

        if (colony->buildings[BUILDING_FOOD_REPLICATORS] == 0) {
            return;
        }

        int16_t industry_spent = 0;
        int16_t industry_pool = colony->production[ECON_INDUSTRY] * 2;
        int16_t food_pool = colony->production[ECON_FOOD] * 2;

        int16_t ind_our = (uint8_t)colony->industry2_needed_for_our_empire;
        int16_t ind_androids = (uint8_t)colony->industry2_needed_for_androids;
        int16_t ind_assimilated = (uint8_t)colony->industry2_needed_for_assimilated;
        int16_t ind_conquered = (uint8_t)colony->industry2_needed_for_conquered;

        // Tier 1: Our Empire requirements
        int16_t surplus_ind = (industry_pool - ind_our) / 2;
        if (surplus_ind > 0) {
            int16_t food_goal = (uint8_t)colony->food2_needed_for_our_empire;
            int16_t deficit = (food_goal - food_pool + 1) / 2;
            if (deficit > 0) {
                int16_t cost = (surplus_ind < (deficit * 2)) ? surplus_ind : (deficit * 2);
                cost = (cost / 2) * 2;
                industry_spent = cost;
                food_pool += cost;
                industry_pool -= (cost / 2);
            }

            // Tier 2: Androids and Assimilated populations
            surplus_ind = (industry_pool - ind_our - ind_androids - ind_assimilated) / 2;
            if (surplus_ind > 0) {
                int16_t food_goal_t2 = (int16_t) static_cast<uint8_t>(colony->food2_needed_for_our_empire) + (int16_t) static_cast<uint8_t>(colony->food2_needed_for_assimilated);
                int16_t deficit_t2 = (food_goal_t2 - food_pool + 1) / 2;
                if (deficit_t2 > 0) {
                    int16_t cost = (surplus_ind < (deficit_t2 * 2)) ? surplus_ind : (deficit_t2 * 2);
                    cost = (cost / 2) * 2;
                    industry_spent += cost;
                    food_pool += cost;
                    industry_pool -= (cost / 2);
                }

                // Tier 3: Conquered populations
                surplus_ind = (industry_pool - ind_conquered - ind_our - ind_androids - ind_assimilated) / 2;
                if (surplus_ind > 0) {
                    int16_t food_goal_t3 = (uint8_t)colony->food2_needed_for_our_empire +
                        (uint8_t)colony->food2_needed_for_assimilated +
                        (uint8_t)colony->food2_needed_for_conquered;
                    int16_t deficit_t3 = (food_goal_t3 - food_pool + 1) / 2;
                    if (deficit_t3 > 0) {
                        int16_t cost = (surplus_ind < (deficit_t3 * 2)) ? surplus_ind : (deficit_t3 * 2);
                        cost = (cost / 2) * 2;
                        industry_spent += cost;
                        food_pool += cost;
                        industry_pool -= (cost / 2);
                    }

                    // Tier 4: Native populations
                    surplus_ind = (industry_pool - ind_conquered - ind_our - ind_androids - ind_assimilated) / 2;
                    if (surplus_ind > 0) {
                        int16_t food_goal_t4 = (uint8_t)colony->food2_needed_for_our_empire +
                            (uint8_t)colony->food2_needed_for_assimilated +
                            (uint8_t)colony->food2_needed_for_conquered +
                            (uint8_t)colony->food2_needed_for_natives;
                        int16_t deficit_t4 = (food_goal_t4 - food_pool + 1) / 2;
                        if (deficit_t4 > 0) {
                            int16_t cost = (surplus_ind < (deficit_t4 * 2)) ? surplus_ind : (deficit_t4 * 2);
                            industry_spent += (cost / 2) * 2;
                        }
                    }
                }
            }
        }

        int16_t final_food_gain = industry_spent / 2;
        int16_t final_industry_cost = 0;
        if (replicator_food_pp != 0) {
            int16_t maximum_food_gain = colony->production[ECON_INDUSTRY] / replicator_food_pp;
            if (maximum_food_gain < 0) {
                maximum_food_gain = 0;
            }
            if (final_food_gain > maximum_food_gain) {
                final_food_gain = maximum_food_gain;
            }
            final_industry_cost = final_food_gain * replicator_food_pp;
        }

        colony->n_food_replicated = (int8_t)final_food_gain;
        colony->production[ECON_FOOD] += final_food_gain;
        colony->production[ECON_INDUSTRY] -= final_industry_cost;
    }

    void __cdecl Update_Player_Stats(s_player* plr) {
        int player_idx = static_cast<int>(plr - MOX::player);

        int total_research = 0;
        int total_bc = 0;
        int total_pop = 0;
        int total_food = 0;
        int total_industry = 0;
        int total_surplus_food = 0;
        int total_food_maintenance = 0;

        // Process Colonies
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner == player_idx && colony->outpost_flag == 0) {
                total_pop += static_cast<uint8_t>(colony->n_pops);
                total_food += colony->production[ECON_FOOD];

                if (!Colony_Is_Blockaded(colony)) {
                    total_surplus_food += colony->production[ECON_FOOD];
                    total_food_maintenance += static_cast<uint8_t>(colony->maintenance[ECON_FOOD]);
                }

                total_industry += colony->production[ECON_INDUSTRY];

                if (EVENTS::Event_Check_Colony_Researching(i) == 0) {
                    total_research += colony->production[2];
                }
                total_bc += colony->production[ECON_BC];
            }
        }

        // Process Ships (Science Labs)
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->owner == player_idx) {
                // Check for Scout Lab bit.
                if (struct_::Test_Bit_Field(ship->d.special_device_flags, SPECIAL_SCOUT_LAB) && ship->status < 4) {
                    uint8_t size_class = (uint8_t)ship->d.size;
                    if (size_class < SHIP_SIZE_COUNT) {
                        total_research += scout_lab_research_bonus[size_class];
                    }
                }
            }
        }

        // Process Leaders
        for (int i = 0; i < MAX_LEADERS; ++i) {
            s_leader_data* leader = &MOX::leaders[i];
            if (Player_Owns_Officer(plr, leader) && Leader_At_Anomaly(leader) == 0) {
                uint32_t skills = leader->general_skills;

                if (skills & LEADER_GENERAL_SKILL_MEGAWEALTH2) {
                    total_bc += 15;
                } else if (skills & LEADER_GENERAL_SKILL_MEGAWEALTH) {
                    total_bc += 10;
                }

                if (skills & LEADER_GENERAL_SKILL_RESEARCHER2) {
                    int level_bonus = OFFICER::Owned_Officer_Level(i) * 5 + 5;
                    total_research += (level_bonus * 15) / 10;
                } else if (skills & LEADER_GENERAL_SKILL_RESEARCHER) {
                    int level_bonus = OFFICER::Owned_Officer_Level(i) * 5 + 5;
                    total_research += level_bonus;
                }
            }
        }

        // Process Diplomacy & Agreements
        plr->tribute_income = 0;
        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (plr->trade_treaty[i]) {
                total_bc += plr->current_trade_agreement_level[i];
            }

            if (plr->research_treaty[i]) {
                int16_t res_level = plr->current_research_agreement_level[i];
                if (res_level < 0) {
                    total_bc += res_level;
                } else {
                    total_research += res_level;
                }
            }

            if (MOX::player[i].tribute_treaty[player_idx] != 0) {
                int tribute = Tribute_Received_From(player_idx, i);
                plr->tribute_income += tribute;
            }
        }

        total_bc += plr->tribute_income;

        // Update Player Record
        plr->total_pop = static_cast<int16_t>(total_pop);
        plr->food_produced = static_cast<int16_t>(total_food);
        plr->industry_produced = static_cast<int16_t>(total_industry);

        if (total_research > 0x7FFF) {
            plr->research_produced = 0x7FFF;
        } else {
            plr->research_produced = static_cast<int16_t>(total_research);
        }

        uint16_t surplus_food_u =
            static_cast<uint16_t>(total_surplus_food) -
            static_cast<uint16_t>(total_food_maintenance);
        int16_t surplus_food = static_cast<int16_t>(surplus_food_u);
        plr->surplus_food = static_cast<int16_t>(surplus_food);

        if (surplus_food > 0) {
            if (plr->traits[TRAIT_FANTASTIC_TRADERS] == 0) {
                surplus_food /= 2;
            }
            total_bc += surplus_food;
        }

        if (total_bc > 0x7FFF) {
            total_bc = 0x7FFF;
        }
        plr->bc_produced = static_cast<int16_t>(total_bc);

        Compute_Player_Maintenance(plr, nullptr);
        plr->surplus_bc = plr->bc_produced - static_cast<int16_t>(plr->total_maintenance);
    }

    void __cdecl Player_Gets_Tech_App(s_player* plr, int16_t tech_app_id) {
        if (plr == nullptr || tech_app_id < 0 || tech_app_id >= TECH_APP_COUNT) {
            return;
        }
        plr->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
        plr->got_new_app = 1;

        switch (tech_app_id) {
            case TECH_APP_BATTLEOIDS:
                if (Player_May_Get_Tech_App(plr, TECH_APP_ARMOR_BARRACKS)) {
                    Player_Gets_Tech_App(plr, TECH_APP_ARMOR_BARRACKS);
                }
                break;
            case TECH_APP_CONFEDERATION:
                plr->traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_CONFEDERATION;
                break;
            case TECH_APP_FEDERATION:
                plr->traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_FEDERATION;
                break;
            case TECH_APP_GALACTIC_UNIFICATION:
                plr->traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_GALACTIC_UNIFICATION;
                break;
            case TECH_APP_IMPERIUM:
                plr->traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_IMPERIUM;
                break;
            case TECH_APP_SUB_SPACE_COMMUNICATIONS:
            case TECH_APP_TACHYON_COMMUNICATIONS:
            case TECH_APP_JUMP_GATE:
            case TECH_APP_STAR_GATE:
                Compute_All_Star_Colony_Stuff();
                break;
            default:
                INITSHIP::Update_Player_Ship_Designs((int16_t)(plr - MOX::player));
                SHIPMOVE::Update_All_Ship_Ranges();
                break;
        }

        Do_Player_Colony_Calculations((int16_t)(plr - MOX::player));

        const int16_t research_field = plr->current_research_field;
        const bool researching_field = research_field >= 0 && research_field < TECH_FIELD_COUNT;
        if (static_cast<uint8_t>(plr->current_research_application) == static_cast<uint16_t>(tech_app_id) &&
            researching_field &&
            plr->tech_fields[research_field] == 2) {
            bool advance_field = true;
            bool inspect_remaining_apps =
                plr->traits[TRAIT_CREATIVE] != 0 || keep_research_on_finds != 0;
            if (inspect_remaining_apps && plr->traits[TRAIT_UNCREATIVE] != 0) {
                Ensure_Uncreative_Field_OK(plr, research_field);
            }
            if (inspect_remaining_apps || Field_Is_Gray_Box(research_field)) {
                auto& field_data = TECHDATA::technology_fields[research_field];
                int16_t* field_techs = field_data.tech;

                for (int i = 0; i < 4; ++i) {
                    if (field_techs[i] != tech_app_id && plr->tech_applications[field_techs[i]] == 1) {
                        advance_field = false;
                        plr->current_research_application = static_cast<char>(field_techs[i]);
                    }
                }
            }

            if (advance_field) {
                Advance_Player_Field(plr, research_field, 1);
                Resolve_Research_Completion(plr, keep_research_on_finds != 0, 0);
            }
        } else {
            int field_id = TECHDATA::technology_applications[tech_app_id].tech_field_id;
            // Application 125 (Phase Shifter) has no field (TECH_FIELD_INVALID);
            // indexing tech_fields with it would read out of bounds.
            if (field_id >= 0 && field_id < TECH_FIELD_COUNT) {
                if (plr->traits[TRAIT_UNCREATIVE] != 0) {
                    Ensure_Uncreative_Field_OK(plr, field_id);
                }
                if (plr->tech_fields[field_id] == 2) {
                    Advance_Player_Field(plr, field_id, 1);
                }
            }
        }
    }

    int __cdecl Colony_Job_Production(s_colony* colony, short econ_type, s_colony_job_production* out) {
        s_colony_job_production final;
        s_colony_job_production cur_stat;

        bool is_blockaded = Colony_Is_Blockaded(colony);
        int leader_idx = Colony_Officer(colony);
        int8_t owner = colony->owner;
        int8_t govt_trait = MOX::player[owner].traits[TRAIT_CURRENT_GOVERNMENT];
        int govt_type = govt_trait;

        memset(&final, 0, sizeof(s_colony_job_production));

        short officer_bonus = 0;
        bool base_morale_applies = (govt_trait / 2 != GOVERNMENT_UNIFICATION / 2);

        if (leader_idx != -1) {
            auto& leader = MOX::leaders[leader_idx];
            if (econ_type == ECON_FOOD) {
                if (leader.special_skills & LEADER_COLONY_SKILL_FARMING_LEADER2) {
                    officer_bonus = (OFFICER::Owned_Officer_Level(leader_idx) + 1) * 3;
                } else if (leader.special_skills & LEADER_COLONY_SKILL_FARMING_LEADER) {
                    officer_bonus = OFFICER::Owned_Officer_Level(leader_idx) * 2 + 2;
                }
            } else if (econ_type == ECON_INDUSTRY) {
                if (leader.special_skills & LEADER_COLONY_SKILL_LABOR_LEADER2) {
                    officer_bonus = (OFFICER::Owned_Officer_Level(leader_idx) + 1) * 3;
                } else if (leader.special_skills & LEADER_COLONY_SKILL_LABOR_LEADER) {
                    officer_bonus = OFFICER::Owned_Officer_Level(leader_idx) * 2 + 2;
                }
            } else if (econ_type == ECON_RESEARCH) {
                if (leader.special_skills & LEADER_COLONY_SKILL_SCIENCE_LEADER2) {
                    officer_bonus = (OFFICER::Owned_Officer_Level(leader_idx) + 1) * 3;
                } else if (leader.special_skills & LEADER_COLONY_SKILL_SCIENCE_LEADER) {
                    officer_bonus = OFFICER::Owned_Officer_Level(leader_idx) * 2 + 2;
                }
            }
        }

        for (int i = 0; i < colony->n_pops; ++i) {
            uint32_t pop_val = colony->pop[i];
            if (POP::Is_Assigned(pop_val) && POP::Get_Prof(pop_val) == (uint32_t)econ_type) {
                uint16_t race = POP::Get_Race(pop_val);
                short gravity_factor = Gravity_Player_Production_Factor(colony, race);

                memset(&cur_stat, 0, sizeof(s_colony_job_production));
                short pop_base_prod = Colony_Pop_Base_Prod_Produced(colony, &colony->pop[i], econ_type, out ? &cur_stat : nullptr);

                if (out) {
                    final.base_prod += cur_stat.base_prod;
                    final.racial_bonuses[race] += cur_stat.racial_bonuses[race];
                    for (int j = 0; j < TECH_APP_COUNT; ++j) final.tech_apps[j] += cur_stat.tech_apps[j];
                    for (int j = 0; j < BUILDING_COUNT; ++j) final.buildings_prod[j] += cur_stat.buildings_prod[j];
                    final.food_bonus += cur_stat.food_bonus;
                    final.artifacts_bonus += cur_stat.artifacts_bonus;
                    final.intelligence_bonus += cur_stat.intelligence_bonus;
                }

                int scaled_base = (int)pop_base_prod * 20;
                cur_stat.base_prod_scaled = scaled_base;
                final.base_prod_scaled += scaled_base;

                bool morale_applies = base_morale_applies;
                if (race == 8 /* Androids/Cybernetic */) {
                    switch (android_morale) {
                        case ANDROID_MORALE_FARMERS_ONLY:
                            morale_applies = morale_applies && econ_type == ECON_FOOD;
                            break;
                        case ANDROID_MORALE_SCIENTISTS_ONLY:
                            morale_applies = morale_applies && econ_type == ECON_RESEARCH;
                            break;
                        case ANDROID_MORALE_FARMERS_AND_WORKERS:
                            morale_applies = morale_applies && econ_type != ECON_RESEARCH;
                            break;
                        case ANDROID_MORALE_WORKERS_AND_SCIENTISTS:
                            morale_applies = morale_applies && econ_type != ECON_FOOD;
                            break;
                        case ANDROID_MORALE_FARMERS_AND_SCIENTISTS:
                            morale_applies = morale_applies && econ_type != ECON_INDUSTRY;
                            break;
                        case ANDROID_MORALE_ALL_DROIDS:
                            break;
                        case ANDROID_MORALE_NONE:
                            morale_applies = false;
                            break;
                        default:
                            morale_applies = morale_applies && econ_type == ECON_INDUSTRY;
                            break;
                    }
                }

                cur_stat.gravity_penalty = (4 - gravity_factor) * (int)pop_base_prod * -5;
                final.gravity_penalty += cur_stat.gravity_penalty;

                cur_stat.officer_bonus = officer_bonus * (int)pop_base_prod;
                final.officer_bonus += cur_stat.officer_bonus;

                if (POP::Is_Conquered(pop_val)) {
                    cur_stat.conquered_penalty = (int)pop_base_prod * -5;
                    final.conquered_penalty += cur_stat.conquered_penalty;
                }

                if (morale_applies) {
                    cur_stat.morale_bonus = colony->morale * (int)pop_base_prod;
                }
                final.morale_bonus += cur_stat.morale_bonus;

                int8_t government_modifier = 0;
                if ((uint8_t)govt_type < GOVERNMENT_COUNT) {
                    const s_government_bonus& government_bonus =
                        government_bonuses[govt_type];
                    if (econ_type == ECON_FOOD) {
                        government_modifier = government_bonus.food;
                    } else if (econ_type == ECON_INDUSTRY) {
                        government_modifier = government_bonus.production;
                    } else if (econ_type == ECON_RESEARCH) {
                        government_modifier = government_bonus.science;
                    }
                }

                if (econ_type <= ECON_INDUSTRY /* FOOD or INDUSTRY */) {
                    cur_stat.govt_bonus = (int)pop_base_prod * government_modifier;
                    final.govt_bonus += cur_stat.govt_bonus;

                    if (is_blockaded) {
                        cur_stat.blockaded_penalty = (int)pop_base_prod * -10;
                        final.blockaded_penalty += cur_stat.blockaded_penalty;
                    }
                } else if (econ_type == ECON_RESEARCH) {
                    cur_stat.govt_bonus = (int)pop_base_prod * government_modifier;
                    final.govt_bonus += cur_stat.govt_bonus;
                }

                if (MOX::player[owner].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    int diff_bonus = colony_job_production_difficulty_lookup[MOX::settings.game_difficulty];
                    cur_stat.difficulty_bonus = diff_bonus;
                    final.difficulty_bonus += diff_bonus;
                }

                cur_stat.final_prod = cur_stat.officer_bonus + cur_stat.pollution_penalty +
                    cur_stat.difficulty_bonus + cur_stat.morale_bonus + cur_stat.govt_bonus +
                    cur_stat.gravity_penalty + cur_stat.blockaded_penalty +
                    cur_stat.conquered_penalty + scaled_base;

                if (cur_stat.final_prod > 0) {
                    final.final_prod += cur_stat.final_prod;
                }
            }
        }

        if (out) {
            memcpy(out, &final, sizeof(s_colony_job_production));
        }

        if (econ_type == ECON_FOOD) {
            return (final.final_prod + 20) / 40;
        }
        return (final.final_prod + 10) / 20;
    }

    void __cdecl Compute_Player_Maintenance(s_player* plr, STRBUILD::s_builder* debug_out) {
        int16_t player_idx = static_cast<int16_t>(plr - MOX::player);

        plr->command_points = 0;
        plr->command_points_used = 0;
        for (int i = 0; i < 6; ++i) {
            plr->maintenance[i] = 0;
        }

        int32_t leader_cp_bonus = 0;
        int32_t bonus_leader_idx = -1;

        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            s_leader_data* leader = &MOX::leaders[i];
            if (leader->player_index == player_idx && leader->status >= 0 && leader->status < 3) {
                if (Leader_At_Anomaly(leader) == 0) {
                    plr->maintenance[5] += OFFICER::Officer_Maintenance(player_idx, i);

                    int32_t cp_val = 0;
                    if (leader->general_skills & LEADER_GENERAL_SKILL_OPERATIONS2) {
                        cp_val = OFFICER::Owned_Officer_Level(i) * 3 + 3;
                    } else if (leader->general_skills & LEADER_GENERAL_SKILL_OPERATIONS) {
                        cp_val = OFFICER::Owned_Officer_Level(i) * 2 + 2;
                    }

                    if (leader_cp_bonus < cp_val) {
                        leader_cp_bonus = cp_val;
                        bonus_leader_idx = i;
                    }
                }
            }
        }

        int32_t n_star_bases = 0;
        int32_t n_battlestations = 0;
        int32_t n_fortresses = 0;
        int32_t warlord_bonus = 0;

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* col = &MOX::colony[i];
            if (col->owner == player_idx && col->outpost_flag == 0) {
                plr->maintenance[0] += (uint16_t)(uint8_t)col->maintenance[ECON_BC];

                if (plr->traits[TRAIT_WARLORD] != 0) {
                    warlord_bonus += command_points.warlord_planet;
                }

                if (col->buildings[BUILDING_STAR_BASE] != 0) {
                    n_star_bases++;
                } else if (col->buildings[BUILDING_BATTLESTATION] != 0) {
                    n_battlestations++;
                } else if (col->buildings[BUILDING_STAR_FORTRESS] != 0) {
                    n_fortresses++;
                }
            }
        }

        int32_t comm_tech_idx = -1;
        int32_t comm_tech_bonus_per_base = 0;
        if (plr->tech_applications[TECH_APP_HYPERSPACE_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            comm_tech_idx = TECH_APP_HYPERSPACE_COMMUNICATIONS;
            comm_tech_bonus_per_base = command_points.hyperspace_communications;
        } else if (plr->tech_applications[TECH_APP_SUB_SPACE_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            comm_tech_idx = TECH_APP_SUB_SPACE_COMMUNICATIONS;
            comm_tech_bonus_per_base = command_points.sub_space_communications;
        } else if (plr->tech_applications[TECH_APP_TACHYON_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            comm_tech_idx = TECH_APP_TACHYON_COMMUNICATIONS;
            comm_tech_bonus_per_base = command_points.tachyon_communications;
        }

        int32_t comm_tech_total = comm_tech_bonus_per_base * (n_star_bases + n_battlestations + n_fortresses);
        int16_t base_cp = command_points.starting;
        base_cp = (int16_t)(base_cp + (int16_t)(n_star_bases * command_points.star_base));
        base_cp = (int16_t)(base_cp + (int16_t)(n_battlestations * command_points.battlestation));
        base_cp = (int16_t)(base_cp + (int16_t)(n_fortresses * command_points.star_fortress));
        base_cp = (int16_t)(base_cp + (int16_t)warlord_bonus);
        base_cp = (int16_t)(base_cp + (int16_t)comm_tech_total);
        base_cp = (int16_t)(base_cp + (int16_t)leader_cp_bonus);
        plr->command_points = base_cp;

        int32_t imperium_bonus = 0;
        if (plr->traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_IMPERIUM) {
            imperium_bonus = base_cp / 2;
            plr->command_points = base_cp + (int16_t)imperium_bonus;
        }

        if (debug_out) {
            STRBUILD::Sprintf(debug_out, "%d %s\n", command_points.starting, starting_command_points_msg);

            if (n_star_bases > 0) {
                if (n_star_bases == 1)
                    STRBUILD::Sprintf(debug_out, d_from_1_star_base_msg, command_points.star_base);
                else
                    STRBUILD::Sprintf(debug_out, d_from_d_star_bases_msg, n_star_bases * command_points.star_base, n_star_bases);
            }
            if (n_battlestations > 0) {
                if (n_battlestations == 1)
                    STRBUILD::Sprintf(debug_out, d_from_1_battlestation_msg, command_points.battlestation);
                else
                    STRBUILD::Sprintf(debug_out, d_from_d_battlestations_msg, n_battlestations * command_points.battlestation, n_battlestations);
            }
            if (n_fortresses > 0) {
                if (n_fortresses == 1)
                    STRBUILD::Sprintf(debug_out, d_from_1_fortress_msg, command_points.star_fortress);
                else
                    STRBUILD::Sprintf(debug_out, d_from_d_fortresses_msg, n_fortresses * command_points.star_fortress, n_fortresses);
            }
            if (comm_tech_total != 0) {
                STRBUILD::Sprintf(debug_out, "%d : %s\n", comm_tech_total, TECHDATA::technology_applications[comm_tech_idx].name);
            }
            if (leader_cp_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "%d : %s\n", leader_cp_bonus, MOX::leaders[bonus_leader_idx].name);
            }
            if (warlord_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "%d %s\n", warlord_bonus, from_warlord_msg);
            }
            if (imperium_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "%d %s\n", imperium_bonus, from_imperium_msg);
            }
            if (plr->command_points > 5) {
                STRBUILD::Sprintf(debug_out, "%d %s\n", (int)plr->command_points, total_command_points_msg);
            }
            STRBUILD::Sprintf(debug_out, "-------------------\n");
        }

        int32_t active_freighters = plr->n_freighters;
        if (plr->surplus_freighters > 0) {
            active_freighters -= plr->surplus_freighters;
        }
        plr->maintenance[1] = (int16_t)(active_freighters / 2);

        int32_t ship_counts[HULL_CLASS_COUNT] = {0};
        int32_t noncombat_ships = 0;
        int32_t noncombat_command_points = 0;

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* s = &MOX::ship[i];
            if (s->owner == player_idx && s->status < 3 && s->just_built == 0) {
                uint8_t size_class = (uint8_t)s->d.size;
                uint8_t usage = ship_command_points_usage[size_class];
                if (s->d.ship_type == SHIP_TYPE_COMBAT) {
                    ship_counts[size_class]++;
                } else {
                    noncombat_ships++;
                    noncombat_command_points += usage;
                }
                plr->command_points_used += usage;
            }
        }

        if (debug_out) {
            for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
                if (ship_counts[i] > 0) {
                    if (ship_counts[i] == 1) {
                        STRBUILD::Sprintf(debug_out, d_from_1_s_ship_msg, ship_command_points_usage[i], TECHDATA::hull_data[i].name);
                    } else {
                        STRBUILD::Sprintf(debug_out, d_from_d_s_ships_msg, ship_command_points_usage[i] * ship_counts[i], ship_counts[i], TECHDATA::hull_data[i].size_name);
                    }
                }
            }
            if (noncombat_ships > 0) {
                if (noncombat_ships == 1)
                    STRBUILD::Sprintf(debug_out, d_from_1_noncombat_ship_msg, noncombat_command_points);
                else
                    STRBUILD::Sprintf(debug_out, d_from_d_noncombat_ships_msg, noncombat_command_points, noncombat_ships);
            }
            const char* used_msg = (plr->command_points_used == 1) ? total_command_point_used_msg : total_command_points_used_msg;
            STRBUILD::Sprintf(debug_out, "%d %s\n", (int)plr->command_points_used, used_msg);
            STRBUILD::Sprintf(debug_out, "-------------------\n");
        }

        if (plr->command_points_used != 0) {
            if (plr->command_points < plr->command_points_used) {
                int32_t diff = plr->command_points_used - plr->command_points;
                int32_t cost_mult = (plr->objectives == PLAYER_OBJECTIVE_HUMAN) ? 10 : (12 - MOX::settings.game_difficulty);
                plr->maintenance[2] = (int16_t)(cost_mult * diff);
                if (debug_out) {
                    STRBUILD::Sprintf(debug_out, "(%s %d * %d + %d * %d : %d BC)\n", ship_maintenance_costs_msg, 0, (int)plr->command_points, (int)cost_mult, (int)diff, (int)plr->maintenance[2]);
                }
            } else {
                plr->maintenance[2] = 0;
                if (debug_out) {
                    STRBUILD::Sprintf(debug_out, "(%s 0 BC)\n", ship_maintenance_costs_msg);
                }
            }
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            plr->maintenance[3] += (int16_t)BILL::Get_Their_Spy_Number(player_idx, (int16_t)i);
        }

        plr->maintenance[4] = 0;
        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (plr->tribute_treaty[i] != 0) {
                plr->maintenance[4] += (int16_t)Tribute_Received_From(i, player_idx);
            }
        }

        plr->total_maintenance = 0;
        for (int i = 0; i < 6; ++i) {
            plr->total_maintenance += plr->maintenance[i];
        }
    }

    void __cdecl Colony_BC_Production(s_colony* colony, STRBUILD::s_builder* debug_buf) {
        if (colony->n_pops == 0) {
            colony->production[ECON_BC] = 0;
            return;
        }

        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
            colony->production[ECON_BC] = 0;
            if (debug_buf) {
                STRBUILD::Copy(debug_buf, anomaly_msg);
            }
            return;
        }

        s_player* owner_ptr = &MOX::player[colony->owner];
        int officer_idx = Colony_Officer(colony);

        int morale_bonus = 0;
        int trade_goods_bc = 0;
        int galactic_currency_bc = 0;
        int stock_exchange_bc = 0;
        int spaceport_bc = 0;
        int gov_bonus_bc = 0;
        int leader_bonus_bc = 0;

        int total_normal_pops = 0;
        int total_immune_pops = 0;

        int base_tax_per_pop = owner_ptr->traits[TRAIT_TAX_BONUS] * 2 + 4;

        for (int i = 0; i < (uint8_t)colony->n_pops; ++i) {
            int32_t race_idx = POP::Get_Race(colony->pop[i]);
            if (race_idx < MAX_PLAYERS) {
                if (MOX::player[race_idx].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    total_normal_pops++;
                } else {
                    total_immune_pops++;
                }
            }
        }

        int owner_idx = (int)colony->owner;
        if (MOX::player[owner_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
            base_tax_per_pop += (int)difficulty_bc_lookup_table[MOX::settings.game_difficulty];
        }

        int total_pops = total_normal_pops + total_immune_pops;
        int taxes_collected = (base_tax_per_pop * total_pops + 2) / 4;

        if (total_pops > 0 && (MOX::player[owner_idx].traits[TRAIT_CURRENT_GOVERNMENT] / 2) != GOVERNMENT_UNIFICATION / 2) {
            morale_bonus = colony->morale * taxes_collected;
            morale_bonus = (morale_bonus + (morale_bonus < 0 ? -10 : 10)) / 20;
        }

        int special_planet_bc = 0;
        int8_t planet_special = MOX::planet[colony->planet].planet_special;
        if (planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS) {
            special_planet_bc = planet_bc_bonus.gold_deposits;
        } else if (planet_special == SYSTEM_SPECIAL_GEM_DEPOSITS) {
            special_planet_bc = planet_bc_bonus.gem_deposits;
        }

        int16_t producing = colony->producing[0];
        if (producing == COLONY_PRODUCTION_AI_TRADE_GOODS ||
            producing == COLONY_PRODUCTION_NONE ||
            producing == COLONY_PRODUCTION_TRADE_GOODS) {
            int surplus_ind = (int)colony->production[ECON_INDUSTRY] - (int)colony->maintenance[ECON_INDUSTRY];
            if (surplus_ind > 0) {
                uint16_t ratio = owner_ptr->traits[TRAIT_FANTASTIC_TRADERS] == 0
                    ? trade_goods_ratio.normal
                    : trade_goods_ratio.fantastic_traders;
                trade_goods_bc = (surplus_ind * (int)ratio + 99) / 100;
            }
        }

        int income_base = taxes_collected + special_planet_bc;

        if (MOX::player[owner_idx].tech_applications[TECH_APP_GALACTIC_CURRENCY_EXCHANGE] == TECH_RESEARCH_STATUS_RESEARCHED) {
            galactic_currency_bc = static_cast<int>(
                static_cast<int64_t>(income_base) *
                bc_bonus.galactic_currency_exchange / 100);
        }

        if (colony->buildings[BUILDING_STOCK_EXCHANGE] != 0) {
            stock_exchange_bc = static_cast<int>(
                static_cast<int64_t>(income_base) *
                bc_bonus.planetary_stock_exchange / 100);
        }

        if (colony->buildings[BUILDING_SPACEPORT] != 0) {
            spaceport_bc = static_cast<int>(
                static_cast<int64_t>(income_base) *
                bc_bonus.spaceport / 100);
        }

        int8_t government = MOX::player[owner_idx].traits[TRAIT_CURRENT_GOVERNMENT];
        if ((uint8_t)government < GOVERNMENT_COUNT) {
            int64_t money_bonus = (int64_t)income_base *
                government_bonuses[government].money * 5 / 100;
            gov_bonus_bc = (int)money_bonus;
        }

        if (officer_idx != -1) {
            if (MOX::leaders[officer_idx].special_skills & LEADER_COLONY_SKILL_FINANCIAL_LEADER2) {
                int16_t lvl = OFFICER::Owned_Officer_Level(officer_idx);
                leader_bonus_bc = ((lvl + 1) * income_base * 3 + 10) / 20;
            } else if (MOX::leaders[officer_idx].special_skills & LEADER_COLONY_SKILL_FINANCIAL_LEADER) {
                int16_t lvl = OFFICER::Owned_Officer_Level(officer_idx);
                leader_bonus_bc = ((lvl + 1) * income_base + 5) / 10;
            }
        }

        colony->production[ECON_BC] = (int16_t)(colony->n_industry_taxed + stock_exchange_bc +
            galactic_currency_bc + trade_goods_bc +
            special_planet_bc + morale_bonus + taxes_collected +
            leader_bonus_bc + gov_bonus_bc + spaceport_bc);

        if (debug_buf) {
            // fmtpara: \aT+15,+65. defines tab stops for the column layout, \x1A0 restores
            // left alignment, and the row formats below use \t/\r for column and paragraph breaks.
            STRBUILD::Sprintf(debug_buf, "%s", "\x07"
                                               "T+15,+65.");
            STRBUILD::Sprintf(debug_buf, "%s", "\x1A"
                                               "0");

            if (taxes_collected != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", taxes_collected, taxes_collected_msg);
            }
            if (morale_bonus > 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", morale_bonus, morale_bonus_msg);
            } else if (morale_bonus < 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", morale_bonus, morale_penalty_msg);
            }
            if (special_planet_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", special_planet_bc, special_income_msg);
            }
            if (leader_bonus_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", leader_bonus_bc, leader_bonus_msg);
            }
            if (galactic_currency_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", galactic_currency_bc, TECHDATA::buildings[BUILDING_CURRENCY_EXCHANGE].name);
            }
            if (stock_exchange_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", stock_exchange_bc, TECHDATA::buildings[BUILDING_STOCK_EXCHANGE].name);
            }
            if (spaceport_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", spaceport_bc, TECHDATA::buildings[BUILDING_SPACEPORT].name);
            }
            if (gov_bonus_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", gov_bonus_bc, government_bonus_msg);
            }
            if (trade_goods_bc != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", trade_goods_bc, trade_goods_msg);
            }
            if (colony->n_industry_taxed != 0) {
                STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", (int)colony->n_industry_taxed, taxed_from_industry_msg);
            }
            STRBUILD::Sprintf(debug_buf, "%s", "\r");
            STRBUILD::Sprintf(debug_buf, "\t%+d\t%s\r", (int)colony->production[ECON_BC], total_income_msg);
        }
    }

    void __cdecl Colony_Research_Production(s_colony* colony, STRBUILD::s_builder* debug_out) {
        s_colony_job_production breakdown;
        int32_t res_lab_bonus, supercomp_bonus, cybernet_bonus, autolab_bonus;

        if (colony->n_pops == 0) {
            colony->production[ECON_RESEARCH] = 0;
            return;
        }

        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
            colony->production[ECON_RESEARCH] = 0;
            if (debug_out != nullptr) {
                STRBUILD::Copy(debug_out, anomaly_msg);
            }
            return;
        }

        s_colony_job_production* breakdown_ptr = (debug_out != nullptr) ? &breakdown : nullptr;
        int32_t job_production = Colony_Job_Production(colony, ECON_RESEARCH, breakdown_ptr);

        res_lab_bonus = colony->buildings[BUILDING_RESEARCH_LAB]
            ? research_colony_bonus.research_lab
            : 0;
        supercomp_bonus = colony->buildings[BUILDING_SUPERCOMPUTER]
            ? research_colony_bonus.supercomputer
            : 0;
        cybernet_bonus = colony->buildings[BUILDING_GALACTIC_CYBERNET]
            ? research_colony_bonus.galactic_cybernet
            : 0;
        autolab_bonus = colony->buildings[BUILDING_AUTOLAB]
            ? research_colony_bonus.autolab
            : 0;

        int16_t total_research = (int16_t)(autolab_bonus + supercomp_bonus + cybernet_bonus + res_lab_bonus + job_production);
        colony->production[ECON_RESEARCH] = total_research;

        if (debug_out != nullptr) {
            // fmtpara: \aT+15,+65. defines tab stops for the column layout, \x1A0 restores
            // left alignment, and the row formats below use \t/\r for column and paragraph breaks.
            STRBUILD::Sprintf(debug_out, "%s", "\x07"
                                               "T+15,+65.");
            STRBUILD::Sprintf(debug_out, "%s", "\x1A"
                                               "0");

            if (breakdown.base_prod != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", breakdown.base_prod, produced_by_scientists_msg);
            }
            if (breakdown.artifacts_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", breakdown.artifacts_bonus, artifacts_bonus_msg);
            }
            if (breakdown.intelligence_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", breakdown.intelligence_bonus, intelligence_bonus_msg);
            }

            for (int i = 0; i < MAX_PLAYERS; ++i) {
                if (breakdown.racial_bonuses[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%+d\t%s %s\r", (int)breakdown.racial_bonuses[i], MOX::player[i].race_name, bonus_msg);
                }
            }

            if (breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_ANDROID] != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_ANDROID], android_bonus_msg);
            }

            for (int i = 0; i < 204; ++i) {
                if (breakdown.tech_apps[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)breakdown.tech_apps[i], TECHDATA::technology_applications[i].name);
                }
            }

            for (int i = 0; i < BUILDING_COUNT; ++i) {
                if (breakdown.buildings_prod[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)breakdown.buildings_prod[i], TECHDATA::buildings[i].name);
                }
            }

            if (breakdown.gravity_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.gravity_penalty, 20), gravity_penalty_msg);
            }
            if (breakdown.conquered_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.conquered_penalty, 20), conquered_penalty_msg);
            }
            if (breakdown.blockaded_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.blockaded_penalty, 20), blockaded_penalty_msg);
            }
            if (breakdown.morale_bonus > 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.morale_bonus, 20), morale_bonus_msg);
            } else if (breakdown.morale_bonus < 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.morale_bonus, 20), morale_penalty_msg);
            }
            if (breakdown.govt_bonus > 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.govt_bonus, 20), government_bonus_msg);
            } else if (breakdown.govt_bonus < 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.govt_bonus, 20), government_penalty_msg);
            }
            if (breakdown.officer_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.officer_bonus, 20), leader_bonus_msg);
            }

            if (res_lab_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t+%d\t%s\r", res_lab_bonus, TECHDATA::buildings[BUILDING_RESEARCH_LAB].name);
            }
            if (supercomp_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t+%d\t%s\r", supercomp_bonus, TECHDATA::buildings[BUILDING_SUPERCOMPUTER].name);
            }
            if (cybernet_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t+%d\t%s\r", cybernet_bonus, TECHDATA::buildings[BUILDING_GALACTIC_CYBERNET].name);
            }
            if (autolab_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t+%d\t%s\r", autolab_bonus, TECHDATA::buildings[BUILDING_AUTOLAB].name);
            }

            STRBUILD::Sprintf(debug_out, "\r");
            STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)total_research, total_research_produced_msg);
        }
    }

    void __cdecl Colony_Food_Production(s_colony* colony, STRBUILD::s_builder* debug_out) {
        if (colony->n_pops == 0) {
            colony->production[ECON_FOOD] = 0;
            return;
        }

        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        if (EVENTS::Event_Check_Space_Anomaly(colony_idx)) {
            colony->production[ECON_FOOD] = 0;
            if (debug_out) {
                STRBUILD::Copy(debug_out, anomaly_msg);
            }
            return;
        }

        int16_t hydro_bonus = 0;
        int16_t subterranean_bonus = 0;
        s_colony_job_production breakdown;
        int32_t base_job_prod = 0;

        if (colony->food2_per_farmer == 0) {
            memset(&breakdown, 0, sizeof(breakdown));
        } else {
            base_job_prod = Colony_Job_Production(colony, 0, debug_out ? &breakdown : nullptr);
        }

        if (colony->buildings[BUILDING_HYDROPONIC_FARM]) {
            hydro_bonus = food_colony_bonus.hydroponic_farms;
        }
        if (colony->buildings[BUILDING_SUBTERRANEAN_FARMS]) {
            subterranean_bonus = food_colony_bonus.subterranean_farms;
        }

        colony->production[ECON_FOOD] = static_cast<int16_t>(hydro_bonus + subterranean_bonus + base_job_prod);

        if (debug_out) {
            // fmtpara: \aT+15,+65. defines tab stops for the column layout, \x1A0 restores
            // left alignment, and the row formats below use \t/\r for column and paragraph breaks.
            STRBUILD::Sprintf(debug_out, "%s", "\x07"
                                               "T+15,+65.");
            STRBUILD::Sprintf(debug_out, "%s", "\x1A"
                                               "0");

            if (breakdown.base_prod != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.base_prod, 2), produced_by_farmers_msg);
            }
            if (breakdown.food_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.food_bonus, 2), aquatic_bonus_msg);
            }

            for (int i = 0; i < MAX_PLAYERS; ++i) {
                int16_t val = breakdown.racial_bonuses[i];
                if (val != 0) {
                    char* formatted = Format_Quotient(val, 2);
                    STRBUILD::Sprintf(debug_out, "\t%s\t%s %s\r", formatted, MOX::player[i].race_name, bonus_msg);
                }
            }

            if (breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_NATIVE] != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_NATIVE], 2), native_bonus_msg);
            }
            if (breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_ANDROID] != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_ANDROID], 2), android_bonus_msg);
            }

            for (int i = 0; i < 204; ++i) {
                if (breakdown.tech_apps[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.tech_apps[i], 2), TECHDATA::technology_applications[i].name);
                }
            }

            for (int i = 0; i < BUILDING_COUNT; ++i) {
                if (breakdown.buildings_prod[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.buildings_prod[i], 2), TECHDATA::buildings[i].name);
                }
            }

            if (breakdown.gravity_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.gravity_penalty, 40), gravity_penalty_msg);
            }
            if (breakdown.conquered_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.conquered_penalty, 40), conquered_penalty_msg);
            }
            if (breakdown.blockaded_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.blockaded_penalty, 40), blockaded_penalty_msg);
            }
            if (breakdown.morale_bonus > 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.morale_bonus, 40), morale_bonus_msg);
            } else if (breakdown.morale_bonus < 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.morale_bonus, 40), morale_penalty_msg);
            }
            if (breakdown.govt_bonus > 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.govt_bonus, 40), government_bonus_msg);
            } else if (breakdown.govt_bonus < 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.govt_bonus, 40), government_penalty_msg);
            }
            if (breakdown.officer_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.officer_bonus, 40), leader_bonus_msg);
            }

            if (hydro_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int32_t)hydro_bonus, TECHDATA::buildings[BUILDING_HYDROPONIC_FARM].name);
            }
            if (subterranean_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int32_t)subterranean_bonus, TECHDATA::buildings[BUILDING_SUBTERRANEAN_FARMS].name);
            }

            if (colony->n_food_replicated != 0) {
                int replication_amount = (int)(uint8_t)colony->n_food_replicated;
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", replication_amount, TECHDATA::buildings[BUILDING_FOOD_REPLICATORS].name);
                colony->production[ECON_FOOD] += (int16_t)replication_amount;
            }

            STRBUILD::Sprintf(debug_out, "%s", "\r");
            STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)colony->production[ECON_FOOD], COLCALC::total_food_produced_msg);
        }
    }

    void __cdecl Colony_Industry_Production(s_colony* colony, STRBUILD::s_builder* debug_out) {
        s_colony_job_production breakdown;
        int64_t base_worker_prod;
        int32_t bonus_recycling = 0;
        int32_t bonus_automated_factory = 0;
        int32_t bonus_robo_miner = 0;
        int32_t bonus_deep_core = 0;
        int32_t bonus_robotic_factory = 0;

        colony->n_industry_recyclers = 0;
        uint8_t n_pops = colony->n_pops;

        if (n_pops == 0) {
            colony->production[ECON_INDUSTRY] = 0;
            return;
        }

        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        if (EVENTS::Event_Check_Space_Anomaly(colony_idx)) {
            colony->production[ECON_INDUSTRY] = 0;
            colony->pollution = 0;
            colony->imports[ECON_FOOD] = 0;
            if (debug_out) {
                STRBUILD::Copy(debug_out, anomaly_msg);
            }
            return;
        }

        s_colony_job_production* p_breakdown = debug_out ? &breakdown : nullptr;
        base_worker_prod = Colony_Job_Production(colony, ECON_INDUSTRY, p_breakdown);

        bonus_automated_factory = colony->buildings[BUILDING_AUTOMATED_FACTORY]
            ? production_colony_bonus.automated_factory
            : 0;
        bonus_robo_miner = colony->buildings[BUILDING_ROBO_MINER_PLANT]
            ? production_colony_bonus.robo_miners
            : 0;
        bonus_deep_core = colony->buildings[BUILDING_DEEP_CORE_MINE]
            ? production_colony_bonus.deep_core_mines
            : 0;

        if (colony->buildings[BUILDING_ROBOTIC_FACTORY]) {
            bonus_robotic_factory = production_colony_bonus.robotic_factory[MOX::planet[colony->planet].mineral_class];
        }

        if (colony->buildings[BUILDING_RECYCLOTRON]) {
            bonus_recycling = n_pops * production_worker_bonus.recyclotron;
            colony->n_industry_recyclers = static_cast<char>(bonus_recycling);
        }

        if (MOX::player[colony->owner].objectives != PLAYER_OBJECTIVE_HUMAN) {
            int32_t difficulty_bonus = (MOX::settings.game_difficulty * n_pops + 4) >> 3;
            colony->n_industry_recyclers += static_cast<char>(difficulty_bonus);
        }

        int64_t total_prod = bonus_automated_factory + bonus_robotic_factory + bonus_robo_miner +
            bonus_deep_core + bonus_recycling + base_worker_prod;

        int64_t pollution = 0;
        if (colony->buildings[BUILDING_CORE_WASTE_DUMP]) {
            colony->pollution = 0;
        } else {
            int16_t officer_idx = Colony_Officer(colony);
            int64_t base_pollution = base_worker_prod + bonus_robotic_factory;
            int64_t reduction_size = planet_no_pollution_prod[MOX::planet[colony->planet].size];

            if (MOX::player[colony->owner].tech_applications[TECH_APP_NANO_DISASSEMBLERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                reduction_size *= nano_disassemblers_coeff;
            }

            if (colony->buildings[BUILDING_POLLUTION_PROCESSOR]) {
                base_pollution = (base_pollution * pollution_processor_ratio + 99) / 100;
            }
            if (colony->buildings[BUILDING_ATMOSPHERE_RENEWER]) {
                base_pollution = (base_pollution * atmospheric_renewer_ratio + 99) / 100;
            }

            if (officer_idx != -1) {
                int32_t officer_reduction = 0;
                if (MOX::leaders[officer_idx].special_skills & LEADER_COLONY_SKILL_ENVIRONMENTALIST2) {
                    officer_reduction = (OFFICER::Owned_Officer_Level(officer_idx) * 3 + 3) * 5;
                } else if (MOX::leaders[officer_idx].special_skills & LEADER_COLONY_SKILL_ENVIRONMENTALIST) {
                    officer_reduction = (OFFICER::Owned_Officer_Level(officer_idx) * 5 + 5) * 2;
                }
                base_pollution += (officer_reduction * base_pollution) / -100;
            }

            base_pollution -= reduction_size;
            pollution = (base_pollution + 1) / 2;

            if (pollution > 0) {
                int immune_pops = 0;
                for (int i = 0; i < n_pops; ++i) {
                    uint32_t race = POP::Get_Race(colony->pop[i]);
                    if ((race < MAX_PLAYERS && MOX::player[race].traits[TRAIT_ENVIRONMENT_IMMUNE]) || race == 8 /* ANDROID */) {
                        immune_pops++;
                    }
                }

                if (immune_pops != 0) {
                    pollution = (n_pops / 2 + (n_pops - immune_pops) * pollution) / n_pops;
                }

                if (pollution > 0) {
                    total_prod -= pollution;
                    if (debug_out) {
                        int64_t pollution_penalty = pollution * -20;
                        if (pollution_penalty < INT32_MIN) pollution_penalty = INT32_MIN;
                        if (pollution_penalty > INT32_MAX) pollution_penalty = INT32_MAX;
                        breakdown.pollution_penalty = (int32_t)pollution_penalty;
                    }
                } else {
                    pollution = 0;
                }
            } else {
                pollution = 0;
            }
            colony->pollution = (int16_t)pollution;
        }

        colony->production[ECON_INDUSTRY] = (int16_t)total_prod;

        if (debug_out) {
            // fmtpara: \aT+15,+65. defines tab stops for the column layout, \x1A0 restores
            // left alignment, and the row formats below use \t/\r for column and paragraph breaks.
            STRBUILD::Sprintf(debug_out, "%s", "\x07"
                                               "T+15,+65.");
            STRBUILD::Sprintf(debug_out, "%s", "\x1A"
                                               "0");

            if (breakdown.base_prod != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", breakdown.base_prod, produced_by_workers_msg);
            }

            for (int i = 0; i < MAX_PLAYERS; ++i) {
                if (breakdown.racial_bonuses[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%+d\t%s %s\r", (int)breakdown.racial_bonuses[i], MOX::player[i].race_name, bonus_msg);
                }
            }

            if (breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_ANDROID] != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)breakdown.racial_bonuses[COLONY_JOB_RACIAL_BONUS_ANDROID], android_bonus_msg);
            }

            for (int i = 0; i < TECH_APP_COUNT; ++i) {
                if (breakdown.tech_apps[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)breakdown.tech_apps[i], TECHDATA::technology_applications[i].name);
                }
            }

            for (int i = 0; i < BUILDING_COUNT; ++i) {
                if (breakdown.buildings_prod[i] != 0) {
                    STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)breakdown.buildings_prod[i], TECHDATA::buildings[i].name);
                }
            }

            if (breakdown.gravity_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.gravity_penalty, 20), gravity_penalty_msg);
            }

            if (breakdown.conquered_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.conquered_penalty, 20), conquered_penalty_msg);
            }

            if (breakdown.blockaded_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.blockaded_penalty, 20), blockaded_penalty_msg);
            }

            if (breakdown.morale_bonus > 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.morale_bonus, 20), morale_bonus_msg);
            } else if (breakdown.morale_bonus < 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.morale_bonus, 20), morale_penalty_msg);
            }

            if (breakdown.govt_bonus > 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.govt_bonus, 20), government_bonus_msg);
            } else if (breakdown.govt_bonus < 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.govt_bonus, 20), government_penalty_msg);
            }

            if (breakdown.officer_bonus != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.officer_bonus, 20), leader_bonus_msg);
            }

            if (bonus_recycling != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)bonus_recycling, TECHDATA::buildings[BUILDING_RECYCLOTRON].name);
            }
            if (bonus_automated_factory != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)bonus_automated_factory, TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].name);
            }
            if (bonus_robo_miner != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)bonus_robo_miner, TECHDATA::buildings[BUILDING_ROBO_MINER_PLANT].name);
            }
            if (bonus_deep_core != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)bonus_deep_core, TECHDATA::buildings[BUILDING_DEEP_CORE_MINE].name);
            }
            if (bonus_robotic_factory != 0) {
                STRBUILD::Sprintf(debug_out, "\t%+d\t%s %s\r", (int)bonus_robotic_factory, TECHDATA::buildings[BUILDING_ROBOTIC_FACTORY].name, used_msg);
            }

            if (breakdown.pollution_penalty != 0) {
                STRBUILD::Sprintf(debug_out, "\t%s\t%s\r", Format_Quotient(breakdown.pollution_penalty, 20), pollution_penalty_msg);
            }

            if (colony->n_food_replicated != 0) {
                char replicated_food_text[256];
                int32_t replication_cost = (uint8_t)colony->n_food_replicated * replicator_food_pp;
                snprintf(replicated_food_text, sizeof(replicated_food_text), replicated_to_d_food_msg, (uint32_t)(uint8_t)colony->n_food_replicated);
                STRBUILD::Sprintf(debug_out, "\t%d\t%s\r", -replication_cost, replicated_food_text);
                colony->production[ECON_INDUSTRY] -= (int16_t)replication_cost;
            }

            if (colony->n_industry_taxed != 0) {
                STRBUILD::Sprintf(debug_out, "\t-%d\t%s\r", (int)colony->n_industry_taxed, taxed_from_industry_msg);
                colony->production[ECON_INDUSTRY] -= colony->n_industry_taxed;
            }

            STRBUILD::Sprintf(debug_out, "%s", "\r");
            STRBUILD::Sprintf(debug_out, "\t%+d\t%s\r", (int)colony->production[ECON_INDUSTRY], total_industry_produced_msg);
        }
    }
} // namespace COLCALC