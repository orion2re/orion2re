#include "pch.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "config/config_loader.h"
#if __has_include("game/legacy_world.h")
#include "game/legacy_world.h"
#define TURN_BENCHMARK_AI_STATE &AIDATA::state, &game_world::current
#define TURN_BENCHMARK_EXPLICIT_AI 1
#define TURN_BENCHMARK_WORLD_VIEW 1
#elif __has_include("game/ai_legacy.h")
#include "game/ai_legacy.h"
#define TURN_BENCHMARK_AI_STATE &AIDATA::state
#define TURN_BENCHMARK_EXPLICIT_AI 1
#else
#include "game/ai_state.h"
#define TURN_BENCHMARK_AI_STATE
#define TURN_BENCHMARK_EXPLICIT_AI 0
#endif
#include "game/savegame.h"
#include "game/sha256.h"
#include "lua/lua_game.h"
#include "lua/lua_mod_runtime.h"

namespace {
    using t_clock = std::chrono::steady_clock;

    void Bind_AI_World() {
#if defined(TURN_BENCHMARK_WORLD_VIEW)
        game_world::current = legacy_world::World();
#elif TURN_BENCHMARK_EXPLICIT_AI
        AIDATA::state.world = AI_LEGACY::World();
#endif
    }

    template <typename Function>
    double Time_Ms(Function function) {
        const auto start = t_clock::now();
        function();
        return std::chrono::duration<double, std::milli>(t_clock::now() - start).count();
    }

    struct s_phase {
        const char* name;
        double ms;
    };

    template <typename Function>
    void Measure(std::vector<s_phase>* phases, const char* name, Function function) {
        phases->push_back({name, Time_Ms(function)});
    }

    // NEXTTURN::Next_Turn_Calc in production order, stopping before battles.
    // No combat/event decisions are fabricated to make the benchmark headless.
    void Precombat(std::vector<s_phase>* phases) {
        Measure(phases, "prepare", [&] {
            EVENTS::Clear_Deferred_Monster_Event_Target();
            fields::Assign_Auto_Function(SCORE::nullsub, 1);
            MSG_::Allocate_Msg_Slots();
            if (MOX::settings.xenons_exist != 0 && MOX::antarans.field6_0x3c == 0) antarans::Antaran_Invasion_Check();
            Bind_AI_World();
#if __has_include("game/ai_compat.h")
            RUSS::Mox_Update();
#endif
        });
        Measure(phases, "ai_data", [&] { AIDATA::Compute_AI_Data(TURN_BENCHMARK_AI_STATE); });
        Measure(phases, "diplomacy", [&] {
            DIP_SCRN::Resolve_Delayed_Diplomacy_Orders();
            DIP_SCRN::Set_Opportunity_Attacks();
            diplomac::Clear_Diplomacy_Messages();
            npcdiplo::NPC_Diplomacy();
            diplomac::Diplomacy_Growth();
        });
        Measure(phases, "leaders", [&] { aileader::Do_AI_Leaders(TURN_BENCHMARK_AI_STATE); });
        Measure(phases, "build_needs", [&] {
            AIBUILD::Compute_Empire_Building_Needs(TURN_BENCHMARK_AI_STATE);
#if !TURN_BENCHMARK_EXPLICIT_AI
            ai_state::Capture_Build_State();
#endif
        });
        Measure(phases, "colony_ai", [&] { aidudes::All_Colony_AI(TURN_BENCHMARK_AI_STATE); });
        Measure(phases, "movement_ai", [&] { AIMOVE::Move_All_AI(TURN_BENCHMARK_AI_STATE); });
        Measure(phases, "colonization_research", [&] {
            COLONIZE::All_AI_Colonize();
            aitech::All_AI_Tech_Select(TURN_BENCHMARK_AI_STATE);
#if __has_include("game/ai_compat.h")
            RUSS::Mox_Update();
#endif
        });
        Measure(phases, "production_movement", [&] {
            maintain::Make_Scrap_Ships_Dead();
            REPORT::Initialize_Reports();
            TRADE::Process_Trade_And_Research_Agreements();
            SHIPMOVE::Move_All_Ships_Toward_Stars();
        });
        Measure(phases, "spy_colony_calculation", [&] {
#if TURN_BENCHMARK_EXPLICIT_AI
            spy::Resolve_Spies(AIDATA::state.players.data());
#else
            spy::Resolve_Spies();
#endif
            orion2re::lua::game::s_authoritative_population_growth_scope population_growth_scope(true);
            COLCALC::All_Colony_Calculations();
        });
        Measure(phases, "apply_changes", [&] {
            COLCALC::Apply_All_Player_Changes();
            COLCALC::Apply_All_Colony_Changes();
            COLCALC::Do_Surrenders();
#if TURN_BENCHMARK_EXPLICIT_AI
            AIDATA::Complete_Turn(&AIDATA::state);
#else
            AIDATA::Deallocate_AI_Data();
#endif
        });
    }

