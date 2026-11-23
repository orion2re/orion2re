#include "pch.h"
#include "game/fleet_movement.h"
#include "game/legacy_ship_location_codec.h"

namespace antarans {
    s_antaran_fleet_config fleet_config = {
        .growth_delay = {200, 100, 0},
        .resource_growth = {
            .frequency = 25,
            .offensive_multiplier = {100, 150, 200},
            .defensive_multiplier = {100, 150, 200},
        },
        .attack = {
            .roll_range = 200,
            .minimum_offensive_force_shift = 2,
            .impossible_human_weight_multiplier = 3,
            .lucky_victim_weight_divisor = 3,
        },
        .ship_limit = {
            .offensive = {4, 4, 3, 2, 2},
            .defensive = {0, 0, 3, 2, 7},
        },
        .ship_cost = {2, 5, 12, 30, 75},
        .retirement = {
            {100, {100, 150, 200}},
            {199, {100, 150, 200}},
        },
    };

    namespace {
        int Difficulty_Band() {
            if (MOX::settings.game_difficulty == GAME_DIFFICULTY_HARD) {
                return ANTARAN_DIFFICULTY_HARD;
            }
            if (MOX::settings.game_difficulty == GAME_DIFFICULTY_IMPOSSIBLE) {
                return ANTARAN_DIFFICULTY_IMPOSSIBLE;
            }
            return ANTARAN_DIFFICULTY_NORMAL;
        }

        uint8_t Effective_Ship_Cost(int ship_class) {
            if (ship_class < 0 || ship_class >= ANTARAN_SHIP_CLASS_COUNT) {
                return 0;
            }
            uint16_t cost = fleet_config.ship_cost[ship_class];
            if (MOX::antarans.offensive_titan_built != 0) {
                cost = (uint16_t)((cost * 90) / 100);
            }
            return (uint8_t)cost;
        }

        uint8_t Effective_Ship_Limit(const uint8_t* limits, int ship_class, bool is_offensive) {
            if (ship_class < 0 || ship_class >= ANTARAN_SHIP_CLASS_COUNT) {
                return 0;
            }
            if (is_offensive &&
                (MOX::antarans.offensive_limit_exhausted_mask &
                    (uint8_t)(1u << ship_class)) != 0) {
                return 0;
            }
            return limits[ship_class];
        }
    } // namespace

    void Reset_Fleet_Runtime() {
        MOX::antarans.offensive_titan_built = 0;
        MOX::antarans.offensive_limit_exhausted_mask = 0;
    }

    int32_t __cdecl Delta_Stardate() {
        return MOX::stardate - 35000;
    }

    uint16_t __cdecl Antaran_Delay() {
        uint8_t level = Starting_Civilization_Level_For_Classic_Rules_(
            MOX::settings.starting_civilization_level);

        switch (level) {
            case STARTING_CIVILIZATION_LEVEL_PRE_WARP:
                return fleet_config.growth_delay[ANTARAN_CIVILIZATION_PREWARP];
            case STARTING_CIVILIZATION_LEVEL_AVERAGE:
                return fleet_config.growth_delay[ANTARAN_CIVILIZATION_AVERAGE];
            case STARTING_CIVILIZATION_LEVEL_ADVANCED:
            default:
                return fleet_config.growth_delay[ANTARAN_CIVILIZATION_ADVANCED];
        }
    }

    int16_t __cdecl Antarans_Defensive_Resource_Bonus() {
        return (int16_t)fleet_config.resource_growth
            .defensive_multiplier[Difficulty_Band()];
    }

    int16_t __cdecl Antarans_Offensive_Resource_Bonus() {
        return (int16_t)fleet_config.resource_growth
            .offensive_multiplier[Difficulty_Band()];
    }

    int16_t __cdecl No_Small_Ships_After_N_Turns() {
        const s_antaran_retirement_policy& policy =
            fleet_config.retirement[ANTARAN_RETIREMENT_RAIDER];

        if (MOX::settings.game_difficulty < GAME_DIFFICULTY_HARD) {
            return (int16_t)policy.normal_turn_limit;
        }

        return (int16_t)(12500 / (int32_t)policy.divisor[Difficulty_Band()]);
    }

