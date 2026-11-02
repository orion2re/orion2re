#include "pch.h"
#include "ships_rpc.h"
#include "rpc_game.h"
#include "game/ship_read.h"
#include "config/text_encoding.h"
#include "lua/lua_game_ship_identity.h"

#include <algorithm>
#include <map>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

namespace ships_rpc {
    namespace {
        std::string Text(const char* text, size_t size = 1365) {
            std::string out;
            if (text != nullptr) config::Decode_Cp850_To_Utf8(text, strnlen(text, size), &out);
            return out;
        }

        void Design(const s_ship_design* design, const s_ship_data* ship, const char* source, rpc::s_design_observation* out) {
            out->source = source;
            out->name = Text(design->name, sizeof(design->name));
            out->hull_available = ship == nullptr || ship->status < 3;
            auto Ready = [](const char* label) { return label != nullptr && strnlen(label, 1366) <= 1365; };
            if (!ship_read::Valid_Design(design) || (out->hull_available && !Ready(ship_read::Type_Name(design))) ||
                !Ready(ship_read::Shield_Name(design)) || !Ready(ship_read::Computer_Name(design)) ||
                !Ready(ship_read::Armor_Name(design)) || !Ready(ship_read::Drive_Name(design)) || ESTRINGS::E_Strings(12) == nullptr || ESTRINGS::E_Strings(0x51) == nullptr) return;
            if (out->hull_available) out->hull = Text(ship_read::Type_Name(design));
            out->shield = Text(ship_read::Shield_Name(design));
            out->computer = Text(ship_read::Computer_Name(design));
            out->armor = Text(ship_read::Armor_Name(design));
            out->drive = Text(ship_read::Drive_Name(design));
            out->cost = design->cost;
            if (ship == nullptr || ship->status >= 3 || design->ship_type == SHIP_TYPE_COMBAT) {
                for (int i = 0; i < 8; ++i) {
                    const auto* weapon = &design->ship_weapon[i];
                    if (weapon->type == 0) continue;
                    const bool detailed = ship != nullptr && ship->status < 3;
                    const bool show_arc = ship == nullptr || detailed;
                    if (TECHDATA::weapons[weapon->type].name == nullptr || TECHDATA::weapons[weapon->type].name_plural == nullptr) return;
                    for (int mod = 0; mod < WEAPON_MOD_COUNT; ++mod)
                        if ((show_arc || mod == 1 || mod == 2) && (weapon->specials & (1u << mod)) && TECHDATA::weapon_modifications[mod].name_plural == nullptr) return;
                    char label[500], arc[128], modifications[128];
                    // Native production/refit formatting, including zero-count nonempty slots.
                    COLBLDG::Build_Weapon_String(const_cast<s_ship_weapons*>(weapon), label, sizeof(label), 0, weapon->count);
                    rpc::s_design_weapon item;
                    item.label = Text(label, sizeof(label));
                    if (show_arc) {
                        item.arc_available = detailed ? ship_read::Copy_Combat_Weapon_Arc(ship_read::Combat_Arc(weapon->firing_arc), arc, sizeof(arc)) : ship_read::Copy_Weapon_Arc(weapon->firing_arc, arc, sizeof(arc));
                        if (item.arc_available) item.arc = Text(arc, sizeof(arc));
                    }
                    if (show_arc) {
                        item.modifications_available = ship_read::Copy_Weapon_Modifications(weapon->specials, modifications, sizeof(modifications));
                        if (item.modifications_available) item.modifications = Text(modifications, sizeof(modifications));
                    }
                    item.count = weapon->count;
                    item.current_available = ship != nullptr && ship->status < 3;
                    item.current = item.current_available ? (uint8_t)weapon->current_count : 0;
                    out->weapons.push_back(std::move(item));
                }
            }
            for (int i = 0; i < ship_read::Special_Count(ship != nullptr && ship->status < 3); ++i) {
                if (!ship_read::Flag_Present(design->special_device_flags, i)) continue;
                if (!Ready(TECHDATA::specials[i].name)) return;
                const bool damage = ship != nullptr && ship->status < 3;
                out->systems.push_back({Text(TECHDATA::specials[i].name), damage, damage && ship_read::Flag_Present(ship->special_device_damage_flags, i)});
            }
            out->available = true;
        }