    bool Load(const std::vector<uint8_t>& bytes) {
        Bind_AI_World();
        SERIAL::Reader_ reader(bytes.data(), (uint32_t)bytes.size(), "benchmark save underflow");
        SAVEGAME::s_metadata metadata{};
        s_settings settings{};
        uint32_t seed = 0;
        std::string error;
        if (!SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed, &error)) {
            std::fprintf(stderr, "load failed: %s\n", error.c_str());
            return false;
        }
        if (metadata.game_type != 0) {
            std::fprintf(stderr, "benchmark requires a single-player save\n");
            return false;
        }
        // Match FILEDEF::Load_Game without its presentation/resource refreshes.
        MOX::stardate = metadata.stardate;
        MOX::game_type = metadata.game_type;
        MOX2::Init_Map_Defaults();
        MOX::Apply_Save_Settings(&settings);
        MOX::settings.auto_saves = 0;
        MOX::debug_saves_on = 0;
        TECHINIT::Init_Tech();
        strings::Load_Strings();
        // Nebula artwork also supplies the collision mask used by movement.
        MAPGEN::Load_Nebula_Pictures();
        HAROLD::Confirm_Planet_And_Star_Index_Link("turn benchmark load");
        MAINSCR::Update_Star_Owners();
        HAROLD::Load_Leader_Titles();
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            auto& player = MOX::player[i];
            if (player.dead_field != 0) {
                player.research_accumulated = (uint16_t)player.dead_field;
                player.dead_field = 0;
            }
            std::fill(std::begin(player.delayed_diplomacy_orders), std::end(player.delayed_diplomacy_orders), -1);
        }
        game_random::Set_Random_Seed(seed);
        std::printf("save=%.37s version=%u stardate=%d players=%d stars=%d colonies=%d ships=%d seed=%u trailing_bytes=%u\n",
            metadata.description,
            metadata.version,
            MOX::stardate,
            MOX::NUM_PLAYERS,
            MOX::NUM_STARS,
            MOX::NUM_COLONIES,
            MOX::NUM_SHIPS,
            seed,
            reader.Remaining());
        int living_players = 0;
        int living_ai = 0;
        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].eliminated == 0) {
                ++living_players;
                if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN) ++living_ai;
            }
        }
        std::printf("living_players=%d living_ai=%d\n", living_players, living_ai);
        return true;
    }
} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    int repetitions = 0;
    if (argc < 3 || argc > 5) {
        std::fprintf(stderr, "usage: turn-benchmark GAME_ROOT SAVE_FILE [WARM_REPEATS] [precombat|turn|ai]\n");
        return 1;
    }
    if (argc >= 4) {
        const auto parsed = std::from_chars(argv[3], argv[3] + std::strlen(argv[3]), repetitions);
        if (parsed.ec != std::errc{} || *parsed.ptr != '\0' || repetitions < 0 || repetitions > 10000) return 1;
    }
    const bool ai_only = argc == 5 && std::strcmp(argv[4], "ai") == 0;
    const bool precombat = argc < 5 || std::strcmp(argv[4], "precombat") == 0;
    if (argc == 5 && !ai_only && !precombat && std::strcmp(argv[4], "turn") != 0) return 1;

    std::error_code error;
    const auto game_root = std::filesystem::absolute(argv[1], error);
    if (error) return 1;
    const auto save_path = std::filesystem::absolute(argv[2], error);
    if (error) return 1;
    std::ifstream input(save_path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() <= 0 || input.tellg() > SAVEGAME::BUFFER_CAPACITY) return 1;
    input.seekg(0);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    // Legacy code writes relative paths. Never run it in the user's installation.
    auto scratch = std::filesystem::temp_directory_path(error);
    if (error) return 1;
    scratch /= "orion2re-turn-benchmark-" + std::to_string(t_clock::now().time_since_epoch().count());
    if (!std::filesystem::create_directory(scratch, error)) return 1;
    std::filesystem::current_path(scratch, error);
    if (error) return 1;
    const char* mode = "Next_Turn_Calc";
    if (ai_only) mode = "ai-data-only";
    if (precombat) mode = "precombat-only";
    std::printf("scratch=%s mode=%s build=%s\n", scratch.string().c_str(), mode,
#if defined(NDEBUG)
        "Release"
#else
        "Debug"
#endif
    );
    if (vfs::Init(game_root.string().c_str()) == 0) return 2;
    config::s_effective_config effective;
    config::s_cfg_diagnostic diagnostic;
    bool loaded = false;
    if (!config::Load_Startup_Config("orion2re.cfg", "base_game", &effective, &loaded, &diagnostic)) {
        std::fprintf(stderr, "config failed: %s\n", diagnostic.message.c_str());
        return 2;
    }
    allocate::Allocate_Data_Space();
    LOADER::Load_Pictures();

    std::vector<double> times;
    std::vector<uint8_t> expected;
    std::vector<double> phase_totals;
    // The first run is reported separately, not included in warm statistics.
    for (int sample = 0; sample <= repetitions; ++sample) {
        if (!Load(bytes)) return 3;
        Bind_AI_World();
        const int32_t stardate = MOX::stardate;
        std::vector<s_phase> phases;
        phases.reserve(11);
        const double ms = Time_Ms([&] {
            if (ai_only) {
                AIDATA::Compute_AI_Data(TURN_BENCHMARK_AI_STATE);
            } else if (precombat) {
                Precombat(&phases);
            } else {
                NEXTTURN::Next_Turn_Calc();
            }
        });
        if (!ai_only && !precombat && MOX::stardate != stardate + 1) return 4;
        std::vector<uint8_t> result(SAVEGAME::BUFFER_CAPACITY);
        SERIAL::Writer_ writer(result.data(), (uint32_t)result.size(), "benchmark result overflow");
        std::string save_error;
        if (!SAVEGAME::Write_Game(&writer, "turn benchmark", game_random::Get_Random_Seed(), &save_error)) {
            std::fprintf(stderr, "result serialization failed: %s\n", save_error.c_str());
            return 4;
        }
        result.resize(writer.Length());
        const std::string digest = orion2re::crypto::sha256_hex(std::string_view((const char*)result.data(), result.size()));
        if (sample == 0) {
            expected = result;
        } else if (result != expected) {
            std::fprintf(stderr, "repeated single-turn results differ at sample %d\n", sample);
            return 4;
        }
        std::printf("sample=%d %s_ms=%.3f end_stardate=%d end_seed=%u result_sha256=%s\n", sample, sample == 0 ? "first" : "warm", ms, MOX::stardate, game_random::Get_Random_Seed(), digest.c_str());
        phase_totals.resize(phases.size());
        for (size_t i = 0; i < phases.size(); ++i) {
            std::printf("sample=%d phase=%s ms=%.3f\n", sample, phases[i].name, phases[i].ms);
            if (sample != 0) phase_totals[i] += phases[i].ms;
            if (sample == repetitions && repetitions != 0) std::printf("phase_mean=%s ms=%.3f\n", phases[i].name, phase_totals[i] / repetitions);
        }
        if (sample != 0) times.push_back(ms);
    }
    if (!times.empty()) {
        std::sort(times.begin(), times.end());
        double total = 0;
        for (double ms : times) total += ms;
        const double median = (times[(times.size() - 1) / 2] + times[times.size() / 2]) / 2;
        std::printf("repeat_state_equal=1 warm_samples=%zu min_ms=%.3f median_ms=%.3f mean_ms=%.3f max_ms=%.3f\n",
            times.size(),
            times.front(),
            median,
            total / times.size(),
            times.back());
    }
    orion2re::lua::Shutdown_Lua();
#if TURN_BENCHMARK_EXPLICIT_AI
    AIDATA::Reset(&AIDATA::state);
#else
    ai_state::Shutdown();
#endif
    config::Shutdown_Config_Loader();
    config::Shutdown_Runtime();
    vfs::Shutdown();
    return 0;
}
