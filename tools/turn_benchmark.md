# Single-turn benchmark

Build the opt-in binary with the same optimized engine objects as the game:

```sh
cmake -S . -B out/build/turn-release -DCMAKE_BUILD_TYPE=Release \
  -DORION2RE_GAME_DIR= -DORION2RE_BUILD_TESTS=OFF \
  -DORION2RE_BUILD_DOCUMENTATION=OFF
cmake --build out/build/turn-release --target turn-benchmark --parallel 8
```

Do not set `ORION2RE_GAME_DIR` to your installation: deployment is unnecessary.
The benchmark reads your game assets/configuration and save, then runs in a new
temporary working directory (printed as `scratch=...`). It disables autosave
and debug saves; result serialization stays in memory. No proprietary assets
or saves belong in the repository.

```sh
out/build/turn-release/turn-benchmark GAME_ROOT SAVE_FILE [WARM_REPEATS] [precombat|turn|ai]
```

- `precombat` (default): executes the production turn sequence through
  `COLCALC::Do_Surrenders`/`AIDATA::Complete_Turn`, stopping immediately before
  `COMBFIND::Search_For_Battles`. Reports phase timings. This is **not a full
  turn**: combat and all subsequent mechanics are excluded, and stardate does
  not advance. Keep its sequence synchronized with `NEXTTURN::Next_Turn_Calc`.
- `turn`: calls the real `NEXTTURN::Next_Turn_Calc`, without stubbing mechanics,
  and checks that stardate advances by one. Only use saves that do not require
  interactive combat, council, or event screens. This harness does not create
  a display or supply input; reaching presentation code can crash or block.
- `ai`: only calls `AIDATA::Compute_AI_Data`; it does not advance the turn.

Startup, configuration/save loading, resource initialization, result
serialization/hashing, and output are outside the timed interval. Rendering,
autosave, and the end-of-game wrapper in `NEXTTURN::Next_Turn` are not measured.
Application servicing remains enabled at the production turn's AI boundaries;
the AI calculation loops do not call `RUSS::Mox_Update` directly.

`WARM_REPEATS` defaults to zero. With a positive count (maximum 10000), the input
save and seed are restored before each additional sample. The first sample is
reported separately; min/median/mean/max exclude it. Every repeated serialized
result must match the first byte-for-byte or the program exits with an error.
The final-state SHA-256 can also validate runs in separate fresh processes.
Warm samples refer to in-process caches, not cold filesystem-cache tests.

## Local measurements

Apple M4 Max, AppleClang 17, native arm64, `-O3 -DNDEBUG`, engine revision
`0512260` plus this harness. These initial standalone measurements precede the
matched before/after comparison below. Counts are loaded record/slot counts,
not necessarily live entities.

| Save | Scope | Sample method | Median | Range |
| --- | --- | --- | ---: | ---: |
| `SAVE2.GAM`, 3517.5, 5 players, 54 stars, 8 colonies, 32 ships | Full calculation to 3517.6 | 10 fresh processes | 1.303 ms | 1.285–1.501 ms |
| `SAVE10.GAM`, 3551.5, 5 players, 71 stars, 56 colonies, 113 ships | Pre-combat only | 10 warm samples after one first run | 4.833 ms | 4.754–4.998 ms |

The late-save phase means were 4.407 ms for AI data, 0.344 ms for colony AI,
and 0.033 ms for movement AI. AI data accounted for about 91% of the measured
pre-combat work. All 11 pre-combat serialized results matched. All 10
fresh-process full-turn hashes matched.

`SAVE2` has five living players (four AI); `SAVE10` has only two living players
(one AI). Neither save is a large-galaxy/many-AI scaling stress test.

`SAVE10` reaches a human defense-selection popup during a full turn, so no
full-turn latency is claimed for it. `SAVE9` was rejected by the current loader.
In-process full-turn replays of the legacy `SAVE2` produced different serialized
results; use fresh processes for that save rather than trusting warm repeats.
The reason for that runtime-state difference has not been investigated here.

Reproduce the late-save phase benchmark:

