#include "tactical_reads.h"
#include "config/text_encoding.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <uv.h>
#include "yyjson.h"

namespace rpc {
    std::string Tactical_Unit_Token(s_tactical_forwarder* forwarder, const tactical_view::s_unit* unit) {
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

        std::string Row(const tactical_view::s_unit* unit, const std::string& token, bool detail) {
            const char* kinds[] = {"ship", "planet", "satellite", "monster", "contact"};
            const char* sizes[] = {"small", "medium", "large", "huge"};
            std::string json = "{\"unitId\":\"" + token + "\",\"disclosure\":\"" + (unit->contact ? "contact" : "unit") +
                "\",\"kind\":\"" + kinds[unit->kind] + "\",\"side\":\"" + (unit->attacker ? "attacker" : "defender") +
                "\",\"owned\":" + (unit->owned ? "true" : "false") + ",\"position\":{\"x\":" + std::to_string(unit->x) +
                ",\"y\":" + std::to_string(unit->y) + "}";
            if (unit->kind != tactical_view::PLANET) json += ",\"mapSize\":\"" + std::string(sizes[unit->map_size]) + "\"";
            if (!unit->contact) {
                if (unit->kind != tactical_view::PLANET) json += ",\"facing\":" + std::to_string(unit->facing);
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

    std::string Execute_Tactical_Read(const s_read_request* request, void* context, bool* dependent) {
        auto* forwarder = (s_tactical_forwarder*)context;
        const auto unavailable = [&] { return Error_Response(request->encoded_id, -32004, "Unavailable"); };
        const bool active = forwarder->battle_id[0] != '\0' && !forwarder->retiring;
        if (!request->battle_id.empty() && (!active || request->battle_id != forwarder->battle_id)) return unavailable();
        std::string result;
        if (!active) {
            if (request->method != READ_VIEW) return unavailable();
            result = "{\"battle\":null}";
        } else if (request->method == READ_UNIT) {
            tactical_view::s_unit unit;
            bool found = false;
            for (const auto& reference : forwarder->references) {
                if (request->unit_id != reference.token.data()) continue;
                found = forwarder->reader.unit != nullptr && forwarder->reader.unit(reference.slot, &unit);
                break;
            }
            if (!found) return unavailable();
            result = "{\"battleId\":\"" + std::string(forwarder->battle_id) + "\",\"unit\":" + Row(&unit, request->unit_id, true) + "}";
            *dependent = true;
        } else {
            tactical_view::s_view view;
            if (forwarder->reader.view == nullptr || !forwarder->reader.view(&view)) return unavailable();
            struct s_row {
                std::string token, json;
            };
            std::vector<s_row> rows;
            std::string selected;
            for (const auto& unit : view.units) {
                const auto token = Tactical_Unit_Token(forwarder, &unit);
                if (token.empty()) return unavailable();
                rows.push_back({token, Row(&unit, token, false)});
                if (unit.slot == view.selected) selected = token;
            }
            std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.token < b.token; });
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
