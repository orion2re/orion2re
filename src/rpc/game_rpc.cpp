#include "pch.h"

#include "rpc/game_rpc.h"
#include "game/game_events.h"

#include "config/text_encoding.h"
#include "rpc/rpc_game.h"
#include "rpc/rpc_service.h"
#include "rpc/rpc_settings.h"

namespace game_rpc {
    namespace {
        rpc::s_bridge bridge;
        rpc::s_game_store store;
        rpc::s_service service;
        uint64_t subscription = 0;
        bool active = false;
        bool pumping = false;
        uint64_t last_capture_ms = 0;

    } // namespace

    bool Read_Map_Observation(rpc::s_map_observation* observation) {
        if (observation == nullptr) return false;
        *observation = {};
        const int32_t viewer = MOX::PLAYER_NUM;
        if (RUSS::all_ai_game != 0 || viewer < 0 || viewer >= MOX::NUM_PLAYERS || viewer >= MAX_PLAYERS ||
            MOX::player == nullptr || MOX::star == nullptr || (MOX::NUM_NEBULAS != 0 && MOX::nebula == nullptr) ||
            MOX::player[viewer].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[viewer].eliminated != 0 ||
            MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS || MOX::NUM_NEBULAS > MAX_NEBULAS) {
            return false;
        }
        observation->viewer = viewer;
        observation->turn = MOX::stardate;
        observation->systems.reserve(MOX::NUM_STARS);
        for (int16_t index = 0; index < MOX::NUM_STARS; ++index) {
            const s_star_data* star = &MOX::star[index];
            rpc::s_system_observation item;
            item.x = star->x;
            item.y = star->y;
            item.visited = HAROLD::Player_Has_Visited((int16_t)viewer, index) != 0;
            if (item.visited && star->spectral_class != STAR_CLASS_BLACK_HOLE) {
                if (memchr(star->name, 0, sizeof(star->name)) == nullptr) return false;
                // Validate links before the native read-only outpost-format predicate.
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const int16_t planet = star->planet_index[orbit];
                    if (planet < -1 || planet >= MAX_PLANETS || (planet >= 0 && MOX::planet == nullptr)) return false;
                    if (planet >= 0) {
                        const int16_t colony = MOX::planet[planet].colony_index;
                        if (colony < -1 || colony >= MAX_COLONIES || (colony >= 0 && MOX::colony == nullptr)) return false;
                    }
                }
                char name[sizeof(star->name) + 1] = {};
                memcpy(name, star->name, sizeof(star->name));
                if (HAROLD::Star_Is_Outpost_Star(index)) strings::String_To_Lower(name);
                config::Decode_Cp850_To_Utf8(name, strlen(name), &item.name);
                item.name_available = true;
            }
            observation->systems.push_back(std::move(item));
        }
        observation->nebulas.reserve(MOX::NUM_NEBULAS);
        for (int16_t index = 0; index < MOX::NUM_NEBULAS; ++index) {
            observation->nebulas.push_back({MOX::nebula[index].x, MOX::nebula[index].y});
        }
        return true;
    }

    static rpc::s_bridge* Initialize() {
        if (!active) active = rpc::Initialize_Game_Store(&store, &bridge);
        return active ? &bridge : nullptr;
    }

    static void Shutdown() {
        rpc::Stop_Service(&service);
        if (active) rpc::Shutdown_Game_Store(&store);
        active = false;
    }

    static void Pump() {
        if (!active || pumping) return;
        pumping = true;
        rpc::Pump_Game_Queries(&store);
        pumping = false;
    }

    static void Capture_Main_Map() {
        if (!active || !store.loaded || store.busy_depth != 0) return;
        if (!store.snapshots.empty() && !rpc::Needs_Game_Capture(&store)) return;
        const uint64_t now = rpc::Monotonic_Ms();
        if (last_capture_ms != 0 && now - last_capture_ms < rpc::Protocol_Limits()->capture_interval_ms) return;
        last_capture_ms = now;
        rpc::s_map_observation observation;
        if (!Read_Map_Observation(&observation) || !rpc::Capture_Map(&store, &observation)) {
            rpc::Begin_Game_Reset(&store);
        }
    }

    static void Menu() {
        if (active) rpc::End_Game(&store);
    }

    static void Revoke_Perspective() {
        if (active) rpc::Revoke_Game_View(&store);
        last_capture_ms = 0;
    }

    static void Handle_Event(game_events::e_event event, void* context) {
        (void)context;
        if (event == game_events::THREAD_STARTED) {
            if (!rpc::enabled) return;
            rpc::s_service_config service_config;
            service_config.bridge = Initialize();
            service_config.port = rpc::port;
            config::Decode_Cp850_To_Utf8(rpc::shared_key, strlen(rpc::shared_key), &service_config.key);
            std::string diagnostic;
            if (service_config.bridge == nullptr || !rpc::Start_Service(&service, &service_config, &diagnostic)) {
                Shutdown();
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rpc: listener initialization failed");
            } else {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "rpc: listening on loopback port %u", (unsigned)rpc::Service_Port(&service));
            }
            return;
        }
        if (event == game_events::THREAD_STOPPING) {
            Shutdown();
            return;
        }
        if (!active) return;
        switch (event) {
            case game_events::MENU_ENTERED: Menu(); break;
            case game_events::PERSPECTIVE_SELECTION_BEGIN: Revoke_Perspective(); break;
            case game_events::WORLD_REPLACE_BEGIN:
                rpc::Begin_Game_Reset(&store);
                rpc::Pause_Game_Queries(&store);
                last_capture_ms = 0;
                break;
            case game_events::WORLD_UPDATE_BEGIN:
            case game_events::RESOLUTION_BEGIN: rpc::Pause_Game_Queries(&store); break;
            case game_events::WORLD_REPLACE_COMMITTED:
                if (!rpc::Commit_Game_Reset(&store)) rpc::Begin_Game_Reset(&store);
                rpc::Resume_Game_Queries(&store);
                break;
            case game_events::WORLD_REPLACE_FAILED:
            case game_events::WORLD_UPDATE_FAILED:
                rpc::Begin_Game_Reset(&store);
                rpc::Resume_Game_Queries(&store);
                break;
            case game_events::WORLD_UPDATE_COMMITTED:
            case game_events::RESOLUTION_END: rpc::Resume_Game_Queries(&store); break;
            case game_events::MAP_OBSERVATION_POINT: Capture_Main_Map(); break;
            case game_events::IDLE_OBSERVATION_POINT: Pump(); break;
            default: break;
        }
    }

    static void Observe(game_events::e_event event, void* context) {
        try {
            Handle_Event(event, context);
        } catch (...) {
            Shutdown();
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rpc: observer disabled after an internal failure");
        }
    }

    bool Subscribe_To_Game() {
        if (subscription != 0) return false;
        subscription = game_events::Subscribe(Observe, nullptr);
        return subscription != 0;
    }

    void Unsubscribe_From_Game() {
        game_events::Unsubscribe(subscription);
        subscription = 0;
        Shutdown();
    }
} // namespace game_rpc