    int16_t __cdecl No_Medium_Ships_After_N_Turns() {
        const s_antaran_retirement_policy& policy =
            fleet_config.retirement[ANTARAN_RETIREMENT_MARAUDER];

        if (MOX::settings.game_difficulty < GAME_DIFFICULTY_HARD) {
            return (int16_t)policy.normal_turn_limit;
        }

        return (int16_t)(20000 / (int32_t)policy.divisor[Difficulty_Band()]);
    }

    int16_t __cdecl Next_Antaran_Ship_Size() {
        int16_t result = -1;

        for (int16_t i = HULL_CLASS_COUNT - 1; i >= 0; --i) {
            if (result != -1) {
                break;
            }

            int32_t available = (int32_t)MOX::antarans.offensive_ships[i] -
                (int32_t)MOX::antarans.offensive_deployed[i];

            if (available > 0) {
                result = i;
            }
        }

        return result;
    }

    bool __cdecl Antaran_Defensive_Force_At_Maximum() {
        for (int i = 0; i < ANTARAN_SHIP_CLASS_COUNT; ++i) {
            uint8_t max_val = fleet_config.ship_limit.defensive[i];

            // If a ship type has a maximum threshold specified, check if current count meets it
            if (max_val != 0) {
                if (MOX::antarans.defensive_ships[i] < (int16_t)max_val) {
                    return false;
                }
            }
        }
        return true;
    }

    void __cdecl Increment_Antaran_Resource_Level() {
        int32_t delta_stardate = Delta_Stardate();
        uint16_t antaran_delay = Antaran_Delay();
        int32_t diff_stardate = delta_stardate - (int32_t)antaran_delay;
        uint16_t frequency = fleet_config.resource_growth.frequency;

        if (diff_stardate > 0 && (diff_stardate % frequency == 0)) {
            int32_t intervals = diff_stardate / frequency;
            int16_t defensive_bonus = Antarans_Defensive_Resource_Bonus();
            int16_t offensive_bonus = Antarans_Offensive_Resource_Bonus();

            int16_t defensive_increment =
                (int16_t)((defensive_bonus * intervals + 99) / 100);
            int16_t offensive_increment =
                (int16_t)((offensive_bonus * intervals + 99) / 100);

            MOX::antarans.resource_offensive += offensive_increment;

            if (Antaran_Defensive_Force_At_Maximum()) {
                MOX::antarans.resource_offensive += defensive_increment;
            } else {
                MOX::antarans.resource_defensive += defensive_increment;
            }
        }
    }

