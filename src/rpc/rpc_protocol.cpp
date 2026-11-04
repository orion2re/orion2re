#include "rpc_protocol.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <set>
#include <utility>

#include "yyjson.h"

namespace rpc {
    namespace {
        bool Is_Text(yyjson_val* value, std::string_view text) {
            return yyjson_is_str(value) && yyjson_get_len(value) == text.size() &&
                std::memcmp(yyjson_get_str(value), text.data(), text.size()) == 0;
        }

        bool Depth_Allowed(std::string_view input) {
            unsigned depth = 0;
            bool quoted = false, escaped = false;
            for (char c : input) {
                if (quoted) {
                    if (escaped) {
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        quoted = false;
                    }
                } else if (c == '"') {
                    quoted = true;
                } else if (c == '[' || c == '{') {
                    if (++depth > 32) return false;
                } else if ((c == ']' || c == '}') && depth != 0) {
                    --depth;
                }
            }
            return true;
        }

        bool Unique_Keys(yyjson_val* value) {
            if (yyjson_is_obj(value)) {
                std::set<std::string_view> keys;
                auto iter = yyjson_obj_iter_with(value);
                while (auto* key = yyjson_obj_iter_next(&iter)) {
                    if (!keys.emplace(yyjson_get_str(key), yyjson_get_len(key)).second ||
                        !Unique_Keys(yyjson_obj_iter_get_val(key))) return false;
                }
            } else if (yyjson_is_arr(value)) {
                size_t index, count;
                yyjson_val* item;
                yyjson_arr_foreach(value, index, count, item) {
                    if (!Unique_Keys(item)) return false;
                }
            }
            return true;
        }

        bool Only_Keys(yyjson_val* value, std::initializer_list<std::string_view> names) {
            if (!yyjson_is_obj(value)) return false;
            auto iter = yyjson_obj_iter_with(value);
            while (auto* key = yyjson_obj_iter_next(&iter)) {
                bool found = false;
                for (auto name : names) found |= Is_Text(key, name);
                if (!found) return false;
            }
            return true;
        }

        std::string Encode(yyjson_val* value) {
            size_t size = 0;
            char* text = yyjson_val_write(value, YYJSON_WRITE_NOFLAG, &size);
            if (text == nullptr) return {};
            std::string result(text, size);
            std::free(text);
            return result;
        }

        bool Valid_Id(yyjson_val* id) {
            if (yyjson_is_str(id)) return yyjson_get_len(id) > 0 && yyjson_get_len(id) <= 64;
            if (yyjson_is_uint(id)) return yyjson_get_uint(id) <= 9007199254740991ULL;
            return yyjson_is_sint(id) && yyjson_get_sint(id) >= -9007199254740991LL &&
                yyjson_get_sint(id) <= 9007199254740991LL;
        }

        bool Keys_Match(std::string_view configured, yyjson_val* supplied) {
            if (configured.empty() || !yyjson_is_str(supplied) || configured.size() != yyjson_get_len(supplied)) return false;
            unsigned char different = 0;
            for (size_t i = 0; i < configured.size(); ++i)
                different |= (unsigned char)configured[i] ^ (unsigned char)yyjson_get_str(supplied)[i];
            return different == 0;
        }

        struct s_method {
            const char* name;
        };
        constexpr s_method methods[] = {{"rpc.hello"}, {"rpc.describe"}, {"rpc.subscribe"}, {"rpc.unsubscribe"}, {"combat.getView"}, {"combat.getUnit"}, {"combat.getTargetOptions"}, {"combat.selectUnit"}, {"combat.move"}, {"combat.face"}, {"combat.fire"}, {"combat.done"}, {"combat.cancelOperation"}, {"combat.setFireSelection"}, {"combat.listMissiles"}, {"combat.getMissile"}, {"combat.getHitHistory"}};

