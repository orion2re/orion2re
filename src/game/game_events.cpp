#include "game_events.h"

#include <limits>

namespace game_events {
    namespace {
        struct s_listener {
            uint64_t id = 0;
            t_callback callback = nullptr;
            void* context = nullptr;
            t_fleet_box_callback fleet_box_callback = nullptr;
            t_ship_identity_callback ship_identity_callback = nullptr;
            t_ship_destination_callback ship_destination_callback = nullptr;
        };
        s_listener listeners[8];
        uint64_t next_id = 0;
        uint32_t count = 0;
        s_leader_view_scope* leader_owner = nullptr;
        const s_leader_view* leader_view = nullptr;
        uint32_t leader_interruptions = 0;
    } // namespace

    uint64_t Subscribe(t_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (s_listener& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, callback, context};
                ++count;
                return listener.id;
            }
        }
        return 0;
    }

    void Unsubscribe(uint64_t subscription) {
        if (subscription == 0) return;
        for (s_listener& listener : listeners) {
            if (listener.id == subscription) {
                listener = {};
                --count;
                return;
            }
        }
    }

    uint64_t Subscribe_Fleet_Box_Info(t_fleet_box_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (s_listener& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, nullptr, context, callback};
                ++count;
                return listener.id;
            }
        }
        return 0;
    }

    uint64_t Subscribe_Ship_Identity(t_ship_identity_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (s_listener& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, nullptr, context, nullptr, callback};
                ++count;
                return listener.id;
            }
        }
        return 0;
    }

    uint64_t Subscribe_Ship_Destination(t_ship_destination_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (s_listener& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, nullptr, context, nullptr, nullptr, callback};
                ++count;
                return listener.id;
            }
        }
        return 0;
    }

    static void Deliver(e_event event, const s_fleet_box_info* info, int16_t ship = -1, const s_ship_destination_line* line = nullptr) {
        if (count == 0) return;
        uint64_t selected[8];
        for (uint32_t index = 0; index < 8; ++index) selected[index] = listeners[index].id;
        for (uint64_t id : selected) {
            if (id == 0) continue;
            for (const s_listener& listener : listeners) {
                if (listener.id == id) {
                    const s_listener call = listener;
                    if (call.callback != nullptr) call.callback(event, call.context);
                    if (call.fleet_box_callback != nullptr && info != nullptr) call.fleet_box_callback(info, call.context);
                    if (call.ship_identity_callback != nullptr && ship >= 0) call.ship_identity_callback(ship, call.context);
                    if (call.ship_destination_callback != nullptr && line != nullptr) call.ship_destination_callback(line, call.context);
                    break;
                }
            }
        }
    }

    void Publish(e_event event) {
        if (event == WORLD_REPLACE_BEGIN || event == WORLD_UPDATE_BEGIN || event == RESOLUTION_BEGIN || event == PERSPECTIVE_SELECTION_BEGIN || event == MENU_ENTERED || event == THREAD_STOPPING) {
            for (auto* owner = leader_owner; owner != nullptr; owner = owner->parent) {
                owner->ready = false;
                owner->revoked = true;
            }
        }
        Deliver(event, nullptr);
    }

    void Publish_Fleet_Box_Info(const s_fleet_box_info* info) {
        if (info != nullptr) Deliver(FLEET_BOX_INFO_READY, info);
    }

    void Publish_Ship_Identity_Changing(int16_t ship) {
        Deliver(SHIP_IDENTITIES_CHANGING, nullptr, ship);
    }

    void Publish_Ship_Destination(const s_ship_destination_line* line) {
        if (line != nullptr) Deliver(SHIP_DESTINATION_LINE_DRAWN, nullptr, -1, line);
    }

    s_observation_scope::s_observation_scope(e_event entered, e_event exit) : leaving(exit) {
        Publish(entered);
    }

    const s_leader_view* Published_Leader_View() {
        return leader_view;
    }

    s_leader_view_scope::s_leader_view_scope(leader_read::e_kind type, const char* message, const leader_read::s_leader* disclosed_name)
        : kind(type), text(message), name(disclosed_name), parent(leader_owner) {
        if (parent != nullptr) parent->ready = false;
        leader_owner = this;
        Publish(LEADER_VIEW_CLEARED);
    }

    void s_leader_view_scope::Close() {
        if (closed) return;
        closed = true;
        ready = false;
        Publish(LEADER_VIEW_CLEARED);
        leader_owner = parent;
    }

    s_leader_view_scope::~s_leader_view_scope() {
        Close();
    }

    void Ready_Leader_View(leader_read::e_kind kind) {
        if (leader_owner != nullptr && !leader_owner->revoked && leader_owner->kind == kind && leader_interruptions == 0) leader_owner->ready = true;
    }

    void Publish_Leader_View(leader_read::e_kind kind, const leader_read::s_leader* leaders, int16_t count) {
        if (leader_owner == nullptr || !leader_owner->ready || leader_owner->kind != kind || leader_interruptions != 0 || count < 0 || count > 2) return;
        const s_leader_view view{leaders, count};
        leader_view = &view;
        Publish(LEADER_VIEW_READY);
        leader_view = nullptr;
    }

    void Present_Leader_Text(const char* text) {
        if (leader_owner == nullptr || leader_owner->kind != leader_read::MESSAGE_NAME || leader_owner->text != text || leader_owner->name == nullptr) return;
        Ready_Leader_View(leader_read::MESSAGE_NAME);
        Publish_Leader_View(leader_read::MESSAGE_NAME, leader_owner->name, 1);
    }

    void Close_Leader_Text(const char* text) {
        if (leader_owner != nullptr && leader_owner->kind == leader_read::MESSAGE_NAME && leader_owner->text == text) leader_owner->Close();
    }

    s_leader_interruption_scope::s_leader_interruption_scope(const char* own_text) {
        if (leader_owner == nullptr || (own_text != nullptr && own_text == leader_owner->text && !leader_owner->ready)) return;
        interrupted = true;
        ++leader_interruptions;
        leader_owner->ready = false;
        Publish(LEADER_VIEW_CLEARED);
    }

    s_leader_interruption_scope::~s_leader_interruption_scope() {
        if (interrupted) --leader_interruptions;
    }

    s_observation_scope::~s_observation_scope() {
        Publish(leaving);
    }

    s_fleet_box_view_scope::s_fleet_box_view_scope() {
        Publish(FLEET_BOX_VIEW_ENTERED);
        Publish(FLEET_BOX_INFO_CLEARED);
    }

    s_fleet_box_view_scope::~s_fleet_box_view_scope() {
        Publish(FLEET_BOX_VIEW_LEAVING);
        Publish(FLEET_BOX_INFO_CLEARED);
    }

    void s_fleet_box_view_scope::Initialized() {
        if (initialized) return;
        initialized = true;
        Publish(FLEET_BOX_VIEW_INITIALIZED);
    }

    s_main_view_input_scope::s_main_view_input_scope() {
        Publish(MAIN_VIEW_INPUT_ENTERED);
    }

    s_main_view_input_scope::~s_main_view_input_scope() {
        Publish(MAIN_VIEW_INPUT_LEAVING);
    }

    s_world_update_scope::s_world_update_scope(bool new_world) : replacement(new_world) {
        Publish(replacement ? WORLD_REPLACE_BEGIN : WORLD_UPDATE_BEGIN);
    }

    s_world_update_scope::~s_world_update_scope() {
        Publish(replacement ? (committed ? WORLD_REPLACE_COMMITTED : WORLD_REPLACE_FAILED) : (committed ? WORLD_UPDATE_COMMITTED : WORLD_UPDATE_FAILED));
    }

    void s_world_update_scope::Commit() {
        committed = true;
    }

    s_resolution_scope::s_resolution_scope() {
        Publish(RESOLUTION_BEGIN);
    }

    s_resolution_scope::~s_resolution_scope() {
        Publish(RESOLUTION_END);
    }
} // namespace game_events
