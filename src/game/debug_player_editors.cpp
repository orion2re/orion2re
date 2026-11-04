#include "pch.h"

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
#include <imgui.h>

namespace debug_game_editors {
    namespace {
        constexpr const char* k_ship_sizes[] = {
            "Frigate", "Destroyer", "Cruiser", "Battleship", "Titan", "Doom Star"};
        constexpr const char* k_ship_types[] = {
            "Combat", "Colony", "Transport", "Unused", "Outpost"};
        constexpr const char* k_population_jobs[] = {"Farmer", "Worker", "Scientist"};
        constexpr const char* k_hull_classes[] = {
            "Frigate", "Destroyer", "Cruiser", "Battleship", "Titan", "Doom Star", "Star Base", "Battlestation", "Star Fortress"};

        template <typename T>
        struct s_editor_state {
            bool open;
            bool focus_requested;
            bool discard_requested;
            int16_t index;
            int16_t pending_index;
            T baseline;
            T draft;
        };

        s_editor_state<s_player> g_player_editor = {};
        s_editor_state<s_antaran> g_antaran_editor = {};
        int32_t g_settler_slot = 0;
        int32_t g_tech_field = 0;
        int32_t g_tech_application = 0;
        int32_t g_tech_modifier = 0;
        int32_t g_trait = 0;
        int32_t g_history_turn = 0;
        int32_t g_counterparty = 0;

        bool Network_Game_() {
            return MOX::game_type == 2 || MOX::game_type == 3 ||
                NETMOX::net.is_network_active != 0;
        }

        bool Valid_Player_(int16_t player) {
            return MOX::player != nullptr && player >= 0 &&
                player < std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        }

        bool Input_S8_(const char* label, int8_t* value) {
            const int8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S8, value, &step, nullptr, "%d");
        }