        bool Reference(yyjson_val* value) {
            if (!yyjson_is_str(value) || yyjson_get_len(value) != 32) return false;
            for (size_t i = 0; i < 32; ++i) {
                const char c = yyjson_get_str(value)[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
            }
            return true;
        }

        bool Read_Events(yyjson_val* params, uint8_t* mask) {
            if (!Only_Keys(params, {"events"})) return false;
            auto* events = yyjson_obj_get(params, "events");
            if (!yyjson_is_arr(events) || yyjson_arr_size(events) > 5) return false;
            *mask = 0;
            size_t index, count;
            yyjson_val* event;
            yyjson_arr_foreach(events, index, count, event) {
                uint8_t bit = 0;
                if (Is_Text(event, "combat.opened"))
                    bit = EVENT_COMBAT_OPENED;
                else if (Is_Text(event, "combat.closed"))
                    bit = EVENT_COMBAT_CLOSED;
                else if (Is_Text(event, "combat.viewInvalidated"))
                    bit = EVENT_COMBAT_INVALIDATED;
                else if (Is_Text(event, "combat.unitSelected"))
                    bit = EVENT_COMBAT_SELECTED;
                else if (Is_Text(event, "combat.localAttentionChanged"))
                    bit = EVENT_COMBAT_ATTENTION | EVENT_COMBAT_ATTENTION_CLEARED;
                if (bit == 0 || (*mask & bit) != 0) return false;
                *mask |= bit;
            }
            return true;
        }

        std::string Events_Json(uint8_t mask) {
            std::string result = "[";
            if (mask & EVENT_COMBAT_OPENED) result += "\"combat.opened\"";
            if (mask & EVENT_COMBAT_CLOSED) {
                if (mask & EVENT_COMBAT_OPENED) result += ',';
                result += "\"combat.closed\"";
            }
            for (const auto& item : {std::pair{EVENT_COMBAT_INVALIDATED, "combat.viewInvalidated"}, std::pair{EVENT_COMBAT_SELECTED, "combat.unitSelected"}}) {
                if (!(mask & item.first)) continue;
                if (result.size() > 1) result += ',';
                result += "\"" + std::string(item.second) + "\"";
            }
            if (mask & (EVENT_COMBAT_ATTENTION | EVENT_COMBAT_ATTENTION_CLEARED)) {
                if (result.size() > 1) result += ',';
                result += "\"combat.localAttentionChanged\"";
            }
            return result + "]";
        }
    } // namespace

    std::string Error_Response(std::string_view id, int code, const char* message) {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + std::string(id) + ",\"error\":{\"code\":" +
            std::to_string(code) + ",\"message\":\"" + message + "\"}}";
    }

    static std::string Error(std::string_view id, int code, const char* message) {
        return Error_Response(id, code, message);
    }

