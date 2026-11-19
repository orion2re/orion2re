#include "pch.h"

namespace {
    bool Building_Forces_Combat_Reengagement(int16_t building_id) {
        if (building_id <= BUILDING_NO_BUILDING || building_id >= BUILDING_COUNT) {
            return false;
        }

        for (int index = 0; index < ERICNET::COMBAT_REENGAGE_BUILDING_CAPACITY; ++index) {
            const uint8_t configured_building = ERICNET::combat_reengage_buildings[index];
            if (configured_building == BUILDING_NO_BUILDING) {
                return false;
            }
            if (configured_building == building_id) {
                return true;
            }
        }

        return false;
    }
} // namespace

namespace ERICNET {
    int8_t start_civ_level[3] = {3, 5, 9};
    int16_t buildlist[INITIAL_BUILDING_CAPACITY] = {
        BUILDING_STAR_FORTRESS,
        BUILDING_BATTLESTATION,
        BUILDING_STAR_BASE,
        BUILDING_HYDROPONIC_FARM,
        BUILDING_MARINE_BARRACKS,
        BUILDING_BIOSPHERES,
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_HOLO_SIMULATOR,
        BUILDING_SOIL_ENRICHMENT,
        BUILDING_ASTRO_UNIVERSITY,
        BUILDING_PLEASURE_DOME,
        BUILDING_RECYCLOTRON,
        BUILDING_ROBOTIC_FACTORY,
        BUILDING_SPACEPORT,
        BUILDING_ARMOR_BARRACKS,
        BUILDING_DEEP_CORE_MINE,
        BUILDING_CORE_WASTE_DUMP,
        BUILDING_POLLUTION_PROCESSOR,
        BUILDING_RESEARCH_LAB,
        BUILDING_CLONING_CENTER,
        BUILDING_SUBTERRANEAN_FARMS,
        BUILDING_FOOD_REPLICATORS,
        BUILDING_RADIATION_SHIELD,
        BUILDING_GRAVITY_GENERATOR,
        BUILDING_CURRENCY_EXCHANGE,
        BUILDING_GALACTIC_CYBERNET,
        BUILDING_FLUX_SHIELD,
        BUILDING_MISSILE_BASE,
        BUILDING_FIGHTER_GARRISON,
        BUILDING_GROUND_BATTERIES,
        BUILDING_AUTOLAB,
        BUILDING_ATMOSPHERE_RENEWER,
        BUILDING_BARRIER_SHIELD,
        BUILDING_ROBO_MINER_PLANT,
        BUILDING_STOCK_EXCHANGE,
        BUILDING_SUPERCOMPUTER,
        BUILDING_WEATHER_CONTROLLER,
        BUILDING_DIMENSIONAL_PORTAL,
        BUILDING_WARP_INTERDICTOR,
        BUILDING_ARTEMIS_SYSTEM_NET,
        BUILDING_STELLAR_CONVERTER,
        BUILDING_SPACE_ACADEMY,
        BUILDING_NO_BUILDING,
        BUILDING_NO_BUILDING,
    };

    void __cdecl Init_Homeworld_Colony2(::game::s_world_state* world_state, ::game::id_planet planet_idx, int16_t player_idx, uint8_t is_homeworld) {
        const uint8_t civilization_level = Starting_Civilization_Level_For_Classic_Rules_(
            MOX::settings.starting_civilization_level);
        int16_t colony_idx = MOX::NUM_COLONIES;
        MOX::NUM_COLONIES++;

        memset(&MOX::colony[colony_idx], 0, sizeof(s_colony));

        if (COLONY::debug_flag) {
            RUSS::PFMT(0, 0, "INITING HOMEWORLD ph:%u pl:%d num:%d", planet_idx.id, (int)player_idx, (int)MOX::NUM_COLONIES);
            key::Wait_For_Key();
        }

        INITGAME::Init_Colony(world_state, colony_idx, legacy_world::Planet_Id(planet_idx), (uint8_t)player_idx);
        s_colony& colony = MOX::colony[colony_idx];
        COLONY::Set_Colony_Globals(world_state, colony_idx);

        const auto star_idx = game::Planet(world_state, planet_idx).Star_Position(MOX::star);
        MOX::star[star_idx].visited |= (1 << player_idx);

        colony.climate = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).climate;

