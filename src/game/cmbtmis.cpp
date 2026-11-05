#include "pch.h"
#include "combat.h"

namespace CMBTMIS {
    static int16_t incoming_x[MAX_COMBAT_FLYERS];
    static int16_t incoming_y[MAX_COMBAT_FLYERS];
    static bool incoming_valid[MAX_COMBAT_FLYERS];

    struct s_fighter_pursuit_history {
        bool valid;
        bool target_is_missile;
        int16_t target_idx;
        int16_t round;
        int16_t fighter_x;
        int16_t fighter_y;
        int16_t target_x;
        int16_t target_y;
        int32_t range;
        int32_t closing;
        uint8_t failed_rounds;
        bool escaping;
    };

    static s_fighter_pursuit_history fighter_pursuit[MAX_COMBAT_FLYERS];

    struct s_failed_fighter_target {
        bool valid;
        bool target_is_missile;
        int16_t target_idx;
        int16_t round;
        uint8_t failures;
    };

    // A sortie remembers several recent chases so switching targets cannot
    // immediately erase escape evidence. Old entries permit periodic retries.
    static constexpr int kFailedTargetSlots = 4;
    static constexpr int kFailedTargetCooldown = 3;
    static s_failed_fighter_target failed_fighter_targets[MAX_COMBAT_FLYERS][kFailedTargetSlots];

    static s_failed_fighter_target* Failed_Fighter_Target(
        int16_t missile_idx, int16_t target_idx, bool is_missile) {
        for (s_failed_fighter_target& entry : failed_fighter_targets[missile_idx]) {
            const int32_t age = (int32_t)MOX::combat_turn - entry.round;
            if (entry.valid && (age < 0 || age > kFailedTargetCooldown)) entry = {};
            if (entry.valid && entry.target_idx == target_idx &&
                entry.target_is_missile == is_missile) {
                return &entry;
            }
        }
        return nullptr;
    }

    static void Remember_Failed_Fighter_Target(int16_t missile_idx) {
        const s_missile& fighter = MOX::missile[missile_idx];
        s_failed_fighter_target* entry = Failed_Fighter_Target(
            missile_idx, fighter.target_unit_idx, fighter.target_is_missile != 0);
        if (entry == nullptr) {
            entry = &failed_fighter_targets[missile_idx][0];
            for (s_failed_fighter_target& candidate : failed_fighter_targets[missile_idx]) {
                if (!candidate.valid) {
                    entry = &candidate;
                    break;
                }
                if (candidate.round < entry->round) entry = &candidate;
            }
            *entry = {};
        }
        entry->valid = true;
        entry->target_idx = fighter.target_unit_idx;
        entry->target_is_missile = fighter.target_is_missile != 0;
        entry->round = MOX::combat_turn;
        if (entry->failures < 2) ++entry->failures;
    }

    static void Clear_Fighter_Slot_History(int16_t missile_idx) {
        fighter_pursuit[missile_idx] = {};
        memset(failed_fighter_targets[missile_idx], 0, sizeof(failed_fighter_targets[missile_idx]));
        for (int i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            for (s_failed_fighter_target& entry : failed_fighter_targets[i]) {
                if (entry.valid && entry.target_is_missile && entry.target_idx == missile_idx) entry = {};
            }
            if (fighter_pursuit[i].valid && fighter_pursuit[i].target_is_missile &&
                fighter_pursuit[i].target_idx == missile_idx) {
                fighter_pursuit[i] = {};
            }
        }
    }

    void Reset_Fighter_Pursuit_History() {
        memset(fighter_pursuit, 0, sizeof(fighter_pursuit));
        memset(failed_fighter_targets, 0, sizeof(failed_fighter_targets));
    }

    static void Fighter_Target_Position(
        const s_missile& fighter, int16_t* x, int16_t* y) {
        if (fighter.target_is_missile) {
            *x = MOX::missile[fighter.target_unit_idx].x;
            *y = MOX::missile[fighter.target_unit_idx].y;
        } else {
            COMBINIT::Ship_Center_Offsets(fighter.target_unit_idx, x, y);
            *x += MOX::combat_data[fighter.target_unit_idx].x * 20;
            *y += MOX::combat_data[fighter.target_unit_idx].y * 20;
        }
    }

    void Reconsider_Fighter_Target(int16_t missile_idx) {
        const s_missile& fighter = MOX::missile[missile_idx];
        s_fighter_pursuit_history& history = fighter_pursuit[missile_idx];
        if (history.valid && history.round == MOX::combat_turn) return;

        int16_t x, y;
        Fighter_Target_Position(fighter, &x, &y);
        const int32_t range = special::Range(fighter.x, fighter.y, x, y);
        const int32_t elapsed = (int32_t)MOX::combat_turn - history.round;
        if (history.valid && elapsed > 0 &&
            history.target_idx == fighter.target_unit_idx &&
            history.target_is_missile == (fighter.target_is_missile != 0)) {
            const int32_t dx = (int32_t)history.target_x - history.fighter_x;
            const int32_t dy = (int32_t)history.target_y - history.fighter_y;
            const int32_t moved_x = (int32_t)x - history.target_x;
            const int32_t moved_y = (int32_t)y - history.target_y;
            // Only outward motion is evidence of escape. A fast ship holding
            // position or crossing the pursuit line is not assumed to retreat.
            history.escaping = (int64_t)moved_x * dx + (int64_t)moved_y * dy > 0;
            history.closing = (history.range - range) / elapsed;
            if (history.escaping && history.closing <= 0) {
                if (history.failed_rounds < 2) ++history.failed_rounds;
                Remember_Failed_Fighter_Target(missile_idx);
            } else {
                history.failed_rounds = 0;
                s_failed_fighter_target* entry = Failed_Fighter_Target(
                    missile_idx, fighter.target_unit_idx, fighter.target_is_missile != 0);
                if (entry != nullptr) *entry = {};
            }
        } else {
            history.escaping = false;
            history.failed_rounds = 0;
        }

        Retarget_Missile(missile_idx);
        if (fighter.type <= WEAPON_NO_WEAPONS) return;
        if (history.target_idx != fighter.target_unit_idx ||
            history.target_is_missile != (fighter.target_is_missile != 0)) {
            history.escaping = false;
            history.failed_rounds = 0;
        }
        Fighter_Target_Position(fighter, &x, &y);
        history.valid = true;
        history.target_idx = fighter.target_unit_idx;
        history.target_is_missile = fighter.target_is_missile != 0;
        history.round = MOX::combat_turn;
        history.fighter_x = fighter.x;
        history.fighter_y = fighter.y;
        history.target_x = x;
        history.target_y = y;
        history.range = special::Range(fighter.x, fighter.y, x, y);
    }

    int64_t Plasma_Torpedo_Remaining_Damage(
        const s_missile* missile, int64_t base_damage, bool resolving_impact) {
        int64_t speed = Missile_Speed(
            missile->type, missile->specials, missile->owner);
        int64_t distance = speed * ((int64_t)missile->fighter_count + 1) * 20 -
            missile->travel_dist;
        if (resolving_impact) distance -= 20;
        if (distance < 0) distance = 0;

        int64_t dissipation = (distance / 20) * plasma_torpedo_dissipation;
        return base_damage - dissipation;
    }

    int8_t missile_initiative = -1;

