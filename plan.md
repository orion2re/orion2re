# Player-scoped RPC and WebSocket interface: implementation plan

Status: P1 read foundation in progress. Optional libuv-backed WebSockets,
configured-key hello, metadata discovery, game-thread dispatch and an initial
immutable session/map/player-HUD and post-refresh ship projections, atomic audited-topic subscriptions, bounded
state coalescing and committed-capture turn notifications are implemented. Game code is RPC-unaware and
publishes only neutral observer events; the application composes the RPC sidecar.
Most gameplay query families and all remote actions remain uncallable. P0-06, P0-09 and
P0-10 remain open for their unperformed platform/native-game acceptance checks.
Contracts, generated schema/reference/C++ metadata and coverage auditing remain
in `specs/rpc/` and `tools/`; CI checks generation drift. RPC is off by default.
Prepared with AI assistance and static source inspection; runtime behavior and
visibility guarantees require the tests below. This file intentionally lives
outside the generated documentation portal.

## 1. Goals and decisions

- Support independent UI renderers, voice interfaces, and agentic clients through
  the same semantic interface.
- Add screen-local `*_rpc.cpp` adapters, not a central serializer that knows all
  screens. Keep the existing game and its screen/input loops.
- Make player-observable game state globally queryable, independently of which
  screen is open. Do not make clients navigate screens just to read known facts.
- Keep static method contracts separate from object/view responses. Optional
  discovery is explicit; no action schemas attached to ordinary queries.
- Publish turn and UI lifecycle notifications. Clients derive incoming-fleet
  alerts, briefings, economic warnings, and agent policies from queried facts.
  There is no server-side empire-briefing subsystem.
- Use JSON-RPC 2.0 over WebSockets. No compatibility with the fork's binary TCP
  protocol, struct layouts, synthetic screen numbers, or old clients is required.
- Preserve player-visible behavior, game rules, saves, mods, deterministic RNG,
  and existing multiplayer authority. Protocol incompatibility is not permission
  to change those independently.
- Cover every fork protocol feature and amendment, including combat rendering,
  audio, films, setup, and multiplayer UI. Small initial milestones are not the
  final coverage target. Unsafe memory exposure is deliberately replaced, not
  preserved under the label of parity.

### Evidence baseline and meaning of 100% coverage

| Item | Pinned revision / inventory |
| --- | --- |
| Target checkout | `09d4f6107e4043765372421b5d63e74a85f96597` |
| Reviewed P0 target (engine unchanged by the P0 artifact commit) | `e57a55b2b86f53ad42186312a2efb2d2a8a797c0` |
| Data-crea fork, `orionlayer` | `25fdf4209adf11f0a97433576709537946e3d270` |
| Common ancestor | `6f86fc0e8072d0ce6f5d50bb6e72bc0dc85bdb3a` |
| Fork diff from ancestor | 61 files; 8,279 insertions, 5 deletions |
| Fork-only history | 119 commits: 92 touching non-documentation files, 27 documentation-only |
| Protocol inventory | 7 server messages, 14 client messages including HELLO; 38 tagged snapshot blocks plus untagged base state/icon owners |
| Additional inventories | 18 TPOP kinds; 19 nonzero MPLY phases; 7 combat commands; 25 combat event types |
| Native screens | 37 named SCREEN constants, including unknown/unused/exit values |

Coverage means every source change has an explicit disposition and acceptance
test, not byte-for-byte reproduction or a claim that this plan implements it.
Appendices A-H are the coverage ledger. A milestone cannot claim full parity
until its rows have implemented, player-visibility-tested equivalents. New fork
revisions require a new diff/inventory, not an assumption of continued coverage.

Primary fork evidence: `src/ext/ext_server.{cpp,h}`, `src/ext/ext_api.{cpp,h}`,
`src/ext/ext_layout.*`, `README.md`, and `docs/extension-api.md` at the pinned SHA.
Review URL: <https://github.com/Data-crea/orion2re/tree/25fdf4209adf11f0a97433576709537946e3d270>.

## 2. Architecture and ownership

### Required dependency direction (user-directed sidecar boundary)

The game is RPC-unaware. Changes in native game code are limited to neutral
subscription/publication points for existing lifecycle, state-commit and safe
observation boundaries. Native code must not include RPC headers, name RPC types,
call RPC functions, build DTOs, dispatch remote work or branch on RPC build/runtime
settings. Gameplay rules and native control flow do not depend on callbacks.

`src/game/game_events.*` owns a small bounded synchronous observer API. Its events
and lifetime scopes describe native facts (thread start/stop, world replacement
success/failure, resolution, perspective selection, map/idle observation points),
not protocol methods. Registration/unregistration occurs while quiescent or on
the publishing thread. Removing a listener during publication prevents later
delivery to its expired context; listeners added during delivery wait for the
next event. Callbacks observe only, do not throw, and must not recursively mutate
the game. Any future borrowed event payload must be native/read-only and remain
valid only for the callback duration, never an RPC DTO or retained pointer.

Application composition (`src/main.cpp`) connects optional modules to the game
before its thread starts and disconnects them after it joins. RPC implementation
under `src/rpc/` subscribes callbacks, reads native state and uses existing native
APIs on the game thread. It owns all authentication, queues, identities,
projection, JSON, transport and failure isolation. Its observer contains internal
failures rather than changing game behavior. The thread-stopping publication
precedes native resource teardown so observers can wake and join their own work.

Dependency arrows are `game -> neutral events` and `RPC -> game APIs/events`;
there is no `game -> RPC` dependency. Source/architecture tests must enforce this.
Existing game APIs are reused; any API addition or shared-operation extraction
beyond neutral hooks requires separate approval, not an implicit RPC refactor.

```text
Renderer / voice client / agent
           | requests, responses, subscribed notifications
WebSocket service thread
           | bounded copied requests / immutable publications
Game-thread RPC bridge
           | global queries + active-context action dispatch
Existing game state, rules, screen loops, and local UI
```

Proposed components (names below are new, not existing APIs):

| Component | Responsibility |
| --- | --- |
| `src/rpc/rpc_protocol.*` | Strict JSON-RPC envelope, errors, method descriptors, typed conversion utilities |
| `src/rpc/rpc_service.*` | WebSocket lifecycle, authentication, subscriptions, limits, output queues |
| `src/rpc/rpc_bridge.*` | Game-thread requests, immutable results, controller identity, request lifecycle |
| `src/rpc/game_rpc.*` | Sidecar subscription callbacks and native player-scoped snapshot capture |
| `src/rpc/rpc_game.*` | Immutable projected captures, lifecycle identities, query composition and pagination |
| `src/rpc/<owner>_rpc.*` | Owner-specific adapters subscribing to neutral native events and using existing native APIs |

Use plain structs, explicit registration tables, and free functions per the
repository style guide. No reflection over native game structs, plugin hierarchy,
or generic remote-object framework. Separate the RPC service from the opaque
multiplayer router; do not add gameplay authority to `src/network/router_*`.

Reuse yyjson in strict JSON mode. Evaluate a maintained C-compatible WebSocket
backend (libwebsockets is a candidate); approve/pin one only after license,
maintenance, limits, Windows x86/x64, macOS, Linux, and MinGW checks. Reuse libuv
where the backend supports it, without coupling to the router's protocol.

### Screen-local integration

An RPC-side adapter owns its static method table, capture and action handlers.
The native owner publishes neutral initialized/observation/leaving events; only
the subscribing sidecar registers contexts, pumps RPC work and converts views:

```text
initialize native screen state
publish native owner-initialized event
repeat existing screen loop:
    publish native observation/decision-point event
    handle native input through existing operations
    draw/update as before
publish native owner-leaving event before releasing borrowed state
```

The sidecar's context is not native/public state: only copied authorized values
cross threads. Native event payloads do not escape a callback. Neutral nested
owner events let RPC push/restore its own contexts; field-replacement events let
RPC increment its own generation. The engine stores no RPC context or generation.
Do not change `MOX::current_screen` to report RPC-only context identity.

Reuse existing operation functions. Do not extract or modify native gameplay
operations for RPC without separate approval. Do not duplicate input branches or game rules in the sidecar.
Prefer semantic actions to native field IDs. Shared text/list/confirmation
adapters amortize work across screens; per-screen action schemas remain static.

Long-running operations need separate observational/action pump boundaries:
cinematics do not call `Get_Input`, and combat animation polls are not normal
decision points. Prevent recursive mutation dispatch, but allow answering an
explicitly registered nested decision through a deferred intent/continuation
consumed by that dialog's own loop. No unrestricted dispatcher reentry.

### Requests, subscriptions, and bounds

- The service thread owns sockets and encoded output; only the game thread reads
  mutable game/UI state or calls rules. SDL renderer/window work stays on the
  platform thread through copied requests.
- A global query is captured coherently on the game thread at an allowed point;
  if resolution is in progress, return busy or an explicitly dated last snapshot,
  never a half-updated world. Service heartbeats remain independent.
- Support selected-topic subscriptions with an atomic baseline + cursor. Start
  with whole replacements of changed requested topics or invalidation events;
  no engine-wide dirty flags or field-delta protocol is required.
- Coalesce unsent state into a new baseline. Do not silently lose operation
  results or ordered combat/audio events: bound queues and signal a stream gap
  or disconnect. Never rewrite a partially transmitted message.
- Bound connections, input bytes/depth, pending work, result retention, output
  bytes, capture frequency, and commands/time per game poll. Measure largest-game
  snapshots before choosing output caps; overflow is explicit, not unbounded.
- A neutral thread-stopping event must cover normal exit and exceptions at
  `MOX2::thunk_main2`; the RPC observer stops, cancels unstarted work, wakes and
  joins before dependent native state dies, without a future game poll.

## 3. Player-visible-only contract (release blocker)

"What the player sees" means knowledge legitimately available through normal
game UI, even when another screen is currently open. It does not mean all engine
memory, debug-overlay data, future random outcomes, or every byte needed by AI.

### Required rules

1. Bind a connection to the active local player perspective. No caller-selected
   `viewerPlayerId`, spectator/omniscient switch, host-memory privilege, or remote
   Lua/debug execution. Legitimate in-game omniscience follows normal game rules,
   not a transport permission.
2. Filter before encoding, caching, sorting, pagination, totals, events, errors,
   and revision comparison. Hidden-only changes must not invalidate public views.
3. Authorize fields as well as objects: observing a ship does not reveal its full
   design, destination, ETA, cargo, orders, or an opponent's technology/economy.
   A scan/detail query receives only what the native scan/detail UI permits.
4. Omit unknown entities; make unknown fields explicitly unavailable. Distinguish
   unknown from known-empty and last-known from current. Do not invent last-known
   knowledge absent in the native UI or replace old knowledge with hidden live data.
5. Unknown and nonexistent IDs have the same public error. IDs are opaque per
   game/perspective and cannot expose global slot counts through numbering/gaps.
   Underlying slot/generation validation stays internal.
6. List filters/search cannot query hidden properties. Sort only the authorized
   projection. Snapshot metadata counts visible records, not global allocations.
7. Turn/reset metadata comes from committed player-visible state. Load/new game
   changes game identity; hotseat switching revokes the old view, clears queued
   publications/results, and requires a fresh configured-key handshake before exposing
   the next player's state. Existing connections do not silently follow players.
8. All action paths enforce local ownership, native phase/authority, and current
   context. Legal-target/movement previews cannot become oracles for hidden units,
   AI intentions, unrevealed defenses, or future outcomes.
9. Events, retained combat results, modal text/format arguments, resource IDs,
   media, and sound streams pass the same policy. An unseen battle elsewhere is
   not observable merely because this process calculates it.
10. Each DTO field has a documented native UI evidence source and visibility
    predicate before implementation enables it. Unaudited fields fail closed.

### Concrete source anchors and traps

| Area | Source evidence and required treatment |
| --- | --- |
| Systems/planets | `HAROLD::Player_Has_Visited` (`harold.cpp:416`), `Star_Owner(star, viewer)` (`:809`), and `PLNTSUM::Filter_Explored_Planets` (`plntsum.cpp:976`) demonstrate perspective rules. Positions visible on the native map may be public while names/owners/specials/planet details remain unknown. |
| Ships | `SHIPSTAK::Find_Ship_Stacks` / `Remove_Non_Detected_Ships` (`shipstak.cpp:45-258`) rebuild and filter display nodes. Extract read-only policy or capture during a normal authorized refresh; do not invoke this mutable builder for queries or walk stale/unlinked nodes. |
| Routes/ETA | `SHIPS::Do_Ship_Destination_Lines` (`ships.cpp:422-479`) gates enemy routes; `Print_Eta_On_Ship_Icon` (`:482`) reads cached ETA. Global enumeration must reproduce those disclosure rules, not expose every engine destination. |
| Diplomacy | `Enemy_Ship_Heading_Toward_Our_Colony` (`ships.cpp:401`) means non-own, not necessarily at war. Return known relationships separately; clients classify threats. |
| Research/designs | `tech.cpp`, `info.cpp`, `racescrn.cpp`, `cmbtdrw1.cpp`: distinguish own research/choices, publicly known reference material, and scan/intelligence-authorized enemy details. No raw opponent research arrays. |
| Lua | `src/lua/lua_game_*.cpp` offers naming/identity patterns, not a player-visibility layer. Its callback capabilities and generation-only mutations are not RPC authorization. Never expose `game.rng`. |
| Reports | `REPORT::Display_Report_Aux`, `Must_Display_Reports`, and message display can consume reports, redirect ships, or open dialogs. Read copied player-authorized data; never run these as query helpers. |
| Assets/config/saves | No arbitrary LBX entry, filename, directory, save bytes, config dump, mod storage, or resource-path query. Resource access is bounded to allowlisted public reference/current-view content; saves expose local slot metadata and native slot actions only. |

## 4. Common API catalog

All names here are proposed. A group is an allowlisted domain contract, not an
instruction to expose all fields/functions of its native owner. `list/get` share
one visibility policy. Fields that require an active scan, negotiation, lobby, or
dialog return unavailable outside that context, rather than opening it as a read.