    void __cdecl Build_Antaran_Ships(int16_t* resources, int16_t* ships, const uint8_t* max_ships, int8_t is_offensive) {
        int32_t stardate_delta = Delta_Stardate();
        uint16_t antaran_delay = Antaran_Delay();
        int32_t time_ref = stardate_delta - (int32_t)antaran_delay;

        bool debug_headers_printed = false;

        while (true) {
            // Check if we can afford the smallest possible ship
            if (*resources < (short)Effective_Ship_Cost(ANTARAN_SHIP_RAIDER)) {
                return;
            }

            int32_t chosen_size = -1;
            uint16_t starting_size =
                (MOX::settings.starting_civilization_level ==
                    STARTING_CIVILIZATION_LEVEL_ADVANCED);

            for (uint16_t size = starting_size; size < ANTARAN_SHIP_CLASS_COUNT; ++size) {
                bool is_allowed_by_time = false;
                if (size == SHIP_SIZE_SMALL) {
                    if ((int32_t)No_Small_Ships_After_N_Turns() >= time_ref) {
                        is_allowed_by_time = true;
                    }
                } else if (size == SHIP_SIZE_MEDIUM) {
                    if ((int32_t)No_Medium_Ships_After_N_Turns() >= time_ref) {
                        is_allowed_by_time = true;
                    }
                } else {
                    is_allowed_by_time = true;
                }

                if (is_allowed_by_time) {
                    uint8_t cost = Effective_Ship_Cost(size);
                    uint8_t limit = Effective_Ship_Limit(
                        max_ships, size, is_offensive != 0);
                    if (cost != 0 && *resources >= (short)cost && limit > ships[size]) {
                        if (!debug_headers_printed) {
                            debug_headers_printed = true;
                            const char* side = is_offensive ? "Offensive" : "Defensive";
                            snprintf(MOX::temp_string, sizeof(MOX::temp_string), "\n(%4d.%d) Building Antaran %s ships:", (int)MOX::stardate / 10, (int)MOX::stardate % 10, side);

                            if (!is_offensive) {
                                snprintf(MOX::temp_string, sizeof(MOX::temp_string), ">> current def ships (by size) : ");
                                for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
                                    char buf[20];
                                    snprintf(buf, sizeof(buf), "%2d ", MOX::antarans.defensive_ships[i]);
                                    strlcat(MOX::temp_string, buf, sizeof(MOX::temp_string));
                                }
                            } else {
                                snprintf(MOX::temp_string, sizeof(MOX::temp_string), ">> current off ships (by size) : ");
                                for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
                                    char buf[20];
                                    snprintf(buf, sizeof(buf), "%2d ", MOX::antarans.offensive_ships[i]);
                                    strlcat(MOX::temp_string, buf, sizeof(MOX::temp_string));
                                }
                                snprintf(MOX::temp_string, sizeof(MOX::temp_string), ">> off ships deployed (by size): ");
                                for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
                                    char buf[20];
                                    snprintf(buf, sizeof(buf), "%2d ", MOX::antarans.offensive_deployed[i]);
                                    strlcat(MOX::temp_string, buf, sizeof(MOX::temp_string));
                                }
                            }
                        }

                        snprintf(MOX::temp_string, sizeof(MOX::temp_string), "-------- building size %d, resources = %d, n_ships[%d] = %d", (int)size, (int)*resources, (int)size, (int)ships[size]);

                        snprintf(MOX::temp_string, sizeof(MOX::temp_string), "-------- n_max_ships[%d] = %d, cost = %d", (int)size, (int)limit, (int)cost);

                        chosen_size = (int32_t)size;
                        break;
                    }
                }
            }

            if (chosen_size == -1) {
                return;
            }

            // Perform construction
            short current_count = ships[chosen_size];
            ships[chosen_size] = current_count + 1;

            // The offensive cap is permanently exhausted when either fleet reaches it.
            if (chosen_size <= SHIP_SIZE_LARGE &&
                fleet_config.ship_limit.offensive[chosen_size] ==
                    current_count + 1) {
                MOX::antarans.offensive_limit_exhausted_mask |=
                    (uint8_t)(1u << chosen_size);
            }

            uint8_t cost = Effective_Ship_Cost(chosen_size);
            *resources = (short)(*resources - cost);

            snprintf(MOX::temp_string, sizeof(MOX::temp_string), "-------- after build: resources = %3d, n_ships[%d] = %2d", (int)*resources, (int)chosen_size, (int)ships[chosen_size]);

            // Special Titan logic (size 4) on Hard/Impossible difficulty
            if (chosen_size == SHIP_SIZE_TITAN &&
                MOX::antarans.offensive_titan_built == 0 &&
                MOX::settings.game_difficulty > GAME_DIFFICULTY_AVERAGE) {
                MOX::antarans.offensive_titan_built = 1;
            }
        }
    }

    void __cdecl Build_Antaran_Defensive_Ships() {
        strlcpy(MOX::temp_string, "defensive", sizeof(MOX::temp_string));

        Build_Antaran_Ships(
            &MOX::antarans.resource_defensive,
            MOX::antarans.defensive_ships,
            fleet_config.ship_limit.defensive,
            0);
    }

    void __cdecl Build_Antaran_Offensive_Ships() {
        strlcpy(MOX::temp_string, "offensive", sizeof(MOX::temp_string));

        Build_Antaran_Ships(
            &MOX::antarans.resource_offensive,
            MOX::antarans.offensive_ships,
            fleet_config.ship_limit.offensive,
            1);
    }

    void __cdecl Build_List_Of_Stars_By_Player(int16_t player_idx, int16_t* star_list, int16_t* star_count) {
        *star_count = 0;

        for (int16_t star_idx = 0; star_idx < std::ssize(game::current_state.stars); ++star_idx) {
            if (HACCESS::N_Colonies_At_Star(star_idx, player_idx)) {
                star_list[*star_count] = star_idx;
                *star_count += 1;
            }
        }

        int16_t shuffle_limit = *star_count / 2;
        for (int16_t i = 0; i < shuffle_limit; ++i) {
            int16_t idx1 = (int16_t)game_random::Random(*star_count) - 1;
            int16_t idx2 = (int16_t)game_random::Random(*star_count) - 1;

            if (idx1 != idx2) {
                int16_t swapped_star = star_list[idx1];
                star_list[idx1] = star_list[idx2];
                star_list[idx2] = swapped_star;
            }
        }
    }

    int16_t __cdecl Get_Antaran_Target_Star(int16_t player_idx) {
        int16_t star_count;
        int16_t star_indices[MAX_STARS];
        int16_t target_star_idx = -1;

        Build_List_Of_Stars_By_Player(player_idx, star_indices, &star_count);

        if (star_count > 0) {
            for (int16_t i = 0; i < star_count; ++i) {
                // Reservoir sampling: pick the i-th star with 1/(i+1) probability
                if (game_random::Random((int32_t)i + 1) == 1) {
                    target_star_idx = star_indices[i];
                }
            }
        }

        return target_star_idx;
    }

    void __cdecl Antaran_Invasion_Check() {
        Increment_Antaran_Resource_Level();
        Build_Antaran_Offensive_Ships();
        Build_Antaran_Defensive_Ships();

        if (Antarans_Should_Attack()) {
            MOX::antarans.attack_pressure = 0;

            if (Deploy_Antaran_Ships()) {
                MOX::antarans.invasion_stardate = (int)MOX::stardate;
            }
        }
    }

    int16_t __cdecl Antaran_Defensive_Force() {
        int16_t total_force = 0;
        for (int32_t i = 0; i < ANTARAN_SHIP_CLASS_COUNT; ++i) {
            total_force += (int16_t)(MOX::antarans.defensive_ships[i] *
                (uint16_t)Effective_Ship_Cost(i));
        }
        return total_force;
    }

    int16_t __cdecl Antaran_Total_Resources() {
        int16_t total_force;

        total_force = Antaran_Defensive_Force();
        total_force += Antaran_Offensive_Force();
        total_force += MOX::antarans.resource_offensive;
        total_force += MOX::antarans.resource_defensive;

        return total_force;
    }

    bool __cdecl Deploy_Antaran_Ships() {
        int16_t target_star = Get_Antaran_Target_Star(EVENTS::Get_Antaran_Victim());

        snprintf(MOX::temp_string, sizeof(MOX::temp_string), "-< ships (by size)          : ");
        for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%2d ", (int)MOX::antarans.offensive_ships[i]);
            strlcat(MOX::temp_string, buf, sizeof(MOX::temp_string));
        }

        snprintf(MOX::temp_string, sizeof(MOX::temp_string), "-< ships deployed (by size) : ");
        for (int i = 0; i < HULL_CLASS_COUNT; ++i) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%2d ", (int)MOX::antarans.offensive_deployed[i]);
            strlcat(MOX::temp_string, buf, sizeof(MOX::temp_string));
        }

        auto ship_key = game::NO_SHIP;
        const auto destination = game::Destination_At_Row(game::current_state, target_star);
        int16_t x = 0, y = 0;
        int ship_count = 0;
        bool finished = false;

        while (!finished) {
            int16_t size_idx = Next_Antaran_Ship_Size();

            if (size_idx < 0) {
                finished = true;
                continue;
            }

            s_ship_design design;
            INITSHIP::Load_Antaran_Ship_Design(&design, size_idx);

            if (ship_count == 0) {
                ship_key = SHIPS::Set_Nonplayer_Ship_XY(target_star, NONPLAYER_SHIP_TYPE_ANTARAN, &design, 1);
                const auto* ship = game::Find_Ship(game::current_state, ship_key);
                if (!ship) {
                    return false;
                }
                x = ship->x;
                y = ship->y;
            } else {
                ship_key = SHIPMOVE::Create_Nonplayer_Ship(game::id_faction{NONPLAYER_SHIP_TYPE_ANTARAN}, &design, x, y);
            }

            if (ship_key == game::NO_SHIP) {
                finished = true;
            } else {
                const game::id_ship ship_list[] = {ship_key};
                MOX::antarans.offensive_deployed[size_idx]++;

                s_ship_move_info move_info{};
                SHIPMOVE::Evaluate_Fleet_Move(ship_list, destination, &move_info);

                if (move_info.moving != 0 && SHIPMOVE::Order_Fleet_Move(ship_list, destination, &move_info)) {
                    auto* ship = game::Find_Ship(game::current_state, ship_key);
                    ship->status = 1;
                    ship->position.Set(legacy_world::Location_At_Position(game::e_ship_location::in_flight, target_star, game::ANTARES_POSITION));
                    const CREW_QUALITY crew_quality =
                        offensive_crew == e_offensive_crew::elite
                        ? CREW_QUALITY_ELITE
                        : CREW_QUALITY_VETERAN;
                    ship->crew_quality = crew_quality;
                    ship->crew_experience = MOX::crew_data[crew_quality].experience + 1;
                }
            }

            if (ship_count >= 5 || ship_key == game::NO_SHIP) {
                finished = true;
            }
            ship_count++;
        }

        return true;
    }

    int16_t __cdecl Antaran_Offensive_Force() {
        int16_t total_force = 0;

        for (int i = 0; i < ANTARAN_SHIP_CLASS_COUNT; ++i) {
            uint8_t ship_cost = Effective_Ship_Cost(i);
            int16_t ship_count = MOX::antarans.offensive_ships[i];

            total_force += (int16_t)(ship_count * ship_cost);
        }

        return total_force;
    }

    void __cdecl Kill_Antaran_Defensive_Ship(int16_t ship_idx) {
        int32_t defensive_count = (int32_t)MOX::antarans.defensive_ships[ship_idx] - 1;

        if (defensive_count < 1) {
            defensive_count = 0;
        }

        MOX::antarans.defensive_ships[ship_idx] = (int16_t)defensive_count;

        if (MOX::antarans.offensive_ships[ship_idx] > 0) {
            int32_t offensive_count = (int32_t)MOX::antarans.offensive_ships[ship_idx] - 1;

            if (offensive_count < 1) {
                offensive_count = 0;
            }

            MOX::antarans.offensive_ships[ship_idx] = (int16_t)offensive_count;
            MOX::antarans.defensive_ships[ship_idx]++;
        }
    }

    void __cdecl Kill_Antaran_Offensive_Ship(int16_t ship_idx) {
        int32_t count = (int32_t)MOX::antarans.offensive_ships[ship_idx] - 1;

        if (count < 1) {
            count = 0;
        }

        MOX::antarans.offensive_ships[ship_idx] = (int16_t)count;

        Restock_Antaran_Ship(ship_idx);
    }

    void __cdecl Restock_Antaran_Ship(int16_t ship_type_idx) {
        int32_t count = (int32_t)MOX::antarans.offensive_deployed[ship_type_idx] - 1;

        if (count < 1) {
            count = 0;
        }

        MOX::antarans.offensive_deployed[ship_type_idx] = (int16_t)count;
    }

    bool __cdecl Antarans_Should_Attack() {
        (void)Delta_Stardate();

        bool should_attack = false;

        int16_t total_resources = Antaran_Total_Resources();
        int16_t offensive_force = Antaran_Offensive_Force();

        uint8_t force_shift =
            fleet_config.attack.minimum_offensive_force_shift & 31;
        int64_t required_force =
            (int64_t)offensive_force * ((int64_t)1 << force_shift);
        if (required_force < (int32_t)total_resources) {
            return false;
        }

        bool has_available_offensive_ships = false;
        for (int16_t i = 0; i < ANTARAN_SHIP_CLASS_COUNT && !has_available_offensive_ships; ++i) {
            int32_t available_ships =
                (int32_t)MOX::antarans.offensive_ships[i] -
                (int32_t)MOX::antarans.offensive_deployed[i];

            if (available_ships > 0) {
                has_available_offensive_ships = true;
            }
        }

        // If Antarans have ships that aren't already deployed elsewhere, build up pressure
        if (has_available_offensive_ships) {
            MOX::antarans.attack_pressure++;
        }

        // If there's built-up pressure, perform a random check to decide on the attack
        if (MOX::antarans.attack_pressure > 0) {
            int32_t random_val =
                (int32_t)game_random::Random(fleet_config.attack.roll_range) - 1;
            if ((int32_t)MOX::antarans.attack_pressure + 1 > (int32_t)random_val) {
                should_attack = true;
            }
        }

        return should_attack;
    }
} // namespace antarans
