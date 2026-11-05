#include "pch.h"
#include "rpc_tactical_reads.h"
#include "config/text_encoding.h"
#include "game/combat.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <uv.h>
#include "yyjson.h"

namespace rpc {
    std::string Tactical_Unit_Token(s_tactical_forwarder* forwarder, const s_unit* unit) {
        for (const auto& reference : forwarder->references)
            if (reference.slot == unit->slot) return reference.token.data();
        s_tactical_forwarder::s_reference reference;
        reference.slot = unit->slot;
        unsigned char bytes[16];
        if (uv_random(nullptr, nullptr, bytes, sizeof(bytes), 0, nullptr) != 0) {
            forwarder->viewer = -1;
            forwarder->battle_id[0] = '\0';
            forwarder->references.clear();
            forwarder->selected.clear();
            forwarder->retiring = true;
            Revoke_Event_Delivery(forwarder->service);
            return {};
        }
        constexpr char hex[] = "0123456789abcdef";
        for (size_t i = 0; i < sizeof(bytes); ++i) {
            reference.token[i * 2] = hex[bytes[i] >> 4];
            reference.token[i * 2 + 1] = hex[bytes[i] & 15];
        }
        forwarder->references.push_back(reference);
        return reference.token.data();
    }

    namespace {
        struct s_row {
            std::string token, json;
        };

        bool Row_Precedes(const s_row& a, const s_row& b) {
            return a.token < b.token;
        }

        std::string Unavailable(const s_read_request* request) {
            return Error_Response(request->encoded_id, -32004, "Unavailable");
        }

        std::string Unit_Token(s_tactical_forwarder* forwarder, int16_t slot) {
            s_unit unit;
            if (forwarder->reader.unit == nullptr || !forwarder->reader.unit(slot, &unit)) return {};
            return Tactical_Unit_Token(forwarder, &unit);
        }

        std::string Random_Token() {
            unsigned char bytes[16];
            if (uv_random(nullptr, nullptr, bytes, sizeof(bytes), 0, nullptr) != 0) return {};
            constexpr char hex[] = "0123456789abcdef";
            std::string token;
            for (auto byte : bytes) {
                token += hex[byte >> 4];
                token += hex[byte & 15];
            }
            return token;
        }
        std::string Name_Json(const char* name) {
            std::string utf8;
            config::Decode_Cp850_To_Utf8(name, std::strlen(name), &utf8);
            auto* doc = yyjson_mut_doc_new(nullptr);
            auto* value = yyjson_mut_strn(doc, utf8.data(), utf8.size());
            size_t size = 0;
            char* encoded = yyjson_mut_val_write(value, 0, &size);
            std::string result = encoded != nullptr ? std::string(encoded, size) : "\"\"";
            std::free(encoded);
            yyjson_mut_doc_free(doc);
            return result;
        }

        std::string Missile_Token(s_tactical_forwarder* forwarder, int16_t slot) {
            for (const auto& reference : forwarder->missile_references)
                if (reference.slot == slot) return reference.token.data();
            const auto token = Random_Token();
            if (token.empty()) {
                forwarder->retiring = true;
                Revoke_Event_Delivery(forwarder->service);
                return {};
            }
            s_tactical_forwarder::s_reference reference;
            reference.slot = slot;
            std::copy(token.begin(), token.end(), reference.token.begin());
            forwarder->missile_references.push_back(reference);
            return token;
        }

        std::string Missile_Row(const s_missile& missile, const std::string& token) {
            const char* kind = missile.type == 2 ? "torpedo" : missile.type == 4 ? "fighter"
                                                                                 : "missile";
            return "{\"missileId\":\"" + token + "\",\"kind\":\"" + kind + "\",\"name\":" + Name_Json(missile.name) +
                ",\"owned\":" + (missile.owned ? "true" : "false") + ",\"quantity\":" + std::to_string(missile.quantity) +
                ",\"position\":{\"x\":" + std::to_string(missile.x) + ",\"y\":" + std::to_string(missile.y) + "}}";
        }