    bool Process_Request(std::string_view input, std::string_view key, bool control_available, s_session* session, std::string* response, bool* selection_updated, s_read_request* read, s_operation_request* operation) {
        if (operation != nullptr) *operation = {};
        if (read != nullptr) *read = {};
        if (selection_updated != nullptr) *selection_updated = false;
        if (session == nullptr || response == nullptr) return false;
        response->clear();
        if (input.size() > MAX_INPUT_BYTES || !Depth_Allowed(input)) {
            *response = Error("null", -32005, "Limit exceeded");
            return true;
        }
        auto* doc = yyjson_read(input.data(), input.size(), YYJSON_READ_NOFLAG);
        if (doc == nullptr) {
            *response = Error("null", -32700, "Parse error");
            return true;
        }
        auto* root = yyjson_doc_get_root(doc);
        auto* id = yyjson_obj_get(root, "id");
        auto* method = yyjson_obj_get(root, "method");
        auto* params = yyjson_obj_get(root, "params");
        const bool notification = id == nullptr;
        std::string encoded_id = Valid_Id(id) ? Encode(id) : "null";
        if (!Unique_Keys(root) || !Only_Keys(root, {"jsonrpc", "id", "method", "params"}) ||
            !Is_Text(yyjson_obj_get(root, "jsonrpc"), "2.0") || !yyjson_is_str(method) ||
            yyjson_get_len(method) == 0 || yyjson_get_len(method) > 128 || (!notification && !Valid_Id(id)) ||
            (params != nullptr && !yyjson_is_obj(params) && !yyjson_is_arr(params))) {
            *response = Error("null", -32600, "Invalid Request");
        } else if (notification) {
            // Notifications are not an authentication or mutation channel.
        } else if (!session->authenticated && !Is_Text(method, "rpc.hello")) {
            *response = Error(encoded_id, -32001, "Unauthorized");
        } else if (Is_Text(method, "rpc.hello")) {
            auto* role = yyjson_obj_get(params, "requestedRole");
            auto* supplied = yyjson_obj_get(params, "key");
            const bool controller = Is_Text(role, "controller");
            if (!Only_Keys(params, {"key", "requestedRole"}) || !yyjson_is_str(supplied) ||
                yyjson_get_len(supplied) == 0 || yyjson_get_len(supplied) > 384 ||
                (role != nullptr && !controller && !Is_Text(role, "reader"))) {
                *response = Error(encoded_id, -32602, "Invalid params");
            } else if (!Keys_Match(key, supplied)) {
                session->close_after_output = true;
                *response = Error(encoded_id, -32001, "Unauthorized");
            } else if (session->authenticated && session->controller != controller) {
                *response = Error(encoded_id, -32602, "Invalid params");
            } else if (controller && !session->controller && !control_available) {
                *response = Error(encoded_id, -32007, "Control required");
            } else {
                session->authenticated = true;
                session->controller = controller;
                *response = "{\"jsonrpc\":\"2.0\",\"id\":" + encoded_id +
                    ",\"result\":{\"contractVersion\":\"orion2.rpc/0\",\"build\":\"orion2re\",\"role\":\"" +
                    (controller ? "controller" : "reader") + "\"}}";
            }
        } else if (Is_Text(method, "rpc.describe")) {
            if (params != nullptr && (!yyjson_is_obj(params) || yyjson_obj_size(params) != 0)) {
                *response = Error(encoded_id, -32602, "Invalid params");
            } else {
                std::string list;
                for (const auto& item : methods) {
                    if (!list.empty()) list += ',';
                    list += "{\"name\":\"" + std::string(item.name) + "\"}";
                }
                *response = "{\"jsonrpc\":\"2.0\",\"id\":" + encoded_id +
                    ",\"result\":{\"contractVersion\":\"orion2.rpc/0\",\"methods\":[" + list + "],\"events\":" + Events_Json(63) +
                    ",\"limits\":{\"inputBytes\":" + std::to_string(MAX_INPUT_BYTES) + ",\"responseBytes\":" + std::to_string(MAX_RESPONSE_BYTES) +
                    ",\"queuedOutputBytes\":" + std::to_string(MAX_OUTPUT_BYTES) + ",\"readDeadlineMs\":2000,\"outstandingReads\":2,\"operationDispatchDeadlineMs\":2000,\"outstandingOperations\":1}}}";
            }
        } else if (Is_Text(method, "rpc.subscribe") || Is_Text(method, "rpc.unsubscribe")) {
            uint8_t selected = 0;
            const bool subscribe = Is_Text(method, "rpc.subscribe");
            const bool valid = subscribe ? Read_Events(params, &selected) : (params == nullptr || (yyjson_is_obj(params) && yyjson_obj_size(params) == 0));
            if (!valid) {
                *response = Error(encoded_id, -32602, "Invalid params");
            } else {
                session->events = selected;
                if (selection_updated != nullptr) *selection_updated = true;
                *response = "{\"jsonrpc\":\"2.0\",\"id\":" + encoded_id + ",\"result\":{\"events\":" + Events_Json(selected) + "}}";
            }
        } else if (Is_Text(method, "combat.getView") || Is_Text(method, "combat.getUnit") || Is_Text(method, "combat.getTargetOptions") ||
            Is_Text(method, "combat.listMissiles") || Is_Text(method, "combat.getMissile") || Is_Text(method, "combat.getHitHistory")) {
            const bool unit = Is_Text(method, "combat.getUnit");
            const bool options = Is_Text(method, "combat.getTargetOptions");
            const bool missile = Is_Text(method, "combat.getMissile");
            auto* battle_id = yyjson_obj_get(params, "battleId");
            auto* unit_id = yyjson_obj_get(params, "unitId");
            auto* missile_id = yyjson_obj_get(params, "missileId");
            const bool valid = unit ? Only_Keys(params, {"battleId", "unitId"}) && Reference(battle_id) && Reference(unit_id) : missile ? Only_Keys(params, {"battleId", "missileId"}) && Reference(battle_id) && Reference(missile_id)
                                                                                                                                        : (params == nullptr || (Only_Keys(params, {"battleId"}) && (battle_id == nullptr || Reference(battle_id))));
            if (!valid) {
                *response = Error(encoded_id, -32602, "Invalid params");
            } else if ((session->events & 7) != 7) {
                *response = Error(encoded_id, -32008, "Tactical subscription required");
            } else if (read == nullptr) {
                *response = Error(encoded_id, -32004, "Unavailable");
            } else {
                read->method = options ? READ_OPTIONS : READ_VIEW;
                if (unit) read->method = READ_UNIT;
                if (missile) read->method = READ_MISSILE;
                if (Is_Text(method, "combat.listMissiles")) read->method = READ_MISSILES;
                if (Is_Text(method, "combat.getHitHistory")) read->method = READ_HITS;
                read->encoded_id = encoded_id;
                if (battle_id != nullptr) read->battle_id = yyjson_get_str(battle_id);
                if (unit_id != nullptr) read->unit_id = yyjson_get_str(unit_id);
                if (missile_id != nullptr) read->missile_id = yyjson_get_str(missile_id);
            }
        } else if (Is_Text(method, "combat.selectUnit") || Is_Text(method, "combat.move") || Is_Text(method, "combat.face") ||
            Is_Text(method, "combat.fire") || Is_Text(method, "combat.done") || Is_Text(method, "combat.cancelOperation") || Is_Text(method, "combat.setFireSelection")) {
            const bool cancel = Is_Text(method, "combat.cancelOperation");
            const bool select = Is_Text(method, "combat.selectUnit");
            const bool fire = Is_Text(method, "combat.fire");
            const bool move = Is_Text(method, "combat.move");
            const bool face = Is_Text(method, "combat.face");
            const bool selection = Is_Text(method, "combat.setFireSelection");
            auto* sequence = yyjson_obj_get(params, "operationSeq");
            auto* battle = yyjson_obj_get(params, "battleId");
            auto* actor = yyjson_obj_get(params, "actorUnitId");
            auto* decision = yyjson_obj_get(params, "decisionId");
            auto* target = yyjson_obj_get(params, select ? "unitId" : "targetUnitId");
            auto* missile_target = yyjson_obj_get(params, "targetMissileId");
            auto* banks = yyjson_obj_get(params, "banks");
            uint8_t bank_mask = 0;
            int8_t bank_states[8] = {};
            auto* point = yyjson_obj_get(params, move ? "destination" : "toward");
            auto* x = yyjson_obj_get(point, "x");
            auto* y = yyjson_obj_get(point, "y");
            const bool valid_sequence = yyjson_is_uint(sequence) && yyjson_get_uint(sequence) > 0 && yyjson_get_uint(sequence) <= 9007199254740991ULL;
            bool valid = cancel ? Only_Keys(params, {"operationSeq"}) : Only_Keys(params, {"battleId", "actorUnitId", "decisionId", "operationSeq", "unitId", "targetUnitId", "targetMissileId", "destination", "toward", "banks"}) && Reference(battle) && Reference(actor) && Reference(decision);
            if (!cancel) {
                valid = valid && yyjson_obj_size(params) == size_t(select || fire || move || face || selection ? 5 : 4);
                if (selection) {
                    valid = valid && yyjson_is_arr(banks) && yyjson_arr_size(banks) > 0 && yyjson_arr_size(banks) <= 8;
                    size_t index, count;
                    yyjson_val* bank;
                    yyjson_arr_foreach(banks, index, count, bank) {
                        auto* slot = yyjson_obj_get(bank, "slot");
                        auto* state = yyjson_obj_get(bank, "state");
                        if (!Only_Keys(bank, {"slot", "state"}) || yyjson_obj_size(bank) != 2 || !yyjson_is_uint(slot) || yyjson_get_uint(slot) >= 8 ||
                            !(Is_Text(state, "on") || Is_Text(state, "temporaryOff") || Is_Text(state, "off"))) {
                            valid = false;
                            break;
                        }
                        const unsigned i = unsigned(yyjson_get_uint(slot));
                        if (bank_mask & (1u << i)) {
                            valid = false;
                            break;
                        }
                        bank_mask |= uint8_t(1u << i);
                        bank_states[i] = Is_Text(state, "on") ? 1 : Is_Text(state, "off") ? -1
                                                                                          : 0;
                    }
                } else if (fire)
                    valid = valid && ((target != nullptr && missile_target == nullptr && Reference(target)) || (target == nullptr && Reference(missile_target)));
                else if (select)
                    valid = valid && Reference(target);
                else if (move || face)
                    valid = valid && Only_Keys(point, {"x", "y"}) && yyjson_obj_size(point) == 2 &&
                        yyjson_is_int(x) && yyjson_is_int(y) && yyjson_get_sint(x) >= 0 && yyjson_get_sint(x) <= INT16_MAX &&
                        yyjson_get_sint(y) >= 0 && yyjson_get_sint(y) <= INT16_MAX;
            }
            if (!valid || !valid_sequence)
                *response = Error(encoded_id, -32602, "Invalid params");
            else if (!session->controller)
                *response = Error(encoded_id, -32007, "Controller required");
            else if ((session->events & 7) != 7)
                *response = Error(encoded_id, -32008, "Tactical subscription required");
            else if (!cancel && yyjson_get_uint(sequence) <= session->operation_sequence)
                *response = Error(encoded_id, -32012, "Duplicate operation");
            else if (operation == nullptr)
                *response = Error(encoded_id, -32004, "Unavailable");
            else {
                operation->method = OP_DONE;
                if (cancel) operation->method = OP_CANCEL;
                if (select) operation->method = OP_SELECT;
                if (move) operation->method = OP_MOVE;
                if (face) operation->method = OP_FACE;
                if (fire) operation->method = OP_FIRE;
                if (selection) operation->method = OP_FIRE_SELECTION;
                operation->request.encoded_id = encoded_id;
                operation->sequence = yyjson_get_uint(sequence);
                if (!cancel) {
                    session->operation_sequence = operation->sequence;
                    operation->request.battle_id = yyjson_get_str(battle);
                    operation->actor_id = yyjson_get_str(actor);
                    operation->decision_id = yyjson_get_str(decision);
                    if (select || fire) operation->target_id = yyjson_get_str(missile_target != nullptr && fire ? missile_target : target);
                    operation->missile_target = fire && missile_target != nullptr;
                    operation->bank_mask = bank_mask;
                    for (unsigned i = 0; i < 8; ++i) operation->bank_states[i] = bank_states[i];
                    if (move || face) {
                        operation->x = (int16_t)yyjson_get_sint(x);
                        operation->y = (int16_t)yyjson_get_sint(y);
                    }
                }
            }
        } else {
            *response = Error(encoded_id, -32601, "Method not found");
        }
        yyjson_doc_free(doc);
        return !encoded_id.empty();
    }
} // namespace rpc