| Group / common methods | Public content | Native owner / reuse candidates |
| --- | --- | --- |
| `rpc.hello`, `rpc.describe`, `rpc.subscribe`, `rpc.unsubscribe` | Authentication, build/contract identity, explicit method discovery, selected event/state subscriptions | New infrastructure; static descriptor table, not per-object schemas |
| `game.getState(include)`, `game.getSession` | One coherent requested snapshot; game identity, stardate/turn, local perspective, busy/lifecycle status, permitted settings | `mox2.cpp`, `nextturn.cpp`, `netturn.cpp`, `newgame.cpp` |
| `game.listPlayers`, `game.getPlayer` | Contacted/otherwise known empires, displayed names/colors/government/known traits; own economy, food, research, freighters, command points | `mainscr_main.cpp`, `racescrn.cpp`, `info.cpp`, Lua player naming |
| `game.listSystems`, `game.getSystem` | Visible map coordinates, known names/owners, explored content, wormhole/range information when disclosed | `harold.cpp`, `haccess.cpp`, `geo.cpp`, `shipmove.cpp` |
| `game.listPlanets`, `game.getPlanet`, `game.listNebulas` | Known orbit/type/ecology/minerals/specials, colonization suitability and map nebulas where shown | `plntsum.cpp`, `harold.cpp`, `sys.cpp`, Lua planet/system naming |
| `game.listColonies`, `game.getColony` | Own population/jobs/races, buildings/satellites, production/queue, costs/turns/buy eligibility, economy/events, troops, relocation/transport settings; foreign fields only where native intelligence reveals them | `colony*.cpp`, `colsum.cpp`, `colcalc*.cpp`, `colbldg.cpp`, `colxport.cpp`, `newpup.cpp` |
| `game.listShips`, `game.getShip` | All observable ships, own detailed state; observed enemy fields only; route/destination/ETA only if knowable | `ships.cpp`, `shipstak.cpp`, `shipmove.cpp`, `haccess.cpp`; generation pattern in `lua_game_ship_identity.h` |
| `game.listFleets`, `game.getFleet` | Group authorized visible ships; locations, visible membership, orders where known. Fleet IDs must not be raw rebuilt stack indices | `shipstak.cpp`, `fleetpop.cpp`, `flt1.cpp`, `flt2.cpp` |
| `game.getMovementOptions` | Native legal movement verdicts for specified own ships and known destinations, including refusal reasons the native UI reveals | `SHIPMOVE::Ships_Try_To_Move_To`; audit scratch/global side effects before sharing; apply through `Apply_Player_Movement_Order`, not query helpers |
| `game.listDesigns`, `game.getDesign`, `game.getDesignOptions` | Own templates, unlocked components/mods, displayed costs/space/speed; enemy design only at actual scan knowledge level | `design*.cpp`, `desbox.cpp`, `cmbtdrw1.cpp`; audit calculator mutation |
| `game.getResearch`, `game.listResearchOptions`, `game.getTechnology` | Own progress, selectable projects, known technology descriptions and costs; no enemy unlock state | `tech.cpp`, `techdata.cpp`, `science.cpp`, `info.cpp`; `Commit_Selected_Entry` is an operation, not a query |
| `game.listLeaders`, `game.getLeader` | Owned leaders and actually offered/discovered leaders; assignments, skills, cost/maintenance, travel ETA as shown | `officer.cpp`, `mainpups.cpp`; never enumerate all hidden/generated heroes |
| `game.getRelations`, `game.getIntelligence` | Known contacts, own treaties/offers/payments and own spy/agent allocations/missions, visible relation indicators | `racescrn.cpp`, `dip_scrn*.cpp`, `diplomac.cpp`, `spy.cpp` |
| `game.getHistory`, `game.listMessages`, `game.getMessage` | Charts at native granularity/precision and known player coverage; player's report/news/turn messages, links to authorized entities | `info.cpp`, `bill.cpp`, `msg.cpp`, `report.cpp`, `turnsum.cpp`; not hidden exact history behind a coarse chart |
| `ui.getContext`, `ui.getView`, `ui.getControls` | Active context stack, initialized local view, selection, list/scroll/text values, visible interaction tokens; no schemas or hidden cheat hotkeys | Screen sidecars, shared `fields`, `textbox`, `gendraw`, `namestar` adapters |
| `ui.open`, `ui.back`, `ui.close` | Request ordinary navigation through existing flow; actual transition reported separately | Owner's native transition rules, not arbitrary writes to `MOX::current_screen` |
| `ui.activate`, `ui.cancel`, `ui.setText`, `ui.setValue`, `ui.select`, `ui.point` | Shared interactions for registered visible controls; grid coordinates/right-click help where applicable; not an unfiltered key/mouse channel | Typed field adapters plus context-specific dispatch |
| `setup.getView`, `setup.listSaveSlots`, `setup.getHallOfFame` | Main menu/new game/race/customization/flags, local save slot descriptions/dates, loaded high-score table | `mainmenu.cpp`, `newgame.cpp`, `racesel.cpp`, `raceopt.cpp`, `loadsave.cpp`, `score.cpp` |
| `lobby.getView`, `lobby.listSessions`, `lobby.getChat` | Native advertised sessions, setup/position/roster/status text, local chat, ready state | `multplay.cpp`, `hotpop.cpp`, `netstart.cpp`, read-only `netadapter`/`netcode` views; never a network pump inside a query |
| `combat.getView`, `combat.getUnit`, `combat.getTargetOptions`, `combat.getResult` | Active observed battle, authorized scan/weapon details, legal cells/targets, ordnance, board/options dialogs, retained player-visible outcome | `combinit.cpp`, `combat1.cpp`, `cmbtdrw1.cpp`, `cmbt*.cpp` |
| `invasion.getView` | Active bombardment/ground combat, displayed troops/strength/results and rendered effects | `mainpups.cpp`, `colgcbt.cpp`, `newpup.cpp`, `flcqueue.cpp` |
| `reference.get`, `media.getResource`, `presentation.getFrame`, `presentation.getState` | Allowlisted reference text/IDs and current rendering resources, atomic game frame+palette, film progress, language/local rendering/audio options | `info.cpp`, `fmtpara.cpp`, VFS, `video.cpp`, `platform.cpp`, `smack.cpp`; no OS paths |

### Common operations (not arbitrary setters)

| Operation family | Actions to expose through shared native behavior |
| --- | --- |
| Turn/navigation | End/submit turn, cancel native waits where permitted, open normal screens, focus known system/colony/fleet, map pan/zoom |
| Colony | Assign jobs, same-system/interstellar population transfer, select/buy production, edit/reorder queue, refit selection, relocation, demolish with original confirmation/refund, transport and troop interactions |
| Fleet | Select subset, move, merge/relocate, engage/ignore, colonize/outpost, transport/attack, scrap, inspect, assign leader; native eligibility and confirmation required |
| Research/design | Select application/group, review, edit template/name/hull/components/mods, accept/cancel, mutation/race choices when that setup context owns them |
| Leaders/economy/intelligence | Hire/reject/assign/dismiss, tax rate/native sliders, spies and missions as a checked list |
| Diplomacy | Open audience, choose currently offered response, treaty/gift/demand/money/technology/system choices; never inspect or precompute hidden AI reactions |
| Setup/session | Native new/load/save-slot/options, race/color/name choices, host/join/position/ready/chat; existing multiplayer synchronization paths |
| Combat/invasion | Move, face, fire, select weapon mask, shoot missile, select unit, remove stasis, board/raid/marines, done/wait/auto/retreat/options as available; bombard/invade/cease fire and ground-combat controls |
| Presentation | Window show/hide, local speed/VSync/audio ownership options, skip a skippable film, ordinary visible shortcuts; no developer keys, cheats, RNG or console |

Screen-only actions remain context-bound until a shared operation can safely run
without that screen. Expose an independent domain operation only after auditing
its rule, recalculation, mod, confirmation, and multiplayer paths. Do not implement
both independently. For example, current `TECH::Commit_Selected_Entry` is already
a useful shared operation; slot-based pointer-driven research selection is not.

## 5. Notifications, consistency, and action results

Minimal default notifications:

- `game.turnAdvanced { gameId, turn, revision }`: new committed world state can
  be queried. Not inferred from UI transitions or a button press, and not a claim
  that all research/report choices finished. Single-player and network receive
  paths converge on one publication routine; deduplicate by game/turn.
- `ui.contextChanged { contextId, kind, parent, revision }`: an initialized active
  context changes; map visibility behind a modal does not imply active map input.
- `game.sessionChanged`: loaded/new/ended/unloaded or perspective revoked. Clears
  stale handles/publications, without masquerading as another turn.

Optional subscribed streams: `state.changed`, `ui.viewChanged`, operation
completion, ordered combat/render/audio events, film/frame updates. No automatic
incoming-ship or empire-warning events. A client combines ships, colonies, and
known diplomacy after `game.turnAdvanced` to produce its own notifications.

`game.getState(include)` captures requested collections at one revision and turn.
Pagination, if necessary, pins an immutable snapshot and checks perspective
revocation. Never combine an old ship list with a new colony list silently.
Subscriptions establish the baseline and cursor atomically; reconnect gets a new
baseline. Event sequences detect gaps; no durable replay system is required.

Actions carry context/generation and entity references, have validated typed
parameters, and return success/refusal or an operation ticket. Queued is not
applied; dispatched legacy input is not proof of gameplay effect. Keep bounded
results, duplicate-request protection within the live control session, cancellation
of unstarted work, and explicit partial outcomes. No exactly-once promise after
disconnect. A blocking native confirmation becomes a registered child context;
do not suppress it or recursively drain the full command queue.

## 6. Transport/security and presentation

- Optional build and runtime enablement; router-only builds do not acquire RPC.
  Loopback only initially. Bind failure disables RPC with a diagnostic.
- Configured local `interface rpc_key`, matched against the client's first
  `rpc.hello` request (user-directed simplification of the P0 authentication proposal).
  Reader/controller separation, one active controller,
  explicit Origin/Host checks, authentication deadlines. Browser authentication
  uses the first message, not a secret in a URL or an assumed custom header.
- Validate message size/depth, method arguments, IDs, list lengths and deadlines
  before queueing; recheck ownership/context at execution. Rate-limit discovery,
  queries and actions. Logs do not include secrets or hidden payloads.
- No raw native records, Lua eval, filesystem browsing, arbitrary setting writes,
  cheat/debug input, or unscoped keyboard injection. Ordinary shortcut behavior
  maps to authorized context actions, including function/Alt keys where legitimate.
- Frame capture uses an explicit snapshot owner and matching frame/palette
  generation. Never copy the presenter surface from another thread while mutable.
- Combat/audio streams require an active authorized battle and subscriber.
  Preserve logical time, ordering and effect RNG results already chosen by the
  engine. Do not draw new random values for external animation.
- External audio/pacing is an explicit presentation-owner mode with local fallback
  on disconnect, not a consequence of any reader connecting. Timed decisions and
  cinematics keep their intended timing. A hidden window still services frame
  handoff; fully headless execution is a distinct extension, not claimed here.
- Language/asset installation selection is local configuration. Preserve the
  fork's alternate-language resource precedence via trusted local setup, never
  accept remote directories/filename lists. No proprietary assets in fixtures.

## 7. Delivery sequence and acceptance

| Phase | Scope | Completion gate |
| --- | --- | --- |
| P0: contracts and evidence | Approve field allowlists, static registry, backend/license/pin, local authentication, safe points, source-ledger fixtures | Every planned method has ownership/perspective rules; largest-game capture/output budget measured |
| P1: global read foundation | Query bridge, ships/systems/planets/colonies/players/research/leaders/relations/history/messages, coherent snapshots, turn/reset notifications | Two equal visible worlds with different hidden data produce equal public responses; client can derive incoming threats without a briefing API |
| P2: shared UI and context | Context scopes/stack, native screen entries, text/list/help/confirmation/value adapters, safe intent queue | Local and RPC actions share paths; stale identical-looking dialogs cannot consume old actions; no hangs on nested prompts |
| P3: strategic sidecars | Map/colony/queue/refit/fleet/leader/research/design/diplomacy/Info/Planets/Colonies interactions | All strategic block/command rows covered; all numbers/selections/options match native views; no duplicated rules |
| P4: setup, reports and multiplayer UI | New/load/save/options/race/flags, all TPOP and MPLY contexts, council/news/turn reports, hotseat revocation | All screen/subcontext rows have entry/exit/action tests; host/client/chat/position flows use native networking authority |
| P5: battle and invasion parity | All combat views, 7 command ops plus normal controls, 25 event types, ground effects, scan/results/refusals | All combat effect rows emitted from authorized engine hooks; actor/target/viewport safety, hidden-detail filtering, RNG invariance and real battles verified |
| P6: presentation and full parity | Atomic visual frames, resources, films/skip, sound/timing/window/language support and companion fixes | Every ledger row has evidence/test/disposition; disconnect restores local interaction/audio; full supported platform matrix passes |

### Required tests

- **Noninterference:** pair saves/states differing only in undetected ships,
  enemy queues/research/leaders, AI intentions or RNG; compare queries, IDs,
  counts, pagination, errors, previews, notifications and revisions. No difference
  in authorized outputs. Also audit request timing/lookup paths for obvious
  hidden-ID or enumeration oracles; do not claim constant-time gameplay.
- **Native-view parity:** fog/scanners/stealth, known destination vs unknown,
  research/scan disclosure, diplomacy system picker, charts, in-progress popups,
  combat and ground effects. Own and enemy details must be tested separately.
- **Lifecycle:** subscribe during change, late join, load/new/end, hotseat switch,
  context recreation, field mark/release/restore, busy turn, slot reuse,
  cancelled next-turn submission, reports/choices before map, auto-turn and net
  decode. Loading never emits a false turn-advanced event.
- **Actions:** every named operation, refusal, stale entity/context, duplicate,
  expired request, local/remote conflict, modal nested while dispatching,
  partial colonist transfer, demolish confirmation/refund and no offscreen target.
- **Streams:** all 25 event kinds, serial/time/order, multiple subscribers,
  initial combat state, overflow gap/reset, film skip left/right/key, frame count
  semantics, atomic palette, input-delay and animation polls, audio ownership.
- **Protocol:** malformed/oversized/deep JSON, wrong method/type/version,
  fragmented/coalesced WebSocket messages, close/ping, Origin/Host/auth failures,
  slow readers, request floods, bounded queues, partial output, quit during I/O.
- **Schema/documentation:** regenerate the versioned schema from canonical
  method/DTO/field-policy definitions; render human-readable documentation from
  that schema; check deterministic output, local references, field policies,
  schema validity and stale-artifact rejection. CI must detect unregenerated
  schema/reference changes without needing a live game or the fork Git objects.
- **Compatibility/performance:** existing doctest suite, saves/modded rules/RNG
  comparisons against native play, feature off/on, no client vs subscribed client,
  no socket waits on game thread, maximum games and sanitizer/thread checks.
  Native macOS/Linux/Windows x86/x64 and supported MinGW builds are required.

## Appendix A. Complete native SCREEN inventory

Evidence: `src/game/orion2_consts.h:507-544` and
`MOX2::Screen_Control` in `src/game/mox2.cpp:34-216`. Adapter names are proposed;
one sidecar may serve related modes. Unknown/exit/processing states are listed
without pretending each is an interactive screen.