        std::string Row(const s_unit* unit, const std::string& token, bool detail) {
            const char* kinds[] = {"ship", "planet", "satellite", "monster", "contact"};
            const char* sizes[] = {"small", "medium", "large", "huge"};
            std::string json = "{\"unitId\":\"" + token + "\",\"disclosure\":\"" + (unit->contact ? "contact" : "unit") +
                "\",\"kind\":\"" + kinds[unit->kind] + "\",\"side\":\"" + (unit->attacker ? "attacker" : "defender") +
                "\",\"owned\":" + (unit->owned ? "true" : "false") + ",\"position\":{\"x\":" + std::to_string(unit->x) +
                ",\"y\":" + std::to_string(unit->y) + "}";
            if (unit->kind != PLANET) json += ",\"mapSize\":\"" + std::string(sizes[unit->map_size]) + "\"";
            if (unit->owned && unit->kind == SHIP) json += ",\"retreatPending\":" + std::string(unit->retreat_pending ? "true" : "false");
            if (!unit->contact) {
                if (unit->kind != PLANET) json += ",\"facing\":" + std::to_string(unit->facing);
                if (detail) json += ",\"name\":" + Name_Json(unit->name);
            }
            if (detail && unit->own_details) {
                json += ",\"ownDetails\":{\"structureRemaining\":" + std::to_string(unit->structure) +
                    ",\"armorRemaining\":" + std::to_string(unit->armor) + ",\"driveRemaining\":" + std::to_string(unit->drive) +
                    ",\"shieldSystemRemaining\":" + std::to_string(unit->shield) + ",\"computerRemaining\":" + std::to_string(unit->computer) +
                    ",\"speed\":" + std::to_string(unit->speed) + ",\"movementRemaining\":" + std::to_string(unit->movement) +
                    ",\"inStasis\":" + (unit->in_stasis ? "true" : "false") + "}";
            }
            return json + "}";
        }
    } // namespace

    std::string Boarding_Result_Json(const BOARDING::s_result* result, std::string_view report_id) {
        if (result == nullptr || !result->available) return "null";
        const auto& r = *result;
        std::string json = "{\"mode\":\"" + std::string(r.capture ? "capture" : "raid") +
            "\",\"outcome\":\"" + (r.destroyed ? "destroyed" : r.capture ? (r.captured ? "captured" : "failed")
                                                                         : (r.raid_damage ? "raided" : "noDamage")) +
            "\",\"targetName\":" + Name_Json(r.target_name) + ",\"attackerLost\":" + std::to_string(r.attacker_lost) +
            ",\"defenderLost\":" + std::to_string(r.defender_lost) +
            ",\"attackerStrength\":" + std::to_string(r.attacker_strength) + ",\"defenderStrength\":" + std::to_string(r.defender_strength) +
            ",\"targetOwned\":" + (r.now_owned ? "true" : "false") +
            ",\"reportId\":" + (report_id.empty() ? "null" : "\"" + std::string(report_id) + "\"") +
            ",\"reportTimed\":" + (!report_id.empty() && BOARDING::Boarding_Report_Timed() ? "true" : "false");
        if (r.attacker_owned && r.committed >= 0) json += ",\"marinesCommitted\":" + std::to_string(r.committed) +
            ",\"transporterLosses\":" + std::to_string(r.committed - r.arrived);
        if (r.attacker_owned) json += ",\"raidersReturned\":" + std::to_string(r.capture ? 0 : r.attacker_survived);
        if (r.capture || r.attacker_owned) json += ",\"marinesArrived\":" + std::to_string(r.arrived) + ",\"attackerSurvived\":" + std::to_string(r.attacker_survived);
        if (r.capture) json += ",\"defenderStarted\":" + std::to_string(r.defender_started) + ",\"defenderSurvived\":" + std::to_string(r.defender_survived);
        if (r.now_owned) json += ",\"targetCaptured\":" + std::string(r.target_captured ? "true" : "false");
        if (r.capture && r.captured && r.now_owned) json += ",\"capturedMarines\":" + std::to_string(r.captured_marines);
        if (!r.capture) {
            json += ",\"contentAvailable\":" + std::string(r.content_available ? "true" : "false");
            if (r.content_available)
                json += ",\"text\":" + Name_Json(r.raid_text.c_str());
            else
                json += ",\"contentUnavailableReason\":\"limitExceeded\"";
        }
        return json + "}";
    }

