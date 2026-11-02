#include "game_events.h"

#include <limits>
#include <cstring>
#include <cstdio>

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
        s_relations_view_scope* relations_owner = nullptr;
        const s_relations_view* relations_view = nullptr;
        uint32_t relations_interruptions = 0;
        int16_t message_recipient = -1;
        int16_t replaced_colony_owner = -1;
        int16_t replaced_colony_planet = -1;
        s_settlement_view_scope* settlement_owner = nullptr;
        s_settlement_frame* settlement_frame = nullptr;
        const s_settlement_view* settlement_view = nullptr;
        uint32_t settlement_interruptions = 0;
        s_settlement_help_scope* settlement_help = nullptr;
        bool population_staged = false;
        bool queue_staged = false;
        s_colony_queue_scope* queue_owner = nullptr;
        s_colony_player_scope* colony_player = nullptr;
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
        if (event == MAIN_VIEW_INPUT_INTERRUPTED) Invalidate_Settlement_View();
        if (event == WORLD_REPLACE_BEGIN || event == PERSPECTIVE_SELECTION_BEGIN || event == MENU_ENTERED || event == THREAD_STOPPING) {
            for (auto* owner = queue_owner; owner != nullptr; owner = owner->parent) owner->revoked = true;
            for (auto* owner = colony_player; owner != nullptr; owner = owner->parent) owner->revoked = true;
            population_staged = false;
            queue_staged = false;
        }
        if (event == WORLD_REPLACE_BEGIN || event == WORLD_UPDATE_BEGIN || event == RESOLUTION_BEGIN || event == PERSPECTIVE_SELECTION_BEGIN || event == MENU_ENTERED || event == THREAD_STOPPING || event == STRATEGIC_LABELS_REPLACING) {
            for (auto* owner = settlement_owner; owner != nullptr; owner = owner->parent) {
                owner->ready = false;
                if (event != WORLD_UPDATE_BEGIN && event != RESOLUTION_BEGIN) owner->revoked = true;
            }
        }
        if (event == WORLD_REPLACE_BEGIN || event == WORLD_UPDATE_BEGIN || event == RESOLUTION_BEGIN || event == PERSPECTIVE_SELECTION_BEGIN || event == MENU_ENTERED || event == THREAD_STOPPING) {
            for (auto* owner = leader_owner; owner != nullptr; owner = owner->parent) {
                owner->ready = false;
                owner->revoked = true;
            }
            for (auto* owner = relations_owner; owner != nullptr; owner = owner->parent) {
                owner->ready = false;
                owner->revoked = true;
            }
        }
        Deliver(event, nullptr);
    }

    void Publish_Fleet_Box_Info(const s_fleet_box_info* info) {
        if (info != nullptr) Deliver(FLEET_BOX_INFO_READY, info);
    }

    void Publish_Messages_Replacing(int16_t recipient) {
        const int16_t previous = message_recipient;
        message_recipient = recipient;
        Publish(MESSAGES_REPLACING);
        message_recipient = previous;
    }

    int16_t Message_Recipient() {
        return message_recipient;
    }

    void Publish_Colony_Replacing(int16_t owner, int16_t planet) {
        for (auto* frame = settlement_frame; frame != nullptr; frame = frame->parent)
            for (int16_t row = 0; row < frame->count; ++row)
                if (frame->items[row].planet == planet && frame->owner != nullptr) frame->owner->ready = false;
        const auto previous = replaced_colony_owner;
        const auto previous_planet = replaced_colony_planet;
        replaced_colony_owner = owner;
        replaced_colony_planet = planet;
        Publish(COLONY_REPLACING);
        replaced_colony_owner = previous;
        replaced_colony_planet = previous_planet;
    }

    int16_t Replaced_Colony_Planet() {
        return replaced_colony_planet;
    }

    const s_settlement_view* Published_Settlement_View() {
        return settlement_view;
    }
    void Invalidate_Settlement_View() {
        if (settlement_owner == nullptr) return;
        settlement_owner->ready = false;
        Publish(SETTLEMENT_VIEW_CLEARED);
    }

    s_settlement_view_scope::s_settlement_view_scope(int16_t player) : viewer(player), interruption_level(settlement_interruptions), parent(settlement_owner) {
        if (parent != nullptr) parent->ready = false;
        settlement_owner = this;
        if (parent != nullptr) Publish(SETTLEMENT_VIEW_CLEARED);
    }
    void s_settlement_view_scope::Initialized() {
        if (viewer >= 0 && !closed && !revoked && settlement_interruptions == interruption_level) ready = true;
    }
    void s_settlement_view_scope::Close() {
        if (closed) return;
        closed = true;
        ready = false;
        Publish(SETTLEMENT_VIEW_CLEARED);
        settlement_owner = parent;
    }
    s_settlement_view_scope::~s_settlement_view_scope() {
        Close();
    }
    s_settlement_frame::s_settlement_frame() : parent(settlement_frame), owner(settlement_owner) {
        settlement_frame = this;
    }
    s_settlement_frame::~s_settlement_frame() {
        settlement_frame = parent;
        if (owner == nullptr || owner != settlement_owner || !owner->ready || owner->revoked || settlement_interruptions != owner->interruption_level) return;
        const s_settlement_view view{owner->viewer, items, count};
        const auto* previous = settlement_view;
        settlement_view = &view;
        Publish(SETTLEMENT_VIEW_READY);
        settlement_view = previous;
    }
    void Observe_Settlement_Text(int16_t planet, const char* source, const char* text, bool defenses, bool owner_disclosed) {
        auto* frame = settlement_frame;
        if (frame == nullptr || frame->owner == nullptr || frame->owner != settlement_owner || !frame->owner->ready ||
            frame->owner->revoked || settlement_interruptions != frame->owner->interruption_level || text == nullptr || source == nullptr) return;
        int16_t row = 0;
        while (row < frame->count && (frame->items[row].planet != planet || strcmp(frame->items[row].source, source) != 0)) ++row;
        if (row == 8) return;
        if (row == frame->count) ++frame->count;
        auto& item = frame->items[row];
        item.planet = planet;
        item.source = source;
        snprintf(item.text, sizeof(item.text), "%s", text);
        item.defenses = defenses;
        item.owner_disclosed = owner_disclosed;
    }
    s_settlement_interruption_scope::s_settlement_interruption_scope() {
        ++settlement_interruptions;
        if (settlement_owner != nullptr) settlement_owner->ready = false;
        Publish(SETTLEMENT_VIEW_CLEARED);
    }
    s_settlement_interruption_scope::~s_settlement_interruption_scope() {
        --settlement_interruptions;
    }
    s_settlement_help_scope::s_settlement_help_scope(int16_t player, int16_t location, const char* presented, bool identified)
        : viewer(player), planet(location), text(presented), owner_disclosed(identified), parent(settlement_help) {
        settlement_help = this;
    }
    s_settlement_help_scope::~s_settlement_help_scope() {
        settlement_help = parent;
    }
    int16_t Settlement_Help_Viewer(const char* text) {
        return settlement_help != nullptr && settlement_help->text == text ? settlement_help->viewer : -1;
    }
    void Present_Settlement_Help(const char* text) {
        if (Settlement_Help_Viewer(text) < 0) return;
        s_settlement_frame frame;
        Observe_Settlement_Text(settlement_help->planet, "colony-summary", text, false, settlement_help->owner_disclosed);
    }

    int16_t Replaced_Colony_Owner() {
        return replaced_colony_owner;
    }
    static int16_t replaced_design_owner = -1;
    void Publish_Designs_Replacing(int16_t owner) {
        const int16_t previous = replaced_design_owner;
        replaced_design_owner = owner;
        Publish(DESIGNS_REPLACING);
        replaced_design_owner = previous;
    }
    int16_t Replaced_Design_Owner() {
        return replaced_design_owner;
    }
    s_colony_player_scope::s_colony_player_scope(int16_t player) : viewer(player), parent(colony_player) {
        colony_player = this;
    }
    s_colony_player_scope::~s_colony_player_scope() {
        colony_player = parent;
    }
    bool Colony_Player_Context_Matches(int16_t viewer) {
        for (auto* owner = colony_player; owner != nullptr; owner = owner->parent)
            if (owner->revoked || owner->viewer != viewer) return false;
        return true;
    }
    bool Colony_Edits_Active() {
        return population_staged || queue_staged;
    }
    void Set_Colony_Population_Staged(bool staged) {
        population_staged = staged;
    }
    s_colony_queue_scope::s_colony_queue_scope() : parent(queue_owner) {
        queue_owner = this;
        queue_staged = true;
    }
    void s_colony_queue_scope::Hold() {
        held = true;
    }
    void s_colony_queue_scope::Commit() {
        committed = true;
    }
    s_colony_queue_scope::~s_colony_queue_scope() {
        queue_owner = parent;
        if (revoked || parent != nullptr) return;
        if (held) return;
        queue_staged = false;
        if (committed) Publish(COLONY_OBSERVATION_POINT);
    }

    const s_relations_view* Published_Relations_View() {
        return relations_view;
    }

    s_relations_view_scope::s_relations_view_scope(bool type) : report(type), parent(relations_owner) {
        if (parent != nullptr) parent->ready = false;
        relations_owner = this;
        Publish(RELATIONS_VIEW_CLEARED);
    }

    s_relations_view_scope::~s_relations_view_scope() {
        Close();
    }

    void s_relations_view_scope::Close() {
        if (closed) return;
        closed = true;
        ready = false;
        Publish(RELATIONS_VIEW_CLEARED);
        relations_owner = parent;
    }

    void Ready_Relations_View(bool report) {
        if (relations_owner != nullptr && !relations_owner->revoked && relations_owner->report == report && relations_interruptions == 0) relations_owner->ready = true;
    }

    void Publish_Relations_View(bool report, const relations_read::s_disclosure* items, int16_t count) {
        if (relations_owner == nullptr || !relations_owner->ready || relations_owner->report != report || relations_interruptions != 0 || count < 0 || count > 8 || (count != 0 && items == nullptr)) return;
        if (report && count == 1) {
            relations_owner->report_data = items[0];
            relations_owner->has_report = true;
        }
        const s_relations_view view{items, count};
        relations_view = &view;
        Publish(RELATIONS_VIEW_READY);
        relations_view = nullptr;
    }

    void Present_Relations_Report() {
        if (relations_owner == nullptr || !relations_owner->report || !relations_owner->has_report) return;
        Ready_Relations_View(true);
        Publish_Relations_View(true, &relations_owner->report_data, 1);
    }

    s_relations_interruption_scope::s_relations_interruption_scope() {
        if (relations_owner == nullptr) return;
        interrupted = true;
        ++relations_interruptions;
        relations_owner->ready = false;
        Publish(RELATIONS_VIEW_CLEARED);
    }

    s_relations_interruption_scope::~s_relations_interruption_scope() {
        if (interrupted) --relations_interruptions;
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