| Native constant | ID | Existing entry / owner | Proposed RPC context and operations |
| --- | ---: | --- | --- |
| SCREEN_UNKNOWN | -1 | No switch case | unavailable; never a navigation destination |
| SCREEN_MAIN | 0 | `MAINSCR::Main_Screen`, `mainscr_main.cpp` | map; select/focus/pan/zoom/fleet orders/open screens/end turn |
| SCREEN_COLONY | 1 | `COLONY::Colony_Screen`, `colony_main.cpp` | colony; jobs/production/buy/buildings/transports/relocation |
| SCREEN_DESIGN | 3 | `DESIGN::Design_Screen`, `design_main.cpp` | design editor and picker children |
| SCREEN_FLEET | 4 | `FLT1::Fleet_Screen`, `flt1.cpp` | fleets; subset/filter/sort/scroll/move/scrap/relocate/inspect |
| SCREEN_RACE | 6 | `RACESCRN::Race_Screen`, `racescrn.cpp` | contacts/intelligence; spies/missions/audience |
| SCREEN_EXIT | 7 | Return from dispatcher | session ending, cleanup; no view/actions |
| SCREEN_GAME | 8 | `LOADSAVE::_Game_Popup`, `loadsave.cpp` | game menu/options/load/save children |
| SCREEN_INFO | 9 | `INFO::Info_Screen`, `info.cpp` | history/tech review/race stats/reports/reference children |
| SCREEN_MAIN_MENU | 10 | `MAINMENU::Main_Menu_Screen`, `mainmenu.cpp` | new/load/multiplayer/Hall of Fame/credits/exit |
| SCREEN_UNK2 | 11 | No switch case | unused/unsupported, not a new invented screen |
| SCREEN_MAIN_NEXT_TURN | 12 | `NEXTTURN::Next_Turn`, `nextturn.cpp` | processing; do not infer committed turn from entry/exit |
| SCREEN_NEW_GAME | 13 | `NEWGAME::Newgame_Screen`, `newgame.cpp`; initialization | settings, race child, accept/cancel |
| SCREEN_HALL_OF_FAME | 14 | `SCORE::Hall_Of_Fame_Screen`, `score.cpp` | loaded sorted high scores, close |
| SCREEN_MULTI_PLAYER | 15 | `MULTPLAY::Multi_Player_Screen`, `multplay.cpp` | setup and online setup child |
| SCREEN_HOTSEAT | 16 | `HOTPOP::Hotseat_Screen`, `hotpop.cpp` | local roster/race setup, accept/cancel |
| SCREEN_HOTSEAT_SELECT_PLAYER | 17 | `MULTPLAY::Hotseat_Select_Player` | authorized position switch; revoke old perspective |
| SCREEN_PLANET_DATA | 18 | `HAROLD::Planet_Data_Screen`, `harold.cpp` | player-known planet details, close |
| SCREEN_COLONY_SUMMARY | 20 | `COLSUM::Colony_Summary_Screen`, `colsum.cpp` | list/sort/jobs/buy/transfers/open colony |
| SCREEN_START_NET | 21 | `NETSTART::Start_Net_Screen`, `netstart.cpp` | host phases in Appendix C |
| SCREEN_JOIN_NET | 22 | `NETSTART::Join_Net_Screen` | join phases in Appendix C |
| SCREEN_QUEUE_POPUP | 25 | `COLBLDG::Build_Queue_Popup`, `colbldg.cpp` | products/queue/modes/refit, accept/cancel |
| SCREEN_DISTANCE_BETWEEN_STARS | 28 | `MAINSCR2::Distance_Between_Stars_Screen`, `mainscr2.cpp` | known-system distance selector |
| SCREEN_OFFICERS | 29 | `OFFICER::Officers_Screen`, `officer.cpp` | owned/offered leader list, hire/assign/dismiss/inspect |
| SCREEN_COLONIZATION_IN_MAIN | 30 | `MAINSCR2::Planet_Colonization_In_Main_Screen` | colony selection/name/confirmation children |
| SCREEN_BUILDING_OUTPOSTS_IN_MAIN | 31 | `MAINSCR2::Building_Outposts_In_Main_Screen` | outpost selection and confirmation |
| SCREEN_PLANET_SUMMARY | 32 | `PLNTSUM::Planet_Summary_Screen`, `plntsum.cpp` | known planets/filters/sort/send mode/ETA/ship choice |
| SCREEN_COLONY_LANDING | 33 | `COLLAND::Colony_Landing_Screen`, `colland.cpp` | landing presentation/name/dismiss |
| SCREEN_UNLOADING_TRANSPORTS_IN_MAIN | 34 | `MAINSCR2::Unloading_Transports_In_Main_Screen` | troop destination/confirmation |
| SCREEN_SCRAP_FREIGHTERS | 35 | `MAINSCR2::Scrap_Freighters` | slider child and confirmation |
| SCREEN_TECH_CHANGE | 36 | `TECH::Tech_Change`, `tech.cpp` | change research/group/details/accept |
| SCREEN_NET_NEXT_TURN | 37 | `NETTURN::Net_Next_Turn`, `netturn.cpp` | wait/ready/chat; commit only after authoritative state applied |
| SCREEN_REPORTS | 39 | `MAINSCR2::Reports_Screen`, `mainscr2.cpp` | report orchestration and registered decision children |
| SCREEN_TURN_SUMMARY | 40 | `TURNSUM::Turn_Summary_Popup`, `turnsum.cpp` | messages/pages/colony links/dismiss |
| SCREEN_LOAD_NET | 41 | `NETSTART::Load_Net_Screen` | load/position/host phases; do not rely on previous_screen aliasing JOIN_NET |
| SCREEN_SHOW_COMMAND_POINTS | 43 | `MAINSCR2::Show_Command_Points_Screen` | authorized command-point detail/dismiss |
| SCREEN_CREDITS | 44 | `MAINMENU::Credits_Screen`, `mainmenu.cpp` | credits/dismiss |

Enum gaps (2, 5, 19, 23-24, 26-27, 38, 42) are not additional declared screens.

## Appendix B. Nested-context inventory (not just SCREEN values)

Fork synthetic numbers identify evidence only; new contexts use named kinds and
instance IDs. Rows cover all fork-specific contexts plus current native nested
interaction families. Generic boxes/lists are children, not schema-bearing game
objects. Entries with native state 33/40 are also represented in Appendix A.

| Fork ID / TPOP kind | Context and owner | Required state / semantic interaction |
| --- | --- | --- |
| 50 | custom race, `racesel.cpp` / `raceopt.cpp` | name/traits/picks/government, choose/accept/cancel |
| 51 | race selection, `RACESEL::Race_Selection_Screen` | available races/name/color path; never confused with diplomacy SCREEN_RACE |
| 52 / 1 | science room, `SCIENCE::Science_Room` | discovered applications/modifiers, researched/stolen/artifact/captured reason, shown progress/footer/dismiss |
| 53 | initial research selection, `TECH::_Tech_Select` | own valid entries/group list/selection; direct choice not pointer inference |
| 54 | generic shield/computer picker, `desbox.cpp` | offered replacements, costs/space/stats/current choice |
| 55 | weapon picker, `desbox.cpp` | weapons/count/arcs/mods/ammo/selection |
| 56 | special-system picker, `desbox.cpp` | allowed specials/space/selection |
| 57 / 58 | player/AI-initiated audience, `dip_scrn_main.cpp` | displayed statement/reply/menu/ambassador; existing negotiation actions |
| 59 / 3 | leader hire, `MAINPUPS::Random_New_Officer_Popup` | actual offer/cost/maintenance/offer state/choice; parent Leaders screen may keep native 29 |
| 60 / 4 | colony/outpost/drop-troops planet choice, `mainpups.cpp` | shown system/planets/special and selection modes |
| 61 / 5 | system discovery, `MAINPUPS::New_System_Discovery_Popup` | revealed system/special text and dismiss |
| 62 / 6 | leader promotion/marooned hero, `MAINPUPS::Officer_Gains_Level_Popup` | disclosed leader/state/system, dismiss |
| 63 / 7 | GNN/news, `EVENTS::Drive_Event_Screen` | actual formatted news/picture and authorized arguments; not hidden event internals |
| 64 / 8 | battle target choice, `MAINPUPS::Defense_Colony_Selection_Popup` | displayed system/target candidates, choose/cancel |
| 65 | tactical battle, `COMBINIT::Tactical_Combat` | battle identity/view and native actions; parent processing state stays unchanged |
| 66 / 67 | battle scan / boarding, `CMBTDRW1::Detailed_View_Ship` / `Board_Popup` | permitted scan fields; board message/range/marines/choice |
| 68 / 10 | invasion/bombardment choice, `MAINPUPS::Colony_Combat_Popup` | shown troop/pop/building/shield totals/options, explosions, original choices |
| 69 / 11 | ground combat, `COLGCBT::Colony_Combat_Screen` | both displayed troop sides/strength/summary, unit/beam/explosion frames, fast/dismiss |
| 70 / 12 | council, `council.cpp` | nominees/public votes/state/messages/top lines/member order/list choices/vote/ruling |
| 71 / 13 | troop detail, `NEWPUP::Troop_Popup` | shown side/rebels/count/hit points/strength/rendered summary lines |
| 80 / 20 | shared slider, `MAINPUPS::Slider_Popup` | min/max/current/style/text, set/step/accept/cancel; normal freighters supported, cheat invocation not remotely exposed |
| 81 | noncombat ship detail, `CMBTDRW1::Detailed_View_Ship` | authorized ship/subject/name/displayed derived combat record/dismiss |
| 82 / 22 | Antaran room, `ANTAROOM::Main_Antaran_Room_Screen` | actually presented message set/text/dismiss; set follows invasion_stardate, not current turn |
| 83 / 23 | evolutionary mutation, `raceopt.cpp` | baseline picks/current positive and negative picks/group/toggle state/options/accept/refusals |
| 84 / 24 | Dragon bribe, `MAINPUPS::Dragon_New_Star_Popup` | displayed amount/offer or destination mode/eligible stars/formatted message/response |
| 85 / 25 | diplomacy system picker, `DIP_SCRN::Diplomacy_System_Display` | only disclosed eligible system/planet lines, population/homeworld/surrender flags/offer-demand mode/cancel |
| 86 | cinematic, `SMACK::Play_Cinematic` | allowlisted film identity, frames drawn/total, atomic frame stream, original skip semantics |
| 40 / 2 | turn summary, `TURNSUM::Turn_Summary_Popup` | displayed messages/page/colony links/dismiss |
| 33 / 9 | colony landing, `COLLAND::Colony_Landing_Screen` | title/colony/system/planet/name/dismiss |
| no synthetic ID | shared message kinds 1-5, `textbox.cpp` / `gendraw.cpp` | text, timed text, message, warning, confirmation; rendered text/spans and answers, no raw format-code memory |
| no synthetic ID | generic list, `fields::Get_List_Field` | visible item keys/text/enabled/selection/title; choose/cancel |
| no synthetic ID | name/input, `namestar::Input_Box_Popup`, continuous fields | initial/edited text/length/cursor/commit/cancel; never raw caller buffer |
| no synthetic ID | load/save/options menu modes, `loadsave.cpp` | slot metadata, selected slot, valid controls/options; overwrite confirmation |
| no synthetic ID | Info subscreens, `info.cpp` | tech review, history, race stats, turn summary, reference main/how-to/category; navigation/filter/selection |
| no synthetic ID | flag/color and name setup, `racesel.cpp` | flag selection, name fields, accept/cancel |
| no synthetic ID | colony transport/refit, `colxport.cpp` / `colrefit.cpp` | Xport_Popup, Colony_Refit_Popup, Colony_Refit2_Popup; amount/job/destination/candidate ship choices |
| no synthetic ID | map fleet/system boxes, `fleetpop.cpp` / `sys.cpp` / `movebox.cpp` | selected ships/order buttons/scroll/system shown/visible fleet bar and merge-relocation mode |
| no synthetic ID | tax rate, `MAINSCR::Tax_Rate_Popup` | current permitted rate and native choices |
| no synthetic ID | combat options/help, `combat1.cpp` / `cmbtdrw1.cpp` / `textbox.cpp` | options, help regions/text, toggles/dismiss without exposing hidden debug data |
| no synthetic ID | high-score/end-game presentation, `score.cpp` | Hi_Score_Screen, ranking/message/name where native flow permits |
| no synthetic ID | multiplayer nested contexts | all 19 phases in Appendix C; race/name/list/confirmation children share adapters above |

### Current-source input-owner audit checklist

In addition to named screen entrypoints, direct input/list/text/wait callers were
searched across `src/game/*.cpp`. The following owner groups must be covered by
an adapter or explicitly classified as a noninteractive animation poll; they are
not all separate screen kinds:

- `antaroom.cpp`: Main_Antaran_Room_Screen.
- `beams.cpp`: Draw_Shield_Hit; `cmbtai.cpp`: Do_Auto_Ship_Turn;
  `cmbtmov1.cpp`: Wait_Until_Net_Opponent_Finished (animation/wait vs command distinction).
- `cmbtdrw1.cpp`: Detailed_View_Ship, Board_Popup; `combat1.cpp`: Get_Tactical_Input.
- `colbldg.cpp`: Build_Queue_Popup; `colgcbt.cpp`: Colony_Combat_Screen;
  `colland.cpp`: Colony_Landing_Screen; `colony_main.cpp`: Colony_Screen.
- `colrefit.cpp`: Colony_Refit_Popup, Colony_Refit2_Popup; `colsum.cpp`:
  Colony_Summary_Screen; `colxport.cpp`: Xport_Popup.
- `council.cpp`: Control_Main_Council_Screen, Council_Player_Vote,
  Council_Accept_Ruling, Drive_Council_Player_Voted_Winner.
- `desbox.cpp`: Special_Systems_Box, Generic_Replacement_Box,
  Weapons_Replacement_Box; `design_main.cpp`: Design_Screen.
- `dip_scrn.cpp`: Diplomacy_Chat, Diplomacy_Listen_Mode,
  Diplomacy_System_Display, Diplomacy_Demand, Get_Net_Diplomacy_Choices,
  Diplomacy_Generic_System_List.
- `dip_scrn_main.cpp`: Diplomacy_Break_Treaty, Diplomacy_Generic_List,
  Diplomacy_Offer_Gift, Diplomacy_Need_Better_Offer, Diplomacy_Offer_Money,
  Diplomacy_Display_Response, Get_Main_Diplomacy_Choices,
  Diplomacy_Generic_Exchange_Tech_List, Get_Non_Player_Proposals,
  Diplomacy_Fade_In, Diplomacy_Propose_Treaty, Get_Main_Repulsive_Diplomacy_Choices.
- `events.cpp`: Drive_Event_Screen; `fields.cpp`: Get_List_Field, Draw_Field
  (legacy Input_String delegation); `gendraw.cpp`: Message_Box_Exploding,
  Confirmation_Box; `textbox.cpp`: Text_Box_Get_Input, Do_Text_Box, Draw_Help_Entry.
- `flt1.cpp`: Fleet_Screen; `hotpop.cpp`: Hotseat_Screen.
- `info.cpp`: _Tech_Review_Subscreen, Reference_Category_Subscreen,
  History_Subscreen, Race_Stats_Subscreen, Reference_Main_Subscreen,
  Reference_How_To_Subscreen, Turn_Summary_Subscreen.
- `loadsave.cpp`: Do_Load_Game_Popup, Do_Options_Game_Popup,
  Do_Save_Game_Popup, Do_Main_Game_Popup.
- `mainmenu.cpp`: Do_Main_Menu_File_Animation, Credits_Screen, Main_Menu_Screen.
- `mainpups.cpp`: Dragon_New_Star_Popup, Defense_Colony_Selection_Popup,
  Slider_Popup, New_Colony_Selection_Popup, Random_New_Officer_Popup,
  New_System_Discovery_Popup, Officer_Gains_Level_Popup, Colony_Combat_Popup.
- `mainscr.cpp`: Do_Reply_To_Diplomacy_Request, Tax_Rate_Popup, Do_Next_Turn;
  `mainscr2.cpp`: Distance_Between_Stars_Screen; `mainscr_main.cpp`: Main_Screen.
- `multplay.cpp`: Chat_Box_Input_Loop, Multi_Player_Screen,
  Hotseat_Select_Player, Load_Multi_Player_Game_Screen,
  Choose_Multi_Network_Game_Screen, Choose_Network_Plyrs_Screen.
- `namestar.cpp`: Input_Box_Popup; `netstart.cpp`: Net_Select_Race,
  Net_Pick_Position, Join_Net_Screen, Start_Net_Screen, Load_Net_Screen.
- `newgame.cpp`: Newgame_Screen; `newpup.cpp`: Troop_Popup;
  `officer.cpp`: Officers_Screen; `plntsum.cpp`: Planet_Summary_Screen.
- `raceopt.cpp`: Evolutionary_Mutation_Screen; `racescrn.cpp`: Race_Screen;
  `racesel.cpp`: Race_Selection_Screen, Racial_Option_Screen, Flag_Screen.
- `science.cpp`: Science_Room; `score.cpp`: Hall_Of_Fame_Screen, Hi_Score_Screen;
  `tech.cpp`: _Tech_Select, _Tech_List; `turnsum.cpp`: Turn_Summary_Popup.
- `smack.cpp`: Play_Cinematic polls raw keyboard/mouse, outside the field loop.

This is a static source checklist, not a proof of every dynamic route. P2/P4
must instrument context entry/exit and exercise routes, early returns, timed
waits, keyboard-only paths and nested modals. Debug/AI functions are not public
screens merely because they poll input.

## Appendix C. All fork multiplayer phases

Evidence: fork `ext_api.h:426-446` and `ext_api.cpp:1695-1761`. Payload is the
native displayed setup/roster/session/status/chat data, never a raw NETMOX dump.