    std::string Execute_Tactical_Read(const s_read_request* request, void* context, bool* dependent) {
        auto* forwarder = (s_tactical_forwarder*)context;
        const bool active = forwarder->battle_id[0] != '\0' && !forwarder->retiring;
        if (!request->battle_id.empty() && (!active || request->battle_id != forwarder->battle_id)) return Unavailable(request);
        std::string result;
        if (!active) {
            if (request->method != READ_VIEW) return Unavailable(request);
            result = "{\"battle\":null}";
        } else if (request->method == READ_HITS) {
            s_hit_history history;
            if (!Read_Hit_History(&history)) return Unavailable(request);
            result = "{\"battleId\":\"" + std::string(forwarder->battle_id) + "\",\"firstSequence\":" + std::to_string(history.first_sequence) +
                ",\"nextSequence\":" + std::to_string(history.next_sequence) + ",\"retentionGap\":" + (history.first_sequence > 1 ? "true" : "false") + ",\"hits\":[";
            for (size_t i = 0; i < history.hits.size(); ++i) {
                const auto& hit = history.hits[i];
                if (i) result += ',';
                const char* kind = hit.kind == COMBAT::MISSILES_DESTROYED ? "missilesDestroyed" : hit.kind == COMBAT::BIOLOGICAL_HITS ? "biologicalHits"
                                                                                                                                      : "unitDamage";
                result += "{\"sequence\":" + std::to_string(hit.sequence) + ",\"round\":" + std::to_string(hit.round) +
                    ",\"kind\":\"" + kind + "\",\"targetName\":" + Name_Json(hit.target_name) + ",\"amount\":" + std::to_string(hit.amount) +
                    ",\"secondaryAmount\":" + std::to_string(hit.secondary_amount) + "}";
            }
            result += "]}";
            *dependent = true;
        } else if (request->method == READ_MISSILE || request->method == READ_MISSILES) {
            result = "{\"battleId\":\"" + std::string(forwarder->battle_id) + "\",";
            if (request->method == READ_MISSILE) {
                s_missile missile;
                bool found = false;
                for (const auto& reference : forwarder->missile_references)
                    if (request->missile_id == reference.token.data() && forwarder->missile != nullptr && forwarder->missile(reference.slot, &missile)) {
                        found = true;
                        break;
                    }
                if (!found) return Unavailable(request);
                result += "\"missile\":" + Missile_Row(missile, request->missile_id) + "}";
            } else {
                std::vector<s_missile> missiles;
                if (forwarder->missiles == nullptr || !forwarder->missiles(&missiles)) return Unavailable(request);
                std::vector<std::string> rows;
                for (const auto& missile : missiles) {
                    const auto token = Missile_Token(forwarder, missile.slot);
                    if (token.empty()) return Unavailable(request);
                    rows.push_back(Missile_Row(missile, token));
                }
                std::sort(rows.begin(), rows.end());
                result += "\"visibleMissileCount\":" + std::to_string(rows.size()) + ",\"missiles\":[";
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (i) result += ',';
                    result += rows[i];
                }
                result += "]}";
            }
            *dependent = true;
        } else if (request->method == READ_OPTIONS) {
            s_options options;
            if (forwarder->options == nullptr || !forwarder->options(&options)) return Unavailable(request);
            *dependent = true;
            result = "{\"battleId\":\"" + std::string(forwarder->battle_id) + "\",\"control\":{\"available\":" + (options.available ? "true" : "false");
            if (!options.available) {
                result += ",\"reason\":\"noNativeControl\"}}";
            } else {
                if (forwarder->decision_generation != options.generation || forwarder->decision_id.empty()) {
                    forwarder->decision_id = Random_Token();
                    forwarder->decision_generation = options.generation;
                }
                if (forwarder->decision_id.empty()) return Unavailable(request);
                result += ",\"actorUnitId\":\"" + Unit_Token(forwarder, options.actor) + "\",\"decisionId\":\"" + forwarder->decision_id + "\"},\"selectableUnitIds\":[";
                std::vector<std::string> selections;
                for (int16_t slot : options.selectable) selections.push_back(Unit_Token(forwarder, slot));
                std::sort(selections.begin(), selections.end());
                for (size_t i = 0; i < selections.size(); ++i) {
                    if (i) result += ',';
                    result += "\"" + selections[i] + "\"";
                }
                result += "],\"weapons\":[";
                for (size_t i = 0; i < options.weapons.size(); ++i) {
                    const auto& weapon = options.weapons[i];
                    if (i) result += ',';
                    result += "{\"slot\":" + std::to_string(weapon.slot) + ",\"name\":" + Name_Json(weapon.name) +
                        ",\"type\":" + std::to_string(weapon.type) + ",\"count\":" + std::to_string(weapon.count) +
                        ",\"selection\":" + std::to_string(weapon.selection) + ",\"shotsAvailable\":" + std::to_string(weapon.shots) +
                        ",\"ammoOrRecharge\":" + std::to_string(weapon.ammo) + ",\"arc\":" + std::to_string(weapon.arc) +
                        ",\"modifiers\":" + std::to_string(weapon.modifiers) +
                        ",\"ready\":" + (weapon.ready ? "true" : "false") + "}";
                }
                result += "],\"preFireEffects\":{\"absorberSelected\":" + std::string(options.absorber_selected ? "true" : "false") +
                    ",\"absorberCharge\":" + std::to_string(options.absorber_charge) + "},\"movement\":{\"cells\":[";
                for (size_t i = 0; i < options.cells.size(); ++i) {
                    const auto& cell = options.cells[i];
                    if (i) result += ',';
                    result += "{\"x\":" + std::to_string(cell.x) + ",\"y\":" + std::to_string(cell.y) + ",\"nominalCost\":" + std::to_string(cell.cost) + "}";
                }
                result += "]},\"facing\":{\"available\":" + std::string(options.movement > 0 ? "true" : "false") +
                    ",\"turnCost\":" + std::to_string(options.turn_cost) + ",\"movementRemaining\":" + std::to_string(options.movement) + "},\"targets\":[";
                std::vector<std::string> targets;
                for (const auto& target : options.targets) {
                    std::string row = "{\"unitId\":\"" + Unit_Token(forwarder, target.slot) + "\",\"eligibleWeaponSlots\":[";
                    bool first = true;
                    for (unsigned i = 0; i < 8; ++i)
                        if (target.weapons & (1u << i)) {
                            if (!first) row += ',';
                            first = false;
                            row += std::to_string(i);
                        }
                    targets.push_back(row + "],\"preFireEffectEligible\":" + (target.absorber ? "true" : "false") + "}");
                }
                std::sort(targets.begin(), targets.end());
                for (size_t i = 0; i < targets.size(); ++i) {
                    if (i) result += ',';
                    result += targets[i];
                }
                result += "],\"missileTargets\":[";
                std::vector<std::string> missile_targets;
                for (const auto& target : options.missile_targets) {
                    s_missile missile;
                    if (forwarder->missile == nullptr || !forwarder->missile(target.slot, &missile)) return Unavailable(request);
                    const auto token = Missile_Token(forwarder, target.slot);
                    if (token.empty()) return Unavailable(request);
                    auto row = Missile_Row(missile, token);
                    row.pop_back();
                    row += ",\"canFire\":" + std::string(target.weapons ? "true" : "false") + ",\"eligibleWeaponSlots\":[";
                    bool first = true;
                    for (unsigned i = 0; i < 8; ++i)
                        if (target.weapons & (1u << i)) {
                            if (!first) row += ',';
                            first = false;
                            row += std::to_string(i);
                        }
                    missile_targets.push_back(row + "]}");
                }
                std::sort(missile_targets.begin(), missile_targets.end());
                for (size_t i = 0; i < missile_targets.size(); ++i) {
                    if (i) result += ',';
                    result += missile_targets[i];
                }
                result += "],\"boarding\":{\"ownMarines\":" + (options.own_marines == 255 ? std::string("null") : std::to_string(options.own_marines)) +
                    ",\"alreadyBoarded\":" + (options.already_boarded ? "true" : "false") +
                    ",\"minimumMarines\":0,\"maximumMarines\":" + std::to_string(options.maximum_marines) +
                    ",\"modes\":[\"raid\",\"capture\"],\"targetUnitIds\":[";
                std::vector<std::string> boarding_targets;
                for (int16_t slot : options.boarding_targets) boarding_targets.push_back(Unit_Token(forwarder, slot));
                std::sort(boarding_targets.begin(), boarding_targets.end());
                for (size_t i = 0; i < boarding_targets.size(); ++i) {
                    if (i) result += ',';
                    result += "\"" + boarding_targets[i] + "\"";
                }
                result += "]},\"retreat\":{\"available\":" + std::string(options.retreat_refusal == 0 ? "true" : "false") +
                    ",\"reason\":" + (options.retreat_refusal == 0 ? "null" : options.retreat_refusal == 79 ? "\"noSpeed\""
                                                                                                            : "\"retreatPrevented\"") +
                    "},\"canDone\":true}";
            }
        } else if (request->method == READ_UNIT) {
            s_unit unit;
            bool found = false;
            for (const auto& reference : forwarder->references) {
                if (request->unit_id != reference.token.data()) continue;
                found = forwarder->reader.unit != nullptr && forwarder->reader.unit(reference.slot, &unit);
                break;
            }
            if (!found) return Unavailable(request);
            result = "{\"battleId\":\"" + std::string(forwarder->battle_id) + "\",\"unit\":" + Row(&unit, request->unit_id, true) + "}";
            *dependent = true;
        } else {
            s_view view;
            if (forwarder->reader.view == nullptr || !forwarder->reader.view(&view)) return Unavailable(request);
            std::vector<s_row> rows;
            std::string selected;
            for (const auto& unit : view.units) {
                const auto token = Tactical_Unit_Token(forwarder, &unit);
                if (token.empty()) return Unavailable(request);
                rows.push_back({token, Row(&unit, token, false)});
                if (unit.slot == view.selected) selected = token;
            }
            std::sort(rows.begin(), rows.end(), Row_Precedes);
            result = "{\"battle\":{\"battleId\":\"" + std::string(forwarder->battle_id) + "\",\"localSide\":\"" + (view.local_attacker ? "attacker" : "defender") +
                "\",\"round\":" + std::to_string(view.round) + ",\"initiative\":" + (view.initiative ? "true" : "false") +
                ",\"grid\":{\"width\":" + std::to_string(view.width) + ",\"height\":" + std::to_string(view.height) +
                "},\"selectedUnitId\":" + (selected.empty() ? "null" : "\"" + selected + "\"") +
                ",\"visibleUnitCount\":" + std::to_string(rows.size()) + ",\"units\":[";
            for (size_t i = 0; i < rows.size(); ++i) {
                if (i != 0) result += ',';
                result += rows[i].json;
            }
            result += "]}}";
            *dependent = true;
        }
        return "{\"jsonrpc\":\"2.0\",\"id\":" + request->encoded_id + ",\"result\":" + result + "}";
    }
} // namespace rpc