        bool Input_U8_(const char* label, uint8_t* value) {
            const uint8_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U8, value, &step, nullptr, "%u");
        }

        bool Input_S16_(const char* label, int16_t* value) {
            const int16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S16, value, &step, nullptr, "%d");
        }

        bool Input_U16_(const char* label, uint16_t* value) {
            const uint16_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_U16, value, &step, nullptr, "%u");
        }

        bool Input_S32_(const char* label, int32_t* value) {
            const int32_t step = 1;
            return ImGui::InputScalar(label, ImGuiDataType_S32, value, &step, nullptr, "%d");
        }

        template <typename T>
        bool Dirty_(const s_editor_state<T>& editor) {
            return memcmp(&editor.draft, &editor.baseline, sizeof(T)) != 0;
        }

        template <typename T>
        bool Merge_Field_(const T& baseline, const T& draft, const T& live, T* merged) {
            if (memcmp(&draft, &baseline, sizeof(T)) == 0) {
                return true;
            }
            if (memcmp(&live, &baseline, sizeof(T)) != 0 &&
                memcmp(&live, &draft, sizeof(T)) != 0) {
                return false;
            }
            memcpy(merged, &draft, sizeof(T));
            return true;
        }

        bool Merge_Player_(
            const s_player& baseline,
            const s_player& draft,
            const s_player& live,
            s_player* merged) {
            *merged = live;
            bool valid = true;
#define MERGE_PLAYER_FIELD(field) \
    valid = Merge_Field_(baseline.field, draft.field, live.field, &merged->field) && valid
            MERGE_PLAYER_FIELD(picture);
            MERGE_PLAYER_FIELD(name);
            MERGE_PLAYER_FIELD(race_name);
            MERGE_PLAYER_FIELD(eliminated);
            MERGE_PLAYER_FIELD(race);
            MERGE_PLAYER_FIELD(color);
            MERGE_PLAYER_FIELD(personality);
            MERGE_PLAYER_FIELD(objectives);
            MERGE_PLAYER_FIELD(home_planet_id);
            MERGE_PLAYER_FIELD(network_player_id);
            MERGE_PLAYER_FIELD(player_done_flags);
            MERGE_PLAYER_FIELD(dead_field);
            MERGE_PLAYER_FIELD(research_breakthrough);
            MERGE_PLAYER_FIELD(tax_rate);
            MERGE_PLAYER_FIELD(bc);
            MERGE_PLAYER_FIELD(n_freighters);
            MERGE_PLAYER_FIELD(surplus_freighters);
            MERGE_PLAYER_FIELD(command_points);
            MERGE_PLAYER_FIELD(command_points_used);
            MERGE_PLAYER_FIELD(food_freighted);
            MERGE_PLAYER_FIELD(settlers_freighted);
            MERGE_PLAYER_FIELD(settlers);
            MERGE_PLAYER_FIELD(total_pop);
            MERGE_PLAYER_FIELD(food_produced);
            MERGE_PLAYER_FIELD(industry_produced);
            MERGE_PLAYER_FIELD(research_produced);
            MERGE_PLAYER_FIELD(bc_produced);
            MERGE_PLAYER_FIELD(surplus_food);
            MERGE_PLAYER_FIELD(surplus_bc);
            MERGE_PLAYER_FIELD(total_maintenance);
            MERGE_PLAYER_FIELD(maintenance);
            MERGE_PLAYER_FIELD(tech_fields);
            MERGE_PLAYER_FIELD(tech_applications);
            MERGE_PLAYER_FIELD(research_accumulated);
            MERGE_PLAYER_FIELD(captured_orion);
            MERGE_PLAYER_FIELD(captured_antares);
            MERGE_PLAYER_FIELD(won_council_vote);
            MERGE_PLAYER_FIELD(player_defeated);
            MERGE_PLAYER_FIELD(captured_population);
            MERGE_PLAYER_FIELD(last_attacker);
            MERGE_PLAYER_FIELD(ship_design_theme);
            MERGE_PLAYER_FIELD(ship_special_theme);
            MERGE_PLAYER_FIELD(tech_apps_learned);
            MERGE_PLAYER_FIELD(tech_modifiers);
            MERGE_PLAYER_FIELD(hyper_advanced_tech);
            MERGE_PLAYER_FIELD(total_known_tech_cost);
            MERGE_PLAYER_FIELD(knows_racial_stats);
            MERGE_PLAYER_FIELD(last_tech_report_date);
            MERGE_PLAYER_FIELD(player_tech_applications);
            MERGE_PLAYER_FIELD(current_research_field);
            MERGE_PLAYER_FIELD(current_research_application);
            MERGE_PLAYER_FIELD(got_new_app);
            MERGE_PLAYER_FIELD(ship_range);
            MERGE_PLAYER_FIELD(ship_designs);
            MERGE_PLAYER_FIELD(ships_built);
            MERGE_PLAYER_FIELD(contact);
            MERGE_PLAYER_FIELD(just_established_contact);
            MERGE_PLAYER_FIELD(n_times_established_contact);
            MERGE_PLAYER_FIELD(within_range);
            MERGE_PLAYER_FIELD(do_not_research);
            MERGE_PLAYER_FIELD(offensive_stagepoint);
            MERGE_PLAYER_FIELD(best_ftl_drive);
            MERGE_PLAYER_FIELD(ship_speed);
            MERGE_PLAYER_FIELD(officer_for_hire);
            MERGE_PLAYER_FIELD(n_trade_pops);
            MERGE_PLAYER_FIELD(current_trade_agreement_level);
            MERGE_PLAYER_FIELD(trade_agreement_goal);
            MERGE_PLAYER_FIELD(n_research_pops);
            MERGE_PLAYER_FIELD(current_research_agreement_level);
            MERGE_PLAYER_FIELD(research_agreement_goal);
            MERGE_PLAYER_FIELD(fighter_beam_type);
            MERGE_PLAYER_FIELD(fighter_bomb_type);
            MERGE_PLAYER_FIELD(total_research);
            MERGE_PLAYER_FIELD(total_ships);
            MERGE_PLAYER_FIELD(total_ship_strength);
            MERGE_PLAYER_FIELD(total_colonies);
            MERGE_PLAYER_FIELD(failed_to_expand);
            MERGE_PLAYER_FIELD(first_contact);
            MERGE_PLAYER_FIELD(relations);
            MERGE_PLAYER_FIELD(base_relations);
            MERGE_PLAYER_FIELD(treaty);
            MERGE_PLAYER_FIELD(trade_treaty);
            MERGE_PLAYER_FIELD(research_treaty);
            MERGE_PLAYER_FIELD(tribute_treaty);
            MERGE_PLAYER_FIELD(diplomacy_incident);
            MERGE_PLAYER_FIELD(diplomacy_message);
            MERGE_PLAYER_FIELD(diplomacy_severity);
            MERGE_PLAYER_FIELD(diplomacy_value);
            MERGE_PLAYER_FIELD(diplomacy_system);
            MERGE_PLAYER_FIELD(treaty_modifier);
            MERGE_PLAYER_FIELD(trade_modifier);
            MERGE_PLAYER_FIELD(tech_exchange_modifier);
            MERGE_PLAYER_FIELD(peace_modifier);
            MERGE_PLAYER_FIELD(last_bad_diplomatic_incident);
            MERGE_PLAYER_FIELD(broken_treaties_modifier);
            MERGE_PLAYER_FIELD(last_broken_treaty);
            MERGE_PLAYER_FIELD(last_gift);
            MERGE_PLAYER_FIELD(ship_war_losses);
            MERGE_PLAYER_FIELD(colony_war_losses);
            MERGE_PLAYER_FIELD(biological_weapons_flag);
            MERGE_PLAYER_FIELD(time_since_last_attack);
            MERGE_PLAYER_FIELD(threats);
            MERGE_PLAYER_FIELD(dishonored_flag);
            MERGE_PLAYER_FIELD(peace_duration);
            MERGE_PLAYER_FIELD(last_turn_diplomacy_severity);
            MERGE_PLAYER_FIELD(last_broken_treaty_type);
            MERGE_PLAYER_FIELD(last_turn_treaty);
            MERGE_PLAYER_FIELD(diplomacy_proposal_request);
            MERGE_PLAYER_FIELD(diplomacy_proposal_tribute_bribe);
            MERGE_PLAYER_FIELD(diplomacy_proposal_tech_bribe1);
            MERGE_PLAYER_FIELD(diplomacy_proposal_tech_bribe2);
            MERGE_PLAYER_FIELD(diplomacy_proposal_gold_bribe);
            MERGE_PLAYER_FIELD(diplomacy_proposal_war_player);
            MERGE_PLAYER_FIELD(diplomacy_proposal_exchange_max_value);
            MERGE_PLAYER_FIELD(diplomacy_proposal_exchange_tech);
            MERGE_PLAYER_FIELD(diplomacy_proposal_system_bribe);
            MERGE_PLAYER_FIELD(sneak_attack_planet);
            MERGE_PLAYER_FIELD(sneak_attack_message);
            MERGE_PLAYER_FIELD(sneak_attack_player);
            MERGE_PLAYER_FIELD(council_threat);
            MERGE_PLAYER_FIELD(reward_amount);
            MERGE_PLAYER_FIELD(reward_tech);
            MERGE_PLAYER_FIELD(reward_attack_player);
            MERGE_PLAYER_FIELD(food_shortage_duration);
            MERGE_PLAYER_FIELD(trust_worthiness);
            MERGE_PLAYER_FIELD(trust_breaker_player);
            MERGE_PLAYER_FIELD(trust_breaker_treaty);
            MERGE_PLAYER_FIELD(stop_spying_duration);
            MERGE_PLAYER_FIELD(stop_blockading_duration);
            MERGE_PLAYER_FIELD(war_buildup_duration);
            MERGE_PLAYER_FIELD(last_diplomacy_proposal_turn);
            MERGE_PLAYER_FIELD(diplomacy_proposal_rejection);
            MERGE_PLAYER_FIELD(opportunity_planet_num);
            MERGE_PLAYER_FIELD(opportunity_planet_value);
            MERGE_PLAYER_FIELD(opportunity_actual_attacker_ratio);
            MERGE_PLAYER_FIELD(temp_diplomacy_dummy1);
            MERGE_PLAYER_FIELD(diplomacy_demand_rejection_message);
            MERGE_PLAYER_FIELD(last_diplomacy_threat_turn);
            MERGE_PLAYER_FIELD(delayed_diplomacy_orders);
            MERGE_PLAYER_FIELD(traits);
            MERGE_PLAYER_FIELD(evolutionary_upgrade_traits);
            MERGE_PLAYER_FIELD(picked_evolutionary_upgrade);
            MERGE_PLAYER_FIELD(applied_evolutionary_upgrade);
            MERGE_PLAYER_FIELD(fleet_history);
            MERGE_PLAYER_FIELD(tech_history);
            MERGE_PLAYER_FIELD(population_history);
            MERGE_PLAYER_FIELD(production_history);
            MERGE_PLAYER_FIELD(spies);
            MERGE_PLAYER_FIELD(history_btns);
            MERGE_PLAYER_FIELD(ignoring);
            MERGE_PLAYER_FIELD(peace_flags);
            MERGE_PLAYER_FIELD(cheated);
            MERGE_PLAYER_FIELD(stardate_last_offered_hero);
            MERGE_PLAYER_FIELD(current_zoom_level);
            MERGE_PLAYER_FIELD(cur_map_x);
            MERGE_PLAYER_FIELD(cur_map_y);
            MERGE_PLAYER_FIELD(council_voted_for);
            MERGE_PLAYER_FIELD(surrender_to);
            MERGE_PLAYER_FIELD(time_since_last_lucky_event);
            MERGE_PLAYER_FIELD(has_galactic_lore);
            MERGE_PLAYER_FIELD(warned_about_cheating_player);
            MERGE_PLAYER_FIELD(cheated_this_turn);
            MERGE_PLAYER_FIELD(tribute_income);
            MERGE_PLAYER_FIELD(filler);
#undef MERGE_PLAYER_FIELD
            return valid;
        }

        bool Merge_Antaran_(
            const s_antaran& baseline,
            const s_antaran& draft,
            const s_antaran& live,
            s_antaran* merged) {
            *merged = live;
            bool valid = true;
#define MERGE_ANTARAN_FIELD(field) \
    valid = Merge_Field_(baseline.field, draft.field, live.field, &merged->field) && valid
            MERGE_ANTARAN_FIELD(attack_pressure);
            MERGE_ANTARAN_FIELD(resource_offensive);
            MERGE_ANTARAN_FIELD(resource_defensive);
            MERGE_ANTARAN_FIELD(defensive_ships);
            MERGE_ANTARAN_FIELD(offensive_ships);
            MERGE_ANTARAN_FIELD(offensive_deployed);
            MERGE_ANTARAN_FIELD(field6_0x3c);
            MERGE_ANTARAN_FIELD(invasion_stardate);
            MERGE_ANTARAN_FIELD(attack_notified_mask);
            MERGE_ANTARAN_FIELD(offensive_titan_built);
            MERGE_ANTARAN_FIELD(offensive_limit_exhausted_mask);
#undef MERGE_ANTARAN_FIELD
            return valid;
        }

        void Request_Redraw_() {
            SYS::Clear_System_Display_Loaded_Flag();
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            MOX::draw_main_screen_to_back = 1;
            video::Set_Refresh_Stencil();
        }

        bool Valid_Weapon_Arc_(int8_t arc) {
            return arc == 0 || arc == 1 || arc == 2 || arc == 4 || arc == 8 ||
                arc == 0x0f || arc == 0x10;
        }

        bool Validate_Design_(const s_ship_design& design) {
            if (design.name[sizeof(design.name) - 1] != '\0' ||
                design.size < 0 || design.size >= SHIP_SIZE_COUNT ||
                design.ship_type < 0 || design.ship_type >= SHIP_TYPE_COUNT ||
                design.ship_type == SHIP_TYPE_UNUSED ||
                design.shield_type < 0 || design.shield_type >= SHIELD_COUNT ||
                design.ftl_type < 0 || design.ftl_type >= DRIVE_COUNT ||
                design.computer_type < 0 || design.computer_type >= COMPUTER_COUNT ||
                design.armor_type < 0 || design.armor_type >= ARMOR_COUNT ||
                design.picture_num < 0 || design.picture_num > 47 ||
                design.previous_owner > NONPLAYER_SHIP_TYPE_HYDRA) {
                return false;
            }
            for (const s_ship_weapons& weapon : design.ship_weapon) {
                if (weapon.type < 0 || weapon.type >= WEAPON_COUNT ||
                    weapon.count < 0 || weapon.current_count < 0 ||
                    weapon.current_count > weapon.count ||
                    !Valid_Weapon_Arc_(weapon.firing_arc)) {
                    return false;
                }
            }
            return true;
        }

        bool Validate_Player_(const s_player& player) {
            if (player.name[sizeof(player.name) - 1] != '\0' ||
                player.race_name[sizeof(player.race_name) - 1] != '\0' ||
                player.race > 14 || player.color >= MAX_PLAYERS ||
                player.personality >= PLAYER_PERSONALITY_COUNT ||
                !((player.objectives <= PLAYER_OBJECTIVE_ECOLOGIST) ||
                    player.objectives == PLAYER_OBJECTIVE_HUMAN) ||
                player.home_planet_id < -1 || player.home_planet_id >= MOX::planet_count ||
                player.officer_for_hire < -1 || player.officer_for_hire >= MAX_LEADERS ||
                player.settlers_freighted < 0 ||
                player.settlers_freighted > MAX_FREIGHTED_SETTLERS ||
                player.current_research_field < 0 ||
                player.current_research_field >= TECH_FIELD_COUNT ||
                (player.current_research_application != -1 &&
                    (uint8_t)player.current_research_application >= TECH_APP_COUNT) ||
                (player.current_research_field != 0 &&
                    player.current_research_application == -1) ||
                player.council_voted_for < -1 || player.council_voted_for >= MAX_PLAYERS ||
                player.surrender_to < -1 || player.surrender_to >= MAX_PLAYERS) {
                return false;
            }
            for (const s_ship_design& design : player.ship_designs) {
                if (!Validate_Design_(design)) {
                    return false;
                }
            }
            for (int32_t index = 0; index < player.settlers_freighted; ++index) {
                const SETTLER::value_type value = player.settlers[index].val;
                const int16_t source = SETTLER::Get_Source_Colony(value);
                const int16_t destination = SETTLER::Get_Dest_Planet(value);
                if ((source != SETTLER::SOURCE_COLONY_IN_TRANSIT &&
                        (source < 0 || source >= MOX::NUM_COLONIES)) ||
                    destination < 0 || destination >= MOX::planet_count ||
                    SETTLER::Get_Race(value) > POP::RACE_NATIVE ||
                    SETTLER::Get_Prof(value) > POP::PROF_SCIENTIST) {
                    return false;
                }
            }
            return true;
        }

        bool Validate_Antaran_(const s_antaran& antaran) {
            if (antaran.resource_offensive < 0 || antaran.resource_defensive < 0 ||
                antaran.offensive_titan_built > 1) {
                return false;
            }
            for (int32_t index = 0; index < HULL_CLASS_COUNT; ++index) {
                if (antaran.defensive_ships[index] < 0 ||
                    antaran.offensive_ships[index] < 0 ||
                    antaran.offensive_deployed[index] < 0) {
                    return false;
                }
            }
            return true;
        }

        void Recalculate_Player_(int16_t player_index) {
            s_player& player = MOX::player[player_index];
            for (s_ship_design& design : player.ship_designs) {
                design.speed = static_cast<uint8_t>(INITSHIP::Get_FTL_Speed(
                    player_index, design.ftl_type));
                design.cost = DESIGN::Calculate_Design(
                    player_index, &design, &design.combat_speed);
            }
            COLCALC::Compute_All_Star_Colony_Stuff();
            COLCALC::All_Colony_Calculations();
            COLCALC::Compute_Player_Total_Known_Tech_Cost(&player);
            COLCALC::Update_Player_Stats(&player);
            SHIPMOVE::Update_All_Ship_Ranges();
            SHIPMOVE::Recalculate_All_Moving_Fleets();
            SHIPS::Build_Ship_Icons();
            Request_Redraw_();
        }

        void Read_Only_S16_(const char* label, int16_t* value) {
            ImGui::BeginDisabled();
            Input_S16_(label, value);
            ImGui::EndDisabled();
        }

        void Read_Only_S32_(const char* label, int32_t* value) {
            ImGui::BeginDisabled();
            Input_S32_(label, value);
            ImGui::EndDisabled();
        }

        void Read_Only_S8_(const char* label, int8_t* value) {
            ImGui::BeginDisabled();
            Input_S8_(label, value);
            ImGui::EndDisabled();
        }

        void Read_Only_U8_(const char* label, uint8_t* value) {
            ImGui::BeginDisabled();
            Input_U8_(label, value);
            ImGui::EndDisabled();
        }

        void Draw_Identity_(s_player& player) {
            ImGui::InputText("name", player.name, sizeof(player.name));
            ImGui::InputText("race_name", player.race_name, sizeof(player.race_name));
            Input_S8_("picture", &player.picture);
            Input_U8_("eliminated", &player.eliminated);
            Input_U8_("race", &player.race);
            Input_U8_("color", &player.color);
            Input_U8_("personality", &player.personality);
            Input_U8_("objectives", &player.objectives);
            debug_game_widgets::Planet_Reference(
                "home_planet_id", &player.home_planet_id);
            Input_S16_("network_player_id", &player.network_player_id);
            Input_S8_("player_done_flags", &player.player_done_flags);
            ImGui::Text("dead_field: %d (unknown, read-only)", (int)player.dead_field);
        }

        void Draw_Settler_(s_player& player) {
            ImGui::SliderInt(
                "Settler slot", &g_settler_slot, 0, MAX_FREIGHTED_SETTLERS - 1);
            s_settler_info& settler = player.settlers[g_settler_slot];
            int16_t source = SETTLER::Get_Source_Colony(settler.val);
            int16_t destination = SETTLER::Get_Dest_Planet(settler.val);
            uint8_t race = (uint8_t)SETTLER::Get_Race(settler.val);
            uint8_t eta = (uint8_t)SETTLER::Get_Eta(settler.val);
            int8_t job = (int8_t)SETTLER::Get_Prof(settler.val);
            if (Input_S16_("source_colony", &source)) {
                SETTLER::Set_Source_Colony(&settler.val, source);
            }
            if (debug_game_widgets::Planet_Reference(
                    "dest_planet", &destination, false)) {
                SETTLER::Set_Dest_Planet(&settler.val, destination);
            }
            if (Input_U8_("race", &race)) {
                SETTLER::Set_Race(&settler.val, race);
            }
            if (Input_U8_("eta", &eta)) {
                SETTLER::Set_Eta(&settler.val, eta);
            }
            if (debug_game_widgets::Enum(
                    "job", &job, k_population_jobs, 3)) {
                SETTLER::Set_Prof(&settler.val, (POP::e_prof)job);
            }
            ImGui::Text(
                "Packed: 0x%016llX | %s",
                (unsigned long long)settler.val,
                g_settler_slot < player.settlers_freighted ? "active" : "inactive");
        }

        void Draw_Economy_(s_player& player) {
            Input_S8_("research_breakthrough", &player.research_breakthrough);
            Input_U8_("tax_rate", &player.tax_rate);
            Input_S32_("bc", &player.bc);
            Input_S16_("n_freighters", &player.n_freighters);
            Read_Only_S16_("surplus_freighters", &player.surplus_freighters);
            Read_Only_S16_("command_points", &player.command_points);
            Read_Only_S16_("command_points_used", &player.command_points_used);
            Read_Only_S16_("food_freighted", &player.food_freighted);
            Input_S16_("settlers_freighted", &player.settlers_freighted);
            if (ImGui::CollapsingHeader("settlers")) {
                Draw_Settler_(player);
            }
            ImGui::SeparatorText("Calculated empire totals");
            Read_Only_S16_("total_pop", &player.total_pop);
            Read_Only_S16_("food_produced", &player.food_produced);
            Read_Only_S16_("industry_produced", &player.industry_produced);
            Read_Only_S16_("research_produced", &player.research_produced);
            Read_Only_S16_("bc_produced", &player.bc_produced);
            Read_Only_S16_("surplus_food", &player.surplus_food);
            Read_Only_S16_("surplus_bc", &player.surplus_bc);
            Read_Only_S32_("total_maintenance", &player.total_maintenance);
            for (int32_t index = 0; index < 6; ++index) {
                ImGui::PushID(index);
                char label[32];
                snprintf(label, sizeof(label), "maintenance[%d]", index);
                Read_Only_S16_(label, &player.maintenance[index]);
                ImGui::PopID();
            }
        }

        const char* Tech_Field_Name_(int32_t index) {
            const char* name = TECHDATA::technology_fields[index].name;
            return name != nullptr ? name : "Unnamed technology field";
        }

        const char* Tech_Application_Name_(int32_t index) {
            const char* name = TECHDATA::technology_applications[index].name;
            return name != nullptr ? name : "Unnamed technology application";
        }

        void Draw_Research_(s_player& player) {
            ImGui::SliderInt("Technology field index", &g_tech_field, 0, TECH_FIELD_COUNT - 1);
            ImGui::TextUnformatted(Tech_Field_Name_(g_tech_field));
            Input_U8_("tech_fields[index]", &player.tech_fields[g_tech_field]);
            ImGui::SliderInt(
                "Technology application index", &g_tech_application, 0, TECH_APP_COUNT - 1);
            ImGui::TextUnformatted(Tech_Application_Name_(g_tech_application));
            Input_U8_(
                "tech_applications[index]", &player.tech_applications[g_tech_application]);
            Input_S32_("research_accumulated", &player.research_accumulated);
            Input_S8_("captured_orion", &player.captured_orion);
            Input_S8_("captured_antares", &player.captured_antares);
            Input_S8_("won_council_vote", &player.won_council_vote);
            Input_U16_("player_defeated[counterparty]", &player.player_defeated[g_counterparty]);
            Input_S16_("captured_population", &player.captured_population);
            Input_S8_("last_attacker", &player.last_attacker);
            Input_U8_("ship_design_theme", &player.ship_design_theme);
            Input_S8_("ship_special_theme", &player.ship_special_theme);
            for (int32_t index = 0; index < 4; ++index) {
                ImGui::PushID(index);
                char label[40];
                snprintf(label, sizeof(label), "tech_apps_learned[%d]", index);
                Input_S8_(label, &player.tech_apps_learned[index]);
                ImGui::PopID();
            }
            ImGui::SliderInt("Tech modifier index", &g_tech_modifier, 0, 16);
            Input_S8_("tech_modifiers[index]", &player.tech_modifiers[g_tech_modifier]);
            Input_U8_(
                "hyper_advanced_tech[counterparty]",
                &player.hyper_advanced_tech[g_counterparty]);
            Read_Only_S32_("total_known_tech_cost", &player.total_known_tech_cost);
            Input_S8_("knows_racial_stats", &player.knows_racial_stats);
            Input_S32_(
                "last_tech_report_date[counterparty]",
                &player.last_tech_report_date[g_counterparty]);
            if (ImGui::TreeNode("player_tech_applications[counterparty]")) {
                for (int32_t index = 0; index < 27; ++index) {
                    ImGui::PushID(index);
                    Input_S8_("application", &player.player_tech_applications[g_counterparty][index]);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            Input_S8_("current_research_field", &player.current_research_field);
            int16_t current_application = player.current_research_application == -1
                ? -1
                : (uint8_t)player.current_research_application;
            if (Input_S16_("current_research_application", &current_application) &&
                current_application >= -1 && current_application < TECH_APP_COUNT) {
                player.current_research_application = current_application < 0
                    ? -1
                    : (int8_t)(uint8_t)current_application;
            }
            Input_S8_("got_new_app", &player.got_new_app);
            Read_Only_S16_("ship_range", &player.ship_range);
        }

        void Draw_Ship_Design_(s_ship_design& design, int32_t design_index) {
            ImGui::PushID(design_index);
            ImGui::InputText("name", design.name, sizeof(design.name));
            debug_game_widgets::Enum("size", &design.size, k_ship_sizes, SHIP_SIZE_COUNT);
            debug_game_widgets::Enum("ship_type", &design.ship_type, k_ship_types, SHIP_TYPE_COUNT);
            Input_S8_("shield_type", &design.shield_type);
            Input_S8_("ftl_type", &design.ftl_type);
            Read_Only_U8_("speed", &design.speed);
            Input_S8_("computer_type", &design.computer_type);
            Input_S8_("armor_type", &design.armor_type);
            if (ImGui::TreeNode("special_device_flags")) {
                debug_game_widgets::Ship_Specials(
                    "specials", design.special_device_flags, sizeof(design.special_device_flags));
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("ship_weapon")) {
                for (int32_t slot = 0; slot < 8; ++slot) {
                    ImGui::PushID(slot);
                    s_ship_weapons& weapon = design.ship_weapon[slot];
                    char label[32];
                    snprintf(label, sizeof(label), "Slot %d", slot + 1);
                    if (ImGui::TreeNode(label)) {
                        Input_S16_("type", &weapon.type);
                        Input_S8_("count", &weapon.count);
                        Input_S8_("current_count", &weapon.current_count);
                        Input_S8_("firing_arc", &weapon.firing_arc);
                        Input_U16_("specials", &weapon.specials);
                        Input_S8_("ammo", &weapon.ammo);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            Input_S8_("picture_num", &design.picture_num);
            Input_U8_("previous_owner", &design.previous_owner);
            Read_Only_S16_("cost", &design.cost);
            Read_Only_U8_("combat_speed", &design.combat_speed);
            Input_U16_("date_of_design", &design.date_of_design);
            ImGui::PopID();
        }

        void Draw_Designs_(s_player& player) {
            for (int32_t index = 0; index < SHIP_SIZE_COUNT; ++index) {
                char label[64];
                snprintf(
                    label,
                    sizeof(label),
                    "%d: %.*s",
                    index,
                    (int)sizeof(player.ship_designs[index].name),
                    player.ship_designs[index].name);
                if (ImGui::CollapsingHeader(label)) {
                    Draw_Ship_Design_(player.ship_designs[index], index);
                }
                ImGui::PushID(index);
                char built_label[32];
                snprintf(built_label, sizeof(built_label), "ships_built[%d]", index);
                Input_S16_(built_label, &player.ships_built[index]);
                ImGui::PopID();
            }
        }

        void Draw_Strategy_(s_player& player) {
            Input_S8_("just_established_contact", &player.just_established_contact);
            Input_S8_("do_not_research", &player.do_not_research);
            Input_S16_("offensive_stagepoint", &player.offensive_stagepoint);
            Read_Only_S8_("best_ftl_drive", &player.best_ftl_drive);
            Read_Only_U8_("ship_speed", &player.ship_speed);
            debug_game_widgets::Officer_Reference(
                "officer_for_hire", &player.officer_for_hire);
            Read_Only_S16_("n_trade_pops", &player.n_trade_pops);
            Read_Only_S16_("n_research_pops", &player.n_research_pops);
            Input_S8_("fighter_beam_type", &player.fighter_beam_type);
            Input_S8_("fighter_bomb_type", &player.fighter_bomb_type);
            Read_Only_S16_("total_research", &player.total_research);
            Read_Only_S16_("total_ships", &player.total_ships);
            Read_Only_S32_(
                "total_ship_strength[counterparty]",
                &player.total_ship_strength[g_counterparty]);
            Read_Only_S16_("total_colonies", &player.total_colonies);
            Input_S8_("failed_to_expand", &player.failed_to_expand);
        }

        void Draw_Counterparty_Selector_() {
            g_counterparty = std::clamp(g_counterparty, 0, MAX_PLAYERS - 1);
            ImGui::SliderInt("Counterparty slot", &g_counterparty, 0, MAX_PLAYERS - 1);
            if (Valid_Player_((int16_t)g_counterparty)) {
                char race_name[sizeof(MOX::player[0].race_name) + 1];
                strlcpy(
                    race_name,
                    MOX::player[g_counterparty].race_name,
                    sizeof(race_name));
                ImGui::SameLine();
                ImGui::TextUnformatted(race_name);
            } else {
                ImGui::SameLine();
                ImGui::TextUnformatted("Inactive player slot");
            }
        }

        void Draw_Diplomacy_(s_player& player) {
#define DIP_S8(field) Input_S8_(#field, &player.field[g_counterparty])
#define DIP_U8(field) Input_U8_(#field, &player.field[g_counterparty])
#define DIP_S16(field) Input_S16_(#field, &player.field[g_counterparty])
#define DIP_S32(field) Input_S32_(#field, &player.field[g_counterparty])
            DIP_S8(contact);
            DIP_S8(n_times_established_contact);
            DIP_S8(within_range);
            DIP_S16(current_trade_agreement_level);
            DIP_S16(trade_agreement_goal);
            DIP_S16(current_research_agreement_level);
            DIP_S16(research_agreement_goal);
            DIP_S8(first_contact);
            DIP_S8(relations);
            DIP_S8(base_relations);
            DIP_S8(treaty);
            DIP_S8(trade_treaty);
            DIP_S8(research_treaty);
            DIP_S16(tribute_treaty);
            DIP_U8(diplomacy_incident);
            DIP_S8(diplomacy_message);
            DIP_S16(diplomacy_severity);
            DIP_S16(diplomacy_value);
            DIP_S16(diplomacy_system);
            DIP_S16(treaty_modifier);
            DIP_S16(trade_modifier);
            DIP_S16(tech_exchange_modifier);
            DIP_S16(peace_modifier);
            DIP_S8(last_bad_diplomatic_incident);
            DIP_S8(broken_treaties_modifier);
            DIP_S8(last_broken_treaty);
            DIP_S8(last_gift);
            DIP_S16(ship_war_losses);
            DIP_S16(colony_war_losses);
            DIP_S8(biological_weapons_flag);
            DIP_S8(time_since_last_attack);
            DIP_S8(threats);
            DIP_S8(dishonored_flag);
            DIP_S8(peace_duration);
            DIP_S16(last_turn_diplomacy_severity);
            DIP_S8(last_broken_treaty_type);
            DIP_S8(last_turn_treaty);
            DIP_S8(diplomacy_proposal_request);
            DIP_S8(diplomacy_proposal_tribute_bribe);
            DIP_U8(diplomacy_proposal_tech_bribe1);
            DIP_U8(diplomacy_proposal_tech_bribe2);
            DIP_S16(diplomacy_proposal_gold_bribe);
            DIP_S8(diplomacy_proposal_war_player);
            DIP_S32(diplomacy_proposal_exchange_max_value);
            DIP_U8(diplomacy_proposal_exchange_tech);
            DIP_S16(diplomacy_proposal_system_bribe);
            DIP_S16(reward_amount);
            DIP_U8(reward_tech);
            DIP_S8(reward_attack_player);
            DIP_S8(trust_worthiness);
            DIP_S8(trust_breaker_player);
            DIP_S8(trust_breaker_treaty);
            DIP_S8(stop_spying_duration);
            DIP_S8(stop_blockading_duration);
            DIP_S16(last_diplomacy_proposal_turn);
            DIP_S16(diplomacy_proposal_rejection);
            DIP_S16(opportunity_planet_num);
            DIP_S16(opportunity_planet_value);
            DIP_S32(opportunity_actual_attacker_ratio);
            DIP_S16(temp_diplomacy_dummy1);
            DIP_S8(diplomacy_demand_rejection_message);
            DIP_U8(last_diplomacy_threat_turn);
            DIP_S8(delayed_diplomacy_orders);
            DIP_U8(spies);
            DIP_S8(peace_flags);
#undef DIP_S8
#undef DIP_U8
#undef DIP_S16
#undef DIP_S32
            ImGui::SeparatorText("Player-wide diplomacy state");
            Input_S16_("sneak_attack_planet", &player.sneak_attack_planet);
            Input_S8_("sneak_attack_message", &player.sneak_attack_message);
            Input_S8_("sneak_attack_player", &player.sneak_attack_player);
            Input_S8_("council_threat", &player.council_threat);
            Input_S16_("food_shortage_duration", &player.food_shortage_duration);
            Input_S8_("war_buildup_duration", &player.war_buildup_duration);
        }

        void Draw_Traits_And_History_(s_player& player) {
            ImGui::SliderInt("Trait index", &g_trait, 0, TRAIT_COUNT - 1);
            Input_S8_("traits[index]", &player.traits[g_trait]);
            Input_S8_(
                "evolutionary_upgrade_traits[index]",
                &player.evolutionary_upgrade_traits[g_trait]);
            Input_S8_("picked_evolutionary_upgrade", &player.picked_evolutionary_upgrade);
            Input_S8_("applied_evolutionary_upgrade", &player.applied_evolutionary_upgrade);
            ImGui::SeparatorText("History arrays");
            ImGui::SliderInt("History turn", &g_history_turn, 0, 349);
            Input_S8_("fleet_history[turn]", &player.fleet_history[g_history_turn]);
            Input_S8_("tech_history[turn]", &player.tech_history[g_history_turn]);
            Input_S8_("population_history[turn]", &player.population_history[g_history_turn]);
            Input_S8_("production_history[turn]", &player.production_history[g_history_turn]);
            Input_S8_("history_btns", &player.history_btns);
        }

        void Draw_Map_And_Lifecycle_(s_player& player) {
            Input_U8_("ignoring", &player.ignoring);
            Input_S8_("cheated", &player.cheated);
            Input_U16_("stardate_last_offered_hero", &player.stardate_last_offered_hero);
            Input_S8_("current_zoom_level", &player.current_zoom_level);
            Input_S16_("cur_map_x", &player.cur_map_x);
            Input_S16_("cur_map_y", &player.cur_map_y);
            Input_S8_("council_voted_for", &player.council_voted_for);
            Input_S8_("surrender_to", &player.surrender_to);
            Input_S16_("time_since_last_lucky_event", &player.time_since_last_lucky_event);
            Input_S8_("has_galactic_lore", &player.has_galactic_lore);
            Input_S8_("warned_about_cheating_player", &player.warned_about_cheating_player);
            Input_S8_("cheated_this_turn", &player.cheated_this_turn);
            Read_Only_S32_("tribute_income", &player.tribute_income);
            if (ImGui::TreeNode("filler (unknown, read-only)")) {
                for (int32_t index = 0; index < 45; index += 15) {
                    ImGui::Text(
                        "%02d: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                        index,
                        (unsigned)(uint8_t)player.filler[index + 0],
                        (unsigned)(uint8_t)player.filler[index + 1],
                        (unsigned)(uint8_t)player.filler[index + 2],
                        (unsigned)(uint8_t)player.filler[index + 3],
                        (unsigned)(uint8_t)player.filler[index + 4],
                        (unsigned)(uint8_t)player.filler[index + 5],
                        (unsigned)(uint8_t)player.filler[index + 6],
                        (unsigned)(uint8_t)player.filler[index + 7],
                        (unsigned)(uint8_t)player.filler[index + 8],
                        (unsigned)(uint8_t)player.filler[index + 9],
                        (unsigned)(uint8_t)player.filler[index + 10],
                        (unsigned)(uint8_t)player.filler[index + 11],
                        (unsigned)(uint8_t)player.filler[index + 12],
                        (unsigned)(uint8_t)player.filler[index + 13],
                        (unsigned)(uint8_t)player.filler[index + 14]);
                }
                ImGui::TreePop();
            }
        }

        void Draw_Player_Form_(s_player& player) {
            Draw_Counterparty_Selector_();
            if (ImGui::BeginTabBar("player_tabs")) {
                if (ImGui::BeginTabItem("Identity")) {
                    Draw_Identity_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Economy")) {
                    Draw_Economy_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Research")) {
                    Draw_Research_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Designs")) {
                    Draw_Designs_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Strategy")) {
                    Draw_Strategy_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Diplomacy")) {
                    Draw_Diplomacy_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Traits/history")) {
                    Draw_Traits_And_History_(player);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Map/lifecycle")) {
                    Draw_Map_And_Lifecycle_(player);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }

        void Load_Player_(int16_t player) {
            g_player_editor.index = player;
            g_player_editor.pending_index = -1;
            g_player_editor.baseline = MOX::player[player];
            g_player_editor.draft = g_player_editor.baseline;
            g_player_editor.open = true;
            g_player_editor.focus_requested = true;
            g_counterparty = std::clamp<int32_t>(g_counterparty, 0, MAX_PLAYERS - 1);
        }

        void Load_Antaran_(int16_t ignored) {
            (void)ignored;
            g_antaran_editor.index = 0;
            g_antaran_editor.pending_index = -1;
            g_antaran_editor.baseline = MOX::antarans;
            g_antaran_editor.draft = g_antaran_editor.baseline;
            g_antaran_editor.open = true;
            g_antaran_editor.focus_requested = true;
        }

        template <typename T>
        void Draw_Discard_Popup_(
            const char* popup,
            s_editor_state<T>* editor,
            void (*load)(int16_t)) {
            if (editor->discard_requested) {
                ImGui::OpenPopup(popup);
                editor->discard_requested = false;
            }
            if (ImGui::BeginPopupModal(
                    popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Discard unsaved changes?");
                if (ImGui::Button("Discard")) {
                    const int16_t pending = editor->pending_index;
                    ImGui::CloseCurrentPopup();
                    if (pending >= 0) {
                        load(pending);
                    } else {
                        editor->open = false;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep editing")) {
                    editor->pending_index = -1;
                    editor->open = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        void Draw_Player_Editor_() {
            if (!g_player_editor.open || !Valid_Player_(g_player_editor.index)) {
                g_player_editor.open = false;
                return;
            }
            if (g_player_editor.focus_requested) {
                ImGui::SetNextWindowFocus();
                g_player_editor.focus_requested = false;
            }
            ImGui::SetNextWindowSize(ImVec2(760.0f, 680.0f), ImGuiCond_FirstUseEver);
            char title[64];
            snprintf(title, sizeof(title), "Player Editor [%d]", (int)g_player_editor.index);
            bool open = true;
            if (ImGui::Begin(title, &open)) {
                int32_t selected_player = g_player_editor.index;
                const int32_t player_count = std::clamp<int16_t>(
                    MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
                ImGui::BeginDisabled(Dirty_(g_player_editor));
                if (player_count > 0 && ImGui::SliderInt("Player", &selected_player, 0, player_count - 1) &&
                    selected_player != g_player_editor.index) {
                    Load_Player_((int16_t)selected_player);
                }
                ImGui::EndDisabled();
                Draw_Player_Form_(g_player_editor.draft);
                ImGui::Separator();
                const bool dirty = Dirty_(g_player_editor);
                const bool valid = Validate_Player_(g_player_editor.draft);
                s_player merged;
                const bool mergeable = Merge_Player_(
                    g_player_editor.baseline,
                    g_player_editor.draft,
                    MOX::player[g_player_editor.index],
                    &merged);
                ImGui::BeginDisabled(
                    !dirty || !valid || !mergeable || Network_Game_());
                if (ImGui::Button("Save")) {
                    MOX::player[g_player_editor.index] = merged;
                    Recalculate_Player_(g_player_editor.index);
                    Load_Player_(g_player_editor.index);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Reload live")) {
                    Load_Player_(g_player_editor.index);
                }
                ImGui::SameLine();
                if (ImGui::Button("Close")) {
                    if (dirty) {
                        g_player_editor.pending_index = -1;
                        g_player_editor.discard_requested = true;
                    } else {
                        open = false;
                    }
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(
                    !mergeable ? "Conflict with live state"
                               : (!valid ? "Invalid values"
                                         : (dirty ? "Unsaved changes" : "No changes")));
                if (Network_Game_()) {
                    ImGui::TextUnformatted("Player mutations are disabled in network games.");
                }
            }
            ImGui::End();
            if (!open && Dirty_(g_player_editor)) {
                g_player_editor.pending_index = -1;
                g_player_editor.discard_requested = true;
                open = true;
            }
            g_player_editor.open = open;
            Draw_Discard_Popup_("Discard player changes?", &g_player_editor, Load_Player_);
        }

        void Draw_Antaran_Form_(s_antaran& antaran) {
            Input_S16_("attack_pressure", &antaran.attack_pressure);
            Input_S16_("resource_offensive", &antaran.resource_offensive);
            Input_S16_("resource_defensive", &antaran.resource_defensive);
            if (ImGui::BeginTable("antaran_fleets", 4, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Hull");
                ImGui::TableSetupColumn("Defensive");
                ImGui::TableSetupColumn("Offensive reserve");
                ImGui::TableSetupColumn("Offensive deployed");
                ImGui::TableHeadersRow();
                for (int32_t index = 0; index < HULL_CLASS_COUNT; ++index) {
                    ImGui::PushID(index);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(k_hull_classes[index]);
                    ImGui::TableSetColumnIndex(1);
                    Input_S16_("##defensive", &antaran.defensive_ships[index]);
                    ImGui::TableSetColumnIndex(2);
                    Input_S16_("##offensive", &antaran.offensive_ships[index]);
                    ImGui::TableSetColumnIndex(3);
                    Input_S16_("##deployed", &antaran.offensive_deployed[index]);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::Text(
                "field6_0x3c: 0x%02X (unknown, read-only)",
                (unsigned)antaran.field6_0x3c);
            Input_S32_("invasion_stardate", &antaran.invasion_stardate);
            Input_U8_("attack_notified_mask", &antaran.attack_notified_mask);
            Input_U8_("offensive_titan_built", &antaran.offensive_titan_built);
            Input_U8_(
                "offensive_limit_exhausted_mask",
                &antaran.offensive_limit_exhausted_mask);
            ImGui::SeparatorText("Calculated live values");
            ImGui::TextUnformatted("These values update after the draft is saved.");
            ImGui::Text("Total resources: %d", (int)antarans::Antaran_Total_Resources());
            ImGui::Text("Offensive force: %d", (int)antarans::Antaran_Offensive_Force());
            ImGui::Text("Defensive force: %d", (int)antarans::Antaran_Defensive_Force());
            ImGui::Text("Next offensive size: %d", (int)antarans::Next_Antaran_Ship_Size());
            ImGui::Text(
                "Should attack: %s",
                antarans::Antarans_Should_Attack() ? "yes" : "no");
        }

        void Draw_Antaran_Editor_() {
            if (!g_antaran_editor.open) {
                return;
            }
            if (g_antaran_editor.focus_requested) {
                ImGui::SetNextWindowFocus();
                g_antaran_editor.focus_requested = false;
            }
            ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);
            bool open = true;
            if (ImGui::Begin("Antaran Editor", &open)) {
                Draw_Antaran_Form_(g_antaran_editor.draft);
                ImGui::Separator();
                const bool dirty = Dirty_(g_antaran_editor);
                const bool valid = Validate_Antaran_(g_antaran_editor.draft);
                s_antaran merged;
                const bool mergeable = Merge_Antaran_(
                    g_antaran_editor.baseline,
                    g_antaran_editor.draft,
                    MOX::antarans,
                    &merged);
                ImGui::BeginDisabled(
                    !dirty || !valid || !mergeable || Network_Game_());
                if (ImGui::Button("Save")) {
                    MOX::antarans = merged;
                    Load_Antaran_(0);
                    Request_Redraw_();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Reload live")) {
                    Load_Antaran_(0);
                }
                ImGui::SameLine();
                if (ImGui::Button("Close")) {
                    if (dirty) {
                        g_antaran_editor.pending_index = -1;
                        g_antaran_editor.discard_requested = true;
                    } else {
                        open = false;
                    }
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(
                    !mergeable ? "Conflict with live state"
                               : (!valid ? "Invalid values"
                                         : (dirty ? "Unsaved changes" : "No changes")));
                if (Network_Game_()) {
                    ImGui::TextUnformatted("Antaran mutations are disabled in network games.");
                }
            }
            ImGui::End();
            if (!open && Dirty_(g_antaran_editor)) {
                g_antaran_editor.pending_index = -1;
                g_antaran_editor.discard_requested = true;
                open = true;
            }
            g_antaran_editor.open = open;
            Draw_Discard_Popup_(
                "Discard Antaran changes?", &g_antaran_editor, Load_Antaran_);
        }

        void Draw_(const debug_overlay::s_panel_context& context) {
            (void)context;
            Draw_Player_Editor_();
            Draw_Antaran_Editor_();
        }

        debug_overlay::s_screen_panel_registration g_registration(
            debug_overlay::k_all_screens, Draw_);
    } // namespace

    void Open_Player(int16_t player_index) {
        if (!Valid_Player_(player_index)) {
            return;
        }
        if (g_player_editor.open && g_player_editor.index == player_index) {
            g_player_editor.focus_requested = true;
            return;
        }
        if (g_player_editor.open && Dirty_(g_player_editor)) {
            g_player_editor.pending_index = player_index;
            g_player_editor.discard_requested = true;
            return;
        }
        Load_Player_(player_index);
    }

    void Open_Antaran() {
        if (g_antaran_editor.open) {
            g_antaran_editor.focus_requested = true;
            return;
        }
        Load_Antaran_(0);
    }
} // namespace debug_game_editors
#else
namespace debug_game_editors {
    void Open_Player(int16_t player_index) {
        (void)player_index;
    }

    void Open_Antaran() {
    }
} // namespace debug_game_editors
#endif