#include "pch.h"
#include "rpc/colony_rpc.h"
#include "rpc/rpc_game.h"
#include "game/colony_read.h"
#include "config/text_encoding.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>

namespace colony_rpc {
    namespace {
        std::string Text(const char* text, size_t capacity) {
            std::string result;
            if (text != nullptr) config::Decode_Cp850_To_Utf8(text, strnlen(text, capacity), &result);
            return result;
        }

        void Read_Operations(int16_t index, int16_t viewer, rpc::s_colony_operations* out) {
            const auto* colony = &MOX::colony[index];
            // Missing native strings/storage is not an empty operational view.
            if (ESTRINGS::E_Strings(ESTR_POP_D03D_K_DK) == nullptr ||
                ESTRINGS::E_Strings(76) == nullptr || ESTRINGS::E_Strings(12) == nullptr ||
                ESTRINGS::E_Strings(ESTR_CONQUERED) == nullptr || TECHDATA::units[1].name == nullptr || TECHDATA::units[2].name == nullptr ||
                MOX::player[colony->owner].settlers_freighted < 0 ||
                (size_t)MOX::player[colony->owner].settlers_freighted > std::size(MOX::player[colony->owner].settlers)) return;
            for (int i = 0; i < colony->n_pops; ++i) {
                const auto pop = colony->pop[i];
                if (POP::Get_Race(pop) > 9 || POP::Get_Prof(pop) > 2 || !POP::Is_Assigned(pop)) return;
                const auto race = POP::Get_Race(pop);
                if (race < MAX_PLAYERS && memchr(MOX::player[race].race_name, 0, sizeof(MOX::player[race].race_name)) == nullptr) return;
                if (COLONY::Pop_Race_String(index, i) == nullptr || COLONY::Job_String(POP::Get_Prof(pop)) == nullptr) return;
            }
            rpc::s_colony_operations result;
            char text[500];
            if (colony_read::Heading(index, text, 104)) {
                result.heading_available = true;
                result.heading = Text(text, 104);
            }
            colony_read::Population_Text(colony, text, 104);
            result.population_text = Text(text, 104);
            result.autobuild_active = colony_read::Autobuild_Text(colony, text, 40);
            result.autobuild = Text(text, 40);
            result.inbound = colony_read::Inbound_Population(colony);
            result.farm_capacity = colony->max_farms == 0xff ? -1 : colony->max_farms;
            const auto star = MOX::planet[colony->planet].star_index;
            result.blockaded = ((MOX::star[star].blockaded >> colony->owner) & 1) != 0;
            const auto status = colony_read::Status_String_Id(index, viewer);
            if (status >= 0) {
                if (ESTRINGS::E_Strings(status) == nullptr) return;
                result.status_present = true;
                result.status = Text(ESTRINGS::E_Strings(status), 256);
            }
            result.infantry_name = Text(TECHDATA::units[1].name, 256);
            result.armor_name = Text(TECHDATA::units[2].name, 256);
            const int16_t officer = MOX::star[star].officer_index[viewer];
            bool garrison_ready = officer == -1;
            if (officer >= 0 && officer < MAX_LEADERS && MOX::leaders != nullptr)
                garrison_ready = MOX::leaders[officer].player_index == viewer && MOX::leaders[officer].status == 1 &&
                    MOX::leaders[officer].type == LEADER_TYPE_COLONY && MOX::leaders[officer].location == star;
            for (int i = 1; i < ARMOR_COUNT; ++i) garrison_ready &= TECHDATA::armor[i].tech_app_id >= 0 && (size_t)TECHDATA::armor[i].tech_app_id < std::size(MOX::player[viewer].tech_applications);
            for (int i = 1; i <= RIFLE_PLASMA_RIFLE; ++i) garrison_ready &= TECHDATA::rifles[i].tech_app_id >= 0 && (size_t)TECHDATA::rifles[i].tech_app_id < std::size(MOX::player[viewer].tech_applications);
            const auto shield_tech = TECHDATA::personal_shields[PERSONAL_SHIELD_PERSONAL_SHIELD].tech_app_id;
            garrison_ready &= shield_tech >= 0 && (size_t)shield_tech < std::size(MOX::player[viewer].tech_applications);
            for (int16_t unit = 0; unit < 3; ++unit)
                garrison_ready &= COLONY::Player_Troop_Name(viewer, unit, MOX::player[viewer].race) != nullptr;
            if (garrison_ready) {
                s_invasion_side_info summary;
                invasion::Compute_Colony_Ground_Combat_Info(&MOX::colony[index], &summary, false);
                for (int16_t unit = 0; unit < 3; ++unit)
                    result.garrison.push_back({Text(COLONY::Player_Troop_Name(viewer, unit, MOX::player[viewer].race), 256), summary.unit_counts[unit], summary.unit_strengths[unit], (uint8_t)summary.unit_hps[unit]});
                result.garrison_available = true;
            }
            const auto event = EVENTS::Colony_Has_Event(index);
            if (event >= 0) {
                const char* label = colony_read::Event_Label(event);
                if (label == nullptr) return;
                result.event = Text(label, 256);
                result.event_present = true;
            }
            const char* jobs[] = {"farmer", "worker", "scientist"};
            int16_t population_order[MAX_COLONY_POPULATION];
            const auto population_count = colony_read::Population_Order(index, -1, population_order);
            for (int ordinal = 0; ordinal < population_count; ++ordinal) {
                const int i = population_order[ordinal];
                const auto pop = colony->pop[i];
                const auto race = POP::Get_Race(pop);
                rpc::s_colony_worker worker;
                COLMOVE::Colony_Pop_String(index, i, text, sizeof(text));
                worker.label = Text(text, sizeof(text));
                worker.species = Text(COLONY::Pop_Race_String(index, i), 256);
                worker.job = jobs[POP::Get_Prof(pop)];
                worker.state = race == 9 ? "native" : (race == 8 ? "android" : "colonist");
                worker.assigned = POP::Is_Assigned(pop);
                worker.conquered = POP::Is_Conquered(pop);
                if (race < MAX_PLAYERS) worker.player_slot = race;
                result.workforce.push_back(std::move(worker));
            }
            for (int16_t building = 1; building < BUILDING_COUNT; ++building) {
                if (!colony_read::Building_Present(colony, building)) continue;
                const bool satellite = TECHDATA::buildings[building].group == 7;
                if (satellite && colony_read::Satellite_Replaced(colony, building)) continue;
                if (TECHDATA::buildings[building].name == nullptr) return;
                result.structures.push_back({Text(COLONY::Real_Building_Name(building), 256), satellite});
            }
            const char* resources[] = {"food", "industry", "research", "bc"};
            for (int16_t resource = 0; resource < ECON_COUNT; ++resource) {
                colony_read::s_resource_row row;
                colony_read::Resource_Row(colony, resource, &row);
                const auto Icons = [](int16_t tens, int16_t units) { return std::max(0, (int)tens) * 10 + std::max(0, (int)units); };
                const bool exported = resource != ECON_INDUSTRY && colony->imports[resource] < 0;
                result.resources.push_back({resources[resource], Icons(row.net_tens, row.net_units), Icons(row.import_tens, row.import_units), Icons(row.secondary_tens, row.secondary_units), exported || resource == ECON_INDUSTRY ? 0 : Icons(row.shortage_tens, row.shortage_units), exported});
            }
            result.morale_icons = std::clamp((int)colony_read::Morale_Icons(colony, &result.morale_suppressed), -20, 20);
            for (int slot = 0; slot < 7; ++slot) {
                const auto product = colony->producing[slot];
                colony_read::s_product presentation;
                colony_read::Product(colony, product, &presentation);
                result.queue.push_back({presentation.available, presentation.available ? presentation.kind : "unavailable", Text(presentation.label, sizeof(presentation.label))});
            }
            result.production_available = result.queue[0].available;
            if (result.production_available) {
                result.cost = COLCALC::Colony_Producing_Product_Cost(&MOX::colony[index]);
                result.stored = colony->production_spent;
                result.bought = colony->bought_outright;
                result.turns = COLONY::Calculate_Current_Production_Turn_Count(index);
                if (result.turns < 1 || result.turns > 9999) result.turns = 0;
                result.buy_cost = COLCALC::Colony_Cost_To_Buy_Producing_Product(&MOX::colony[index]);
                result.can_buy = colony_read::Can_Buy(index, viewer);
            }
            result.available = true;
            *out = std::move(result);
        }
    } // namespace