| Phase | Fork name | Proposed named context / owner |
| ---: | --- | --- |
| 1 | MP_SETUP | multiplayer setup / multplay |
| 2 | MP_ONLINE_SETUP | endpoint dialog / multplay's setup helper |
| 3 | MP_NET_GAME_NAME | game-name input / multplay + namestar |
| 4 | MP_LOAD_GAME | multiplayer slot list / multplay |
| 5 | MP_HOTSEAT_SETUP | local human roster / hotpop |
| 6 | MP_HOTSEAT_SELECT | authorized player switch / multplay |
| 7 | MP_HOST_INIT | host initializing / netstart |
| 8 | MP_HOST_WAIT_JOINERS | host waiting, begin control / netstart |
| 9 | MP_HOST_WAIT_RACES | host race-info wait / netstart |
| 10 | MP_HOST_SENDING | host transfer status / netstart |
| 11 | MP_HOST_PICK_POSITION | loaded-game host position choice / netstart |
| 12 | MP_HOST_WAIT_LOADED | loaded-game roster/wait / netstart |
| 13 | MP_JOIN_INIT | join initializing / netstart |
| 14 | MP_JOIN_GAME_LIST | advertised sessions / multplay |
| 15 | MP_JOIN_WAITING | joined, waiting for host / netstart |
| 16 | MP_JOIN_GENERATING_MAP | join map generation / netstart |
| 17 | MP_JOIN_PICK_POSITION | loaded-game join position choice / netstart |
| 18 | MP_JOIN_GETTING_DATA | loaded-game receive status / netstart |
| 19 | MP_NET_NEXT_TURN | ready indicators/chat/wait / multplay, netturn, combfind |

MP_NONE (0) means no such context, not another screen. Host operations remain
host-only; transferring full state for multiplayer does not authorize RPC to read it.

## Appendix D. All fork message and state payload dispositions

### Messages and base snapshot

| Fork surface | New equivalent / disposition |
| --- | --- |
| MSG_HELLO / MSG_HELLO_REPLY; subscription/capability bits | `rpc.hello`, explicit subscriptions and contract identity; validate/authenticate before data/actions |
| MSG_STATE | typed requested domain snapshots + active owner view; never copy all records |
| MSG_FIELDS | `ui.getControls` / view updates; meaningful visible control tokens, text/value/geometry where useful; no hidden cheat fields |
| MSG_VISUAL | optional atomic frame+palette capture/subscription, on demand only |
| MSG_EVENT | turn/context/session notifications; fork declares game-started/ended flags but Tick only detects screen/stardate changes; implement lifecycle explicitly |
| MSG_SAVE_SLOTS | local slot metadata in setup/loadsave views, both main menu and game popup |
| MSG_LAYOUT; ext_layout generator/table | replaced by explicit typed DTO/method contracts; no native offset/sizeof API or compatibility generator |
| Base identity/map/settings | permitted session/map/preferences only; no raw s_settings or arbitrary player selection |
| Raw players/stars/ships/colonies/planets/nebulas/leaders/Antarans | map to catalog groups after projection. Hidden allocations, opponent internals and Antaran future invasion state are intentionally not exposed |
| Raw ship icons/node tables + untagged icon owner bytes | visible display order/selection/group references in map/fleet views; do not expose stale node records or invisible ships |
| Newgame temporary settings | setup view; distinguish staged edits from committed settings |

### All 38 tagged snapshot blocks

Evidence: actual tag writes in fork `SerializeState`, not just protocol docs.
Rendered order/selection and availability are retained where meaningful; native
field IDs become context-qualified control/entity references.

| Tag | Required equivalent and owner |
| --- | --- |
| FSEL | map fleet-box selection, visible ordered members / fleetpop |
| FLTS | Fleets view: display order, selection, filters, rows/scroll, scan, relocate/merge / flt1+flt2 |
| OFFS | Leaders view: shown lists/selection/assignment/hire modes / officer |
| INFS | Info history scaling/divisors and authorized turn messages / info |
| DIPL | audience/statement/reply/menu/ambassador/offer state / dip_scrn |
| COLS | actual colony shown and current colony/build-popup modes/autobuild settings / colony+colbldg |
| CBLD | displayed building/satellite layout and hit identities / colony |
| CEVT | displayed plague/population-boom/event status / colony |
| CPRD | displayed product cost and turns / colony+colbldg |
| BLDQ | queue under edit, positions/modes / colbldg |
| BLDL | offered product/refit lists, displayed costs/buy/turn values and queue / colbldg |
| DSGN | staged design and derived displayed values, not only saved template / design |
| DSBX | all three offered component pickers and selections / desbox |
| FMOV | native movement verdicts for selected own fleet and disclosed destinations / fleetpop+shipmove |
| MSGB | text/timed/message/warning/confirmation, answers and rendered format substitutions (fork v2) / textbox+gendraw+fmtpara |
| TPOP | all 18 kinds, local arguments plus authorized rendered tails / Appendix B owners |
| HOFM | initialized sorted score table on either entry route / score |
| MPLY | all 19 phases and displayed payloads / Appendix C |
| CMBT | initialized authorized battle identity/units/turn/selection/viewport/weapon state / combat |
| CMSL | visible in-flight missiles/fighters/ordnance / cmbtmis |
| CRES | retained last observed battle outcome before native cleanup rewrites units / combinit |
| CTGT | native legal target/cell/weapon masks, no hidden-target oracle / combat |
| FBSC | actual map fleet-box scroll position / fleetpop |
| CMEV | ordered per-subscriber authorized battle-effect stream with gap handling / Appendix E |
| CPOP | battle scan/boarding subject, message, range and chosen marines / cmbtdrw1 |
| SHPV | authorized noncombat ship detail, editable own name, displayed derived stats / cmbtdrw1 |
| COPT | options panel and its five native option flags / combat |
| HLPL | active visible help regions/text/identities, right-click behavior / fields+textbox |
| RFIT | actual refit candidate list/first row/scroll / colrefit |
| FBTN | displayed ALL/outpost/colonize/engage/transport/attack eligibility and ignore-enemies state / fleetpop |
| INBX | name popup prompt/current/initial/edited text, bounds/style / namestar |
| TXTF | continuous text fields, lengths/editing/committed text / fields |
| SYSW | actual system window, shown state and visible fleet-bar slots / sys+movebox |
| PLSM | planet list/send mode/filters/sort/scroll, rows, best own ship/ETA and displayed hover verdict / plntsum |
| MREL | map's merge-relocation state, distinct from Fleets mode / mainscr |
| STLR | population-transfer operation result incl partial progress, stop/refusal/question outcomes / colmove+settler |
| CSUM | native sorted Colonies list/first row/product/cost/buy/turn values and buy eligibility / colsum |
| FILM | active film identity, frames drawn and total, typed skip + frame subscription / smack |

### All non-HELLO input messages

| Fork input | Required semantic equivalent / safety condition |
| --- | --- |
| MSG_ACTIVATE | activate authorized context control or named action; mandatory generation/meaning check, never bare index/optional rectangle |
| MSG_INJECT_KEY | registered ordinary shortcut/text intent; no unrestricted SDL key injection |
| MSG_INJECT_CLICK | authorized point/grid/control action in logical coordinates; no renderer calls from game worker |
| MSG_CANCEL_FIELD | context cancel/right-click variant with actual native meaning |
| MSG_SET_JOBS | colony assign-jobs list, own colony, checked population/jobs and gesture state, original rule refusals |
| MSG_SELECT_SHIP | explicit subset selection in both map fleet box and Fleets screen |
| MSG_SHOW_WINDOW | platform-thread window show/hide request, synchronized ownership |
| MSG_INJECT_RIGHT_CLICK | logical point right-click/help/face intent; film skip handled semantically |
| MSG_COMBAT_COMMAND | all 7 ops below, actor/battle/context and native action consumption |
| MSG_SET_SPIES | checked list of own spy counts/missions/agent pool through native setters |
| MSG_INJECT_RAW_KEY | ordinary function/Alt shortcuts routed to permitted UI operations; developer/cheat raw codes intentionally excluded |
| MSG_SETTLE_POPS | same-system atomic form and interstellar/question-answer/job/partial-result form; own live destinations only |
| MSG_DEMOLISH | colony/building identity with native refusal, confirmation, removal/rubble/recalc/refund; blocked under other contexts |

## Appendix E. Complete combat command/event coverage

Seven command ops: CCMD_MOVE, CCMD_FIRE, CCMD_FIRE_MISSILE, CCMD_FACE,
CCMD_SELECT, CCMD_REMOVE_STASIS, CCMD_BOARD. Preserve legal movement/target
checks, initiative rules, weapon-mask restrictions, native refusal boxes, boarding
second step and explicit outcome. Keep coordinates/viewport safe before drawing;
an offscreen missile needs a valid native interaction path, not a fabricated field.

All events below are declared in fork `ext_api.h:201-226`; source hook families
are verified in the fork game diff. Replace positional int16 arguments with named
typed fields. IDs, damage and render details are restricted to the observed battle.

| Event | Required semantics / hook owner |
| --- | --- |
| CEV_BEAM_SHOT | source/target/missile flag/slot/weapon/result and damage; include Energy Absorber discharge without a normal slot / cmbtfire |
| CEV_REFLECT | reflection participants and damage / cmbtfire |
| CEV_BOMB | bomb shot, weapon/slot/hits / cmbtfire |
| CEV_SPECIAL | special activation and following authorized state / cmbtfire |
| CEV_MISSILE_LAUNCH | source/target/type/quantity/speed/anti-missile/slot / cmbtmis |
| CEV_MISSILE_MERGE | live missile merge and resulting quantity, not empty-slot matches / cmbtmis |
| CEV_MISSILE_HIT | hit participants/quantity/damage/target type / cmbtmis |
| CEV_FIGHTER_PASS | pass result, remaining fighters and damage / cmbtmis |
| CEV_MISSILE_GONE | actual live removal with location/type/quantity / cmbtmis |
| CEV_MOVE | endpoints/cost/teleport, recorded at move start / cmbtmov1 |
| CEV_ROTATE | facing change/step / cmbtmov1 |
| CEV_DESTROY | visible death state/owners/location before native cleanup / cmbtfire |
| CEV_RETREAT | retreat/result/location / combat1 |
| CEV_BLAST_HIT | blast kind and displayed damage / cmbtspec |
| CEV_WEB_DAMAGE | web damage / combinit |
| CEV_CAPTURE | boarding capture/destroy/failure, displayed marine outcome / cmbtfir2 |
| CEV_RAID | sent/returned raiders, defenders and damage / cmbtfir2 |
| CEV_COMMAND | operation applied/refused reason, superseded by normal operation results but preserve observability / external combat adapter |
| CEV_SOUND | allowlisted sound identity/handle/logical elapsed ticks / ericnet+sound |
| CEV_SOUND_CHANGE | fade/loop/pitch/stop semantics and handle/time / sound |
| CEV_FIGHTER_BEAM | weapon/group location/facing/quantity/target point / cmbtmis |
| CEV_BEAM_AIM | actual engine-chosen fire/miss endpoints, no extra RNG draw / beams |
| CEV_GYRO_SPIN | actual engine-chosen spin turns / cmbtspec |
| CEV_ABSORB | pool after/change; clear after discharge animation or after side units act, not at side start / cmbtfire+combat1 |
| CEV_CONVERTER_HIT | two totals after four hits: damage through shields and damage shields took, NOT structure damage / cmbtspec |

Ground/bombardment rendering additionally carries displayed unit sprites,
beam colors/endpoints, queued explosions/flics, troop counts/hit points/strength,
summary label/value lines and titles through the invasion adapter. Preserve the
native displayed strength (including its difficulty treatment), not hidden raw
combat strength substituted for it.

## Appendix F. All non-documentation fork commits

Each hash is an auditable disposition, not an instruction to cherry-pick. Use
final amended semantics; compare current upstream before porting a companion
fix. Numbers refer to OrionLayer open fixes, not a contiguous public API version.