```sh
out/build/turn-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE10.GAM 10 precombat
```

For the full-turn baseline, invoke this ten times in fresh processes and compare
`result_sha256` before aggregating `first_ms`:

```sh
out/build/turn-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE2.GAM 0 turn
```

Full-turn process startup and loading are still excluded: `first_ms` measures
only the turn-calculation call, not process wall-clock duration.

## Pre-refactor comparison

Compared `9b9b598` (parent of the AI refactor) with `0512260`. The baseline is a
detached worktree at `out/build/prechange-source`, built into
`out/build/turn-prechange-release`. Both builds use identical CMake settings,
the same benchmark source, unchanged vendored dependencies, game assets,
configuration, and input saves. The harness selects the old or new AI API at
compile time; engine code is not modified. The old pre-combat path retains its
production `Capture_Build_State`/`Deallocate_AI_Data` calls.

Before/after execution order alternated between pairs. Fresh-process cases
used 30 samples per revision. Warm samples came from six processes per
revision, each with one excluded first run and 20 measured replays (120 warm
samples per revision).

| Workload | Before median | After median | Change |
| --- | ---: | ---: | ---: |
| `SAVE2`, full calculation, first call in fresh process | 1.460 ms | 1.299 ms | **11.0% faster** |
| `SAVE10`, pre-combat, first call in fresh process | 4.248 ms | 5.260 ms | **23.8% slower** |
| `SAVE10`, pre-combat, warm replay | 3.311 ms | 4.742 ms | **43.2% slower** |

For the late-save warm samples, median AI-data time increased from **2.867 ms
to 4.308 ms** (50.3% slower, +1.441 ms). This accounts for essentially all the
observed +1.431 ms pre-combat regression; individual phase medians need not sum
exactly to the total median. Colony AI decreased from 0.349 ms to 0.333 ms;
movement AI increased from 0.022 ms to 0.027 ms.

All recorded final-state SHA-256 hashes matched between revisions for each
workload, as did end seeds and stardates. Repeated warm results were also
checked byte-for-byte within each process. This verifies equal serialized
outcomes for these saves, not general behavioral equivalence.

The late save still cannot provide a headless full-turn comparison because of
the human combat popup. The smaller save's full-turn improvement is a
**first-workspace-call** result, not evidence of faster steady-state full
turns. The regression is localized to AI-data calculation, but these timings
alone do not distinguish hash-lookup overhead from allocation/reset costs or
other work within that function. No large-galaxy scaling claim is made.

Raw per-run outputs and phase timings are retained in
`out/build/turn-release/prechange-comparison.json`. With the retained builds,
run either binary with the same arguments, alternating execution order:

```sh
out/build/turn-prechange-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE2.GAM 0 turn
out/build/turn-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE2.GAM 0 turn

out/build/turn-prechange-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE10.GAM 20 precombat
out/build/turn-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE10.GAM 20 precombat
```

## Native sampling profile

Captured each revision separately with macOS `/usr/bin/sample`: 10 seconds at
a 1 ms interval, after the first warm sample began. Each workload replayed
`SAVE10` pre-combat 3000 times after one first run. Both completed successfully;
all 3001 serialized results matched within each process and across revisions.
These profiler runs are separate from the unprofiled latency comparison above.
Instruments/`xctrace` was not installed.

Loading and validation dominate much of the whole-process sample, so the table
below is normalized **only to stacks containing `AIDATA::Compute_AI_Data`**:
1716 samples before, 2455 after. Entries marked inclusive contain their callees;
the rows overlap and must not be added together.

| Hot path | Before | After | Interpretation |
| --- | ---: | ---: | --- |
| Intermediate-route search, inclusive | 6.3% | 26.8% | Main regression candidate |
| Direct-route table `Find`, inclusive | inline array indexing | 13.8% | Included in intermediate-route search |
| Star-assessment map access, exclusive | array indexing | 10.7% | Partly inside route search; also used throughout AI-data loops |
| Turn reset (`Begin_Turn`), inclusive | 1.0% | 1.7% | Not the dominant sampled cost |
| Movement geometry (`Move_Player_1_Turn_To_Star`), inclusive | 73.5% | 48.0% | Large preexisting cost; its smaller share is not evidence of a speedup |