    bool Read_Settlement(int16_t index, rpc::s_map_observation* observation, bool contextual, bool local_ship) {
        if (observation == nullptr || index < 0 || index >= MOX::planet_count || MOX::planet == nullptr) return false;
        const auto* planet = &MOX::planet[index];
        const int16_t system = planet->star_index;
        if (system < 0 || (size_t)system >= observation->systems.size()) return false;
        if (!contextual && !colony_read::System_Settlements_Visible(system, observation->viewer)) return true;
        const int16_t slot = planet->colony_index;
        if (slot == -1) return true;
        if (slot < 0 || slot >= MOX::NUM_COLONIES || MOX::colony == nullptr) return false;
        const auto* colony = &MOX::colony[slot];
        if (colony->owner < 0) return true;
        if (colony->owner >= MOX::NUM_PLAYERS || planet->orbit < 0 || planet->orbit >= 5 ||
            MOX::star[system].planet_index[planet->orbit] != index || colony->planet != index) return false;
        auto found = std::find_if(observation->colonies.begin(), observation->colonies.end(), [system, planet](const auto& item) {
            return item.system == system && item.orbit == planet->orbit + 1;
        });
        if (found == observation->colonies.end()) {
            observation->colonies.emplace_back();
            found = observation->colonies.end() - 1;
        }
        auto& item = *found;
        item.owned = colony->owner == observation->viewer;
        item.unclassified = contextual && !colony_read::System_Settlements_Visible(system, observation->viewer);
        if (item.unclassified) {
            item.strategic = true;
            item.owner_slot = colony->owner;
            item.system = system;
            item.orbit = planet->orbit + 1;
            item.population_available = false;
            const char* race = MOX::player[colony->owner].race_name;
            if (memchr(race, 0, sizeof(MOX::player[colony->owner].race_name)) != nullptr) item.owner_label = Text(race, sizeof(MOX::player[colony->owner].race_name));
            return true;
        }
        item.outpost = colony->outpost_flag != 0;
        item.strategic = true;
        item.owner_slot = colony->owner;
        item.system = system;
        item.orbit = planet->orbit + 1;
        item.population_available = !item.outpost && planet->planet_type == PLANET_TYPE_PLANET && (item.owned || colony_read::Strategic_Population_Disclosed());
        if (item.population_available) {
            if (colony->n_pops > MAX_COLONY_POPULATION) return false;
            item.population = colony->n_pops;
        } else
            item.population = 0;
        const char* race = MOX::player[colony->owner].race_name;
        if (planet->planet_type == PLANET_TYPE_PLANET && memchr(race, 0, sizeof(MOX::player[colony->owner].race_name)) != nullptr) item.owner_label = Text(race, sizeof(MOX::player[colony->owner].race_name));
        char climate[80];
        item.climate_available = planet->planet_type == PLANET_TYPE_PLANET && colony_read::Strategic_Climate(colony, index, climate, sizeof(climate));
        if (item.climate_available) item.climate = Text(climate, sizeof(climate));
        item.defenses_available = planet->planet_type == PLANET_TYPE_PLANET && colony_read::Strategic_Defenses_Visible(system, observation->viewer, local_ship);
        if (!item.defenses_available && planet->planet_type == PLANET_TYPE_PLANET && MOX::ship != nullptr && MOX::NUM_SHIPS >= 0 && MOX::NUM_SHIPS <= MAX_SHIPS) {
            for (int16_t ship = 0; ship < MOX::NUM_SHIPS; ++ship) {
                if (MOX::ship[ship].owner != observation->viewer) continue;
                if (colony_read::Strategic_Local_Ship(&MOX::ship[ship], system, observation->viewer)) {
                    item.defenses_available = true;
                    break;
                }
            }
        }
        item.defenses.clear();
        if (item.defenses_available) {
            for (int16_t ordinal = 0; ordinal < 7; ++ordinal) {
                const int16_t building = colony_read::Strategic_Defense(colony, ordinal);
                if (building < 0) break;
                if (TECHDATA::buildings[building].name == nullptr) {
                    item.defenses_available = false;
                    item.defenses.clear();
                    break;
                }
                item.defenses.push_back(Text(TECHDATA::buildings[building].name, 4096));
            }
        }
        return true;
    }