| Commit(s) | Change | Design disposition / acceptance |
| --- | --- | --- |
| 8dce5c84 | Initial extension, includes fixes 1/2/3/12/14/20/21 internally | Replace server/framing; atomic subscription initial fields; logical pointer intent; jobs; slots; icon owners/node ordering/subset selection; cover base state/messages and all initial hooks |
| 74b06b98 | 3: injected pointer retained | Shared logical input adapter with synchronized ownership, local pointer cannot rewrite RPC intent |
| b2fa0c79 | 5: custom race old_race | Separate behavior-fix audit/test for stock-to-custom accept; absent at target assignment site |
| 3fea2fb1, 77b1fb07 | Screen IDs, 22: race vs diplomacy | Context scopes for race/custom race; do not mutate native current_screen |
| 1982b2a1 | 12: job refusal without blocking helper | Structured refusal/shared rule path or explicit modal, no global suppression flag leakage |
| 62cc8968 | 24: research room/select distinction | Separate initialized science/research contexts |
| a7852545 | 25: activated research row | Semantic entry selection/Commit_Selected_Entry; current upstream already has null guard, not complete RPC provenance handling |
| 0aae309f | 27: fleet view | FLTS |
| 39bfe4fb | 28: fleet ship subset | Both selection-array owners share explicit intent, correct next-loop recount |
| 007b4aa3 | 31: no VSync request | Native vsync setting exists; hidden external presentation must not hang; no duplicate global config path needed |
| fa61b768 | 30: leader view | OFFS |
| aa59219a | 32: Info view | INFS, charts/messages authorized at native precision |
| 59425f3c | 34: main-menu slots | Both load-entry routes return initialized slot metadata |
| f8c859b4 | 35: colony shown | COLS |
| 45aba360 | 36: building layout | CBLD |
| 1a3a449a | 37: colony event state | CEVT |
| 9759f1e0 | 38: product cost/turns | CPRD |
| 67d3c9fe | 39: edited queue | BLDQ |
| 380daf98 | 40: product lists/numbers | BLDL |
| 385d678a, 1780203a | 41/43: window hide amended to opt-in/show | Explicit local presentation mode and platform-thread show/hide, default visible |
| 1e608dfb | 44: staged design | DSGN |
| 1b39a01f | 45: three component pickers | DSBX and contexts 54-56 |
| 6800ee70 | 46: player/AI audience | Separate audience initiation/context identity |
| 2bbc0174 | 47: diplomacy/list locals | DIPL + scoped generic list with copied strings/options |
| adc21aed | 48: move verdict | FMOV, own selection and safe preview |
| 707bcd96 | 29: generic boxes | MSGB, all five box kinds |
| 7fbe221f | 49: turn popups | Every TPOP kind and dynamic tail in Appendix B |
| b1b25b88 | 50: Hall of Fame | HOFM initialized lifetime, both menu and result routes |
| e2a10520 | 51: multiplayer steps | All MPLY phases plus read-only adapter accessors, no pumping |
| 02fa6bff | 42: poll during input delay | Observe initialized context promptly without bypassing delay/action safety |
| 0b15e0b6 | 61: right click point | Context-bound grid face/help/point action |
| ca441bf9 | 52: battle/scan/board contexts | Scoped battle identity and child contexts, no native screen mutation |
| 80484481 | 53: battle state | CMBT |
| 7190eb2d | 55: ordnance | CMSL |
| 9dc9601c | 57: last battle outcome | CRES captured before End_Of_Combat, cleared on new battle/reset/perspective change |
| 31f0aed4 | 54: legal targets | CTGT, native verdict without hidden target disclosure |
| 9b7f61a4 | 59: fleet-box scroll | FBSC |
| 774f8267 | 56: combat event stream | All 25 event semantics and per-subscriber cursor, not one shared drain |
| feeec356 | 58: battle command | All seven CCMD operations and native actor/turn/input boundaries |
| 4417efd2 | 62: stale activation rectangle | Mandatory context generation and control meaning; tests include same index+rect in new modal |
| adfd7bb0 | 63: live missile merge/removal | No events for empty slots; preserve actual event order |
| 0d2edae6 | 65: action viewport bounds | Safe native centering/range/field path for movement/targets/missiles |
| 7a561725 | 64: spies list | Checked own allocation/mission/pool operation through existing setters |
| 55362111 | 66: scan/boarding state | CPOP |
| 998d17e3 | 67: text format values | MSGB rendered spans/text; redact unauthorized raw format arguments |
| f0acb73d | 68: combat options | COPT |
| ebdf5e69 | 69: help regions | HLPL |
| 624081b7 | 70: language/files | Local language override and trusted alternate-install precedence/fallback; clients receive rendered text/resources, not directories |
| 3a015989 | 74: Absorber discharge | BEAM_SHOT without normal slot, record before target death |
| 6f76077b | 79: sound events | Sound/change stream with logical elapsed ticks and explicit external-audio ownership |
| 670bd95c, 36f080e7 | 71 + amendment: hidden animation pacing | External render mode skips animation-only waits, not decision/timed waits; action's first wait classification tested |
| 91501ca3 | 77: fighter beam | CEV_FIGHTER_BEAM |
| 5a97facc | 82: beam aim | CEV_BEAM_AIM uses chosen fire/miss positions without resetting associated sound timing |
| 3d3c74b6 | 83: gyro | CEV_GYRO_SPIN |
| e9b3cf07 | 84: same-count field change | View/control revision compares contents/meaning, not only count |
| b73185d2 | 85: refit row | RFIT |
| e7e823c8 | 86: fleet order buttons | FBTN |
| b5692057 | 87: name input | INBX |
| e3ae40c8 | 88: continuous input | TXTF, typed and committed text |
| dd384efa | 89: raw key codes | Normal function/Alt shortcuts covered semantically; unrestricted/cheat codes deliberately not reproduced |
| 41c73182 | 90: system window | SYSW |
| adbb683c | 91: invasion/ground frames | TPOP 10/11, sprite/beam/flic/title/summary hooks |
| 096c97fb | 92: same-star pops | Same-system checked transfer, refusal/rollback semantics audit |
| e3d81221 | 93: native layout | Intentionally replaced by typed DTOs/static contracts; no raw native schema generator |
| 42f32654, 105c4054 | 94 + amendment: charge | CEV_ABSORB timing: clear after side units act, not start |
| cb2bf131, c255dc67 | 95 + amendment: Converter | CEV_CONVERTER_HIT totals distinguish shield damage from structure |
| be79dadf | 96: council | TPOP 12, public votes/message/list choices |
| 360b8d0c | 97: Planets view | PLSM |
| 2a09cb76 | 98: troop detail | TPOP 13, displayed troop lines/strength |
| 29047f2d | 99: bitmap zero-scale crash | Separate companion rendering fix/test; guard short clipped tractor line, absent in target Draw_Bitmap_Line guard region |
| 35486477 | 101: native combat refusals | Own-target BOARD/phased-target FIRE follow native refusal/modal path, no weapon selection corruption |
| f93344f6 | 102: map merge relocation | MREL distinct from FLTS |
| 96c39753, a9a1f725 | 103 + amendment: distant pops | Explicit answers/job/partial result STLR; reject empty destination before planet access; preserve documented refusal recalculation semantics |
| 4b77ed0b, 435627ff | 104 + amendment: slider | TPOP 20, current min/max/value and correct decrement/increment semantics; comment-line correction is not another behavior |
| 29c61acd, c7205769 | 105 + amendment: Antaran room | TPOP 22, message set follows Antaran invasion date test; expose only presented text |
| 86842dfe | 106: external ship detail | SHPV with authorized derived display data, name controls and scoped lifetime |
| ec66171a | 107: mutation | TPOP 23 current picks/selections/options/refusals, refreshed inside loop |
| 39c5cc6d | 108: Dragon bribe | TPOP 24 offer/paid destination/poor modes, current message and eligible map choices |
| 90656654 | 109: diplomacy picker | TPOP 25 displayed system/planet/capitol/surrender content; no full enemy planet array |
| c024495c | 110: Colonies list | CSUM, capture only during sorted stack-buffer lifetime |
| da87ff51, f6177a05 | 111 + amendment: demolish | Native satellite-before-building walk/refusals/confirm/rubble/refund; identity+current context, reject nested boxes/lists/films and replaced controls; no stale armed action |
| ca4ea2e5, 3096e0ee | 112 + amendment: films | FILM and frame observation/skip during non-field loop; preserve pacing; left/right/key skip; frames drawn = last drawn + 1, not index |

## Appendix G. All documentation-only fork commits

These 27 commits do not add another runtime feature. They are accounted for as
contract/build guidance evidence; only selected approved instructions would
eventually become project documentation, not this root planning file.

| Commit | Disposition |
| --- | --- |
| 6655bfd4 | Fork README/protocol docs/build instructions; replaced by new contract, not copied as current API |
| ad90edd2 | Fix 94 ledger |
| 8df5b5a5 | Fix 94 amendment ledger |
| ca3aed67 | Fix 95 ledger |
| 31d9af5f | Fix 95 amendment ledger |
| 8d7b3b26 | Fix 96 ledger |
| 95345611 | Fix 97 ledger |
| 5f4d7c76 | Fix 98 ledger |
| 7501e682 | Debian/Ubuntu SDL3 dependencies; verify supported build prerequisites, not an RPC feature |
| 24904c28 | Fix 99 ledger |
| ec324fb8 | Fix 101 ledger |
| f41e4cf2 | Fix 102 ledger |
| 519edf8f | Fix 103 ledger |
| 556edc34 | Fix 104 ledger |
| 2ad3e8b4 | Fix 105 ledger |
| 7fa085d5 | Fix 104 amendment ledger |
| 5e4fa21a | Fix 105 amendment ledger |
| 68d83e4f | Fix 103 amendment ledger |
| 51e054cf | Fix 106 ledger |
| 21bdc2b8 | Fix 107 ledger |
| d1efc870 | Fix 108 ledger |
| 0e021bda | Fix 109 ledger |
| 64b570d4 | Fix 110 ledger |
| 89a4ba95 | Fix 111 ledger |
| e885ed8a | Fix 112 ledger |
| 6e12af0d | Fix 112 amendment ledger |
| 25fdf420 | Fix 111 amendment ledger |

## Appendix H. Changed-file coverage and robustness findings

All 61 fork-changed files are assigned below (paths relative to repository root):

| Changed files | Disposition / owner |
| --- | --- |
| `CMakeLists.txt` | Optional RPC targets/options/dependency linkage; no required fork EXT macro |
| `README.md`, `docs/extension-api.md` | Evidence only, future approved contract docs separate |
| `src/ext/ext_server.cpp`, `src/ext/ext_server.h` | Replace binary TCP server with bounded maintained WebSocket transport |
| `src/ext/ext_api.cpp`, `src/ext/ext_api.h` | Split global projection, sidecars, contexts, operations, presentation streams |
| `src/ext/ext_layout.cpp`, `src/ext/ext_layout.h`, `src/ext/ext_layout_table.inc`, `tools/ext_layout_gen.py` | Intentionally not ported: native layouts unnecessary and unsafe as public contract |
| `src/game/antaroom.cpp`, `src/game/council.cpp`, `src/game/events.cpp`, `src/game/science.cpp`, `src/game/score.cpp`, `src/game/turnsum.cpp`, `src/game/colland.cpp`, `src/game/raceopt.cpp`, `src/game/newpup.cpp` | Corresponding scoped report/council/news/research/score/landing/mutation/troop sidecars |
| `src/game/mainpups.cpp` | Shared popup adapters, including Dragon, selection, leader, slider, bombing |
| `src/game/colony.cpp`, `src/game/colsum.cpp`, `src/game/colmove.cpp`, `src/game/colmove.h` | Colony identity/queue/jobs/settlement/demolish/list lifetime; shared native operations |
| `src/game/desbox.cpp`, `src/game/racesel.cpp`, `src/game/tech.cpp` | Picker/setup/research adapters and companion selection fixes |
| `src/game/dip_scrn.cpp`, `src/game/dip_scrn.h`, `src/game/dip_scrn_main.cpp` | Audience and system/list picker adapters, bounded displayed metadata |
| `src/game/fields.cpp`, `src/game/gendraw.cpp`, `src/game/textbox.cpp` | Common lifecycle/control/text/list/help/confirmation bridge, not arbitrary injected IDs |
| `src/game/hotpop.cpp`, `src/game/multplay.cpp`, `src/game/netstart.cpp`, `src/game/netadapter.cpp`, `src/game/netadapter.h`, `src/game/netcode.cpp`, `src/game/netcode.h` | Lobby contexts/rosters/status/chat/read-only snapshots; hotseat revocation |
| `src/game/mox2.cpp`, `src/game/mox2.h`, `src/game/platform.cpp` | Lifecycle/safe points/window/input/presentation ownership and nonblocking shutdown |
| `src/game/beams.cpp`, `src/game/cmbtdrw1.cpp`, `src/game/cmbtfir2.cpp`, `src/game/cmbtfire.cpp`, `src/game/cmbtmis.cpp`, `src/game/cmbtmov1.cpp`, `src/game/cmbtspec.cpp`, `src/game/combat1.cpp`, `src/game/combinit.cpp` | All battle context/command/result/effect hooks in Appendix E |
| `src/game/colgcbt.cpp`, `src/game/flcqueue.cpp` | Ground/bombardment unit/beam/flic capture and displayed summaries |
| `src/game/bitmap.cpp` | Separately reviewed zero-scale rendering guard/test |
| `src/game/ericnet.cpp`, `src/game/sound.cpp`, `src/game/timer.cpp` | Explicit audio-owner/time stream; no automatic global audio/timing change for readers |
| `src/game/smack.cpp` | Film context/observational pump/semantic skip/pacing |
| `src/game/vfs_path.cpp` | Trusted local alternate-language resolution, no remote filesystem API |

### Fork weaknesses the new implementation must not carry over

- `ext_server.cpp:126-177,461-468`: nonblocking RecvExact fails on fragmented
  payloads; use the backend's incremental bounded message handling.
- `ext_server.cpp:372-456` and `ext_api.cpp:4165-4264`: up-to-1-second send waits
  occur in the game polling path; output must stay off the game thread.
- `ext_api.cpp:510-555`: raw complete records expose hidden state; typed
  allowlisted projections replace them, even when that deliberately reduces data.
- `ext_api.cpp:4208-4255`: state/fields/visual are built for any client, regardless
  of individual subscriptions; capture must be demand-driven.
- Version/client identity/permission and result correlation are incomplete in
  the binary path; authenticate and preserve operation identity end to end.
- Optional rectangle freshness cannot distinguish reused modal controls;
  context generations are mandatory for actions.
- `g_injected_mouse_pending` and reading presenter-owned surfaces introduce
  apparent unsynchronized cross-thread access; use explicit copied handoffs.
- The fork changes no files under `tests` or `cmake/tests` relative to the
  ancestor. Its live-fix commit reports are useful leads, not substitute tests.
- The fork README's summary of unflagged effects is not authoritative: inspect
  final hunks (e.g. bitmap guard and actual VSync gating), and do not assume every
  old fix remains absent upstream. Companion fixes stay separate and tested.

### Final completion definition

Complete means all 37 native constants, nested-context families, 19 lobby phases,
38 state tags/base payloads, 21 direction-specific message names, 7 combat ops,
25 combat event types, 119 commits, and 61 changed files have a tested equivalent
or an explicitly justified unsafe/obsolete representation replacement. All
player-observable behavior needed by the fork must remain expressible. No row
is silently dropped, and no feature can bypass visibility to satisfy parity.

This plan's static inventory is verifiable now. Correct field redaction, runtime
route completeness, behavior parity, performance, and security are acceptance
work for implementation, not results claimed by this document.

## Implementation tracking TODOs

All items start unchecked: a design/inventory entry is not an implementation.
Keep task IDs stable. Check an implementation item only after its behavior and
visibility tests pass; record the implementation reference, test evidence, or
approved non-implementation disposition alongside it. Partial work stays unchecked.
The appendices remain the detailed requirements behind these tasks.

### P0. Contracts, dependency approval, and baseline

P0 progress (checked contract tasks do not claim runtime implementation/parity):

| Tasks | Evidence / remaining gate |
| --- | --- |
| P0-01 | `baseline.json` pins original/reviewed/fork/ancestor SHAs. `Validate_Inventories` rechecks Appendix A-E SCREEN values, actual tag writes, messages, popup kinds, multiplayer phases and combat enums; Git-backed F-H checks cover all commits/files. Target drift is reviewed in `specs/rpc/README.md`; no inventory changed. |
| P0-02 | `coverage.json` persists every commit/path, owner and planned test obligations; `--ledger` expands exact Appendix dispositions. Completion records require hashed implementation and passing test reports plus visibility/review evidence. All runtime entries remain planned; evidence integrity checks do not replace human sign-off. |
| P0-03 | `contracts.json` defines request/result types for all 60 named methods, errors, units/ranges, enum values, required/optional fields, explicit unavailable values, action identity and lifecycle metadata. This conservative initial field set is not full fork parity. |
| P0-04 | `methods.json` plus its referenced contract bundle drives offline typed validation and explicit `--describe` output; no independent discovery catalog or schemas in ordinary state DTOs. Runtime handlers are not enabled. |
| P0-05 | Every defined field resolves to a policy in `field_policies.json`, with native evidence/predicate or an explicit disabled disposition. Native source anchors and fail-closed disabled-field fixtures pass. Reviewed evidence is static, not runtime visibility approval; additional unaudited fields stay disabled. |
| P0-06 | `backend.json` pins libwebsockets; the optional P1 build reuses project `uv_a`. Native macOS ARM64 build/lifecycle/live socket checks pass. CI now defines native Linux/macOS/MSVC x86/x64 and POSIX MinGW x86/x64 build checks; their unobserved results remain pending. Not vendored and not a required dependency of disabled/router-only builds. |
| P0-07 | Strict yyjson/JSON-RPC, fixed errors and notification semantics are specified and implemented for the metadata subset. Per user direction, authentication is a simple configured `rpc_key` hello, not a capability file, per-launch secret or approval dialog. Native gameplay dispatch/operation retention remain later work. |
| P0-08 | `lifecycle.md` completes the common source audit: owner-only decisions, animation/timed/raw-film polls, field replacement/restore, child continuations, host/client committed publication and exception-safe teardown. Actual hooks and dynamic route/lifetime tests remain P1/P2/P4 work. |
| P0-09 | The production native initial projection is now profiled on synthetic maximum-capacity arrays (1,024 systems/57 nebulas, wide coordinates and UTF-8-expanding names): one macOS Debug run encoded 211,902 bytes, capture/identity setup 1,623 us, encoding 1,158 us. It fits the 1 MiB bridge bound. Full planned-catalog, live maximum-game and multi-client profiling remain open; these subset measurements do not close the full P0 gate. |
| P0-10 | `native_baseline.json` records eight executed synthetic native-function test targets and fixed RNG checkpoints. Interactive/save/mod/whole-game RNG traces still need a local game installation and fixtures; unit-test results are not those captures. |
| P0-11 | `generate_rpc_schema.py` emits JSON Schema 2020-12, renders `generated/reference.md` from it and generates runtime C++ descriptors/type data from the same bundle. Outputs are versioned; `--check`, renderer/schema tests and CI reject drift. Standard schema validation does not replace protocol/visibility guards. |