        void Route(const s_ship_data* ship, bool head, rpc::s_ship_observation* out) {
            if (ship->location >= 0 && ship->location < MOX::NUM_STARS) {
                out->destination_available = out->eta_available = rpc::SHIP_KNOWN_EMPTY;
            } else if (ship->location >= SHIP_LOCATION_MOVING_OFFSET && ship->location < SHIP_LOCATION_LIMIT) {
                const int16_t system = HAROLD::Absolute_Location(ship->location);
                if (system < 0 || system >= MOX::NUM_STARS) return;
                out->destination_available = rpc::SHIP_CURRENT;
                out->destination_system = system;
                out->eta_available = rpc::SHIP_CONTEXT_REQUIRED;
                if (head && ship->location < SHIP_LOCATION_WORMHOLE_OFFSET) {
                    out->eta_available = rpc::SHIP_CURRENT;
                    out->eta = (uint8_t)ship->turns_left;
                }
            }
        }
    } // namespace

    void Read_Strategic_Ship(int16_t viewer, const s_ship_data* ship, rpc::s_ship_observation* out) {
        const int16_t owner = ship->owner;
        const int16_t type = ship->d.ship_type;
        out->role = type == SHIP_TYPE_COMBAT ? "combat" : (type == SHIP_TYPE_COLONY ? "colony" : (type == SHIP_TYPE_TRANSPORT ? "transport" : (type == SHIP_TYPE_OUTPOST ? "outpost" : "")));
        if (owner > 8 && type == SHIP_TYPE_COMBAT) out->role = "special";
        if (owner >= 0 && owner < MAX_PLAYERS) out->banner = (uint8_t)MOX::player[owner].color;
        if (ship->location >= 0 && ship->location < MOX::NUM_STARS &&
            (out->owned || (ship->status == 0 && ship->x == MOX::star[ship->location].x && ship->y == MOX::star[ship->location].y))) out->system = ship->location;
        const bool hull = type == SHIP_TYPE_COMBAT && owner <= 8 && ship->d.size >= 0 && ship->d.size < HULL_CLASS_COUNT;
        const bool support = type == SHIP_TYPE_COLONY || type == SHIP_TYPE_TRANSPORT || type == SHIP_TYPE_OUTPOST;
        if (!out->owned) out->name_available = owner > 8 || support ? rpc::SHIP_CONTEXT_REQUIRED : rpc::SHIP_UNKNOWN;
        char scanned[80];
        const char* label = hull ? ship_read::Hull_Name(ship->d.size, false) : (support && (uint8_t)MOX::settings.language < 6 ? ship_read::Type_Name(&ship->d) : nullptr);
        if (hull && !out->owned) label = ship_read::Scanned_Label(viewer, ship, scanned, sizeof(scanned)) == ship_read::HULL_LABEL ? scanned : nullptr;
        out->type_available = label != nullptr && strnlen(label, 1366) <= 1365;
        if (out->type_available) out->type = Text(label);
    }

    bool Read_Composition(rpc::s_fleet_observation* out) {
        if (out->slots.empty()) return false;
        const auto* head = &MOX::ship[out->slots[0]];
        char title[120];
        out->heading_available = ship_read::Fleet_Title(head->owner, head, title, sizeof(title));
        if (out->heading_available) out->heading = Text(title, sizeof(title));
        // Special native summaries present a name, not an invented hull histogram.
        if (head->owner > 8) return true;
        ship_read::s_composition counts;
        for (int32_t slot : out->slots) {
            const auto* design = &MOX::ship[slot].d;
            if (design->ship_type != SHIP_TYPE_COMBAT && design->ship_type != SHIP_TYPE_COLONY && design->ship_type != SHIP_TYPE_OUTPOST && design->ship_type != SHIP_TYPE_TRANSPORT) return true;
            if (design->ship_type == SHIP_TYPE_COMBAT && design->size < 0) return false;
            ship_read::Count_Member(design, &counts);
        }
        for (int category = 0; category < 9; ++category) {
            if (counts.counts[category] == 0) continue;
            char line[200];
            if (!ship_read::Composition_Line(category, counts.counts[category], line, sizeof(line))) {
                out->composition.clear();
                return true;
            }
            out->composition.push_back({Text(line, sizeof(line)), counts.counts[category]});
        }
        out->composition_available = true;
        return true;
    }