    bool Read_Colonies(rpc::s_map_observation* observation) {
        if (observation == nullptr || observation->viewer != MOX::PLAYER_NUM || !observation->players_captured ||
            MOX::NUM_COLONIES < 0 || MOX::NUM_COLONIES > MAX_COLONIES ||
            (MOX::NUM_COLONIES != 0 && MOX::colony == nullptr)) return false;
        for (int16_t index = 0; index < MOX::NUM_COLONIES; ++index) {
            const s_colony* colony = &MOX::colony[index];
            // Native Colonies/transport lists include own full colonies, not outposts.
            if (!colony_read::Own_Full_Colony(colony, observation->viewer)) continue;
            const int16_t planet_index = colony->planet;
            if (planet_index < 0 || planet_index >= MOX::planet_count || MOX::planet == nullptr) return false;
            const s_planet_data* planet = &MOX::planet[planet_index];
            const int16_t system = planet->star_index;
            const int orbit = planet->orbit;
            if (system < 0 || (size_t)system >= observation->systems.size() || orbit < 0 || orbit >= 5 ||
                MOX::star[system].planet_index[orbit] != planet_index || planet->colony_index != index ||
                colony->n_pops > MAX_COLONY_POPULATION || colony->military[0] < 0 || colony->military[1] < 0) return false;
            rpc::s_colony_observation item;
            item.owner_slot = observation->viewer;
            item.system = system;
            item.orbit = orbit + 1;
            item.population = colony->n_pops;
            item.infantry = colony->military[0];
            item.armor = colony->military[1];
            Read_Operations(index, observation->viewer, &item.operations);
            observation->colonies.push_back(std::move(item));
        }
        for (int16_t system = 0; system < MOX::NUM_STARS; ++system) {
            if (!colony_read::System_Settlements_Visible(system, observation->viewer)) continue;
            for (int orbit = 0; orbit < 5; ++orbit) {
                const int16_t planet = MOX::star[system].planet_index[orbit];
                if (planet != -1 && !Read_Settlement(planet, observation)) return false;
            }
        }
        observation->settlements_complete = true;
        observation->colonies_captured = true;
        return true;
    }
} // namespace colony_rpc