Inventory/descriptor checks: `python3 -m unittest discover -s tests/rpc -p 'test_*.py'`.
These checks do not prove runtime visibility, protocol handling or native parity.
Verified for this P0 increment: 41 Python tests pass, including optional standard
JSON Schema metaschema/request validation on this host; schema/reference drift
checks pass; `bash tools/run-tests.sh` builds and passes all 105 existing CTest
targets on macOS ARM64; the native backend/libuv lifecycle evaluation passes.
Other backend/platform checks, maximum-game capture, native interactive and
whole-game RNG/save/mod comparisons were not run.

- [x] P0-01 — Reconfirm the target/fork SHAs and refresh Appendices A-H if either baseline changes.
- [x] P0-02 — Establish a feature-to-implementation/test ledger for all 119 commits and 61 changed files.
- [x] P0-03 — Define method/request/result DTOs, errors, enum meanings, units, nullability, and lifecycle availability for the common API catalog.
- [x] P0-04 — Define one static method-descriptor source for validation and explicit discovery; keep schemas out of normal state responses.
- [x] P0-05 — Document each public field's native UI evidence and player-visibility predicate; leave unaudited fields disabled.
- [ ] P0-06 — Select, license-review, pin, and build-check the WebSocket backend on all supported targets.
- [x] P0-07 — Define strict yyjson parsing and supported JSON-RPC request/notification semantics without a legacy binary compatibility layer.
- [x] P0-08 — Audit game-thread query/dispatch safe points, nested decisions, animation polls, cinematic loops, and cleanup paths.
- [ ] P0-09 — Measure maximum-game capture size/cost and set explicit connection, request, queue, output, retention, and polling budgets.
- [ ] P0-10 — Capture native interaction, save/mod behavior, and RNG baselines for later parity comparisons.
- [x] P0-11 — Generate and maintain a versioned machine-readable schema from the canonical method/DTO/field-policy source, suitable for conversion to human-readable documentation. Generate a reference from that schema and check both outputs for drift; never hand-maintain a second schema or method catalog.

### P1. Transport, bridge, global queries, and visibility

#### Current read-foundation increment

`src/rpc/` implements the optional libwebsockets/libuv transport, strict yyjson
envelope and source-driven argument checks, configured-key hello, supported-method
discovery, per-client limits, bounded immutable output and copied-work bridge.
Implemented/advertised methods are `rpc.hello`, `rpc.describe`, `rpc.subscribe`,
`rpc.unsubscribe`, `game.getSession`,
`game.getState`, `game.listSystems`, `game.getSystem`, `game.listNebulas`,
`game.listShips`, `game.getShip`, `game.listPlayers`, `game.getPlayer`,
`game.listColonies` and `game.getColony`.
`getState.include` is explicitly restricted to session/systems/nebulas/ships/players/colonies. Remaining
query families, UI/media and all actions remain method-not-found;
planned schemas alone do not authorize or implement them.

The application registers an RPC observer before the game thread starts. Neutral
thread-start/stopping events start and stop the sidecar through the existing
normal/exception funnel before native resource teardown. No game file includes
RPC headers or names RPC functions/settings/types; architecture tests enforce it.
Local `rpc_enabled`,
`rpc_port` and `rpc_key` preferences are unsaved/unsynchronized and startup-only.
The key is converted from native CP850 config storage to UTF-8 for exact comparison.
No approval dialog, per-launch secret file or arbitrary viewer selection exists.

The sidecar captures at neutral initialized-map and completed ship-icon refresh events, then
serves the immutable capture from neutral idle events independently of the open
screen. Captures explicitly report `last-coherent-capture`, turn/revision and
snapshot identity; no latest-live-state claim is made. World replacement attempts,
success/failure, resolution and perspective-selection events drive sidecar busy,
identity and revocation handling. Failed replacements fail closed. Hotseat/view
changes close old connections and require a fresh key handshake.

The initial projection contains native map positions and the bound human viewer's
visited bit; visited non-black-hole names are bounded CP850-decoded copies with
native outpost lowercase. Contact/lore-only names and system owner remain
`unaudited`; global planet and foreign economy data is not projected. Ship observations
copy only fresh surviving displayed links, map positions, own/foreign classification,
owned names and owner references authorized by the same capture's known-player
roster/banner colors. The Q-07 follow-up copies non-selection map-authorized routes
and the displayed head's cached ordinary-route ETA; enemy names/design/cargo remain
unread. Enemy routes additionally require enemy-moves presentation or the existing
native incoming predicate, not detection, contact or omniscience alone. Stationary
own routes are known-empty, selection-only/wormhole/member ETA is context-required,
and special destinations remain unaudited. Private destination bindings are discarded
before retaining snapshots. Player
captures include the native known/eliminated/omniscient roster and only the bound
player's already calculated treasury/HUD values; foreign economy is never read.
Tagged `Player.atWar` reads only the viewer's native treaty classification for
current living foreign contacts; self/eliminated/omniscience-only rows stay unknown.
War-only increment verification: 86 Python tests including 35 live cases and the
RPC/native/transport CTest targets pass normally and with ASAN/UBSAN on macOS ARM64;
generation drift, affected C++ formatting and whitespace checks pass. No native
files change; the own-colony increment's 106-target RPC-disabled result still applies.
Own full-colony captures filter out foreign/unowned/outpost records before reading
links/population, then publish same-capture system/owner handles, one-based orbital
position and cached resident population. Planet handles, production and buy details
remain unaudited; no colony builder or calculator is invoked. Colony handles are
read-only observations replaced on authorized projection changes, not durable
native identities or action capabilities. See `specs/rpc/colony_view.md`.
Own-colony increment verification on macOS ARM64: 84 Python tests including 34 live
WebSocket cases pass; relevant RPC/native/transport CTest targets pass normally and
with ASAN/UBSAN; the RPC-disabled suite passes all 106 CTest targets. Schema drift,
affected C++ formatting and whitespace checks pass. Synthetic/native-array coverage
does not close interactive saved-game, multiplayer or other-platform parity gates.
Nebula positions
follow native unconditional map drawing. Hidden-only changes do not revise views.
Lists use opaque snapshot-pinned cursors, bounded to 64 cursors/four retained
captures; expiry/eviction is explicit object-unavailable, never mixed revisions.
Tests cover paired hidden-state changes, native formatting/RNG preservation,
revocation/late results, cached read behavior and real WebSocket query dispatch.
Subscriptions atomically return a selected-topic baseline and cursor on the game
thread. Only session/systems/nebulas/ships/players/colonies topics are accepted; `state.changed` notifications
invalidate the selected public projection without disclosing hidden-only changes.
Authenticated connections receive default `game.turnAdvanced`, emitted once per connection only
after a higher committed turn is in the queryable map capture. These ordered
events are never coalesced. The native resolution scope and authoritative network
decode scope prevent intermediate publication; native differential-decode tests
verify committed event ordering, with RPC/live tests covering paused captures.
Unsubscribe is connection-owned; reconnect requires a fresh baseline, and view
revocation clears subscriptions. Capture is requested by queued reads/subscriptions
or native resolution/network commit boundaries for default turn notifications,
not merely an idle authenticated connection. C++ ownership/bounds/visibility tests
and live transport tests pass on macOS ARM64. Unsent state invalidations coalesce
per subscription in both bridge and service queues, preserving partially sent
messages and all non-coalescible responses/events. Cursor jumps mean re-read the
selected topics, not replay lost events. Output overflow explicitly disconnects.
Other ordered gameplay event producers remain later work, not advertised functionality.

Ship-containing state/baseline requests use one retained post-refresh capture for
all topics, explicitly dated `last-native-ship-refresh`, never mixed-age data.
Ship handles are read-only observation handles, replaced on projected ship-set
changes; durable native entity identity and action use remain unimplemented.
Maximum-size ship collections paginate; oversized full state/baselines return
explicit limit errors. See `specs/rpc/ship_refresh.md` for exact field/lifetime rules.

See `specs/rpc/p1.md` for build/test commands, verified evidence and remaining P0
gates. P1 completion is not claimed by this foundation increment.

#### Infrastructure

- [x] P1-01 — Add optional build/runtime RPC enablement without adding dependencies to disabled or router-only builds.
- [x] P1-02 — Implement bounded WebSocket connection/message handling and explicit bind-failure diagnostics.
- [x] P1-03 — Implement a simple hello matching the predefined key in local orion2re settings, reader/controller permissions, one controller, Host/Origin validation, and authentication deadlines; no approval dialogs or per-launch credential files.
- [x] P1-04 — Implement strict envelope/argument validation, structured errors, rate limits, and redacted diagnostics.
- [x] P1-05 — Implement the game-thread request queue and immutable result/publication handoff; prohibit network waits in game callbacks.
- [x] P1-06 — Implement coherent requested-topic capture, game/perspective identity, revisions, entity generations, and opaque public IDs (initial session/map subset; topology replacement revokes handles).
- [x] P1-07 — Return explicit busy/unavailable or dated snapshots during unsafe capture periods; keep service heartbeats responsive.
- [x] P1-08 — Implement atomic subscription baseline/cursor setup, unsubscribe, reconnect baselines, and demand-driven capture (initial audited topics; `rpc_game_test.cpp` and live subscription tests).
- [x] P1-09 — Implement bounded per-client output and state coalescing without rewriting partially sent messages or losing operation results silently (`rpc_output_test.cpp`, bridge ordered-result/overflow tests and live 100-change burst; macOS normal and ASAN/UBSAN).
- [x] P1-10 — Implement startup/quit/fatal-exit cleanup, cancellation, wakeup, and join independent of another game poll.

#### Shared player-view policy

- [x] V-01 — Bind authorization to the active local perspective; reject arbitrary viewer selection and remote privileged/omniscient switches (all implemented queries).
- [ ] V-02 — Implement known-system/planet/nebula disclosure using audited native exploration/map rules.
- [x] V-03 — Implement read-only ship detection/visibility policy without rebuilding or walking stale display nodes during queries (approved post-refresh copy of surviving displayed links; actual native builder/filter, stealth/omniscience, hidden-state, lifetime and malformed-link tests; no native policy extraction).
- [ ] V-04 — Gate enemy design/scan details, destinations, orders, cargo, and ETA separately from ship existence.
- [ ] V-05 — Gate player/contact/trait/economy, colony, leader, research, diplomacy, intelligence, and chart fields individually.
- [ ] V-06 — Preserve native distinctions between current, last-known, unknown, and known-empty information.
- [x] V-07 — Filter before IDs, totals, sorting, search, pagination, caching, errors, events, and revision comparisons (implemented projection; unsupported filters/streams remain uncallable).
- [x] V-08 — Make unknown/nonexistent ID errors indistinguishable and reject filters/previews that probe hidden properties (implemented object/cursor paths).
- [ ] V-09 — Apply visibility to messages/format arguments, legal-target previews, retained battle results, sound, effects, frames, and resources.
- [x] V-10 — Revoke views, handles, pending work/results, snapshots and subscriptions on load/reset/perspective change.
- [x] V-11 — Require a fresh configured-key handshake after hotseat perspective revocation; never let an existing connection silently follow the next empire.
- [ ] V-12 — Exclude raw structs, native layouts, RNG/Lua/debug access, arbitrary files/config/mod storage, save bytes, and cheat shortcuts.

#### Common query API groups

- [x] Q-01 — Implement `rpc.hello`, `rpc.describe`, `rpc.subscribe`, and `rpc.unsubscribe` (subscriptions explicitly limited to initial audited topics, `state.changed` and `game.turnAdvanced`).
- [x] Q-02 — Implement `game.getSession` and coherent `game.getState(include)` for the explicitly enabled initial topics.
- [x] Q-03 — Implement `game.listPlayers` and `game.getPlayer` with own HUD/economy and known-contact projections (audited name/color/contact/eliminated roster and own signed treasury/HUD; `player_view.md`, native roster/Lore/hidden-state tests, stable handles, pagination, subscriptions and live transport; traits/government/research remain unenabled pending their field audits).
- [ ] Q-04 — Implement `game.listSystems` and `game.getSystem` with authorized map/range/wormhole data.
- [ ] Q-05 — Implement `game.listPlanets`, `game.getPlanet`, and `game.listNebulas`.
- [ ] Q-06 — Implement `game.listColonies` and `game.getColony`, including authorized production, jobs, buildings, troops, events, and transfer settings (own non-outpost roster, same-capture system/owner, orbital position, cached resident population and infantry/armor counts implemented; planet handles, production/buy details, jobs/buildings, remaining troop details, events/transfers and foreign intelligence remain unaudited).
- [ ] Q-07 — Implement `game.listShips` and `game.getShip`, including conditional destination/ETA and scan disclosure (dated position/owned/name, same-capture known-player owners, non-selection map-authorized destinations and head-only cached ordinary-route ETA implemented and tested; initialized selection/scan/fleet-box detail and durable entity identity remain open).
- [ ] Q-08 — Implement `game.listFleets` and `game.getFleet` without exposing raw rebuilt stack indices.
- [ ] Q-09 — Implement `game.getMovementOptions` with audited scratch-state handling and no hidden-information oracle.
- [ ] Q-10 — Implement `game.listDesigns`, `game.getDesign`, and `game.getDesignOptions`.
- [ ] Q-11 — Implement `game.getResearch`, `game.listResearchOptions`, and `game.getTechnology`.
- [ ] Q-12 — Implement `game.listLeaders` and `game.getLeader` for owned/offered/discovered leaders only.
- [ ] Q-13 — Implement `game.getRelations` and `game.getIntelligence` (contact-gated native war classification is projected as tagged `Player.atWar`; full relations/treaties/intelligence methods remain unimplemented).
- [ ] Q-14 — Implement `game.getHistory`, `game.listMessages`, and `game.getMessage` without consuming reports or improving hidden precision.
- [x] Q-15 — Implement and test bounded snapshot-pinned pagination for enabled lists, plus bounded complete `getState` captures.

#### Minimal notifications

- [x] N-01 — Emit `game.turnAdvanced` only after committed queryable state; cover single-player resolution and authoritative network-state application (existing neutral scopes, delayed map capture; RPC paused/ordered/client tests, live turn/query equality, native differential-decode commit ordering; interactive parity remains a P0 gate).
- [ ] N-02 — Emit `ui.contextChanged` for initialized active contexts, nested dialogs, and parent restoration rather than raw screen assignments.
- [ ] N-03 — Emit `game.sessionChanged` for new/load/end/unload/revocation and invalidate stale identities.
- [ ] N-04 — Add selected `state.changed` / `ui.viewChanged` subscriptions without emitting events for hidden-only changes.
- [x] N-05 — Validate a client example deriving incoming hostile ships from ships/colonies/known diplomacy after a turn event; add no server briefing API (`tools/rpc_incoming_threats.py`, pure client/CLI tests and live turn-event → coherent state → client alert test; source/limits in `specs/rpc/incoming_threats.md`; synthetic/native-function and live-transport evidence, not interactive parity).

V-03/Q-07 approach approved: reuse the already calculated state via a neutral
post-refresh hook and explicitly dated ship observations. The proposed native
detection-policy extraction was not approved or applied and is not required by
this implementation. Queries never invoke the builder or inspect stale nodes.

Q-07 route/ETA evidence is in `specs/rpc/ship_refresh.md`: reuse native incoming
and absolute-location predicates on fresh surviving icon heads; copy only cached
ordinary-route ETA, never movement previews. Preserve the native outpost predicate's
ignored player argument, without treating it or "enemy" as hostility. No native
code/API changes or shared-policy extraction. Known-empty, context-required,
unknown and unaudited route knowledge stay distinct. Selection-only fields require
a future initialized-context observation, not stale globals.

