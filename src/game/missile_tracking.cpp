#include "pch.h"

#include "missile_tracking.h"

namespace CMBTMIS {
    int16_t incoming_x[MAX_COMBAT_FLYERS];
    int16_t incoming_y[MAX_COMBAT_FLYERS];
    bool incoming_valid[MAX_COMBAT_FLYERS];

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

    void Clear_Fighter_Slot_History(int16_t missile_idx) {
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

    void __cdecl Kill_Missile(int16_t missile_idx) {
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
                Kill_Missile(missile_idx);
                return;
            }
        }
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
} // namespace CMBTMIS