The star-assessment access routine appeared as `<deduplicated_symbol>` in the
profile. Resolving sampled addresses with `nm -n -C` identified libc++'s
`__hash_table<...s_star_assessment...>::__emplace_unique_key_args`, used by
`ai->stars[id]`. Disassembly confirmed that hot offsets include bucket lookup,
key matching, and the existing-key return path, before the allocation branch.
The template name does not mean each sampled access inserts a new node.

### Source-level optimization targets (unfixed `0512260`)

1. **Route-leg lookup:** `src/game/aidata.cpp:1004–1005` performs two
   `N_Turns_From_To` lookups per candidate intermediary. Each now calls
   `s_direct_route_table::Find` (`src/game/aidata_state.h:65–77`), which resolves
   both destination and source-row hash maps. Previously this was flattened
   array indexing. Resolve/reuse compact destination offsets and row references
   outside the candidate scan, preserving sparse-ID support and availability
   semantics.
2. **Repeated star lookup:** `aidata.cpp:998–1010`, `205–215`, and `477–487`
   repeatedly access `ai->stars[id]` in nested loops. Hoist stable assessment
   references where safe rather than repeating hash/insertion-access machinery.
3. **Allocation/reset tuning is secondary for this save.** The reset stack is
   small in this profile. Sampling is not an allocation census, so it does not
   rule out allocation problems on larger workloads.

This evidence prioritizes reducing lookup work before redesigning workspace
allocation or optimizing unchanged movement physics. It does not quantify the
speedup of an unimplemented fix. No engine behavior was changed for profiling.

Profiles, validated workload logs, and normalized symbol counts are retained in
`out/build/turn-release/profiles/` as `before-cpu.txt`, `after-cpu.txt`,
`before-run.log`, `after-run.log`, and `summary.json`. For example, reproduce the
after capture with the rebuilt harness:

```sh
mkdir -p out/build/turn-release/profiles
out/build/turn-release/turn-benchmark ~/Games/moo2 ~/Games/moo2/SAVE10.GAM 3000 precombat \
  > out/build/turn-release/profiles/after-run.log 2>&1 &
pid=$!
# Wait for sample=1 warm_ms=... in the log before attaching.
sample "$pid" 10 1 -mayDie -file out/build/turn-release/profiles/after-cpu.txt
wait "$pid"
```

Use `out/build/turn-prechange-release/turn-benchmark` for the before capture,
and run captures serially. Do not interpret timings collected while sampling
as unprofiled benchmark results.

## Lookup optimization

The implementation keeps the ID-keyed assessment/route maps and their existing
ownership. It hoists stable star-assessment references out of inner loops, then
builds a short-lived `s_route_search_lookup` during `Compute_AI_Data` after the
direct route tables have been populated. The lookup contains resolved row
references, destination offsets, and live star-assessment references, sized by
the current world counts rather than object-ID high-water marks.

Intermediate-route scans reuse the source row and destination offset and read
the candidate row/offset through that lookup. Callers without a prepared lookup
retain the ID-based path. Out-of-range sparse IDs fall back to the maps.
`Store`/`Clear` revisions reject stale prepared route views. The borrowed lookup
must be discarded before table/state replacement or move, or assessment erasure;
it is never stored in or copied with `s_state`.

Regression tests cover prepared versus ID-based searches, normal/extended
choices, equal-turn tie order, zero/255 durations, missing cells, bitmap growth,
full-width sparse IDs, copied workspaces, stale views after `Store`/`Clear`, and
live assessment changes across map rehashing. Serialized save results must also
match the preserved unfixed binary before accepting timing results.

The isolated reference-hoisting step measured 4.757 → 4.511 ms median pre-combat
time on `SAVE10` (80 warm samples per variant, alternating execution order),
with matching result hashes. Post-commit measurements of the combined fix are
retained separately under `out/build/turn-release/`; do not substitute profiler
timings or these intermediate measurements for that final comparison.