Q-07 cached fleet-box ETA producer prerequisite: neutral borrowed-result hooks
now observe only `Print_Fleet_Box_Info`'s already selected non-preview cached
route branch. Native print replacement, fleet/icon rebuild, close and main-view
entry/exit publish clear boundaries. Typed callbacks share the bounded neutral
listener storage; native code remains unaware of RPC. This is not yet a public
selection/scan implementation: RPC does not consume the payload, enrich retained
snapshots or inspect selection globals. Current context-required answers are
unchanged. Safe same-capture identity binding, slot reuse, complete context and
world/perspective/resolution revocation tests remain open before enabling it.
See `specs/rpc/ship_refresh.md`; Q-07 and V-04 remain unchecked.

Producer-prerequisite verification (macOS ARM64): 94 Python tests including 36
live WebSocket cases; RPC/native/transport, neutral-event and fleet owner targets
pass normally and with ASAN/UBSAN; all 106 RPC-disabled targets pass. Generation
drift, changed C++ formatting and whitespace checks pass. The exact-path drift
audit now records the two new neutral-hook owners and the already implemented
N-05 client artifact. No public fields/methods/topics are enabled by these hooks.
Synthetic/native-function and existing live-transport evidence do not close
interactive saved-game/multiplayer or other-platform gates; local leak detection
remains unsupported.

Route increment verification (macOS ARM64): 81 Python tests including 33 live
WebSocket cases pass; relevant RPC/native/transport targets pass normally and with
ASAN/UBSAN; RPC-disabled `tools/run-tests.sh` passes all 106 CTest targets. Generated
schema/reference/C++ metadata drift, affected C++ formatting and whitespace checks
pass. Native maximum-capacity sanitizer coverage caught and verified the fix for
packed-field reference binding in stack validation. LeakSanitizer is unsupported
on this runtime and disabled for ASAN/UBSAN runs; interactive saved-game parity
and other-platform checks remain open. See ship-refresh evidence for reproduction.

Ship-name contract clarified by the user: `Ship.name` is the canonical in-game
ship name; do not change its semantics or substitute a hull-class/UI label. No
`displayName` addition is approved or required by this decision. A native UI branch
that discloses only a hull label does not authorize the enemy's canonical name.
Enemy names remain unaudited until independently authorized by native evidence
and initialized-context visibility tests. Q-06/Q-07/Q-13 stay unchecked for their
remaining field/context families; N-05 validates the implemented read subsets,
not full query or interactive parity.

N-05 verification on macOS ARM64: 93 Python tests including 36 live WebSocket
cases pass; relevant RPC/native/transport targets pass normally and with ASAN/UBSAN;
the RPC-disabled `tools/run-tests.sh` passes all 106 CTest targets. Generation drift,
affected C++ formatting and whitespace checks pass. The client rejects stale,
newer-turn, cross-game and mixed-metadata responses; busy/limit/unknown knowledge
is not an all-clear. It copies cached ETA only and invokes no server aggregation,
movement calculator or gameplay action. Interactive saved-game/multiplayer and
other-platform acceptance remain open; LeakSanitizer remains unsupported locally.

Q-06 cached troop-count follow-up adds tagged `Colony.troops` infantry/armor
counts from the own full colony's existing native military array. Exact native
display evidence, zero/nonnegative int16 bounds and exclusions are documented in
`specs/rpc/colony_view.md`. Foreign/outpost counts are filtered before reading or
validation, including under omniscience. No militia, strength, technology,
roundoff, inbound units or future outcomes are inferred. Counts share the same
immutable population/map/player/ship capture and never enrich older snapshots.
Q-06 remains unchecked for its remaining fields and contexts.

Troop-count verification (macOS ARM64): 96 Python tests including 37 live
WebSocket cases pass; RPC/native/transport targets pass normally and under
ASAN/UBSAN; all 106 RPC-disabled targets pass. Generation drift, affected C++
formatting and whitespace checks pass. No native code/API changes. These are
synthetic/native-function and live-transport checks, not interactive saved-game,
multiplayer or other-platform parity; LeakSanitizer remains unsupported locally.

### P2. Shared UI adapters and operation lifecycle

- [ ] P2-01 — Implement scoped context registration, stack/parent restoration, and unregister-before-native-state-release behavior.
- [ ] P2-02 — Implement context/control generations for rebuild, replacement, mark/release/restore, and local-action invalidation.
- [ ] P2-03 — Implement `ui.getContext`, `ui.getView`, and `ui.getControls` with only visible semantic controls.
- [ ] P2-04 — Implement `ui.open`, `ui.back`, and `ui.close` through native navigation rules, not direct screen-global writes.
- [ ] P2-05 — Implement `ui.activate`, `ui.cancel`, `ui.select`, and `ui.setValue` with typed control-specific behavior.
- [ ] P2-06 — Implement `ui.setText` for legacy/continuous input, editing/committed text, limits, cursor, accept, and cancel.
- [ ] P2-07 — Implement `ui.point` for authorized logical-coordinate grids, right-click/help, and normal map interactions.
- [ ] P2-08 — Implement shared visible lists, row identities, enabled flags, selection, scrolling, and cancel behavior.
- [ ] P2-09 — Implement message, timed-message, warning, and confirmation child contexts with rendered text/spans and actual answers.
- [ ] P2-10 — Implement active help-region/text capture without exposing hidden/debug controls or raw format-code memory.
- [ ] P2-11 — Implement request/operation identity, queued/applied/rejected/dispatched outcomes, bounded result lookup, deadlines, and unstarted cancellation.
- [ ] P2-12 — Implement live-session duplicate protection, explicit partial outcomes, and unknown-after-disconnect behavior.
- [ ] P2-13 — Implement nonrecursive mutation dispatch and deferred intents consumed by registered nested decision loops.
- [ ] P2-14 — Preserve native input delays and distinguish observational animation polls from action-consumption points.
- [ ] P2-15 — Route legitimate function/Alt shortcuts through authorized actions; block unscoped keyboard/mouse injection and cheats.
- [ ] P2-16 — Share operation bodies between native and RPC input; audit recalculation, mod, confirmation, and multiplayer effects before exposing domain commands.

### P3. Strategic screens and actions

- [ ] P3-01 — Add the map sidecar: pan/zoom, focus/select known entities, open screens, and submit/cancel turn where native flow permits.
- [ ] P3-02 — Add map fleet/system box views: visible membership/order, selection, scroll, system/fleet-bar slots, buttons, and merge-relocation mode.
- [ ] P3-03 — Add colony view conversion: selected colony/modes, buildings/satellites/layout, events, jobs, production and displayed derived values.
- [ ] P3-04 — Implement checked colony job-list assignment through the existing rule path, including refusal and gesture-state handling.
- [ ] P3-05 — Implement same-system colonist transfers with tested refusal/atomicity semantics.
- [ ] P3-06 — Implement interstellar colonist transfers with explicit native-question answers/job choice, partial progress, stop reasons, and empty-slot rejection.
- [ ] P3-07 — Implement production selection/buy, queue editing/reordering, autobuild modes, and displayed eligibility/cost/turn values.
- [ ] P3-08 — Implement colony transport, relocation, refit candidates/scroll, and troop-detail contexts.
- [ ] P3-09 — Implement demolish by colony/building identity, native satellite/building ordering, refusal/confirmation/rubble/recalculation/refund, and nested-context exclusion.
- [ ] P3-10 — Add the Colonies sidecar: native list order/sort/window, jobs, transfers, production/buy numbers, eligibility, and colony navigation.
- [ ] P3-11 — Add the Fleets sidecar: subset selection in both native selection models, filters/sort/scroll, scan, relocate, and merge modes.
- [ ] P3-12 — Implement fleet movement, engage/ignore, colonize/outpost, transport/attack, scrap, and leader assignment through native eligibility/confirmation paths.
- [ ] P3-13 — Add the Planets sidecar: explored-list filtering/sort/send modes, rows/scroll, best own ship/ETA, and disclosed hover verdicts.
- [ ] P3-14 — Add research/change/group-list adapters and direct semantic entry selection through the shared commit operation.
- [ ] P3-15 — Add the staged-design sidecar: names/hull/components/mods/counts/arcs/ammo, displayed costs/space/speed, accept/cancel.
- [ ] P3-16 — Add shield/computer, weapon, and special-system picker contexts with actual offered choices.
- [ ] P3-17 — Add Leaders views and hire/reject/assign/dismiss/inspect operations with correct offered/owned visibility.
- [ ] P3-18 — Add Races/intelligence views and checked spy/mission/agent-pool list operations.
- [ ] P3-19 — Add player- and AI-initiated diplomacy contexts, displayed messages/menus, chat/listen/reply flows, and native choices.
- [ ] P3-20 — Add treaty/gift/demand/money/technology/system picker and counteroffer adapters without precomputing hidden AI reactions.
- [ ] P3-21 — Add Info subscreens: history scaling, technology review, race statistics, messages, and reference main/how-to/category navigation.
- [ ] P3-22 — Add distance selection, command-point detail, tax-rate controls, and freighter slider/scrap flows.
- [ ] P3-23 — Exercise every strategic direct input owner in Appendix B, including delegated list/help paths and early returns.

### P4. Setup, reports, and session UI

- [ ] P4-01 — Implement `setup.getView`, `setup.listSaveSlots`, and `setup.getHallOfFame`.
- [ ] P4-02 — Add main-menu, menu-file animation, credits, new-game settings, accept/cancel, and exit contexts.
- [ ] P4-03 — Add race selection, custom race/picks/government/name, flag/color selection, and stock-to-custom accept handling.
- [ ] P4-04 — Add game menu/options/load/save modes, both slot-list entry routes, editable descriptions, overwrite/refusal confirmations, and no raw save/path access.
- [ ] P4-05 — Add Hall of Fame/high-score/end-game contexts on both menu and result-entry routes, after their data is initialized.
- [ ] P4-06 — Add science discovery/reason/progress, initial research choice, and evolutionary-mutation contexts.
- [ ] P4-07 — Add leader-hire and promotion/marooned-hero report contexts with current choices and text.
- [ ] P4-08 — Add colony/outpost/drop-troops target selection, discovery, landing, naming, and unloading flows.
- [ ] P4-09 — Add GNN/news, turn-summary pages/message links, report orchestration, and presented Antaran-room contexts.
- [ ] P4-10 — Add council state/nominees/votes/messages/member order/vote/ruling contexts and list choices.
- [ ] P4-11 — Add Dragon offer/paid-destination/too-poor modes and dynamic eligible-star choices.
- [ ] P4-12 — Implement `lobby.getView`, `lobby.listSessions`, and `lobby.getChat` using read-only views, not network-pumping query helpers.
- [ ] P4-13 — Implement native host/join/position/ready/chat actions with proper ownership and multiplayer synchronization.
- [ ] P4-14 — Implement local hotseat setup and secure authorized perspective-switch flow.
- [ ] P4-15 — Exercise all report/setup/lobby direct input owners in Appendix B, including keyboard-only routes, timed waits, cancel, nesting, and restoration.

#### All 19 multiplayer phases (Appendix C)

- [ ] MP-01 — MP_SETUP: multiplayer mode/setup controls.
- [ ] MP-02 — MP_ONLINE_SETUP: endpoint dialog and validation.
- [ ] MP-03 — MP_NET_GAME_NAME: game-name entry and duplicate-name warning.
- [ ] MP-04 — MP_LOAD_GAME: native multiplayer save-slot list.
- [ ] MP-05 — MP_HOTSEAT_SETUP: local human roster/race setup.
- [ ] MP-06 — MP_HOTSEAT_SELECT: position choice and perspective revocation/reauthorization.
- [ ] MP-07 — MP_HOST_INIT: host initialization status.
- [ ] MP-08 — MP_HOST_WAIT_JOINERS: roster/wait/begin controls.
- [ ] MP-09 — MP_HOST_WAIT_RACES: race-info wait status.
- [ ] MP-10 — MP_HOST_SENDING: host transfer status.
- [ ] MP-11 — MP_HOST_PICK_POSITION: loaded-game host position choice.
- [ ] MP-12 — MP_HOST_WAIT_LOADED: loaded-game roster/wait/begin controls.
- [ ] MP-13 — MP_JOIN_INIT: join initialization status.
- [ ] MP-14 — MP_JOIN_GAME_LIST: advertised sessions and native join choice.
- [ ] MP-15 — MP_JOIN_WAITING: joined/waiting-for-host status.
- [ ] MP-16 — MP_JOIN_GENERATING_MAP: generation status without hidden map disclosure.
- [ ] MP-17 — MP_JOIN_PICK_POSITION: loaded-game join position choice.
- [ ] MP-18 — MP_JOIN_GETTING_DATA: loaded-game receive status, not raw transferred state.
- [ ] MP-19 — MP_NET_NEXT_TURN: ready indicators, chat, and native wait controls.

### P5. Combat and invasion

- [ ] P5-01 — Implement `combat.getView`, `combat.getUnit`, `combat.getTargetOptions`, and `combat.getResult` for the active observed battle.
- [ ] P5-02 — Add scoped battle identity/actor/turn/selection/viewport/weapon state and child-context lifetimes.
- [ ] P5-03 — Expose only authorized units/ordnance/scan details and native legal move/target masks without hidden-target oracles.
- [ ] P5-04 — Add scan, noncombat ship detail/name, boarding range/marines/choices, help, and combat-options views.
- [ ] P5-05 — Implement CCMD_MOVE through native legal cells and safe viewport/range handling.
- [ ] P5-06 — Implement CCMD_FIRE with native target/weapon-mask/phase/refusal handling.
- [ ] P5-07 — Implement CCMD_FIRE_MISSILE, including a safe native path for offscreen missiles.
- [ ] P5-08 — Implement CCMD_FACE with explicit logical target coordinates.
- [ ] P5-09 — Implement CCMD_SELECT with native ownership/initiative restrictions.
- [ ] P5-10 — Implement CCMD_REMOVE_STASIS with the original eligibility and confirmation flow.
- [ ] P5-11 — Implement CCMD_BOARD, its second step, native refusal boxes, and marine/raid choices.
- [ ] P5-12 — Implement normal battle done/wait/auto/retreat/options controls and observed opponent wait paths.
- [ ] P5-13 — Capture retained observed battle results before native cleanup; clear them on new battle/reset/revocation.
- [ ] P5-14 — Implement bounded ordered per-subscriber combat streams, initial state, sequence/time metadata, and explicit gap/resnapshot behavior.
- [ ] P5-15 — Implement `invasion.getView`, bombard/invade/destroy/mind-control/cease-fire/drop-troop choices, and original confirmations/refusals.
- [ ] P5-16 — Capture displayed ground troop counts/hit points/strength, summary labels/values, titles, unit sprites, beams/colors, and explosions/flics.
- [ ] P5-17 — Audit every battle/ground hook for visibility, native ordering, and no extra RNG draws.

#### All 25 combat event types (Appendix E)