        int16_t population_limit = COLCALC::Colony_Race_Pop_Limit(world_state, &colony, player_idx);

        if (civilization_level == STARTING_CIVILIZATION_LEVEL_ADVANCED) {
            int32_t growth_mod = MOX::player[player_idx].traits[TRAIT_POP_GROWTH_BONUS];
            colony.n_pops = (int8_t)(((growth_mod + 300) * (int32_t)population_limit * 6) / 3000);

            if (population_limit < 9) {
                colony.n_pops = (int8_t)(((int32_t)(uint8_t)colony.n_pops + (int32_t)population_limit) / 2);
            }

            if (colony.n_pops == 0) {
                colony.n_pops = 1;
            }

            if (is_homeworld != 0 && (uint8_t)colony.n_pops < 8) {
                colony.n_pops = 8;
            }
        } else {
            colony.n_pops = INITGAME::start.population;
        }

        for (int i = 0; i < (int)(uint8_t)colony.n_pops; ++i) {
            uint32_t job_bits;
            if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).food_per_farmer == 0 || MOX::player[player_idx].traits[TRAIT_EATS_MINERALS] != 0) {
                job_bits = ((i % 2) + 1) << 7;
            } else {
                job_bits = (i % 3) << 7;
            }

            uint32_t pop_val = colony.pop[i];
            pop_val = (pop_val & ~0x180) | job_bits;
            pop_val = (pop_val & ~0x00F) | (player_idx & 0x00F);
            pop_val = (pop_val & ~0x070) | ((uint32_t)(player_idx << 4) & 0x070);
            pop_val &= ~0x400;
            pop_val |= 0x200;
            colony.pop[i] = pop_val;
        }

        memset(colony.buildings, 0, sizeof(colony.buildings));

        if (MOX::player[colony.owner].traits[TRAIT_CURRENT_GOVERNMENT] < GOVERNMENT_UNIFICATION && is_homeworld != 0) {
            Add_Building(world_state, colony_idx, BUILDING_CAPITOL);
        }

        int16_t added_buildings_count = 0;
        for (int j = 0; j < INITIAL_BUILDING_CAPACITY; ++j) {
            int16_t building_id = ERICNET::buildlist[j];
            if (building_id == BUILDING_NO_BUILDING) {
                break;
            }
            if (added_buildings_count >= ERICNET::start_civ_level[civilization_level]) {
                break;
            }

            if ((uint8_t)colony.n_pops <= (added_buildings_count * 3) / 2) {
                break;
            }

            if (COLCALC::Colony_Can_Build_Product(world_state, &colony, building_id)) {
                bool can_add = true;
                switch (building_id) {
                    case BUILDING_ATMOSPHERE_RENEWER:
                    case BUILDING_CORE_WASTE_DUMP:
                    case BUILDING_POLLUTION_PROCESSOR:
                        can_add = (MOX::player[player_idx].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0);
                        break;
                    case BUILDING_CLONING_CENTER:
                    case BUILDING_GRAVITY_GENERATOR:
                        can_add = ((int32_t)(uint8_t)colony.n_pops < (int32_t)population_limit);
                        break;
                    case BUILDING_HOLO_SIMULATOR:
                    case BUILDING_PLEASURE_DOME:
                        can_add = (MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] / 2 != GOVERNMENT_UNIFICATION / 2);
                        break;
                    case BUILDING_HYDROPONIC_FARM:
                    case BUILDING_SOIL_ENRICHMENT:
                    case BUILDING_SUBTERRANEAN_FARMS:
                        can_add = (MOX::player[player_idx].traits[TRAIT_EATS_MINERALS] == 0);
                        break;
                    default:
                        can_add = true;
                        break;
                }

                if (can_add) {
                    Add_Building(world_state, colony_idx, building_id);
                    added_buildings_count++;
                }
            }
        }

        if (civilization_level == STARTING_CIVILIZATION_LEVEL_AVERAGE &&
            is_homeworld != 0 && starting_ships != 0) {
            COLBLDG::Create_Ship(player_idx, 0, colony_idx);

            COLBLDG::Create_Ship(player_idx, 0, colony_idx);

            if (starting_outpost_ship != 0) {
                s_ship_design outpost_ship_design = {};
                INITSHIP::Load_Outpost_Ship_Design(player_idx, &outpost_ship_design);
                COLBLDG::Create_Ship_From_Design(world_state, player_idx, &outpost_ship_design, colony_idx);
            }

            s_ship_design colony_ship_design = {};
            INITSHIP::Load_Colony_Ship_Design(player_idx, &colony_ship_design);
            COLBLDG::Create_Ship_From_Design(world_state, player_idx, &colony_ship_design, colony_idx);
        }

        colony.food2_per_farmer = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony.planet))).food_per_farmer;
        colony.max_farms = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony.planet))).max_farms;
        colony.climate = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony.planet))).climate;
        colony.military[0] = INITGAME::start.marines; // Ground Defense Units

        colony.production[0] = 0;
        colony.production[1] = 0;
        colony.production[2] = 0;
        colony.production[3] = 0;

        colony.maintenance[0] = 0;
        colony.maintenance[1] = 0;
        colony.maintenance[2] = 0;
        colony.maintenance[3] = 0;
    }

    void __cdecl Init_Homeworld_Colony(::game::id_planet planet_idx, int16_t player_idx) {
        Init_Homeworld_Colony2(&::game::world, legacy_world::Planet_Id(planet_idx), player_idx, 1);
    }

    void __cdecl Init_Non_Homeworld_Colony(::game::id_planet planet_idx, int16_t player_idx) {
        Init_Homeworld_Colony2(&::game::world, legacy_world::Planet_Id(planet_idx), player_idx, 0);
    }

    int __cdecl Mox_Play_Sound(int16_t sound_id) {
        if (MOX::settings.sound_fx_on != 0 && MOX::super_fast_flag == 0) {
            return sound::Play_Sound(sound_id);
        }

        return 0;
    }

    void __cdecl Do_All_Ships_XP_Check() {
        int16_t instructor_bonuses[MAX_PLAYERS];
        memset(instructor_bonuses, 0, sizeof(instructor_bonuses));

        Best_Instructor_Bonus(&game::world, instructor_bonuses);

        for (int16_t ship_idx = 0; ship_idx < MAX_SHIPS; ++ship_idx) {
            s_ship_data& ship = MOX::ship[ship_idx];

            if (ship.status < 5 && (uint8_t)ship.owner < MAX_PLAYERS) {
                int16_t owner_id = HACCESS::Ship_Owner(ship_idx);
                int16_t bonus = instructor_bonuses[owner_id];
                Do_XP_For_Ship(ship_idx, bonus);
            }
        }
    }

    void __cdecl Calc_Ship_Level(int16_t ship_idx) {
        s_ship_data* ship = &MOX::ship[ship_idx];
        int16_t level = 0;

        for (int i = 0; i < CREW_QUALITY_ULTRA_ELITE; ++i) {
            if (ship->crew_experience >= MOX::crew_data[level + 1].experience) {
                level++;
            }
        }

        s_player* owner = &MOX::player[(int16_t)ship->owner];
        if (owner->traits[TRAIT_WARLORD] != 0) {
            level++;
        }

        if (level > 5) {
            level = 5;
        }

        ship->crew_quality = (int8_t)level;
    }

    void __cdecl Do_XP_For_Ship(int16_t ship_index, int16_t xp_gain) {
        Do_Academy_At_Location_Check(&::game::world, ship_index);

        int32_t total_xp = MOX::ship[ship_index].crew_experience +
            xp_gain + crew_gains_turn_xp;
        const int32_t max_xp = MOX::crew_data[CREW_QUALITY_ELITE].experience;

        if (total_xp > max_xp) {
            total_xp = max_xp;
        }

        MOX::ship[ship_index].crew_experience = total_xp;
        Calc_Ship_Level(ship_index);
    }

    void __cdecl Do_Academy_At_Location_Check(::game::s_world_state* world_state, int16_t ship_index) {
        s_ship_data& ship = MOX::ship[ship_index];

        // Only active ships gain experience
        if (ship.status == 0) {
            int16_t star_idx = ship.Legacy_Location(MOX::star);
            int8_t ship_owner = ship.owner;

            for (int i = 0; i < 5; ++i) {
                ::game::id_planet planet_idx = MOX::star[star_idx].planet_index[i];
                if (planet_idx != ::game::NO_PLANET) {
                    int16_t colony_idx = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(planet_idx))).colony_index;
                    if (colony_idx > -1) {
                        s_colony& colony = MOX::colony[colony_idx];
                        // If colony is owned by the same player and has a Space Academy
                        if (colony.owner == ship_owner && colony.buildings[BUILDING_SPACE_ACADEMY] != 0) {
                            ++ship.crew_experience;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Add_Building(::game::s_world_state* world_state, int16_t colony_idx, int16_t building_idx) {
        s_colony* colony = &MOX::colony[colony_idx];
        s_player* player = &MOX::player[(uint8_t)colony->owner];
        s_planet_data* planet = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(colony->planet)));
        const auto star_idx = planet->Star_Position(MOX::star);
        s_star_data* star = &MOX::star[star_idx];

        uint8_t add_to_list = 1;
        uint8_t recompute_star = 0;

        switch (building_idx) {
            case BUILDING_ARMOR_BARRACKS:
                colony->tank_roundoff = 0;
                {
                    int16_t limit = invasion::Colony_Tank_Barracks_Limit(colony);
                    if (limit > 2) limit = 2;
                    if (colony->military[1] < limit) {
                        colony->military[1] = limit;
                    }
                }
                break;

            case BUILDING_ARTEMIS_SYSTEM_NET:
            case BUILDING_DIMENSIONAL_PORTAL:
            case BUILDING_WARP_INTERDICTOR:
                recompute_star = 1;
                break;

            case BUILDING_CAPITOL:
                player->home_planet_id = colony->planet;
                for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
                    if (i != colony_idx && MOX::colony[i].owner == colony->owner) {
                        COLBLDG::Delete_Prod_From_Colony_Queue(i, BUILDING_CAPITOL);
                    }
                }
                break;

            case BUILDING_COLONY_BASE:
                star->ignore_colony_ships &= ~(1 << (colony->owner & 0x1F));
                break;

            case BUILDING_CORE_WASTE_DUMP:
                if (colony->buildings[BUILDING_POLLUTION_PROCESSOR]) {
                    colony->buildings[BUILDING_POLLUTION_PROCESSOR] = 0;
                    player->bc += TECHDATA::buildings[BUILDING_POLLUTION_PROCESSOR].cost;
                }
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_POLLUTION_PROCESSOR);
                if (colony->buildings[BUILDING_ATMOSPHERE_RENEWER]) {
                    colony->buildings[BUILDING_ATMOSPHERE_RENEWER] = 0;
                    player->bc += TECHDATA::buildings[BUILDING_ATMOSPHERE_RENEWER].cost;
                }
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_ATMOSPHERE_RENEWER);
                break;

            case BUILDING_GAIA_TRANSFORMATION:
                if (colony->climate == PLANET_CLIMATE_TERRAN) {
                    planet->climate = PLANET_CLIMATE_GAIA;
                    planet->food_per_farmer += (MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] - MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN]);
                }
                add_to_list = 0;
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_TERRAFORMING);
                break;

            case BUILDING_MARINE_BARRACKS:
                colony->infantry_roundoff = 0;
                {
                    int16_t limit = invasion::Colony_Infantry_Barracks_Limit(colony);
                    if (limit > 4) limit = 4;
                    if (colony->military[0] < limit) {
                        colony->military[0] = limit;
                    }
                }
                break;

            case BUILDING_BARRIER_SHIELD:
                colony->buildings[BUILDING_FLUX_SHIELD] = 0;
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_FLUX_SHIELD);
                /* fallthrough */
            case BUILDING_FLUX_SHIELD:
                colony->buildings[BUILDING_RADIATION_SHIELD] = 0;
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_RADIATION_SHIELD);
                /* fallthrough */
            case BUILDING_RADIATION_SHIELD:
                break;

            case BUILDING_SOIL_ENRICHMENT:
                planet->food_per_farmer += COLCALC::food_farmer_bonus.soil_enrichment;
                add_to_list = 0;
                planet->environmental_alterations |= 0x2;
                break;

            case BUILDING_STAR_FORTRESS:
                colony->buildings[BUILDING_BATTLESTATION] = 0;
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_BATTLESTATION);
                /* fallthrough */
            case BUILDING_BATTLESTATION:
                colony->buildings[BUILDING_STAR_BASE] = 0;
                COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_STAR_BASE);
                /* fallthrough */
            case BUILDING_STAR_BASE:
                recompute_star = 1;
                break;

            case BUILDING_TERRAFORMING: {
                ::game::id_planet p_idx = colony->planet;
                int8_t old_climate = colony->climate;
                switch (old_climate) {
                    case PLANET_CLIMATE_BARREN:
                        if (star->planet_index[0] == p_idx || star->planet_index[1] == p_idx) {
                            ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate = PLANET_CLIMATE_DESERT;
                        } else if (star->planet_index[3] == p_idx || star->planet_index[4] == p_idx) {
                            ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate = PLANET_CLIMATE_TUNDRA;
                        } else {
                            ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate = (game_random::Random(2) != 1) ? PLANET_CLIMATE_TUNDRA : PLANET_CLIMATE_DESERT;
                        }
                        break;
                    case PLANET_CLIMATE_DESERT:
                        ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate = PLANET_CLIMATE_ARID;
                        break;
                    case PLANET_CLIMATE_TUNDRA:
                        ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate = PLANET_CLIMATE_SWAMP;
                        break;
                    case PLANET_CLIMATE_OCEAN:
                    case PLANET_CLIMATE_SWAMP:
                    case PLANET_CLIMATE_ARID:
                        ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate = PLANET_CLIMATE_TERRAN;
                        break;
                }
                add_to_list = 0;
                ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).food_per_farmer += (MOX::food_per_farmer_table[::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).climate] - MOX::food_per_farmer_table[old_climate]);
                ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).n_terraforms += 1;
            } break;

            case BUILDING_ARTIFICIAL_PLANET: {
                ::game::id_planet target_planet = ::game::NO_PLANET;
                int8_t size = 0;

                // Look for a gas giant first.
                for (int i = 0; i < 5; ++i) {
                    ::game::id_planet p_idx = star->planet_index[i];
                    if (p_idx != ::game::NO_PLANET && ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).planet_type == PLANET_TYPE_GAS_GIANT) {
                        target_planet = p_idx;
                        size = PLANET_SIZE_HUGE;
                        break;
                    }
                }
                // Otherwise use an asteroid belt.
                if (target_planet == ::game::NO_PLANET) {
                    for (int i = 0; i < 5; ++i) {
                        ::game::id_planet p_idx = star->planet_index[i];
                        if (p_idx != ::game::NO_PLANET && ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(p_idx))).planet_type == PLANET_TYPE_ASTEROID) {
                            target_planet = p_idx;
                            size = PLANET_SIZE_LARGE;
                            break;
                        }
                    }
                }

                if (target_planet != ::game::NO_PLANET) {
                    s_planet_data* tp = &::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(target_planet)));
                    tp->planet_type = PLANET_TYPE_PLANET;
                    tp->size = size;
                    tp->gravity_class = PLANET_GRAVITY_NORMAL_G;
                    tp->group = PLANET_GROUP_BOMBARDED;
                    tp->climate = PLANET_CLIMATE_BARREN;
                    tp->climate_bg_type = (int8_t)game_random::Random(3) - 1;
                    tp->mineral_class = MINERAL_RESOURCE_ABUNDANT;
                    tp->food_per_farmer = MOX::food_per_farmer_table[PLANET_CLIMATE_BARREN];
                    tp->n_terraforms = 0;
                    tp->max_farms = MOX::planet_max_farms[size];
                    tp->max_population = COLCALC::population_capacity.size[size];
                    tp->environmental_alterations = 0;
                }
                add_to_list = 0;
            } break;
        }

        if (add_to_list) {
            colony->buildings[building_idx] = 1;
        }

        if (recompute_star) {
            COLCALC::Compute_Star_Colony_Stuff(world_state, star);
            if (building_idx == BUILDING_WARP_INTERDICTOR) {
                SHIPMOVE::Update_Ship_ETAs((int32_t)colony->owner);
            }
        }

        if (Building_Forces_Combat_Reengagement(building_idx)) {
            star->ignore_combat_ships = 0;
        }

        COLCALC::Colony_Environmental_Stuff(world_state, colony);
    }

    void __cdecl Best_Instructor_Bonus(game::s_world_state* world_state, int16_t* instructor_bonus_by_player) {
        for (const auto& leader_entry : world_state->leaders) {
            const int32_t leader_index = static_cast<int32_t>(leader_entry.first.id);
            const s_leader_data& leader = game::Leader(world_state, leader_index);
            const int16_t player_index = (int8_t)leader.player_index;

            if ((leader.type == LEADER_TYPE_COLONY) &&
                ((int8_t)leader.status >= 0) &&
                ((int8_t)leader.eta == 0) &&
                (player_index > -1) &&
                (player_index < MAX_PLAYERS)) {
                int16_t bonus = 0;

                if (OFFICER::Officer_Has_Special_Skill(world_state, player_index, leader_index, 0x16)) {
                    bonus = (int16_t)OFFICER::Officer_Skill_Bonus(world_state, (uint32_t)player_index, leader_index, 0x16);
                } else if (OFFICER::Officer_Has_Special_Skill(world_state, player_index, leader_index, 0x17)) {
                    bonus = (int16_t)OFFICER::Officer_Skill_Bonus(world_state, (uint32_t)player_index, leader_index, 0x17);
                }

                if (instructor_bonus_by_player[player_index] < bonus) {
                    instructor_bonus_by_player[player_index] = bonus;
                }
            }
        }
    }

    void __cdecl Remove_Building(::game::s_world_state* world_state, int16_t colony_idx, int16_t building_id) {
        MOX::colony[colony_idx].buildings[building_id] = 0;

        if (Building_Forces_Combat_Reengagement(building_id)) {
            ::game::id_planet planet_id = MOX::colony[colony_idx].planet;
            const auto star_id = game::Planet(world_state, planet_id).Star_Position(MOX::star);
            MOX::star[star_id].ignore_combat_ships = 0;
        }

        switch (building_id) {
            case BUILDING_BATTLESTATION:
            case BUILDING_STAR_BASE:
            case BUILDING_STAR_FORTRESS:
                for (int16_t i = 0; i < 7; ++i) {
                    int16_t prod_val = MOX::colony[colony_idx].producing[i];
                    if (COLBLDG::Colony_Production_Is_Queued_Ship(prod_val)) {
                        int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(prod_val);
                        if (MOX::ship[ship_idx].d.size >= SHIP_SIZE_LARGE) {
                            COLBLDG::Delete_From_Colony_Queue(colony_idx, i);
                            i--;
                        }
                    }
                }
                // Fallthrough
            case BUILDING_ARTEMIS_SYSTEM_NET:
            case BUILDING_DIMENSIONAL_PORTAL:
            case BUILDING_WARP_INTERDICTOR: {
                const auto star_idx = game::Planet(world_state, MOX::colony[colony_idx].planet).Star_Position(MOX::star);
                COLCALC::Compute_Star_Colony_Stuff(world_state, &MOX::star[star_idx]);
                if (building_id == BUILDING_WARP_INTERDICTOR) {
                    SHIPMOVE::Update_Ship_ETAs((int32_t)MOX::colony[colony_idx].owner);
                    return;
                }
                break;
            }

            case BUILDING_CAPITOL:
                invasion::Assign_New_Home_Planet((int8_t)MOX::colony[colony_idx].owner, colony_idx);
                return;

            case BUILDING_MARINE_BARRACKS:
                for (int16_t i = 0; i < 7; ++i) {
                    int16_t prod_val = MOX::colony[colony_idx].producing[i];
                    if (COLBLDG::Colony_Production_Is_Queued_Ship(prod_val)) {
                        int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(prod_val);
                        if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_TRANSPORT) {
                            COLBLDG::Delete_From_Colony_Queue(colony_idx, i);
                            i--;
                        }
                    }
                }
                return;

            case BUILDING_BARRIER_SHIELD:
            case BUILDING_FLUX_SHIELD:
            case BUILDING_RADIATION_SHIELD:
                if (::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(MOX::colony[colony_idx].planet))).climate < PLANET_CLIMATE_BARREN) {
                    COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_TERRAFORMING);
                }
                // Fallthrough
            case BUILDING_BIOSPHERES:
                COLCALC::Colony_Environmental_Stuff(world_state, &MOX::colony[colony_idx]);
                invasion::Enforce_Population_Limits_At_Colony(&MOX::colony[colony_idx]);
                COLCALC::Colony_Calculation(colony_idx);
                break;

            default:
                break;
        }
    }
} // namespace ERICNET