    int16_t missile_group_offset[16][16] = {
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {-11, -7, -3, -8, 6, -11, 10, -4, 12, 6, 3, 8, -6, 11, -10, 4},
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-12, 7, -9, -3, -6, -11, 3, -8, 12, -7, 9, 3, 6, 11, -3, 8},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {11, 7, 3, 8, -6, 11, -10, 4, -12, -6, -3, -8, 6, -11, 10, -4},
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-12, 7, -9, -3, -6, -11, 3, -8, 12, -7, 9, 3, 6, 11, -3, 8},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
    };
    int16_t torpedo_group_offset[16][8] = {
        {12, -12, 12, 12, -12, 12, -12, -12},
        {4, -16, 15, 8, -4, 16, -15, -8},
        {-1, 18, 18, 1, 1, 18, -18, -1},
        {-9, -14, 17, -5, 9, 14, -17, 5},
        {-12, -12, 12, -12, 12, 12, -12, 12},
        {-17, -5, 9, -14, 17, 5, -9, 14},
        {-18, 1, 1, -18, 18, 1, -1, 18},
        {-15, 8, -4, -16, 15, -8, 4, 16},
        {-12, 12, -12, -12, 12, -12, 12, 12},
        {-4, 16, -15, -8, 4, -16, 15, 8},
        {1, -18, -18, -1, -1, -18, 18, 1},
        {9, 14, -17, 5, -9, -14, 17, -5},
        {12, 12, -12, 12, -12, -12, 12, -12},
        {17, 5, -9, 14, -17, -5, 9, -14},
        {18, -1, -1, 18, -18, -1, 1, -18},
        {15, -8, 4, 16, -15, 8, -4, -16},
    };
    int8_t __cdecl Did_Missile_Hit(int16_t missile_idx, int16_t x, int16_t y) {
        int32_t dx = (int32_t)x - (int32_t)MOX::missile[missile_idx].x;
        int32_t dy = (int32_t)y - (int32_t)MOX::missile[missile_idx].y;
        int32_t dist_sq = dx * dx + dy * dy;

        if (MOX::missile[missile_idx].target_is_missile == 1) {
            return dist_sq < 64 /* 0x40 */;
        }

        int16_t target_idx = MOX::missile[missile_idx].target_unit_idx;
        int32_t radius = (int32_t)MOX::missile_hit_radius[MOX::combat_data[target_idx].size_class];
        int32_t radius_sq = radius * radius;

        return dist_sq < radius_sq;
    }

    int16_t __cdecl Missile_Owner(int16_t missile_idx) {
        return MOX::missile[missile_idx].owner;
    }

    int8_t __cdecl On_Combat_Screen(int16_t x, int16_t y) {
        if (MOX::cur_combat_x <= x && x < MOX::cur_combat_x + 32 /* 0x20 */) {
            if (MOX::cur_combat_y <= y && y < MOX::cur_combat_y + 18 /* 0x12 */) {
                return 1;
            }
        }
        return 0;
    }

    int8_t __cdecl Missile_On_Combat_Screen(int16_t missile_idx) {
        s_missile* missile = &MOX::missile[missile_idx];

        return CMBTMIS::On_Combat_Screen(missile->x / 20, missile->y / 20);
    }

    static int32_t Fighter_Contact_Distance(
        const s_missile& fighter, int16_t target_x, int16_t target_y, int32_t radius) {
        int32_t x = fighter.x;
        int32_t y = fighter.y;
        int32_t travel = 0;
        const int32_t budget = fighter.travel_dist > 0 ? fighter.travel_dist : 0;
        for (;;) {
            const int32_t dx = (int32_t)target_x - x;
            const int32_t dy = (int32_t)target_y - y;
            if ((int64_t)dx * dx + (int64_t)dy * dy < (int64_t)radius * radius) {
                return travel;
            }
            if (fighter.speed == 0) return INT32_MAX;

            // Match seeking's integer movement and strict contact test without
            // moving the real sortie. Split the last step at its remaining budget.
            int32_t step = fighter.speed;
            if (travel < budget && budget - travel < step) step = budget - travel;
            int32_t move_x, move_y;
            if (abs(dx) >= abs(dy)) {
                move_x = dx < 0 ? -step : step;
                move_y = move_x * dy / dx;
            } else {
                move_y = dy < 0 ? -step : step;
                move_x = move_y * dx / dy;
            }
            if (move_x > 0) x = x + move_x < target_x - 3 ? x + move_x : target_x - 3;
            if (move_x < 0) x = x + move_x > target_x + 3 ? x + move_x : target_x + 3;
            if (move_y > 0) y = y + move_y < target_y - 3 ? y + move_y : target_y - 3;
            if (move_y < 0) y = y + move_y > target_y + 3 ? y + move_y : target_y + 3;
            travel += step;
        }
    }

    struct s_fighter_target_score {
        int16_t target_idx = -1;
        bool is_missile = false;
        int16_t rank = 3;
        int16_t range = INT16_MAX;
        int32_t time = INT32_MAX;
        int32_t speed = INT32_MAX;
        int16_t size = INT16_MAX;
        bool worthwhile = false;
        bool current = false;
    };

    static bool Fighter_Target_Is_Better(
        const s_fighter_target_score& candidate, const s_fighter_target_score& best) {
        if (best.target_idx == -1) return true;
        if (candidate.rank != best.rank) return candidate.rank < best.rank;
        if (candidate.worthwhile != best.worthwhile) return candidate.worthwhile;
        // Pursue keeps a poor assignment when every alternative is also poor,
        // rather than alternating between previously unsuccessful chases.
        if (!candidate.worthwhile && candidate.current != best.current) return candidate.current;
        const int32_t band = candidate.time == INT32_MAX ? INT32_MAX : (candidate.time + 1023) / 1024;
        const int32_t best_band = best.time == INT32_MAX ? INT32_MAX : (best.time + 1023) / 1024;
        if (band != best_band) return band < best_band;
        if (candidate.speed != best.speed) return candidate.speed < best.speed;
        if (candidate.size != best.size) return candidate.size < best.size;
        if (candidate.time != best.time) return candidate.time < best.time;
        return candidate.range < best.range;
    }

    static void Consider_Fighter_Target(
        int16_t missile_idx, int16_t target_idx, bool is_missile, int16_t rank, int16_t x, int16_t y, int32_t speed, int32_t radius, int16_t size, s_fighter_target_score* best) {
        const s_missile& fighter = MOX::missile[missile_idx];
        s_fighter_target_score candidate;
        candidate.target_idx = target_idx;
        candidate.is_missile = is_missile;
        candidate.rank = rank;
        candidate.speed = speed;
        candidate.size = size;
        candidate.current = fighter.target_unit_idx == target_idx &&
            (fighter.target_is_missile != 0) == is_missile;
        candidate.range = special::Range(fighter.x, fighter.y, x, y);
        const int32_t distance = Fighter_Contact_Distance(fighter, x, y, radius);
        const int32_t budget = fighter.travel_dist > 0 ? fighter.travel_dist : 0;
        if (distance != INT32_MAX && distance <= budget) {
            candidate.time = 0;
        } else {
            int32_t closing = (int32_t)fighter.speed * 20;
            const s_fighter_pursuit_history& history = fighter_pursuit[missile_idx];
            const s_failed_fighter_target* failure = Failed_Fighter_Target(
                missile_idx, target_idx, is_missile);
            uint8_t failed_rounds = failure != nullptr ? failure->failures : 0;
            if (history.valid && history.target_idx == target_idx &&
                history.target_is_missile == is_missile && history.escaping) {
                if (history.failed_rounds > failed_rounds) failed_rounds = history.failed_rounds;
                if (history.closing > 0 && history.closing < closing) {
                    closing = history.closing;
                }
            }
            if (failed_rounds >= 2 || (failure != nullptr && !candidate.current)) closing = 0;
            if (distance != INT32_MAX && closing > 0) {
                candidate.time = (int32_t)(((int64_t)(distance - budget) * 1024 +
                                               closing - 1) /
                                     closing) +
                    failed_rounds * 1024;
            }
        }
        candidate.worthwhile = candidate.time != INT32_MAX &&
            candidate.time <= (int32_t)fighter_max_pursuit_turns * 1024;
        if (!candidate.worthwhile && fighter_pursuit_fallback == FIGHTER_PURSUIT_RETURN) return;
        if (Fighter_Target_Is_Better(candidate, *best)) *best = candidate;
    }

    static void Find_Smarter_Fighter_Target(
        int16_t missile_idx, int16_t target_player, int16_t* best_target, bool* best_is_missile) {
        const s_missile& fighter = MOX::missile[missile_idx];
        s_fighter_target_score best;

        for (int16_t ship_idx = 1; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
            const s_combat_data& ship = MOX::combat_data[ship_idx];
            if (ship.owner != target_player || ship.is_captured != 0 ||
                ship.special_status_flag == 4 ||
                CMBTMOV1::Ship_Is_In_Stasis(ship_idx) ||
                ship.unit_status != 0 ||
                !CMBTFIRE::Target_Is_Legal_For_Weapon(fighter.type, 0, ship_idx, 0)) {
                continue;
            }

            const int16_t rank = COMBINIT::Is_Combat_Satellite(&ship) ? 1 : 0;
            int16_t center_x, center_y;
            COMBINIT::Ship_Center_Offsets(ship_idx, &center_x, &center_y);
            Consider_Fighter_Target(missile_idx, ship_idx, false, rank, ship.x * 20 + center_x, ship.y * 20 + center_y, ship.current_speed, MOX::missile_hit_radius[ship.size_class], ship.size_class, &best);
        }

        if (CMBTFIRE::Weapon_Can_Fire_At_Missiles(fighter.type, fighter.specials)) {
            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                const s_missile& other = MOX::missile[i];
                if (i == missile_idx || other.type <= WEAPON_NO_WEAPONS ||
                    other.quantity <= 0 || other.owner != target_player ||
                    (other.target_is_missile == 0 &&
                        other.target_unit_idx == other.source_unit_idx) ||
                    TECHDATA::weapons[other.type].type != WEAPON_TYPE_FIGHTER ||
                    !CMBTFIRE::Target_Is_Legal_For_Weapon(fighter.type, 0, i, 1)) {
                    continue;
                }

                int16_t chain_idx = i;
                int16_t chain_length = 0;
                while (MOX::missile[chain_idx].target_is_missile != 0 &&
                    chain_length++ < MAX_COMBAT_FLYERS) {
                    chain_idx = MOX::missile[chain_idx].target_unit_idx;
                    if (chain_idx < 0 || chain_idx >= MAX_COMBAT_FLYERS ||
                        chain_idx == missile_idx) {
                        break;
                    }
                }
                if (chain_length > MAX_COMBAT_FLYERS || chain_idx == missile_idx ||
                    chain_idx < 0 || chain_idx >= MAX_COMBAT_FLYERS) {
                    continue;
                }

                Consider_Fighter_Target(missile_idx, i, true, 0, other.x, other.y, other.speed, 8, -1, &best);
            }
        }

        if (MOX::combat_total_ship_count > 0 &&
            MOX::combat_data[0].owner == target_player &&
            MOX::combat_data[0].unit_status == 0 &&
            CMBTFIRE::Does_Combat_Planet_Have_Defenses() &&
            CMBTFIRE::Target_Is_Legal_For_Weapon(fighter.type, 0, 0, 0)) {
            int16_t center_x, center_y;
            COMBINIT::Ship_Center_Offsets(0, &center_x, &center_y);
            const s_combat_data& planet = MOX::combat_data[0];
            Consider_Fighter_Target(missile_idx, 0, false, 2, planet.x * 20 + center_x, planet.y * 20 + center_y, 0, MOX::missile_hit_radius[planet.size_class], planet.size_class, &best);
        }
        *best_target = best.target_idx;
        *best_is_missile = best.is_missile;
    }

    void __cdecl Retarget_Missile(int16_t missile_idx) {
        int16_t best_target = -1;
        bool best_is_missile = false;
        s_missile& missile = MOX::missile[missile_idx];
        if (smarter_fighter_retargeting &&
            TECHDATA::weapons[missile.type].type == WEAPON_TYPE_FIGHTER &&
            missile.target_is_missile == 0 &&
            missile.target_unit_idx == missile.source_unit_idx) {
            return;
        }
        int16_t owner_player = MOX::combat_data[missile.source_unit_idx].owner;

        if (TECHDATA::weapons[missile.type].type == WEAPON_TYPE_FIGHTER ||
            (owner_player < MAX_PLAYERS && MOX::player[owner_player].tech_applications[TECH_APP_DAUNTLESS_GUIDANCE_SYSTEM] == TECH_RESEARCH_STATUS_RESEARCHED)) {
            int16_t min_range = 30000;
            int16_t target_player = MOX::combat_attacker_player;
            if (owner_player == MOX::combat_attacker_player) {
                target_player = MOX::combat_defender_player;
            }

            if (smarter_fighter_retargeting &&
                TECHDATA::weapons[missile.type].type == WEAPON_TYPE_FIGHTER &&
                missile.fighter_count > 0) {
                Find_Smarter_Fighter_Target(
                    missile_idx, target_player, &best_target, &best_is_missile);
            } else {
                for (int16_t ship_idx = 0; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
                    if (CMBTFIRE::Target_Is_Legal_For_Weapon(missile.type, 0, ship_idx, 0)) {
                        // Index 0 is the planet/base defenses
                        if (ship_idx != 0 || CMBTFIRE::Does_Combat_Planet_Have_Defenses()) {
                            s_combat_data& ship = MOX::combat_data[ship_idx];

                            if (ship.owner == target_player &&
                                ship.is_captured == 0 &&
                                ship.special_status_flag != 4 /* Stasis */ &&
                                !CMBTMOV1::Ship_Is_In_Stasis(ship_idx) &&
                                ship.unit_status == 0 &&
                                !Missile_Targets_Ship(&missile, ship_idx)) {
                                int16_t range = CMBTAI::Range_To_Missile(ship_idx, missile_idx);
                                if (range < min_range) {
                                    min_range = range;
                                    best_target = ship_idx;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (best_target == -1) {
            if (TECHDATA::weapons[missile.type].type == WEAPON_TYPE_FIGHTER &&
                MOX::combat_data[missile.source_unit_idx].unit_status == 0) {
                missile.target_unit_idx = missile.source_unit_idx;
                missile.target_is_missile = 0;
            } else {
                Kill_Missile(missile_idx);
            }
        } else {
            if (missile.target_unit_idx != best_target ||
                (missile.target_is_missile != 0) != best_is_missile) {
                fighter_pursuit[missile_idx] = {};
            }
            missile.target_unit_idx = best_target;
            missile.target_is_missile = best_is_missile;
        }
    }

    char __cdecl Move_Missile(int16_t missile_idx, int16_t target_x, int16_t target_y, int16_t move_dist) {
        s_missile* missile = &MOX::missile[missile_idx];
        int16_t type = missile->type;
        char hit_flag = 0;
        int16_t dx, dy;
        incoming_valid[missile_idx] = false;

        if (missile->x == target_x && missile->y == target_y) {
            return 1;
        }

        if (type > WEAPON_NO_WEAPONS) {
            const int16_t start_x = missile->x;
            const int16_t start_y = missile->y;
            int16_t dist = move_dist;
            if (missile->travel_dist < dist) {
                dist = missile->travel_dist;
            }

            Coord_Movement(missile->x, missile->y, target_x, target_y, (uint8_t)dist, &dx, &dy);

            missile->x += dx;
            missile->y += dy;

            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                s_missile* other = &MOX::missile[i];
                if (other->type > WEAPON_NO_WEAPONS && other->target_is_missile == 1 && other->target_unit_idx == missile_idx) {
                    if (Move_Missile(i, missile->x, missile->y, dist)) {
                        Resolve_Missile(i, missile->x, missile->y);
                    }
                }
            }

            if (dx > 0) {
                if (target_x - 3 < missile->x) {
                    missile->x = target_x - 3;
                }
            } else if (dx < 0) {
                if (missile->x < target_x + 3) {
                    missile->x = target_x + 3;
                }
            }

            if (dy > 0) {
                if (target_y - 3 < missile->y) {
                    missile->y = target_y - 3;
                }
            } else if (dy < 0) {
                if (missile->y < target_y + 3) {
                    missile->y = target_y + 3;
                }
            }

            hit_flag = Did_Missile_Hit(missile_idx, target_x, target_y);
            if (hit_flag) {
                // The three-pixel stand-off can distort a shallow approach into
                // a diagonal; retain the position before that final step.
                incoming_x[missile_idx] = start_x;
                incoming_y[missile_idx] = start_y;
                incoming_valid[missile_idx] = true;
            }
            COMBAT::Missile_Changed(missile_idx);
            missile->travel_dist -= dist;
            missile->facing_dir = (int8_t)Get_Facing(missile_idx, target_x, target_y);
        }

        missile = &MOX::missile[missile_idx];
        type = missile->type;
        if ((type == WEAPON_PLASMA_TORPEDO || type == WEAPON_DRAGON_BREATH) &&
            hit_flag == 0 &&
            (missile->specials & WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION) == 0) {
            int16_t owner = Missile_Owner(missile_idx);
            owner = Missile_Owner(missile_idx);
            int64_t bonus = MOX::ordnance_bonus[owner];
            if (missile->specials & WEAPON_MOD_FLAG_OVERLOADED) {
                bonus += 50;
            }

            int64_t base_damage =
                (int64_t)TECHDATA::weapons[type].max_damage * bonus / 100;
            int64_t remaining_damage;
            if (type == WEAPON_PLASMA_TORPEDO) {
                remaining_damage = Plasma_Torpedo_Remaining_Damage(
                    missile, base_damage, false);
            } else {
                int64_t speed = Missile_Speed(type, missile->specials, owner);
                int64_t distance =
                    speed * ((int64_t)missile->fighter_count + 1) * 20 -
                    missile->travel_dist;
                remaining_damage = base_damage - (distance / 20) * 15;
            }

            if (remaining_damage < 1) {
                Kill_Missile(missile_idx);
            }
        }

        return hit_flag;
    }

    void __cdecl Coord_Movement(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t step, int16_t* out_x, int16_t* out_y) {
        int16_t dx = x2 - x1;
        int16_t dy = y2 - y1;
        int32_t abs_dx = (dx < 0) ? -dx : dx;
        int32_t abs_dy = (dy < 0) ? -dy : dy;

        if (abs_dx < abs_dy) {
            int16_t sy = (dy < 0) ? -1 : 1;
            int16_t v = (int16_t)(sy * step);

            *out_y = v;
            *out_x = (int16_t)((int32_t)v * dx / dy);
        } else {
            int16_t sx = (dx < 0) ? -1 : 1;
            int16_t v = (int16_t)(sx * step);

            *out_x = v;
            *out_y = (int16_t)((int32_t)v * dy / dx);
        }
    }

    int16_t __cdecl Get_Facing(int16_t missile_idx, int16_t target_x, int16_t target_y) {
        return CMBTMOV1::Get_Facing(
            MOX::missile[missile_idx].x,
            MOX::missile[missile_idx].y,
            target_x,
            target_y);
    }

    int16_t __cdecl Missile_Shield_Bearing(int16_t missile_idx, int16_t target_idx) {
        const s_missile& missile = MOX::missile[missile_idx];
        const int16_t x = incoming_valid[missile_idx] ? incoming_x[missile_idx] : missile.x;
        const int16_t y = incoming_valid[missile_idx] ? incoming_y[missile_idx] : missile.y;
        return CMBTDRW1::Relative_Bearing_XY(target_idx, x, y);
    }

    void __cdecl Land_Returning_Fighters(
        s_combat_data* source, int16_t type, int16_t returned_count) {
        for (int16_t i = 0; i < 8 && returned_count > 0; ++i) {
            s_combat_weapon_slot* slot = &source->weapons[i];
            if (slot->weapon_id != type) {
                continue;
            }

            int16_t capacity = COMBINIT::Surviving_Weapon_Count_From_Hits(
                source->weapon_hits[i],
                source->weapon_hits_per_unit[i]);

            if (slot->ammo == 0) {
                slot->count = 0;
            }

            int16_t missing_count = capacity - slot->count;
            if (missing_count <= 0) {
                continue;
            }

            int16_t restored_count =
                returned_count < missing_count ? returned_count : missing_count;
            slot->count = (uint8_t)(slot->count + restored_count);
            returned_count -= restored_count;
            slot->ammo = 1;
            slot->shots_left = 0;
        }
    }

    int16_t __cdecl Resolve_Missile(
        int16_t missile_idx,
        int16_t x,
        int16_t y,
        bool is_seeking) {
        int16_t owner = Missile_Owner(missile_idx);
        MOX::missile_warning = 0;

        s_missile* missile = &MOX::missile[missile_idx];
        if (missile->is_active != 1) {
            MOX::missile_warning = 0;
            return 0;
        }

        int16_t source_unit_idx = missile->source_unit_idx;
        int16_t target_unit_idx = missile->target_unit_idx;
        int16_t type = missile->type;
        uint16_t specials = missile->specials;
        int16_t target_is_missile = missile->target_is_missile;
        const bool targets_missile = target_is_missile != 0;

        if (target_unit_idx == source_unit_idx && target_is_missile == 0) {
            int16_t reload_count = missile->quantity / 4;
            Land_Returning_Fighters(
                &MOX::combat_data[source_unit_idx], type, reload_count);
            Kill_Missile(missile_idx);
            return 0;
        }

        if (!Is_Missile_Target_Alive(missile_idx)) {
            return 0;
        }

        int16_t shield_idx = 0;
        int16_t evade = 0;
        uint16_t weapon_flags = TECHDATA::weapons[type].natural_mods;
        uint16_t applied_hits = 0;
        int16_t shield_hit_dir = 0;

        if (target_is_missile == 0) {
            if (COMBINIT::Does_Combat_Ship_Have_Special(target_unit_idx, SPECIAL_LIGHTNING_FIELD)) {
                if (lightning_field_mode == LIGHTNING_FIELD_MODE_UNIFORM) {
                    int16_t destroyed = 0;
                    int16_t roll_count = missile->quantity;
                    for (int16_t i = 0; i < roll_count; ++i) {
                        if (game_random::Random(100) <= lightning_field_ratio) {
                            destroyed++;
                        }
                    }
                    missile->quantity -= destroyed;
                } else {
                    for (int16_t i = 0; i < missile->quantity; ++i) {
                        if (game_random::Random(100) <= lightning_field_ratio) {
                            missile->quantity--;
                        }
                    }
                }

                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                    CMBTSPEC::Lightning_Field(target_unit_idx, missile_idx, x, y);
                }
            }

            CMBTFIR2::Final_Fire(
                target_unit_idx, x, y, missile_idx, is_seeking);

            const bool is_fighter = TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER;
            int16_t bearing = is_fighter
                ? CMBTDRW1::Relative_Bearing_XY(target_unit_idx, missile->x, missile->y)
                : Missile_Shield_Bearing(missile_idx, target_unit_idx);
            int32_t idx = (bearing * 2 + 22) / 45 + MOX::combat_data[target_unit_idx].facing_dir;
            int32_t sign = idx >> 31;
            shield_hit_dir = ((((idx ^ sign) - sign) & 0xF) ^ sign) - sign;

            if (is_fighter) {
                shield_idx = Find_Weakest_Shield(target_unit_idx);
                evade = 0;
            } else {
                shield_idx = CMBTFIRE::Facing_Shield(bearing);
                evade = MOX::combat_data[target_unit_idx].tac_val_evade;
            }

            if (owner < MAX_PLAYERS) {
                if (MOX::player[owner].tech_applications[TECH_APP_SENSORS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= missile_defense.sensors;
                } else if (MOX::player[owner].tech_applications[TECH_APP_NEUTRON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= missile_defense.neutron_scanner;
                } else if (MOX::player[owner].tech_applications[TECH_APP_TACHYON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= missile_defense.tachyon_scanner;
                }
            } else {
                evade -= missile_defense.neutron_scanner;
            }

            if (evade < 0) {
                evade = 0;
            } else if (specials & WEAPON_MOD_FLAG_ECCM) {
                evade /= 2;
            }

            if (type == WEAPON_DRAGON_BREATH) {
                evade = 0;
            }
        }

        if (missile->quantity <= 0) {
            Kill_Missile(missile_idx);
            return 0;
        }

        if (specials & WEAPON_MOD_FLAG_MIRV) {
            missile->quantity *= 4;
        }

        int16_t dmg_accum = 0;
        int16_t sys_dmg_accum = 0;

        if (TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
            missile->is_active = 0;
            if (type == WEAPON_ASSAULT_SHUTTLE) {
                CMBTFIR2::Resolve_Capture(missile->source_unit_idx, target_unit_idx, missile->quantity, x, y);
                missile->fighter_count = 0;
            } else {
                if (type != WEAPON_BOMBER) {
                    int16_t dmg_out = 0;
                    int16_t sys_dmg = 0;
                    CMBTFIRE::Fire_Fighter_Beam(missile_idx, shield_idx, target_is_missile, &dmg_out, &sys_dmg);
                    dmg_accum = dmg_out;
                    sys_dmg_accum = sys_dmg;
                    if (type == WEAPON_INTERCEPTOR || targets_missile) {
                        missile->fighter_count--;
                    }
                }

                if (missile->fighter_count != 0 &&
                    type != WEAPON_INTERCEPTOR &&
                    !targets_missile) {
                    int16_t dmg_out = 0;
                    int16_t sys_dmg = 0;
                    CMBTFIRE::Fire_Fighter_Bomb(missile_idx, shield_idx, &dmg_out, &sys_dmg);
                    missile->fighter_count--;
                    dmg_accum += dmg_out;
                    sys_dmg_accum += sys_dmg;
                }

                if (!targets_missile) CMBTFIRE::Record_Unit_Hit(target_unit_idx, dmg_accum, sys_dmg_accum);
                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                    Fighter_Combat_SFX(missile_idx, x, y, dmg_accum, sys_dmg_accum);
                }

                dmg_accum = 0;
                sys_dmg_accum = 0;
            }
        } else {
            int64_t bonus = MOX::ordnance_bonus[Missile_Owner(missile_idx)];
            if (specials & WEAPON_MOD_FLAG_OVERLOADED) {
                bonus += 50;
            }
            int64_t damage =
                (int64_t)TECHDATA::weapons[type].max_damage * bonus / 100;

            if ((type == WEAPON_PLASMA_TORPEDO || type == WEAPON_DRAGON_BREATH) &&
                !(specials & WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION)) {
                if (type == WEAPON_PLASMA_TORPEDO) {
                    damage = Plasma_Torpedo_Remaining_Damage(
                        missile, damage, true);
                } else {
                    int64_t distance = 9980 - missile->travel_dist; // 0x26fc
                    if (distance < 0) distance = 0;
                    damage -= (distance / 20) * 15;
                }
                if (damage <= 0) {
                    missile->quantity = 0;
                }
            }

            if (target_unit_idx == 0 && TECHDATA::weapons[type].type == WEAPON_TYPE_TORPEDO) {
                damage /= 2;
            }

            if (damage > INT16_MAX) damage = INT16_MAX;
            int16_t damage_to_apply = (int16_t)damage;

            int16_t loop_count = 0;
            if (missile->quantity > 0) {
                do {
                    int16_t roll = game_random::Random(100);

                    if (COMBINIT::Does_Combat_Ship_Have_Special(target_unit_idx, SPECIAL_DISPLACEMENT_DEVICE)) {
                        if (Missile_Defense_Roll_Succeeds(
                                game_random::Random(100),
                                missile_defense.displacement_device_missiles_miss_percent)) {
                            roll = -1000;
                        }
                    }

                    if (MOX::combat_data[target_unit_idx].special_status_flag == 1 &&
                        missile_defense.cloaking_device_missile_defense == 0) {
                        if (Missile_Defense_Roll_Succeeds(
                                game_random::Random(100),
                                missile_defense.cloaking_device_missile_miss_percent)) {
                            roll = -1000;
                        }
                    }

                    if (evade < roll) {
                        if (!(specials & WEAPON_MOD_FLAG_ENVELOPING) ||
                            !CMBTFIRE::Enveloping_Applies_To_Target(
                                target_unit_idx)) {
                            int16_t dmg_out = damage_to_apply;
                            int16_t sys_dmg = 0;
                            int16_t applied = CMBTFIRE::Apply_Ship_Damage(target_unit_idx, &dmg_out, &sys_dmg, shield_idx, specials, weapon_flags, 0, 0, specials & WEAPON_MOD_FLAG_EMISSION_GUIDANCE);
                            dmg_accum += dmg_out;
                            sys_dmg_accum += sys_dmg;
                            applied_hits |= applied;
                        } else {
                            for (int16_t i = 0; i < 4; ++i) {
                                int16_t dmg_out = damage_to_apply;
                                int16_t sys_dmg = 0;
                                CMBTFIRE::Apply_Ship_Damage(target_unit_idx, &dmg_out, &sys_dmg, i, specials, weapon_flags, 0, 0, specials & WEAPON_MOD_FLAG_EMISSION_GUIDANCE);
                                dmg_accum += dmg_out;
                                sys_dmg_accum += sys_dmg;
                            }
                        }
                    }
                    loop_count++;
                } while (loop_count < missile->quantity);
            }
            CMBTFIRE::Record_Unit_Hit(target_unit_idx, dmg_accum, sys_dmg_accum);
            Kill_Missile(missile_idx);
        }

        if (sys_dmg_accum + dmg_accum > 0) {
            if (target_is_missile == 0) {
                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                    if (!CMBTDRW1::Ship_Completely_On_Screen(target_unit_idx)) {
                        COMBINIT::Snap_Center_Combat_Screen(x / 20, y / 20);
                    }
                    int16_t offset_x, offset_y;
                    COMBINIT::Ship_Center_Offsets(target_unit_idx, &offset_x, &offset_y);

                    BEAMS::Add_Damage_Indicator_For_Ship(
                        (MOX::cur_combat_x * -20) - offset_x + x,
                        (MOX::cur_combat_y * -20) - offset_y + y,
                        target_unit_idx,
                        1,
                        dmg_accum,
                        sys_dmg_accum);

                    if (sys_dmg_accum <= 0) {
                        while (!BEAMS::Done_Drawing_Damage_Message_Queue()) {
                            timer::Mark_Time();
                            video::Set_Page_Off();
                            CMBTDRW1::Draw_Main_Combat_Screen();
                            graphics::Set_Window(0, 0, 639, 350);
                            video::Clip_On();

                            if (target_unit_idx == MOX::dont_draw_ship) {
                                int16_t d_x, d_y;
                                COMBINIT::Ship_Center_Offsets(target_unit_idx, &d_x, &d_y);
                                CMBTDRW1::Draw_Ship(target_unit_idx,
                                    (MOX::cur_combat_x * -20) - d_x + x,
                                    (MOX::cur_combat_y * -20) - d_y + y);
                            }

                            BEAMS::Draw_Damage_Message_Queue();
                            RUSS::Mox_Sync_Update();
                            timer::Release_Time(1);
                            video::Clip_Off();
                            graphics::Reset_Window();
                        }
                    } else {
                        BEAMS::Draw_Shield_Hit_Once_Through(target_unit_idx, shield_hit_dir, MOX::cur_combat_x * -20 + x, MOX::cur_combat_y * -20 + y);
                    }
                }
            }
        }

        if (target_is_missile == 0) {
            int16_t death_type = CMBTFIRE::Is_Combat_Ship_Dead(target_unit_idx);
            if (death_type != 0) {
                int16_t off_x, off_y;
                COMBINIT::Ship_Center_Offsets(target_unit_idx, &off_x, &off_y);
                CMBTFIRE::Destroy_Ship(target_unit_idx, x - off_x, y - off_y, death_type);
            }
        }

        if (TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
            if (missile->fighter_count == 0) {
                int16_t source = missile->source_unit_idx;
                if (MOX::combat_data[source].owner == Missile_Owner(missile_idx) &&
                    COMBAT1::Combat_Ship_Is_Functional(source) &&
                    type != WEAPON_ASSAULT_SHUTTLE) {
                    missile->target_unit_idx = source;
                    missile->target_is_missile = 0;
                    missile->is_active = 1;
                    return 0;
                }
                Kill_Missile(missile_idx);
                return 0;
            }

            if (missile->target_unit_idx == 0 && missile->target_is_missile == 0 &&
                !CMBTFIRE::Does_Combat_Planet_Have_Defenses()) {
                CMBTMIS::Retarget_Missile(missile_idx);
                return 0;
            }
        }

        return 0;
    }

    int32_t __cdecl Missile_Speed(int16_t type, uint16_t flags, int16_t player_idx) {
        bool boost_drive = false;
        bool boost_td = false;
        int32_t missile_speed = 0;

        switch (type) {
            case WEAPON_NUCLEAR_MISSILE:
                missile_speed = speed_base.nuclear_missile;
                boost_drive = true;
                break;
            case WEAPON_MERCULITE_MISSILE:
                missile_speed = speed_base.merculite_missile;
                boost_drive = true;
                break;
            case WEAPON_PULSON_MISSILE:
                missile_speed = speed_base.pulson_missile;
                boost_drive = true;
                break;
            case WEAPON_ZEON_MISSILE:
                missile_speed = speed_base.zeon_missile;
                boost_drive = true;
                break;

            case WEAPON_ANTI_MATTER_TORP:
                missile_speed = speed_base.anti_matter_torpedo;
                break;
            case WEAPON_PROTON_TORPEDO:
                missile_speed = speed_base.proton_torpedo;
                break;

            case WEAPON_PLASMA_TORPEDO:
                missile_speed = speed_base.plasma_torpedo;
                break;
            case WEAPON_DRAGON_BREATH:
                missile_speed = speed_base.plasma_breath;
                break;

            case WEAPON_ASSAULT_SHUTTLE:
                missile_speed = speed_base.assault_shuttle;
                boost_drive = true;
                boost_td = true;
                break;

            case WEAPON_HEAVY_FIGHTER:
                missile_speed = speed_base.heavy_fighter;
                boost_drive = true;
                boost_td = true;
                break;

            case WEAPON_BOMBER:
                missile_speed = speed_base.bomber;
                boost_drive = true;
                boost_td = true;
                break;

            case WEAPON_INTERCEPTOR:
                missile_speed = speed_base.interceptor;
                boost_drive = true;
                boost_td = true;
                break;

            default: {
                char message[64];
                snprintf(message, sizeof(message), "Missile_Speed_ failed, unknown flyer %d", type);
                exitm::Exit_With_Message(message);
            }
        }

        if (boost_drive) {
            int32_t drive_level = INITSHIP::Best_Warp_Drive(player_idx);
            missile_speed += (drive_level * 2) - 2;
        }

        if (boost_td && player_idx >= 0 && player_idx < MAX_PLAYERS &&
            MOX::player[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
            missile_speed += COMBAT1::td_combat_speed_bonus;
        }

        if (flags & WEAPON_MOD_FLAG_FAST) {
            missile_speed += speed_fast_bonus;
        }

        return missile_speed;
    }

    int16_t __cdecl Missile_Facing(int16_t missile_idx, int16_t target_idx, int16_t target_is_missile) {
        int16_t target_x;
        int16_t target_y;
        int16_t off_x;
        int16_t off_y;

        if (target_is_missile == 0) {
            COMBINIT::Ship_Center_Offsets(target_idx, &off_x, &off_y);

            target_x = (int16_t)MOX::combat_data[target_idx].x * 20 + off_x;
            target_y = (int16_t)MOX::combat_data[target_idx].y * 20 + off_y;
        } else {
            target_x = MOX::missile[target_idx].x;
            target_y = MOX::missile[target_idx].y;
        }

        return CMBTMOV1::Get_Facing(
            MOX::missile[missile_idx].x,
            MOX::missile[missile_idx].y,
            target_x,
            target_y);
    }

    int16_t __cdecl Is_Missile_Target_Alive(int16_t missile_idx) {
        int16_t result = 0;
        s_missile* missile = &MOX::missile[missile_idx];

        if (missile->target_is_missile != 0) {
            // Anti-missile rocket logic: target is another missile
            if (MOX::missile[missile->target_unit_idx].quantity > 0) {
                return 1;
            }
        } else {
            // Standard missile logic: target is a ship
            if (missile->target_unit_idx == 0) {
                return 1;
            }

            if (CMBTFIRE::Is_Combat_Ship_Dead(missile->target_unit_idx) == 0) {
                result = 1;
            }
        }

        return result;
    }

    int16_t __cdecl Find_Weakest_Shield(int16_t ship_idx) {
        int16_t weakest_idx = 0;
        int16_t min_shield = 200;

        for (int16_t i = 0; i < 4; ++i) {
            if (MOX::combat_data[ship_idx].shield_arc_current[i] < min_shield) {
                min_shield = MOX::combat_data[ship_idx].shield_arc_current[i];
                weakest_idx = i;
            }
        }

        return weakest_idx;
    }

    void __cdecl Get_Multiple_Torpedo_Coords(int16_t missile_idx, int16_t group_size, int16_t* out_x, int16_t* out_y) {
        uint8_t facing_dir = (uint8_t)MOX::missile[missile_idx].facing_dir;

        memset(out_x, 0, sizeof(*out_x) * 4);
        memset(out_y, 0, sizeof(*out_y) * 4);

        int32_t stardate_mod = (int32_t)MOX::stardate % 2;

        if (group_size == 2) {
            if (stardate_mod == 1) {
                out_x[0] = torpedo_group_offset[facing_dir][0];
                out_y[0] = torpedo_group_offset[facing_dir][1];
                out_x[1] = torpedo_group_offset[facing_dir][4];
                out_y[1] = torpedo_group_offset[facing_dir][5];
            } else {
                out_x[0] = torpedo_group_offset[facing_dir][2];
                out_y[0] = torpedo_group_offset[facing_dir][3];
                out_x[1] = torpedo_group_offset[facing_dir][6];
                out_y[1] = torpedo_group_offset[facing_dir][7];
            }
        } else if (group_size == 3) {
            if (stardate_mod == 1) {
                out_x[1] = torpedo_group_offset[facing_dir][0];
                out_y[1] = torpedo_group_offset[facing_dir][1];
                out_x[2] = torpedo_group_offset[facing_dir][4];
                out_y[2] = torpedo_group_offset[facing_dir][5];
            } else {
                out_x[1] = torpedo_group_offset[facing_dir][2];
                out_y[1] = torpedo_group_offset[facing_dir][3];
                out_x[2] = torpedo_group_offset[facing_dir][6];
                out_y[2] = torpedo_group_offset[facing_dir][7];
            }
        } else if (group_size == 4) {
            for (int16_t i = 0; i < 4; ++i) {
                out_x[i] = torpedo_group_offset[facing_dir][i * 2];
                out_y[i] = torpedo_group_offset[facing_dir][i * 2 + 1];
            }
        }
    }

    void __cdecl Get_Multiple_Missile_Coords(int16_t missile_idx, int16_t quantity, int16_t* out_x, int16_t* out_y) {
        uint8_t dir = (uint8_t)MOX::missile[missile_idx].facing_dir;

        memset(out_x, 0, sizeof(*out_x) * 9);
        memset(out_y, 0, sizeof(*out_y) * 9);

        if ((uint16_t)(quantity - 2) > 7) {
            return;
        }

        switch (quantity) {
            case 2:
                out_x[0] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][2] / 2);
                out_y[0] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][3] / 2);
                out_x[1] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][10] / 2);
                out_y[1] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][11] / 2);
                break;

            case 3:
                if ((MOX::stardate % 2) == 1) {
                    out_x[1] = CMBTMIS::missile_group_offset[dir][6];
                    out_y[1] = CMBTMIS::missile_group_offset[dir][7];
                    out_x[2] = CMBTMIS::missile_group_offset[dir][14];
                    out_y[2] = CMBTMIS::missile_group_offset[dir][15];
                } else {
                    out_x[1] = CMBTMIS::missile_group_offset[dir][2];
                    out_y[1] = CMBTMIS::missile_group_offset[dir][3];
                    out_x[2] = CMBTMIS::missile_group_offset[dir][10];
                    out_y[2] = CMBTMIS::missile_group_offset[dir][11];
                }
                break;

            case 4:
                out_x[0] = CMBTMIS::missile_group_offset[dir][2];
                out_y[0] = CMBTMIS::missile_group_offset[dir][3];
                out_x[1] = CMBTMIS::missile_group_offset[dir][6];
                out_y[1] = CMBTMIS::missile_group_offset[dir][7];
                out_x[2] = CMBTMIS::missile_group_offset[dir][10];
                out_y[2] = CMBTMIS::missile_group_offset[dir][11];
                out_x[3] = CMBTMIS::missile_group_offset[dir][14];
                out_y[3] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 5:
                out_x[1] = CMBTMIS::missile_group_offset[dir][2];
                out_y[1] = CMBTMIS::missile_group_offset[dir][3];
                out_x[2] = CMBTMIS::missile_group_offset[dir][6];
                out_y[2] = CMBTMIS::missile_group_offset[dir][7];
                out_x[3] = CMBTMIS::missile_group_offset[dir][10];
                out_y[3] = CMBTMIS::missile_group_offset[dir][11];
                out_x[4] = CMBTMIS::missile_group_offset[dir][14];
                out_y[4] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 6:
                out_x[0] = CMBTMIS::missile_group_offset[dir][2];
                out_y[0] = CMBTMIS::missile_group_offset[dir][3];
                out_x[1] = CMBTMIS::missile_group_offset[dir][4];
                out_y[1] = CMBTMIS::missile_group_offset[dir][5];
                out_x[2] = CMBTMIS::missile_group_offset[dir][6];
                out_y[2] = CMBTMIS::missile_group_offset[dir][7];
                out_x[3] = CMBTMIS::missile_group_offset[dir][10];
                out_y[3] = CMBTMIS::missile_group_offset[dir][11];
                out_x[4] = CMBTMIS::missile_group_offset[dir][12];
                out_y[4] = CMBTMIS::missile_group_offset[dir][13];
                out_x[5] = CMBTMIS::missile_group_offset[dir][14];
                out_y[5] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 7:
                out_x[1] = CMBTMIS::missile_group_offset[dir][2];
                out_y[1] = CMBTMIS::missile_group_offset[dir][3];
                out_x[2] = CMBTMIS::missile_group_offset[dir][4];
                out_y[2] = CMBTMIS::missile_group_offset[dir][5];
                out_x[3] = CMBTMIS::missile_group_offset[dir][6];
                out_y[3] = CMBTMIS::missile_group_offset[dir][7];
                out_x[4] = CMBTMIS::missile_group_offset[dir][10];
                out_y[4] = CMBTMIS::missile_group_offset[dir][11];
                out_x[5] = CMBTMIS::missile_group_offset[dir][12];
                out_y[5] = CMBTMIS::missile_group_offset[dir][13];
                out_x[6] = CMBTMIS::missile_group_offset[dir][14];
                out_y[6] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 8:
                for (int32_t i = 0; i < 8; ++i) {
                    out_x[i] = CMBTMIS::missile_group_offset[dir][i * 2];
                    out_y[i] = CMBTMIS::missile_group_offset[dir][i * 2 + 1];
                }
                break;

            case 9:
                for (int32_t i = 0; i < 8; ++i) {
                    out_x[i + 1] = CMBTMIS::missile_group_offset[dir][i * 2];
                    out_y[i + 1] = CMBTMIS::missile_group_offset[dir][i * 2 + 1];
                }
                break;
        }
    }

    void __cdecl Fighter_Combat_SFX(int16_t missile_idx, int16_t x, int16_t y, int16_t dmg, int16_t sys_dmg) {
        int16_t missile_type;
        int16_t target_unit_idx;
        int16_t target_x_scr, target_y_scr;
        int16_t source_x_scr, source_y_scr;
        int16_t beam_count;
        int16_t missile_x_offsets[10];
        int16_t missile_y_offsets[10];
        int16_t beams[18];
        int16_t shot_idx;
        int16_t off_x, off_y;
        uint8_t drawing;

        s_missile* missile = &MOX::missile[missile_idx];
        missile_type = missile->type;
        target_unit_idx = missile->target_unit_idx;

        if (missile->target_is_missile) {
            if (!Missile_On_Combat_Screen(target_unit_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::missile[target_unit_idx].x / 20, MOX::missile[target_unit_idx].y / 20);
            }
            target_x_scr = MOX::missile[target_unit_idx].x - MOX::cur_combat_x * 20;
            target_y_scr = MOX::missile[target_unit_idx].y - MOX::cur_combat_y * 20;
        } else {
            if (!CMBTDRW1::Ship_Completely_On_Screen(target_unit_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[target_unit_idx].x, MOX::combat_data[target_unit_idx].y);
            }
            target_x_scr = x - MOX::cur_combat_x * 20;
            target_y_scr = y - MOX::cur_combat_y * 20;
        }

        source_x_scr = missile->x - MOX::cur_combat_x * 20;
        source_y_scr = missile->y - MOX::cur_combat_y * 20;

        beam_count = (missile->quantity + (missile->quantity < 0 ? 3 : 0)) / 4;
        if (beam_count > 9) beam_count = 9;
        if (beam_count < 1) beam_count = 1;

        CMBTMIS::Get_Multiple_Missile_Coords(missile_idx, beam_count, missile_x_offsets, missile_y_offsets);
        shot_idx = 0;

        do {
            timer::Mark_Time();
            video::Set_Page_Off();
            missile->type = WEAPON_NO_WEAPONS;
            CMBTDRW1::Draw_Main_Combat_Screen();
            missile->type = missile_type;

            if (!missile->target_is_missile && missile->target_unit_idx == MOX::dont_draw_ship) {
                COMBINIT::Ship_Center_Offsets(missile->target_unit_idx, &off_x, &off_y);
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                CMBTDRW1::Draw_Ship(missile->target_unit_idx, target_x_scr - off_x, target_y_scr - off_y);
                video::Clip_Off();
                graphics::Reset_Window();
            }

            for (int16_t i = 0; i < beam_count; ++i) {
                beams[i * 2] = missile_x_offsets[i] + source_x_scr - 2;
                beams[i * 2 + 1] = missile_y_offsets[i] + source_y_scr - 2;
            }

            CMBTMIS::Draw_Fighter(missile_idx, source_x_scr, source_y_scr, 1);
            int16_t owner = CMBTMIS::Missile_Owner(missile_idx);
            drawing = BEAMS::Draw_Fighter_Beam(beams, beam_count, target_x_scr, target_y_scr, MOX::fighter_beam_type[owner], shot_idx);

            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
            shot_idx++;
        } while (drawing);

        if (missile->target_is_missile) {
            BEAMS::Add_Damage_Indicator_For_Missile(target_x_scr, target_y_scr, 1, dmg);
            BEAMS::Draw_Damage_Message_Queue_Until_Done();
        } else {
            COMBINIT::Ship_Center_Offsets(target_unit_idx, &off_x, &off_y);
            BEAMS::Add_Damage_Indicator_For_Ship(target_x_scr - off_x, target_y_scr - off_y, target_unit_idx, 1, dmg, sys_dmg);

            while (!BEAMS::Done_Drawing_Damage_Message_Queue()) {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                if (target_unit_idx == MOX::dont_draw_ship) {
                    COMBINIT::Ship_Center_Offsets(target_unit_idx, &off_x, &off_y);
                    CMBTDRW1::Draw_Ship(target_unit_idx, target_x_scr - off_x, target_y_scr - off_y);
                }
                BEAMS::Draw_Damage_Message_Queue();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
                video::Clip_Off();
                graphics::Reset_Window();
            }
        }
    }

    int16_t __cdecl Missile_Type(int16_t missile_idx) {
        switch (MOX::missile[missile_idx].type) {
            case WEAPON_NUCLEAR_MISSILE:
                return 0;
            case WEAPON_MERCULITE_MISSILE:
                return 1;
            case WEAPON_PULSON_MISSILE:
                return 2;
            case WEAPON_ZEON_MISSILE:
                return 3;
            case WEAPON_ANTI_MATTER_TORP:
                return 4;
            case WEAPON_PROTON_TORPEDO:
                return 5;
            case WEAPON_PLASMA_TORPEDO:
            case WEAPON_DRAGON_BREATH:
                return 6;
            case WEAPON_ASSAULT_SHUTTLE:
                return 3;
            case WEAPON_HEAVY_FIGHTER:
                return 1;
            case WEAPON_BOMBER:
                return 2;
            case WEAPON_INTERCEPTOR:
                return 0;
            default:
                return 0;
        }
    }

    int16_t __cdecl Picture_Count(int16_t missile_idx) {
        s_missile* missile = &MOX::missile[missile_idx];
        int8_t weapon_type_class = TECHDATA::weapons[missile->type].type;
        int16_t count;

        if (weapon_type_class == WEAPON_TYPE_MISSILE) {
            count = (missile->quantity / 5) + 1;
            if (count <= 0) {
                return 1;
            }
            if (count > 9) {
                return 9;
            }
            return count;
        }

        if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
            count = (missile->quantity / 5) + 1;
            if (count <= 0) {
                return 1;
            }
            if (count > 4) {
                return 4;
            }
            return count;
        }

        if (weapon_type_class == WEAPON_TYPE_FIGHTER) {
            count = missile->quantity;
            if (count <= 9) {
                count = 9;
            }
            return count;
        }

        return 1;
    }

    void __cdecl Kill_Missile(int16_t missile_idx) {
        if (MOX::missile[missile_idx].type > WEAPON_NO_WEAPONS) COMBAT::Retire_Missile(missile_idx);
        incoming_valid[missile_idx] = false;
        Clear_Fighter_Slot_History(missile_idx);
        // Clear the missile data structure
        memset(&MOX::missile[missile_idx], 0, sizeof(s_missile));

        // Scan for all active anti-missile rockets that were targeting this missile
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (MOX::missile[i].target_unit_idx == missile_idx &&
                MOX::missile[i].target_is_missile == 1) {
                Retarget_Missile(i);
            }
        }
    }

    void __cdecl Merge_Missile(int16_t missile_idx) {
        if (MOX::apply_damage_to_planet == 0) {
            Kill_Missile(missile_idx);
            return;
        }

        s_missile* missile = &MOX::missile[missile_idx];
        if (missile->type <= WEAPON_NO_WEAPONS) {
            return;
        }

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* other = &MOX::missile[i];
            if (missile != other &&
                missile->type == other->type &&
                missile->specials == other->specials &&
                missile->target_unit_idx == other->target_unit_idx &&
                missile->target_is_missile == other->target_is_missile &&
                missile->x == other->x &&
                missile->y == other->y &&
                missile->owner == other->owner &&
                (missile_cross_round_merge ||
                    missile->fighter_count == other->fighter_count)) {
                // Keep oversized groups separate without losing flyers. MIRV
                // needs room for its fourfold expansion when resolving impact.
                const int32_t quantity_limit =
                    (missile->specials & WEAPON_MOD_FLAG_MIRV) != 0
                    ? INT16_MAX / 4
                    : INT16_MAX;
                if ((int32_t)other->quantity + missile->quantity > quantity_limit) {
                    continue;
                }
                other->quantity += missile->quantity;
                COMBAT::Missile_Changed(i);
                Kill_Missile(missile_idx);
                return;
            }
        }
    }

    void __cdecl Draw_Fighter(int16_t missile_idx, int16_t base_x, int16_t base_y, int16_t combat_view_type) {
        int16_t rand_x = 0;
        int16_t rand_y = 0;
        int16_t num_missiles;
        int16_t x_coords[10];
        int16_t y_coords[10];
        int16_t type;
        int16_t owner;
        int16_t window_height;
        int16_t facing_dir;
        s_animation_header* fighter_anim;
        int16_t final_target_idx = Final_Flyer_Target(MOX::missile, missile_idx);

        num_missiles = MOX::missile[missile_idx].quantity / 4;
        if (num_missiles > 9) {
            num_missiles = 9;
        }
        if (num_missiles < 1) {
            num_missiles = 1;
        }

        Get_Multiple_Missile_Coords(missile_idx, num_missiles, x_coords, y_coords);

        if (combat_view_type == 1) {
            window_height = 479;
        } else {
            window_height = 350;
        }

        graphics::Set_Window(0, 0, 639, window_height);
        video::Clip_On();

        for (int16_t i = 0; i < num_missiles; ++i) {
            type = Missile_Type(missile_idx);

            if (combat_view_type == 0 && (MOX::ship_frame % 3 == 0)) {
                uint32_t old_seed = game_random::Get_Random_Seed();
                game_random::Set_Random_Seed((uint32_t)clock() + i + missile_idx);
                rand_x = (int16_t)game_random::Random(3) - 2;
                rand_y = (int16_t)game_random::Random(3) - 2;
                game_random::Set_Random_Seed(old_seed);
            }

            owner = Missile_Owner(missile_idx);
            facing_dir = (uint8_t)MOX::missile[missile_idx].facing_dir;
            fighter_anim = MOX::fighter_seg[owner][type * 16 + facing_dir];

            if (final_target_idx == MOX::cur_ship) {
                animate::Set_Animation_Frame(fighter_anim, (MOX::ship_frame + missile_idx) % 3);
            } else {
                animate::Reset_Animation_Frame(fighter_anim);
            }

            animate::Draw(x_coords[i] + base_x + rand_x - 12,
                y_coords[i] + base_y + rand_y - 12,
                fighter_anim);
        }

        video::Clip_Off();
        graphics::Reset_Window();
    }

    void __cdecl Draw_Missile(int16_t missile_idx, int16_t screen_x, int16_t screen_y, int16_t draw_fullscreen_view) {
        if (MOX::missile[missile_idx].type > WEAPON_NO_WEAPONS) {
            if (Missile_On_Combat_Screen(missile_idx) != 0 || draw_fullscreen_view != 0) {
                Missile_Owner(missile_idx);
                int16_t type_idx = Missile_Type(missile_idx);
                int16_t facing_idx = (int16_t)(uint8_t)MOX::missile[missile_idx].facing_dir;
                int16_t final_target_idx = Final_Flyer_Target(MOX::missile, missile_idx);

                if (TECHDATA::weapons[MOX::missile[missile_idx].type].type != WEAPON_TYPE_FIGHTER) {
                    if (MOX::missile[missile_idx].target_unit_idx == MOX::cur_ship ||
                        (MOX::missile[missile_idx].type == WEAPON_PROTON_TORPEDO &&
                            proton_torpedo_apply_speed_base == 0)) {
                        animate::Set_Animation_Frame(MOX::msl_seg[type_idx * 16 + facing_idx], (MOX::ship_frame + missile_idx) % 3);
                    } else {
                        animate::Reset_Animation_Frame(MOX::msl_seg[type_idx * 16 + facing_idx]);
                    }
                }

                if (final_target_idx != MOX::cur_ship || MOX::draw_active_missiles == 1) {
                    int16_t weapon_type = MOX::missile[missile_idx].type;
                    if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_FIGHTER) {
                        Draw_Fighter(missile_idx, screen_x, screen_y, draw_fullscreen_view);
                        return;
                    }

                    int16_t count = Picture_Count(missile_idx);
                    int16_t coords_x[10];
                    int16_t coords_y[10];

                    if (TECHDATA::weapons[weapon_type].type != WEAPON_TYPE_MISSILE && weapon_type != WEAPON_ANTI_MATTER_TORP) {
                        Get_Multiple_Torpedo_Coords(missile_idx, count, coords_y, coords_x);
                        graphics::Set_Window(0, 0, 0x27f, (draw_fullscreen_view == 1) ? 0x1df : 0x15e);
                        video::Clip_On();
                        for (int32_t i = 0; i < count; ++i) {
                            animate::Draw(coords_x[i] + screen_x - 0x17, coords_y[i] + screen_y - 0x17, MOX::msl_seg[type_idx * 16 + facing_idx]);
                        }
                        video::Clip_Off();
                        graphics::Reset_Window();
                    } else {
                        Get_Multiple_Missile_Coords(missile_idx, count, coords_y, coords_x);
                        graphics::Set_Window(0, 0, 0x27f, (draw_fullscreen_view == 1) ? 0x1df : 0x15e);
                        video::Clip_On();
                        for (int32_t i = 0; i < count; ++i) {
                            animate::Draw(coords_x[i] + screen_x - 0xc, coords_y[i] + screen_y - 0xc, MOX::msl_seg[type_idx * 16 + facing_idx]);
                        }
                        video::Clip_Off();
                        graphics::Reset_Window();
                    }
                }
            }
        }
    }

    int64_t __cdecl Fighter_Ocv(int16_t player_idx) {
        int64_t ocv;
        int16_t best_computer_idx;
        int16_t pilot_bonus;

        ocv = CMBTFIR2::Fleet_Offensive_Bonus(player_idx);
        best_computer_idx = INITSHIP::Best_Computer(player_idx);

        // Add computer offensive bonus
        ocv += TECHDATA::computers[best_computer_idx].bonus;

        // Players 0-7 get their racial/trained ship attack trait bonus
        if (player_idx < MAX_PLAYERS) {
            ocv += MOX::player[player_idx].traits[TRAIT_SHIP_ATTACK];
        }

        pilot_bonus = CMBTFIR2::Get_Fleet_Pilot_Bonus(player_idx);

        return ocv + pilot_bonus + flyer_combat_modifier.fighters_offense_bonus;
    }

    void __cdecl Next_Missile_Id(int16_t* missile_id, int16_t owner) {
        *missile_id = -1;
        int16_t active_total = 0;
        int16_t active_side = 0;
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (MOX::missile[i].type == WEAPON_NO_WEAPONS) {
                if (*missile_id == -1) {
                    *missile_id = i;
                }
            } else {
                ++active_total;
                if (MOX::missile[i].owner == owner) {
                    ++active_side;
                }
            }
        }
        if (active_total >= COMBINIT::max_flyers_total ||
            (owner >= 0 && active_side >= COMBINIT::max_flyers_per_side)) {
            *missile_id = -1;
        }
    }

    void __cdecl Fire_Missile(int16_t source_idx, int16_t target_idx, int16_t weapon_slot, int16_t is_anti_rocket) {
        int16_t missile_idx;
        int16_t source_offset_x, source_offset_y;
        int16_t target_coords_x, target_coords_y;

        Next_Missile_Id(&missile_idx, MOX::combat_data[source_idx].owner);
        if (missile_idx != -1) {
            incoming_valid[missile_idx] = false;
            Clear_Fighter_Slot_History(missile_idx);
            memset(&MOX::missile[missile_idx], 0, sizeof(s_missile));

            MOX::missile[missile_idx].source_unit_idx = source_idx;
            MOX::missile[missile_idx].hits_taken = 0;
            MOX::missile[missile_idx].owner = MOX::combat_data[source_idx].owner;
            MOX::missile[missile_idx].type = MOX::combat_data[source_idx].weapons[weapon_slot].weapon_id;
            MOX::missile[missile_idx].quantity = MOX::combat_data[source_idx].weapons[weapon_slot].count;
            MOX::missile[missile_idx].specials = MOX::combat_data[source_idx].weapons[weapon_slot].specials;
            MOX::missile[missile_idx].target_unit_idx = target_idx;

            COMBINIT::Ship_Center_Offsets(source_idx, &source_offset_x, &source_offset_y);
            MOX::missile[missile_idx].x = (MOX::combat_data[source_idx].x * 20) + source_offset_x;
            MOX::missile[missile_idx].y = (MOX::combat_data[source_idx].y * 20) + source_offset_y;

            MOX::missile[missile_idx].facing_dir = (int8_t)Missile_Facing(missile_idx, target_idx, is_anti_rocket);

            MOX::missile[missile_idx].fighter_count =
                MOX::combat_init != 0 ? missile_initiative : 0;

            MOX::missile[missile_idx].speed = (uint8_t)Missile_Speed(MOX::missile[missile_idx].type,
                MOX::missile[missile_idx].specials,
                MOX::missile[missile_idx].owner);
            MOX::missile[missile_idx].travel_dist = (int16_t)((uint16_t)(uint8_t)MOX::missile[missile_idx].speed * 20);
            MOX::missile[missile_idx].is_active = 1;
            MOX::missile[missile_idx].target_is_missile = (int8_t)is_anti_rocket;
            COMBAT::Missile_Changed(missile_idx);

            if (TECHDATA::weapons[MOX::missile[missile_idx].type].type == WEAPON_TYPE_FIGHTER) {
                MOX::missile[missile_idx].quantity = (int16_t)MOX::combat_data[source_idx].weapons[weapon_slot].count << 2;

                switch (MOX::missile[missile_idx].type) {
                    case WEAPON_ASSAULT_SHUTTLE:
                    case WEAPON_BOMBER:
                        MOX::missile[missile_idx].fighter_count =
                            MOX::missile[missile_idx].type == WEAPON_BOMBER
                            ? fighter_shots.bomber
                            : 1;
                        break;
                    case WEAPON_HEAVY_FIGHTER:
                        MOX::missile[missile_idx].fighter_count =
                            fighter_shots.heavy_fighter;
                        break;
                    case WEAPON_INTERCEPTOR:
                        MOX::missile[missile_idx].fighter_count =
                            fighter_shots.interceptor;
                        break;
                }
            }

            if (is_anti_rocket == 0) {
                int16_t target_offset_x, target_offset_y;
                COMBINIT::Ship_Center_Offsets(target_idx, &target_offset_x, &target_offset_y);
                target_coords_x = (MOX::combat_data[target_idx].x * 20) + target_offset_x;
                target_coords_y = (MOX::combat_data[target_idx].y * 20) + target_offset_y;
            } else {
                target_coords_x = MOX::missile[target_idx].x;
                target_coords_y = MOX::missile[target_idx].y;
            }

            if (MOX::apply_damage_to_planet == 1 &&
                (MOX::super_fast_flag == 0 ||
                    ((MOX::missile[missile_idx].type != WEAPON_PROTON_TORPEDO ||
                         proton_torpedo_apply_speed_base != 0) &&
                        MOX::missile[missile_idx].type != WEAPON_DRAGON_BREATH))) {
                CMBTSPEC::Missile_Launch_FX(missile_idx, target_coords_x, target_coords_y, is_anti_rocket);
            } else {
                Resolve_Missile(missile_idx, target_coords_x, target_coords_y);
            }

            Merge_Missile(missile_idx);
        }
    }

    int32_t __cdecl Get_Missile_Hits(int16_t missile_id, int16_t ship_idx, uint16_t flags) {
        int32_t hits = 0;
        int16_t armor_idx;
        int16_t absorption;

        switch (missile_id) {
            case WEAPON_NUCLEAR_MISSILE:
                hits = flyer_hit_points.nuclear_missile;
                break;

            case WEAPON_MERCULITE_MISSILE:
                hits = flyer_hit_points.merculite_missile;
                break;

            case WEAPON_PULSON_MISSILE:
                hits = flyer_hit_points.pulson_missile;
                break;

            case WEAPON_ZEON_MISSILE:
                hits = flyer_hit_points.zeon_missile;
                break;

            case WEAPON_ANTI_MATTER_TORP:
            case WEAPON_PROTON_TORPEDO:
            case WEAPON_PLASMA_TORPEDO:
            case WEAPON_DRAGON_BREATH:
                hits = 5;
                break;

            case WEAPON_ASSAULT_SHUTTLE:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.assault_shuttle *
                    (absorption + 100) / 200;
                break;

            case WEAPON_HEAVY_FIGHTER:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.heavy_fighter *
                    (absorption + 100) / 200;
                break;

            case WEAPON_BOMBER:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.bomber *
                    (absorption + 100) / 200;
                break;

            case WEAPON_INTERCEPTOR:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.interceptor *
                    (absorption + 100) / 200;
                break;

            default:
                break;
        }

        if (flags & WEAPON_MOD_FLAG_HEAVILY_ARMORED) {
            hits *= 2;
        }

        if (hits == 0) {
            hits = 1;
        }

        return hits;
    }

    void __cdecl Draw_Missiles(void) {
        for (int16_t i = MAX_COMBAT_FLYERS - 1; i >= 0; --i) {
            int16_t screen_x = MOX::missile[i].x - (MOX::cur_combat_x * 20);
            int16_t screen_y = MOX::missile[i].y - (MOX::cur_combat_y * 20);

            Draw_Missile(i, screen_x, screen_y, 0);
        }
    }

    void __cdecl Draw_Missile_Status(int16_t x_off, int16_t y_off, int16_t missile_idx) {
        char status_str[40];
        char msg_buf[80];
        s_animation_header* anim;
        const char* target_name;

        graphics::Fill(401, 379, 488, 467, 0);

        anim = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 45, MOX::global_cache_seg);
        animate::Draw(401, 379, anim);

        CMBTMIS::Draw_Missile(missile_idx, x_off + 44, y_off + 44, 1);
        fonts::Set_Font_Style(0, &MOX::combat_green);

        KEN::Ken_Get_Text_Message(52, msg_buf, 80);
        strlcpy(status_str, msg_buf, sizeof(status_str));

        if (MOX::missile[missile_idx].target_is_missile == 0) {
            target_name = MOX::combat_data[MOX::missile[missile_idx].target_unit_idx].name;
        } else {
            int16_t target_idx = MOX::missile[missile_idx].target_unit_idx;
            target_name = TECHDATA::weapons[MOX::missile[target_idx].type].name;
        }

        strlcat(status_str, target_name, sizeof(status_str));
        fonts::Print_Centered(x_off + 43, y_off + 2, status_str);

        snprintf(status_str, sizeof(status_str), "%i", MOX::missile[missile_idx].quantity);

        int16_t weapon_type = MOX::missile[missile_idx].type;
        if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_MISSILE) {
            int16_t msg_id = (MOX::missile[missile_idx].quantity == 1) ? 53 : 107;
            KEN::Ken_Get_Text_Message(msg_id, msg_buf, 80);
            strlcat(status_str, msg_buf, sizeof(status_str));
        } else if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_FIGHTER) {
            strlcat(status_str, " ", sizeof(status_str));
            const char* weapon_name;
            if (weapon_type == WEAPON_BOMBER) {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_BOMBER].name : TECHDATA::weapons[WEAPON_BOMBER].name_plural;
            } else if (weapon_type == WEAPON_INTERCEPTOR) {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_INTERCEPTOR].name : TECHDATA::weapons[WEAPON_INTERCEPTOR].name_plural;
            } else if (weapon_type == WEAPON_HEAVY_FIGHTER) {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_HEAVY_FIGHTER].name : TECHDATA::weapons[WEAPON_HEAVY_FIGHTER].name_plural;
            } else {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_ASSAULT_SHUTTLE].name : TECHDATA::weapons[WEAPON_ASSAULT_SHUTTLE].name_plural;
            }
            strlcat(status_str, weapon_name, sizeof(status_str));
        } else {
            int16_t msg_id = (MOX::missile[missile_idx].quantity == 1) ? 54 : 108;
            KEN::Ken_Get_Text_Message(msg_id, msg_buf, 80);
            strlcat(status_str, msg_buf, sizeof(status_str));
        }

        fonts::Print_Centered(x_off + 43, y_off + 81, status_str);
    }

    int64_t __cdecl Missile_Dcv(int16_t weapon_idx, int16_t player_idx) {
        int64_t bonus = 0;
        int64_t speed = Missile_Speed(weapon_idx, 0, player_idx);
        int64_t base_dcv = speed * 5;

        if (TECHDATA::weapons[weapon_idx].type == WEAPON_TYPE_FIGHTER) {
            bonus = CMBTFIR2::Get_Fleet_Pilot_Bonus(player_idx);
            int16_t fleet_base = 0;
            bonus += CMBTFIR2::Fleet_Defensive_Bonus(player_idx, 0, &fleet_base);

            // Add defense trait bonus if player is valid
            if (player_idx < MAX_PLAYERS && player_idx != -1) {
                bonus += MOX::player[player_idx].traits[TRAIT_SHIP_DEFENSE];
            }
        }

        switch (weapon_idx) {
            case WEAPON_NUCLEAR_MISSILE:
                return base_dcv + flyer_combat_modifier.nuclear_missile_defense_bonus;
            case WEAPON_MERCULITE_MISSILE:
                return base_dcv + flyer_combat_modifier.merculite_missile_defense_bonus;
            case WEAPON_PULSON_MISSILE:
                return base_dcv + flyer_combat_modifier.pulson_missile_defense_bonus;
            case WEAPON_ZEON_MISSILE:
                return base_dcv + flyer_combat_modifier.zeon_missile_defense_bonus;
            case WEAPON_ASSAULT_SHUTTLE:
            case WEAPON_HEAVY_FIGHTER:
            case WEAPON_BOMBER:
            case WEAPON_INTERCEPTOR:
                return base_dcv + bonus;
            default:
                return base_dcv;
        }
    }
    void Record_Missile_Hit(int16_t slot, int32_t destroyed) {
        if (COMBAT::Current_Viewer() < 0 || COMBAT::Local_Observer() != COMBAT::Current_Viewer() ||
            MOX::missile == nullptr || slot < 0 || slot >= MAX_COMBAT_FLYERS || MOX::missile[slot].type <= WEAPON_NO_WEAPONS) return;
        COMBAT::s_hit hit;
        hit.round = int32_t(MOX::combat_turn) + 1;
        hit.kind = COMBAT::MISSILES_DESTROYED;
        hit.amount = destroyed;
        strlcpy(hit.target_name, TECHDATA::weapons[MOX::missile[slot].type].name, sizeof(hit.target_name));
        COMBAT::Append_Hit(&hit);
    }
} // namespace CMBTMIS