- [ ] CE-01 — CEV_BEAM_SHOT, including slotless Energy Absorber discharge and ordering before death.
- [ ] CE-02 — CEV_REFLECT.
- [ ] CE-03 — CEV_BOMB.
- [ ] CE-04 — CEV_SPECIAL and the authorized state that follows it.
- [ ] CE-05 — CEV_MISSILE_LAUNCH.
- [ ] CE-06 — CEV_MISSILE_MERGE for live missiles only.
- [ ] CE-07 — CEV_MISSILE_HIT.
- [ ] CE-08 — CEV_FIGHTER_PASS.
- [ ] CE-09 — CEV_MISSILE_GONE for actual live removal only.
- [ ] CE-10 — CEV_MOVE at move start, including teleport/cost semantics.
- [ ] CE-11 — CEV_ROTATE.
- [ ] CE-12 — CEV_DESTROY before native cleanup.
- [ ] CE-13 — CEV_RETREAT.
- [ ] CE-14 — CEV_BLAST_HIT.
- [ ] CE-15 — CEV_WEB_DAMAGE.
- [ ] CE-16 — CEV_CAPTURE and displayed boarding outcome.
- [ ] CE-17 — CEV_RAID.
- [ ] CE-18 — CEV_COMMAND outcome observability mapped to operation results.
- [ ] CE-19 — CEV_SOUND with logical elapsed ticks.
- [ ] CE-20 — CEV_SOUND_CHANGE with handle/fade/loop/pitch semantics.
- [ ] CE-21 — CEV_FIGHTER_BEAM.
- [ ] CE-22 — CEV_BEAM_AIM from already chosen endpoints without resetting sound timing.
- [ ] CE-23 — CEV_GYRO_SPIN from the engine's actual chosen spin.
- [ ] CE-24 — CEV_ABSORB with discharge/after-side-action clearing, not side-start clearing.
- [ ] CE-25 — CEV_CONVERTER_HIT with shield-share and through-shield totals correctly distinguished from structure damage.

### P6. Presentation and companion behavior fixes

- [ ] P6-01 — Implement `reference.get` and bounded `media.getResource` for allowlisted public/current-view content, not arbitrary files/LBX entries.
- [ ] P6-02 — Implement `presentation.getState` and atomic `presentation.getFrame` with matching frame/palette generation and optional subscriptions.
- [ ] P6-03 — Implement platform-thread show/hide and hidden-window frame servicing without renderer access from the worker.
- [ ] P6-04 — Implement explicit external presentation/audio ownership and automatic local fallback on release/disconnect.
- [ ] P6-05 — Implement combat sound/change delivery and handle ownership without silencing native audio merely because a reader connects.
- [ ] P6-06 — Preserve logical effect/sound timing while optionally skipping animation-only waits; keep input delays and timed decisions intact.
- [ ] P6-07 — Add film identity/progress/frame capture and an observational pump in the non-field cinematic loop.
- [ ] P6-08 — Implement native skippable-film left/right/key behavior, correct frames-drawn semantics, and unchanged pacing without sticky injected input.
- [ ] P6-09 — Implement trusted local language/alternate-install precedence and fallback; never accept remote filesystem settings.
- [ ] P6-10 — Reuse native VSync/speed options and verify hidden external presentation cannot hang.
- [ ] P6-11 — Audit/test the custom-race old_race companion fix against current upstream before applying it separately.
- [ ] P6-12 — Audit/test the short clipped bitmap/tractor-line zero-scale companion guard separately from RPC implementation.
- [ ] P6-13 — Verify research null/selection guards and colony refusal/recalculation semantics against current upstream rather than blindly porting old patches.

### Coverage sign-off: every native SCREEN constant (Appendix A)

For interactive entries, sign-off includes view/action behavior, lifecycle,
early-return/nesting tests, and visibility. For inactive/control states, record
the explicit noninteractive disposition instead of inventing a screen adapter.

- [ ] S-01 — SCREEN_UNKNOWN (-1): unavailable/no navigation destination.
- [ ] S-02 — SCREEN_MAIN (0): map and nested system/fleet controls.
- [ ] S-03 — SCREEN_COLONY (1): colony and child interactions.
- [ ] S-04 — SCREEN_DESIGN (3): staged design and pickers.
- [ ] S-05 — SCREEN_FLEET (4): fleets and selection/order modes.
- [ ] S-06 — SCREEN_RACE (6): contacts/intelligence/diplomacy.
- [ ] S-07 — SCREEN_EXIT (7): session-end and cleanup behavior.
- [ ] S-08 — SCREEN_GAME (8): game/options/load/save modes.
- [ ] S-09 — SCREEN_INFO (9): all Info subscreens.
- [ ] S-10 — SCREEN_MAIN_MENU (10): menu and its routes.
- [ ] S-11 — SCREEN_UNK2 (11): unused/unsupported disposition.
- [ ] S-12 — SCREEN_MAIN_NEXT_TURN (12): processing, not a premature turn notification.
- [ ] S-13 — SCREEN_NEW_GAME (13): settings and setup children.
- [ ] S-14 — SCREEN_HALL_OF_FAME (14): initialized score data and both entry routes.
- [ ] S-15 — SCREEN_MULTI_PLAYER (15): setup and online dialog.
- [ ] S-16 — SCREEN_HOTSEAT (16): local roster/race setup.
- [ ] S-17 — SCREEN_HOTSEAT_SELECT_PLAYER (17): authorized perspective switch.
- [ ] S-18 — SCREEN_PLANET_DATA (18): authorized planet details.
- [ ] S-19 — SCREEN_COLONY_SUMMARY (20): colony list and operations.
- [ ] S-20 — SCREEN_START_NET (21): host phases.
- [ ] S-21 — SCREEN_JOIN_NET (22): join phases.
- [ ] S-22 — SCREEN_QUEUE_POPUP (25): products, edited queue, and refit.
- [ ] S-23 — SCREEN_DISTANCE_BETWEEN_STARS (28): known-system distance choice.
- [ ] S-24 — SCREEN_OFFICERS (29): leader views/actions and hire children.
- [ ] S-25 — SCREEN_COLONIZATION_IN_MAIN (30): selection/confirmation/name flow.
- [ ] S-26 — SCREEN_BUILDING_OUTPOSTS_IN_MAIN (31): outpost flow.
- [ ] S-27 — SCREEN_PLANET_SUMMARY (32): known planets and send/filter modes.
- [ ] S-28 — SCREEN_COLONY_LANDING (33): presentation/name/dismiss.
- [ ] S-29 — SCREEN_UNLOADING_TRANSPORTS_IN_MAIN (34): troop destination/confirmation.
- [ ] S-30 — SCREEN_SCRAP_FREIGHTERS (35): slider and confirmation.
- [ ] S-31 — SCREEN_TECH_CHANGE (36): research change/group/detail flow.
- [ ] S-32 — SCREEN_NET_NEXT_TURN (37): ready/chat/wait and committed-state notification.
- [ ] S-33 — SCREEN_REPORTS (39): report orchestration and child decisions.
- [ ] S-34 — SCREEN_TURN_SUMMARY (40): pages/messages/links/dismiss.
- [ ] S-35 — SCREEN_LOAD_NET (41): load/position/host phases without previous-screen alias assumptions.
- [ ] S-36 — SCREEN_SHOW_COMMAND_POINTS (43): displayed command-point details.
- [ ] S-37 — SCREEN_CREDITS (44): credits/dismiss.

### Coverage sign-off: every fork popup kind

- [ ] TP-01 — TPOP kind 1: science reason/discoveries/modifiers/progress/footer.
- [ ] TP-02 — TPOP kind 2: turn-summary messages/pages/colony links.
- [ ] TP-03 — TPOP kind 3: leader offer/cost/maintenance/current choices.
- [ ] TP-04 — TPOP kind 4: colony/outpost/troop planet selection and shown system/special.
- [ ] TP-05 — TPOP kind 5: discovered system/special/dismiss.
- [ ] TP-06 — TPOP kind 6: leader promotion/marooned-hero state.
- [ ] TP-07 — TPOP kind 7: actual GNN/news text/picture and authorized arguments.
- [ ] TP-08 — TPOP kind 8: battle target candidates and cancel.
- [ ] TP-09 — TPOP kind 9: landing title/planet/colony/system/name.
- [ ] TP-10 — TPOP kind 10: invasion/bombardment modes/options/numbers/explosions.
- [ ] TP-11 — TPOP kind 11: ground-combat sides/strength/summary/rendered frame.
- [ ] TP-12 — TPOP kind 12: council votes/state/messages/member order/list choices.
- [ ] TP-13 — TPOP kind 13: displayed troop/rebel details and summary lines.
- [ ] TP-20 — TPOP kind 20: slider min/max/current/text and native steps.
- [ ] TP-22 — TPOP kind 22: presented Antaran message, with correct message-set selection.
- [ ] TP-23 — TPOP kind 23: current mutation picks/options/refusals/accept.
- [ ] TP-24 — TPOP kind 24: Dragon offer/destination/poor modes and eligible stars.
- [ ] TP-25 — TPOP kind 25: disclosed diplomacy system/planet/surrender choices.

### Coverage sign-off: every tagged state block (Appendix D)

These are feature-parity checks, not instructions to retain binary tags or raw
layouts. Record the typed view/API and test implementing each equivalent.

- [ ] B-01 — FSEL: visible fleet-box members/order/selection.
- [ ] B-02 — FLTS: Fleets screen view and modes.
- [ ] B-03 — OFFS: Leaders view and modes.
- [ ] B-04 — INFS: authorized Info chart scaling/messages.
- [ ] B-05 — DIPL: displayed audience/dialogue/menu state.
- [ ] B-06 — COLS: shown colony and staged modes/settings.
- [ ] B-07 — CBLD: displayed building/satellite layout/identities.
- [ ] B-08 — CEVT: displayed colony event state.
- [ ] B-09 — CPRD: displayed production cost/turns.
- [ ] B-10 — BLDQ: queue under edit and modes.
- [ ] B-11 — BLDL: offered products/refits and displayed numbers.
- [ ] B-12 — DSGN: staged design and derived values.
- [ ] B-13 — DSBX: all component picker offerings/selections.
- [ ] B-14 — FMOV: safe native movement verdicts.
- [ ] B-15 — MSGB: all box kinds and rendered substitutions.
- [ ] B-16 — TPOP: all 18 popup kinds and dynamic tails.
- [ ] B-17 — HOFM: initialized sorted scores.
- [ ] B-18 — MPLY: all 19 multiplayer phases.
- [ ] B-19 — CMBT: authorized live battle state.
- [ ] B-20 — CMSL: visible live ordnance.
- [ ] B-21 — CRES: retained observed battle result.
- [ ] B-22 — CTGT: safe native legal cell/target/weapon masks.
- [ ] B-23 — FBSC: actual fleet-box scroll.
- [ ] B-24 — CMEV: ordered per-subscriber effect stream and gaps.
- [ ] B-25 — CPOP: scan/board dialog state.
- [ ] B-26 — SHPV: authorized noncombat ship detail/name.
- [ ] B-27 — COPT: options panel and five native flags.
- [ ] B-28 — HLPL: active visible help content/regions.
- [ ] B-29 — RFIT: actual refit list/window/scroll.
- [ ] B-30 — FBTN: actual fleet order controls/eligibility.
- [ ] B-31 — INBX: name input initial/edited/current text and prompt.
- [ ] B-32 — TXTF: continuous text editing/committed state.
- [ ] B-33 — SYSW: actual system window/visible fleet bar.
- [ ] B-34 — PLSM: known planet rows/send modes/ship/ETA/hover verdicts.
- [ ] B-35 — MREL: map merge-relocation mode distinct from Fleets mode.
- [ ] B-36 — STLR: explicit transfer progress/stop/question/refusal outcome.
- [ ] B-37 — CSUM: current native sorted colony list and production/buy values.
- [ ] B-38 — FILM: active film/progress/skip/frame stream.

### Coverage sign-off: base payloads, protocol replacements, and remaining contexts

- [ ] COV-01 — Map all 21 direction-specific message names to implemented methods/streams or explicit unsafe/obsolete representation replacements.
- [ ] COV-02 — Cover base game/map/settings/new-game temporary values with typed permitted projections.
- [ ] COV-03 — Replace every raw-record family with authorized DTOs; explicitly exclude hidden Antaran schedules and opponent internals.
- [ ] COV-04 — Cover untagged icon owners/node order/selection without exposing hidden/stale nodes.
- [ ] COV-05 — Cover initial fields/state on connection and same-count field-content changes without legacy HELLO/rectangle assumptions.
- [ ] COV-06 — Replace LAYOUT/ext_layout tables/generator with explicit method/DTO contracts; record the intentional non-port disposition.
- [ ] COV-07 — Cover all non-HELLO input intents in Appendix D, including normal function/Alt shortcuts, without an unsafe raw-input escape hatch.
- [ ] COV-08 — Verify custom-race/race-selection/research/designer contexts 50-56 are distinct and never change native screen identity for reporting.
- [ ] COV-09 — Verify audience contexts 57/58, battle/scan/board contexts 65-67, noncombat ship detail 81, and film 86 lifetimes.
- [ ] COV-10 — Verify generic boxes/lists/name fields/help, flag/color choices, transport/refit, Info subscreens, tax, map boxes, and high-score contexts not represented by TPOP.
- [ ] COV-11 — Classify animation/AI/wait input polls and exercise cinematic raw-input paths without treating them as unrestricted decision contexts.
- [ ] COV-12 — Attach implementation/test/disposition evidence for every row of the 92 non-documentation commit ledger, including final amendments and companion fixes.
- [ ] COV-13 — Reconcile all 27 documentation-only commits as evidence/build guidance, without copying obsolete protocol documentation into the public portal.
- [ ] COV-14 — Reconcile all 61 changed-file dispositions in Appendix H; verify no protocol-related hunk is silently omitted.

### Final validation and release gates

- [ ] T-01 — Pass paired-state noninterference tests for hidden ships/colonies/research/leaders/AI intentions/RNG across responses, IDs, totals, errors, previews, events, and revisions.
- [ ] T-02 — Pass fog/scanner/stealth/destination/ETA/scan disclosure tests and current/last-known/unknown field tests.
- [ ] T-03 — Compare charts, diplomacy pickers, reports, modals, combat, and ground displays against native visible data and precision.
- [ ] T-04 — Pass context replacement/restore tests, including identical field indices/rectangles in different dialogs and local/RPC input conflicts.
- [ ] T-05 — Pass new/load/end/unload/hotseat/reset/slot-reuse tests; prove old perspective publications cannot leak after revocation.
- [ ] T-06 — Pass committed-turn notification tests for cancelled submission, auto-turn, report/research interruptions, and network decode; loading must not look like turn advancement.
- [ ] T-07 — Pass atomic baseline/subscription/change/reconnect and snapshot-pinned pagination tests where applicable.
- [ ] T-08 — Pass operation refusal/duplicate/deadline/cancellation/disconnect/partial-result and nested-confirmation tests.
- [ ] T-09 — Pass all seven combat command and 25 event-type tests, including viewport, native refusals, ordering, streams/gaps, and RNG invariance.
- [ ] T-10 — Pass film left/right/key skip, frame count/palette ownership, input-delay/animation timing, and audio ownership/fallback tests.
- [ ] T-11 — Pass malformed/deep/oversized JSON, invalid envelope/method/type, unauthorized request, Host/Origin, and authentication-timeout tests.
- [ ] T-12 — Pass fragmented/coalesced WebSocket, partial output, close/ping, slow-reader, flood, bounded-memory, and multi-consumer tests.
- [ ] T-13 — Pass bind-failure, quit/fatal-exit during I/O, wake/join, and no-future-game-poll shutdown tests.
- [ ] T-14 — Run the existing doctest suite and save/mod/native-operation comparisons; verify read queries consume no gameplay RNG.
- [ ] T-15 — Run maximum-size-game capture/queue/output profiling and verify agreed budgets with no socket waits on the game thread.
- [ ] T-16 — Run supported sanitizer/thread checks and review all live-state/platform handoffs.
- [ ] T-17 — Build feature on/off on macOS, Linux, Windows x86/x64, and supported MinGW targets; verify router-only builds stay independent.
- [ ] T-18 — Exercise native-only, no-client, reader-only, external-controller, multi-reader, and disconnect-to-local-fallback modes.
- [ ] T-19 — Exercise representative renderer, voice-control, and agent clients against the same contracts; keep policies/briefings client-side.
- [ ] T-20 — Confirm all coverage checklists and ledger rows have evidence, all visibility release blockers are resolved, and no hidden-data bypass was introduced to satisfy parity.