    bool Read_Owned_Inspection(rpc::s_map_observation* out) {
        if (MOX::ship == nullptr || MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS) {
            return false;
        }
        std::vector<int16_t> active;
        for (int16_t slot = 0; slot < MOX::NUM_SHIPS; ++slot) {
            const auto* ship = &MOX::ship[slot];
            if (ship->owner == out->viewer && ship->status >= 0 && ship->status < 3) {
                const int16_t type = ship->d.ship_type;
                if (type != SHIP_TYPE_COMBAT && type != SHIP_TYPE_COLONY && type != SHIP_TYPE_TRANSPORT && type != SHIP_TYPE_OUTPOST) return false;
                active.push_back(slot);
            }
        }
        std::map<std::tuple<int16_t, int16_t, int16_t>, size_t> groups;
        for (int16_t slot : active) {
            const auto* ship = &MOX::ship[slot];
            ship_read::s_fleet_key native_key;
            ship_read::Fleet_Key(ship, &native_key);
            const auto key = std::make_tuple(native_key.location, native_key.x, native_key.y);
            const auto group = groups.find(key);
            size_t ordinal;
            if (group == groups.end()) {
                rpc::s_fleet_observation item;
                item.x = ship->x;
                item.y = ship->y;
                ordinal = out->fleets.size();
                out->fleets.push_back(std::move(item));
                groups.emplace(key, ordinal);
            } else
                ordinal = group->second;
            out->fleets[ordinal].slots.push_back(slot);
        }
        std::vector<int16_t> entries(2 * active.size());
        std::vector<bool> heads(MOX::NUM_SHIPS);
        for (auto& fleet : out->fleets) {
            for (size_t row = 0; row < fleet.slots.size(); ++row) {
                entries[row * 2] = (int16_t)fleet.slots[row];
                entries[row * 2 + 1] = ship_read::Member_Order(&MOX::ship[fleet.slots[row]].d);
            }
            ship_read::Order_Member_Entries(entries.data(), (int16_t)fleet.slots.size());
            for (size_t row = 0; row < fleet.slots.size(); ++row) fleet.slots[row] = entries[row * 2];
            heads[fleet.slots[0]] = true;
            if (!Read_Composition(&fleet)) return false;
        }
        for (int slot = 0; slot < 6; ++slot) {
            const auto* design = &MOX::player[out->viewer].ship_designs[slot];
            if (memchr(design->name, 0, sizeof(design->name)) == nullptr) return false;
            if (design->name[0] == '\0') continue;
            rpc::s_design_observation item;
            Design(design, nullptr, "slot", &item);
            out->designs.push_back(std::move(item));
        }
        for (int16_t slot = 0; slot < MOX::NUM_SHIPS; ++slot) {
            const auto* ship = &MOX::ship[slot];
            if (ship->owner != out->viewer) continue;
            int16_t colony = -1;
            const bool flying = ship->status >= 0 && ship->status < 3;
            if (!flying && !ship_read::Committed_Queue_Ship(out->viewer, slot, &colony)) continue;
            if (memchr(ship->d.name, 0, sizeof(ship->d.name)) == nullptr) return false;
            out->owned_authority.emplace_back(slot, orion2re::lua::game::Ship_Generation(slot));
            if (!flying) {
                rpc::s_design_observation design;
                Design(&ship->d, ship, ship->status == 4 ? "refit" : "queued", &design);
                const auto planet = MOX::colony[colony].planet;
                design.colony_system = MOX::planet[planet].star_index;
                design.colony_orbit = MOX::planet[planet].orbit + 1;
                out->designs.push_back(std::move(design));
                continue;
            }
            rpc::s_ship_observation item;
            item.owned = true;
            item.owner_slot = out->viewer;
            item.slot = slot;
            item.name = Text(ship->d.name, sizeof(ship->d.name));
            item.x = ship->x;
            item.y = ship->y;
            Read_Strategic_Ship(out->viewer, ship, &item);
            auto& detail = item.inspection;
            detail.status = "active";
            if (ship->location >= 0 && ship->location < MOX::NUM_STARS) detail.system = ship->location;
            Route(ship, heads[slot], &item);
            s_ship_design configuration;
            if (ship_read::Valid_Design(&ship->d))
                INITSHIP::Copy_Current_Ship_Configuration(ship->owner, &ship->d, &configuration);
            else
                configuration = ship->d;
            rpc::s_design_observation design;
            design.ship_slot = slot;
            Design(&configuration, ship, "ship", &design);
            out->designs.push_back(std::move(design));
            if (ship_read::Valid_Design(&configuration) && ship_read::Type_Name(&configuration) != nullptr && strnlen(ship_read::Type_Name(&configuration), 1366) <= 1365) {
                detail.role = ship->d.ship_type == SHIP_TYPE_COMBAT ? "combat" : (ship->d.ship_type == SHIP_TYPE_COLONY ? "colony" : (ship->d.ship_type == SHIP_TYPE_TRANSPORT ? "transport" : "outpost"));
                detail.type = Text(ship_read::Type_Name(&configuration));
                detail.available = true;
                if (flying) {
                    char crew[sizeof(MOX::temp_string)];
                    if (ship->d.ship_type == SHIP_TYPE_COMBAT) {
                        char quality[sizeof(MOX::temp_string)];
                        detail.crew_available = ship_read::Crew_Text(ship->crew_quality, quality, sizeof(quality));
                        crew[0] = '\0';
                        if (detail.crew_available) detail.experience_available = ship_read::Format_Fleet_Crew_Text(quality, ship->crew_experience, crew, sizeof(crew));
                    } else
                        detail.crew_available = ship_read::Detail_Crew_Text(ship->crew_quality, crew, 256);
                    detail.crew = Text(crew, sizeof(crew));
                    detail.experience = detail.experience_available ? ship->crew_experience : 0;
                    if (ship->d.previous_owner <= 14) {
                        ship_read::s_condition condition;
                        ship_read::Condition(slot, &condition);
                        detail.structure = condition.structure;
                        detail.structure_max = condition.structure_max;
                        detail.armor = condition.armor;
                        detail.armor_max = condition.armor_max;
                        detail.subsystems = condition.subsystems;
                        detail.drive_visible = condition.drive_visible;
                        detail.shield_visible = condition.subsystems && configuration.shield_type > 0;
                        detail.drive = condition.drive;
                        detail.drive_max = condition.drive_max;
                        detail.computer = condition.computer;
                        detail.computer_max = condition.computer_max;
                        detail.shield = condition.shield;
                        detail.shield_max = condition.shield_max;
                        detail.combat_speed = condition.combat_speed;
                        detail.warp_speed = condition.warp_speed;
                        detail.shield_strength = condition.shield_strength;
                        detail.shield_strength_max = condition.shield_strength_max;
                        detail.condition_available = condition.available;
                    }
                    if (ship->crew_quality >= 0 && ship->crew_quality < CREW_QUALITY_COUNT && (ship->officer_index == -1 || (MOX::leaders != nullptr && ship->officer_index >= 0 && ship->officer_index < MAX_LEADERS))) {
                        int16_t defense, evasion, attack;
                        INITSHIP::Read_Ship_Combat_Bonuses(ship, &configuration, &defense, &evasion, &attack);
                        detail.attack = attack;
                        detail.defense = defense;
                        detail.evasion = evasion;
                        detail.capabilities_available = true;
                    }
                }
            }
            out->ships.push_back(std::move(item));
        }
        out->inspection_captured = true;
        out->owned_ambient = true;
        out->ships_captured = true;
        return true;
    }
} // namespace ships_rpc
