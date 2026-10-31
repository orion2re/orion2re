# P0 evidence and outstanding gates

Prepared with AI assistance; static evidence and performed checks are distinguished
below. Function names are more durable
than line numbers; anchors below refer to `baseline.json`'s reviewed target.

## P0-06: pinned backend and remaining platform gate

Candidate: libwebsockets v4.5.4, upstream revision
`c56adc3c62ee2545c77e0ba4cce67441be6955db`. The upstream tag API resolved directly
to this commit during review and the evaluation checkout matches it exactly.
`backend.json` pins the selected profile. P1 now links it only in an explicitly
enabled experimental game build, reusing project `uv_a`; no submodule was added.
Default and router-only builds do not require or link it. See `p1.md` for current
live transport tests and the startup/shutdown integration.

Primary evidence (review the immutable revision before adoption):

- [Tag resolution](https://api.github.com/repos/warmcat/libwebsockets/git/ref/tags/v4.5.4)
- [License](https://github.com/warmcat/libwebsockets/blob/c56adc3c62ee2545c77e0ba4cce67441be6955db/LICENSE)
- [Build/platform documentation](https://github.com/warmcat/libwebsockets/blob/c56adc3c62ee2545c77e0ba4cce67441be6955db/READMEs/README.build.md)

The main library is MIT (not the old LGPL-plus-linking-exception assumption).
The license identifies retained BSD-3-Clause SHA-1, BSD-2-Clause half-precision,
zlib-licensed components, an Apache-2.0 mbedTLS wrapper/extensions, OFL fonts, and
CC0 examples. MIT/BSD terms appear compatible with GPL-3.0-or-later; distribution
still requires the applicable notices and a review of the **actual selected**
source/dependency set. This is technical review, not maintainer/legal approval.

Upstream documents Visual Studio, MinGW, Linux x86/ARM and OSX builds. It documents
libuv use and matching Windows runtime-library settings. That does not establish
current Windows x86/x64, Linux or MinGW build success for our configuration.
Do not substitute upstream platform claims for the required project build matrix.

Recommended evaluation profile: server WebSocket/text only, loopback, no TLS for
the initial local-only service, no compression, examples, HTTP file serving or
unneeded protocols/plugins. Confirm exact CMake options and linked libraries at
the candidate revision rather than relying on defaults (upstream documents TLS
enabled by default). Reuse pinned project libuv if integration is practical; do
not couple the game-thread bridge to its event loop or multiplayer router.

Before approval: review release/security maintenance history, diagnostics,
incremental receive/fragment semantics, writable callbacks and partial-output
ownership, message/connection limits and shutdown wake/join behavior. Run a tiny
bounded echo/fragmentation/slow-reader harness against the exact candidate on
macOS, Linux, Windows x86/x64 and supported MinGW toolchains. Record compiler,
flags, transitive dependency versions, linkage and test logs.

Performed: native macOS ARM64 / AppleClang 17 Debug static build, using project
libuv `840404ce8ba7cc0204be52389a6cfff9f2c90fb6`, with IPv6 enabled and TLS,
compression, client, examples, file operations and backend JSON-RPC disabled.
`tests/rpc/backend_probe.c` links that library and tests create, a wake
request, asynchronous close drain, final destruction and successful foreign-loop
close without a listener. This is not a live-socket fragmentation or shutdown test.
The selected build needs libc/platform networking and pinned libuv, not OpenSSL,
zlib, mbedTLS, Brotli, SQLite or font/display dependencies. Retain MIT/BSD notices
(including built-in SHA-1) if this profile is eventually vendored/distributed.

Reproduce after building the existing unit suite:

```sh
git clone --depth 1 --branch v4.5.4 https://github.com/warmcat/libwebsockets.git out/p0-libwebsockets
python3 tools/check_rpc_backend.py
```

The original evaluation checker verifies pins, reads all options from `backend.json`, and writes
configure/build/link logs under ignored `out/`. It does not download or install
anything. There are no Windows/MinGW cross-compilers on this host. Linux,
Windows x86/x64, MinGW and macOS x86_64 checks have not run; the complete supported
target gate is not met. P0-06 stays open. Backend adoption must not be inferred
from this one native build. P1 additionally passes actual local WebSocket tests
(including slow-reader shutdown) and ASAN/UBSAN checks; the newly wired CI matrix
still needs observed results before P0-06 can be checked complete.

## P0-08: candidate pump sites, not approved hooks

| Source / function | Verified observation | Requirement before use |
| --- | --- | --- |
| `src/game/fields.cpp:167`, `fields::Get_Input` | Checks exit, returns early for input delay or insufficient fields, then interprets input | Context must already own initialized fields; observe during delay without bypassing it. This is not a universal action pump. |
| `src/game/fields.cpp`, `Timed_Wait_For_Input` | Timed polling can invoke the assigned auto-function | Audit timeout and callback reentry; do not shorten decisions or drain arbitrary actions here. |
| `src/game/mox2.cpp:34`, `Screen_Control` | Dispatches native screens, including turn paths | Register in each actual initialized screen, not by writing `current_screen`; a dispatcher boundary alone proves no local-buffer lifetime. |
| `src/game/nextturn.cpp:38`, `Next_Turn` | Calculates turn, then can play films/score screens; later clears done flags | Mark global captures busy during resolution. Choose committed publication after all required state is stable; screen transition is not a turn event. |
| `src/game/netturn.cpp`, `Host_Next_Turn` / `Client_Next_Turn` | Host calls native next turn; client receives/decode payload, restores allocation, updates ship icons; chat polls occur while waiting | No query-driven networking pump. Find one publication boundary after host calculation/client decode; test duplicate/cancel/elimination paths. |
| `src/game/combat1.cpp:45`, `Get_Tactical_Input` | Input can open nested config-reload warning/message boxes and poll again | Child context must accept only its own deferred intent; no recursive global mutation dispatch. |
| `src/game/combat1.cpp:997`, auto-combat input polling | `Get_Tactical_Input` also appears inside auto-combat handling | An input poll is not proof of a human action boundary; separate observation from native-authorized command consumption. |
| `src/game/smack.cpp:102`, `Play_Cinematic` frame loop | Polls key/mouse, draws/presents, updates frame; does not call `Get_Input` | Separate film observation/skip boundary, copied frame+palette ownership; preserve pacing and left/right/key skip semantics. |
| `src/game/timer.cpp:50`, `Release_Time` | Sleeps based on adjusted ticks and elapsed time | Do not use a generic wait as a mutation pump or skip decision waits because a reader is connected. |
| `src/game/mox2.cpp:434`, `thunk_main2` | Normal return and exit/standard/unknown exceptions converge before sound/resource/network cleanup | Stop RPC, cancel unstarted work, wake/join service before dependent state dies; cannot require another game poll. |

`platform::Check_Exit` can throw. Manual cleanup after a loop is not guaranteed
on unwind: `NETTURN::Net_Next_Turn` temporarily points `CHAT::chat_info` at stack
storage; client turn paths temporarily install COMBFIND pointers. A new RPC scope
must unregister its own borrowed state on every exit without trying to fix all
unrelated legacy cleanup. No RPC callbacks may survive context destruction.

The completed static integration decisions are in `lifecycle.md`, covering field
replacement/restoration, child-only decisions, host/client committed publication
and exception-safe stop/wake/join ordering. Every actual screen/dialog registration still needs
entry/replacement/exit lifetime review and tests, including same-count field
replacement, nested dialogs during actions, observations during animation, quit
during film and quit during I/O. No hook has been installed or certified safe.

## P0-09: budgets must follow measurements

Do not equate the binary fork snapshot size with a typed JSON projection, and do
not invent a maximum-game latency or output cap. There is no capture code yet.
All of the following gates are **unmeasured**:

1. Use a locally owned maximum-size game with worst-case own/visible units,
   colonies, contacts, design/queue lengths and modded research categories. Keep
   proprietary saves/assets outside the repository; use synthetic fixtures for
   automated tests. Record revision, platform, build, mod digest and scenario.
2. At a committed authorized game-thread point, measure projection time, copied
   DTO bytes, encoding time, encoded UTF-8 bytes, peak temporary allocation and
   retained snapshot bytes. Record cold capture and repeated p50/p95/max; test
   requested topic subsets separately from complete state and frame/media data.
3. Repeat feature off/on, no client, one reader, controller, multiple subscribers,
   reconnect/baseline and slow reader. Confirm hidden-only changes produce no
   public revision/ID/count/size difference beyond authorized content.
4. Set numeric connection, pre-auth deadline, frame/message/depth/string/list,
   pending-request, per-poll count/time, capture-frequency, encoded-output,
   immutable-retention, event-log and result-ticket bounds from these results.
   Specify per-connection and global totals, not just one queue's cap.
5. Verify request rejection before queueing, coalesced unsent state baseline,
   ordered-event gap/disconnect, no partially transmitted message rewrite,
   cancellation and bounded shutdown. No socket wait on the game thread.

`contracts.json.limits` now specifies provisional numeric connection/input/depth,
queue, global/per-client output, retention, capture-frequency and polling caps,
including explicit overflow behavior. They are finite starting constraints, not
measured or approved budgets. Source capacities are 1,024 stars, 10,000 ships,
5,120 planets/colonies, 8 players, 67 leaders and 57 nebulas (`src/game/consts.h`).
The maximum-size **authorized projection** still needs measurement; a Python JSON
size or raw engine sizeof cannot stand in for native capture cost. No game-thread
capture implementation or local maximum-game save is available in this checkout.
P0-09 remains blocked on those prerequisites, not on inventing another limit.

## P0-10: native compatibility baseline procedure

Before engine hooks, record a local scripted interaction trace and checksums of
local save copies for new/load, one ordinary turn, research choice, colony jobs,
queue edit/buy/demolish refusal+confirmation, fleet subset/move, diplomacy, native
combat/ground effects, film skip and hotseat switch. Include network host/client
decode and cancellation paths. Record configuration/mod digests and seeds without
publishing proprietary save content, dialogue or assets.

Compare game RNG state/call counts at matching semantic boundaries, and saved
game state after the same native decisions, for RPC disabled, enabled/no client,
reader subscriptions and controller interactions. Account for native timestamps
or other nondeterministic metadata explicitly; do not normalize gameplay changes
away. `native_baseline.json` records eight actually run native-function test
targets, case/assertion counts, and existing fixed RNG checkpoints. These tests
use their existing synthetic fixtures/fakes, not an interactive game. They cover
save/config snapshots, config/mod loading, population transfer and turn/RNG
behavior, but cannot establish native-view, timing or full-game parity.

No original game assets or local gameplay saves are included in `data/` (its
base LBX-named directories contain project replacements, not a complete game
installation). A real local game installation and save/interaction fixture are
needed for the outstanding recordings. Do not import proprietary assets/saves
into the repository to close the gate. P0-10 remains open for those captures.
