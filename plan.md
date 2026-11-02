# Player-scoped RPC and WebSocket interface: implementation plan

## 1. Goals and working rules

- Provide one semantic interface for independent renderers, voice clients and
  agents. Keep client policies, briefings and threat classification client-local.
- Preserve gameplay, saves, multiplayer authority, mods and deterministic RNG.
  Do not change compatibility-sensitive native behavior as part of RPC work.
- Always prefer canonical game behavior. The canonical game UI is the source of
  truth for RPC disclosure and semantics: expose what it shows and withhold what
  it hides for the same player, field and active context. Audit and test that
  authority before enabling a field; do not invent a divergent RPC policy.
- Read repository instructions and relevant source/tests before editing. Make
  small, tested changes; never commit or push without a request, and preserve
  unrelated work, including untracked `.voc/`.
- Keep this file to rules, high-level decisions and TODOs. No restoration-specific
  narratives, source audits, implementation explanations, measurements or test
  histories. Document current contracts and durable decisions, not the history of
  producing them. Tests are the primary behavioral evidence; canonical field
  policies hold native authorization anchors. Keep only unique supporting audits
  in `specs/rpc/`; do not duplicate policies, commands or per-increment test counts.
- Keep task IDs stable and partial items unchecked. Checked subset tasks do not
  imply full-catalog or interactive parity.
- Missing assets/save fixtures block interactive parity verification, not
  implementation. Distinguish synthetic scope/native-function and live-transport
  evidence from interactive saved-game/multiplayer parity.
- Regenerate schema, reference and C++ metadata from `specs/rpc/methods.json`,
  `contracts.json` and `field_policies.json`; never edit generated output manually.
- Preserve coverage obligations for every fork change. Historical inventories and
  exact dispositions are in [the coverage baseline](specs/rpc/planning_baseline.md),
  with pinned revisions in `specs/rpc/baseline.json`. Refresh inventories when a
  baseline changes; update implementation traceability at capability completion.

## 2. Architecture and ownership

- Native code remains RPC-unaware. Changes are limited to neutral lifecycle and
  observation hooks: no RPC headers, DTOs, dispatch, settings or control-flow
  dependency. Other native API additions or shared-policy/operation extraction
  require separate approval.
- Application composition connects the optional sidecar before the game thread
  starts and disconnects after it joins. Dependency direction is
  `game -> neutral events` and `RPC -> game APIs/events`, never `game -> RPC`.
- Only the game thread reads mutable native state or calls game operations.
  Transport threads own sockets and encoded output. Only copied requests and
  immutable publications cross the bounded bridge; no game-thread socket waits.
- Neutral callbacks are synchronous, observation-only and non-throwing. Borrowed
  native payloads are callback-local; no borrowed pointers escape.
- Reuse already calculated authorized state. Queries must not invoke mutable
  builders, renderers, movement calculators or report-consuming routines.
  Default to extracting shared read-only disclosure/presentation helpers used by
  both native UI and RPC before proposing observation hooks or alternative
  architectures. Give helpers the actual player/result context, not blanket
  access to a record. Add only the owner-lifetime publications that cannot be
  supplied safely by existing state; do not infer authorization from screen
  globals or build a UI framework to recover shared read semantics.
  Audited existing read-only range and planet-suitability predicates may run
  during coherent capture; this does not authorize proposed ship movement/ETA
  calculation or a new calculator.
- Capture all requested topics, including session metadata, coherently. Never
  enrich retained snapshots in place or splice later UI values into older
  captures. During resolution, return busy or explicitly dated authorized state.
- Global reads are independent of the open screen. Context-bound fields/actions
  require initialized active owner/selection authority, not just screen numbers,
  background drawing, cached payloads or stale selection globals.
- Screen-local sidecars own context/control generations and nested lifetimes.
  Do not change native `MOX::current_screen` to report RPC-only context identity.
  Reuse native operations only after auditing rules, recalculation, confirmation,
  mods and multiplayer authority; never duplicate native input branches/rules.
- Bound requests, captures, retention, pagination, subscriptions and output.
  Baseline/cursor setup is atomic. Coalesce only unsent state invalidations;
  preserve partial messages and ordered events/results.
- Revoke authorization-dependent snapshots, pinned pages, queued work,
  subscriptions, bridge publications and service-owned output at their lifecycle
  boundaries. Preserve ambient offscreen reads where still authorized.
- Icon rebuild revokes context-dependent authority; only fresh authorized native
  presentation may restore it. A revoked context page may expire even if the new
  public projection is equal. Compare hidden-only noninterference with lifecycle
  boundaries held equal; private node/epoch changes alone must not churn public
  handles, ordering, revisions or notifications.
- Thread stopping precedes native teardown and wakes/joins sidecar work without
  another native poll, including exceptions.
- Follow C-oriented C++ style: plain structs, free functions and explicit
  ownership; no native-layout reflection or generic remote-object framework.

## 3. Player-visible-only contract (release blocker)

- Bind connections to the active local human perspective. No caller-selected
  viewer, host-memory privilege, remote debug/omniscience switch, Lua evaluation
  or RNG access. Native omniscience follows native per-field disclosure rules.
- Authorize objects and fields independently. Detection, contact, ownership and
  omniscience alone do not authorize routes, ETA, designs, cargo or orders.
- Filter before hidden-data copying, IDs, sorting, counts, validation/errors,
  pagination, caching, revisions and notifications. Hidden-only slot/node/epoch
  activity must not affect public observations.
- Omit unknown objects. Distinguish current, last-known, unknown, known-empty,
  context-required and unaudited fields. Do not invent knowledge or replace old
  knowledge with hidden live data. Unreviewed fields stay disabled.
- IDs are opaque per world/perspective. Unknown, nonexistent and no-longer-visible
  handles have indistinguishable public errors. Read observation handles are not
  durable identities or action capabilities; native allocation gaps stay private.
- Load/reset and perspective revocation clear stale identities/publications.
  Hotseat revocation requires fresh configured-key hello; existing connections
  must not silently follow the next empire.
- Recheck action ownership, native phase, local authority and context at execution.
  Previews, target lists, errors and timing must not become hidden-object or
  future-outcome oracles.
- Apply visibility to messages/arguments, retained results, media, effects, sound
  and frames. Locally calculating an unseen battle does not authorize it.
- `Ship.name` remains the canonical in-game name. Never substitute hull/UI labels
  or add `displayName` without approval. Hull disclosure does not authorize names.
- Preserve unsigned-byte cached ETA, including zero. Current ETA requires a
  current same-capture destination. Withheld destinations are not known-empty;
  special destinations must not acquire fabricated system handles.
- Preserve native incoming-predicate non-own, not necessarily hostile, semantics
  and native outpost behavior. Add no server briefing/threat API.
- Never read foreign treasury/HUD, including under omniscience. Raw colony morale
  requires a separate suppression, rounding and icon-cap audit.
- Exclude raw layouts, arbitrary resources/files/config/mod storage, save bytes,
  cheats and unrestricted keyboard/mouse injection.

## 4. Common API catalog

This is the planned surface, not an enablement list. Advertise only implemented,
field-audited methods/topics; schemas alone grant no authority.

| Group / common methods | Scope |
| --- | --- |
| `rpc.hello`, `rpc.describe`, `rpc.subscribe`, `rpc.unsubscribe` | Authentication, discovery and bounded selected streams |
| `game.getState(include)`, `game.getSession` | Coherent requested capture and lifecycle metadata |
| `game.listPlayers`, `game.getPlayer` | Known roster and separately authorized empire fields |
| `game.listSystems`, `game.getSystem`, `game.listPlanets`, `game.getPlanet`, `game.listNebulas` | Authorized map/exploration/planet knowledge |
| `game.listColonies`, `game.getColony` | Own details and separately audited foreign intelligence |
| `game.listShips`, `game.getShip`, `game.listFleets`, `game.getFleet` | Observations with field-specific route/detail authority |
| `game.getMovementOptions` | Native legal verdicts without hidden-information oracles |
| `game.listDesigns`, `game.getDesign`, `game.getDesignOptions` | Own designs/options and scan-authorized enemy details |
| `game.getResearch`, `game.listResearchOptions`, `game.getTechnology` | Own research and authorized reference knowledge |
| `game.listLeaders`, `game.getLeader` | Owned and actually offered/discovered leaders |
| `game.getRelations`, `game.getIntelligence` | Known contacts, own treaties and authorized intelligence |
| `game.getHistory`, `game.listMessages`, `game.getMessage` | Native precision and non-consuming reports |
| `ui.getContext`, `ui.getView`, `ui.getControls` | Initialized active contexts and visible semantic controls |
| `ui.open`, `ui.back`, `ui.close` | Native navigation, not screen-global writes |
| `ui.activate`, `ui.cancel`, `ui.setText`, `ui.setValue`, `ui.select`, `ui.point` | Typed context-bound interactions |
| `setup.getView`, `setup.listSaveSlots`, `setup.getHallOfFame` | Native setup, local slot metadata and scores |
| `lobby.getView`, `lobby.listSessions`, `lobby.getChat` | Native lobby views without query-time network pumping |
| `combat.getView`, `combat.getUnit`, `combat.getTargetOptions`, `combat.getResult` | Observed battle and retained authorized results |
| `invasion.getView` | Authorized bombardment/ground-combat views |
| `reference.get`, `media.getResource`, `presentation.getFrame`, `presentation.getState` | Allowlisted content and atomic presentation |

## 5. Notifications, consistency and operations

- Turn advancement follows higher committed, coherently queryable state, not
  screen transitions or submission. Deduplicate by game/turn; loading is not a turn.
- Session notifications describe new/load/end/unload/revocation. Context changes
  describe initialized active/nested lifetimes, not raw screen assignments.
- Subscription baselines/cursors are atomic. Hidden-only changes emit nothing.
  Reconnect requires a fresh baseline; coalesced invalidations mean re-read, not
  replay lost ordered events.
- Pagination pins immutable captures and checks revocation. Combined state must
  not mix captures. Expiry/overflow is explicit, never truncation.
- Actions carry context/generation and entity references. Distinguish queued,
  dispatched, applied, refused, partial and unknown-after-disconnect outcomes.
  Bound results, duplicate protection, deadlines and cancellation.
- Nested decisions use owner-consumed deferred intents, not recursive mutation
  dispatch or suppressed native confirmations. Animation/cinematic observation
  polls are not unrestricted decision points.

## 6. Transport, security and presentation

- Use optional loopback JSON-RPC 2.0/WebSockets, off by default. Disabled and
  router-only builds must not acquire RPC dependencies or gameplay authority.
  Bind failure disables RPC with a diagnostic.
- Use strict yyjson validation, configured local `rpc_key` hello, roles, one
  controller, Host/Origin checks and authentication deadlines. No per-launch secret
  file or approval dialog; never put secrets in URLs/logs.
- Validate bounded messages, depth, arguments, IDs, queues and deadlines before
  dispatch; rate-limit and recheck authority at execution.
- Keep renderer/window work on its platform owner. Frames require atomic matching
  frame/palette ownership; never read a mutating presenter surface.
- External presentation/audio ownership is explicit with disconnect-to-local
  fallback. Preserve timing, input delays, skippability and chosen effect RNG;
  reader connections must not silence audio or alter gameplay.
- Resource/language/install selection is trusted local configuration, not remote
  filesystem access. Do not add proprietary assets or saves to fixtures.

## 7. Delivery and validation rules

- Deliver bounded client capabilities, not individual fields or prerequisite
  commits. Before starting, name the outcome, related methods/fields/contexts,
  acceptance tests and exclusions. Audit related variants together and stop when
  the agreed capability passes; do not expand it opportunistically.
- Map native producers, authorization, owner lifetimes, identity, coherence and
  revocation before implementation. If a substantial shared dependency is missing,
  make it an explicit milestone or defer dependent fields. Do not build competing
  per-field lifecycle mechanisms or a speculative generic framework.
- Prefer one owner for each state transition. Separate native projection,
  normalization/linking, snapshot retention and wire encoding with concrete free
  functions as needed. Reuse existing mechanisms before adding flags, epochs,
  queues or caches; keep hidden-data filtering and thread boundaries explicit.
- Keep commits reviewable without treating every commit as a separate project.
  Reassess scope when investigation repeatedly finds prerequisites without an
  end-to-end result. Progress reports state delivered capability, checks and
  blockers; record timings automatically where practical, not in manual diaries.

| Phase | Completion gate |
| --- | --- |
| P0: contracts/evidence | Field policies, registry, backend/platform approval and capture/output budgets |
| P1: global reads | Coherent authorized queries/subscriptions and read-family noninterference |
| P2: shared UI | Context lifetimes, shared native controls and nonrecursive operations |
| P3: strategic sidecars | Strategic views/actions share native behavior and visibility |
| P4: setup/reports/multiplayer UI | Setup/report/lobby lifetimes and native multiplayer authority |
| P5: battle/invasion | Observed views/actions/effects with unchanged ordering and RNG |
| P6: full parity | All coverage obligations, lifecycle modes and supported platforms verified |

- During development, run focused tests. Before a requested code commit, build
  affected targets and run relevant tests, affected formatting and whitespace
  checks; check generator drift when canonical inputs or generation change.
- Run relevant ASAN/UBSAN checks for native-hook, ownership or lifetime changes
  before treating them as complete. At capability completion, run normal and
  sanitizer integration tests, relevant live RPC Python/transport tests, the
  RPC-disabled regression suite and generator checks. Include affected native
  owners. Shared commands live in `specs/rpc/README.md#verification`.
- For documentation-only changes, run affected contract/audit/link checks and
  whitespace checks, not game builds or unrelated runtime suites. Keep completion
  notes short: outcome, checks and outstanding limitations, without repeated
  architecture summaries, historical counts or fixture-debugging narratives.
- Report unperformed checks accurately. Unsupported local LeakSanitizer may be
  disabled without disabling ASAN/UBSAN; do not claim leak verification.
- Require noninterference, native-view parity, lifecycle/reuse, atomic baselines,
  pinned-page revocation, protocol/output bounds, action authority and platform
  checks before release sign-off. Capability-sized delivery does not waive these
  requirements or turn synthetic tests into interactive parity evidence.

## Implementation tracking TODOs

### P0. Contracts, dependency approval and baseline

- [x] P0-01 — Pin target/fork revisions and audit inventories when baselines change.
- [x] P0-02 — Establish exact coverage for all fork commits and changed files.
- [x] P0-03 — Define typed requests/results, errors, units, enums and lifecycle availability.
- [x] P0-04 — Use one descriptor source for validation and explicit discovery.
- [x] P0-05 — Require native evidence and field policies; disable unaudited fields.
- [ ] P0-06 — Approve, pin and build-check the WebSocket backend across supported targets.
- [x] P0-07 — Specify strict JSON-RPC/yyjson handling without legacy binary compatibility.
- [x] P0-08 — Audit safe points, nested decisions, animation/cinematic polls and cleanup.
- [ ] P0-09 — Profile maximum-game captures/output and finalize runtime budgets.
- [ ] P0-10 — Capture interactive native/save/mod/RNG parity baselines.
- [x] P0-11 — Generate schema/reference/C++ metadata from canonical inputs; reject drift.

### P1. Transport, bridge, global queries and visibility

#### Infrastructure

- [x] P1-01 — Add optional RPC independent of disabled/router-only builds.
- [x] P1-02 — Bound WebSocket connections/messages and diagnose bind failures.
- [x] P1-03 — Implement configured-key hello, roles, one controller, Host/Origin and authentication deadlines.
- [x] P1-04 — Validate envelopes/arguments with structured errors, limits and redacted diagnostics.
- [x] P1-05 — Implement copied game-thread work and immutable publications without network waits.
- [x] P1-06 — Implement coherent initial session/map capture and opaque lifecycle/object identities.
- [x] P1-07 — Return busy/unavailable or explicitly dated snapshots during unsafe periods.
- [x] P1-08 — Implement atomic audited-topic baselines, unsubscribe and demand-driven capture.
- [x] P1-09 — Bound/coalesce unsent state output while preserving partial messages and ordered results.
- [x] P1-10 — Cancel/wake/join on shutdown independently of another game poll.

#### Shared player-view policy

- [x] V-01 — Bind enabled queries to local perspective; reject caller-selected/privileged views.
- [x] V-02 — Complete system/planet/nebula disclosure using audited native rules.
- [x] V-03 — Copy surviving ship-refresh observations without query-time rebuilds or stale traversal.
- [ ] V-04 — Independently authorize design/scan details, destinations, orders, cargo and ETA.
- [ ] V-05 — Complete player/colony/leader/research/diplomacy/intelligence/chart field policies.
- [ ] V-06 — Preserve current/last-known/unknown/known-empty distinctions across query families.
- [x] V-07 — Filter enabled projections before IDs, counts, sorting, caching, pages, revisions and events.
- [x] V-08 — Make unavailable-handle errors indistinguishable; reject hidden-property probes.
- [ ] V-09 — Apply visibility to messages, previews, battle results, effects, sound, frames and resources.
- [x] V-10 — Revoke enabled views, identities, work/results, snapshots and subscriptions on world/perspective changes.
- [x] V-11 — Require fresh hello after perspective revocation; never silently follow hotseat changes.
- [ ] V-12 — Complete raw-layout/debug/RNG/Lua/file/config/save/cheat exclusions.

#### Common query groups

- [x] Q-01 — Implement hello/discovery/subscriptions for audited topics/events.
- [x] Q-02 — Implement session and coherent selected-state reads for enabled topics.
- [x] Q-03 — Implement player list/detail for audited known-roster and own HUD/economy fields.
- [x] Q-04 — Complete system list/detail with authorized map/range/wormhole data.
- [x] Q-05 — Complete planet list/detail and nebula reads.
- [ ] Q-06 — Complete colony production/jobs/buildings/troops/events/transfers and foreign intelligence.
  - [x] Deliver committed own-full-colony operations inspection: production/queue, workforce, native operating presentations, structures/satellites, normal garrison, events, purchase display and current inbound logistics, with coherent subscriptions and revocation; see [the client workflow](specs/rpc/README.md#owned-colony-operations-dashboard).
  - [ ] Complete separately audited foreign-colony intelligence. Own outposts remain outside this full-colony projection; colony-changing actions remain P3 work, not parity implied by the finished inspection subset.
- [ ] Q-07 — Complete conditional ship destination/ETA, selection/scan details and durable identity.
  - [x] Deliver ambient owned ship condition/capability/configuration inspection alongside fleets, committed designs and empire topics; see [owned inspection](specs/rpc/owned_fleet_view.md). This is not foreign scan, action or durable-identity parity.
  - [x] Finish one bounded public cached-ETA path: authorize the surviving own wormhole-route head from canonical native presentation.
  - [x] Prove initialized owner plus current open-box/selection authority for that path through child interruption and replacement; never promote pre-initialization results.
  - [x] Bind that path only to current surviving refresh associations; reject stale selections and slot/node reuse without later node traversal or hidden-fingerprint effects.
  - [x] Publish fresh coherent immutable captures for all requested topics, including session; never enrich older snapshots.
  - [x] Revoke bounded ETA context data through retained/pinned captures, subscriptions, bridge work/publications and service output on close/replacement/rebuild, owner exit, world updates, resolution and perspective changes; preserve authorized ambient reads.
  - [x] Test the bounded path's initialization, revocation, reuse, copied lifetimes, hidden-only noninterference, coherent state/baselines, pinned pages and indistinguishable unavailable handles before disclosure.
  - [x] Extend the completed cached fleet-box ETA path to surviving non-own wormhole heads with independently current ambient destinations, safe private association and end-to-end revocation.
  - [x] Extend to a surviving non-own departing wormhole head with actual same-frame contextual route-line and cached-ETA presentation, revoking destination and ETA together without weakening ambient gates.
  - [x] Extend contextual destination and cached fleet-box ETA to ordinary departure/transit using audited native movement encodings and the same complete capture/revocation pipeline.
  - [ ] Extend conditional destination/ETA and selection/scan disclosure to remaining native contexts; audit each additional field and establish durable identity separately.
- [ ] Q-08 — Implement fleet list/detail without rebuilt-stack identifiers.
  - [x] Deliver owned active fleet groups and native ordered membership through existing list/get, coherent captures, pagination and revocation; groups are observation handles, not rebuilt-stack IDs.
  - [ ] Complete separately authorized non-owned fleet grouping and remaining full-catalog parity.
- [ ] Q-09 — Implement movement options with audited scratch state and no hidden-data oracle.
- [ ] Q-10 — Implement authorized design list/detail/options.
  - [x] Deliver owned committed slots, active configurations and independently authorized committed queue/refit configurations through existing list/get and subscriptions. Preserve source distinctions and native suppression; staged/generated designs are excluded.
  - [ ] Complete separately audited options, foreign scan designs and remaining full-catalog parity.
- [x] Q-11 — Implement own research/choices and authorized technology references.
- [x] Q-12 — Implement owned/offered/discovered leader reads.
- [x] Q-13 — Implement relations/own-intelligence dashboard reads with individually audited fields and scoped native disclosures; see [the client workflow](specs/rpc/README.md#relations-and-own-intelligence).
- [x] Q-14 — Implement native-precision history and non-consuming retained-summary reads; see [the turn-review workflow](specs/rpc/README.md#turn-review-historical-trends-and-retained-messages).
- [x] Q-15 — Implement bounded pinned pagination and complete captures for enabled lists.

#### Notifications

- [x] N-01 — Emit committed queryable turns through single-player and authoritative network paths.
- [ ] N-02 — Emit initialized active/nested context changes and parent restoration.
- [ ] N-03 — Emit session new/load/end/unload/revocation changes and invalidate identities.
- [ ] N-04 — Complete selected state/UI subscriptions without hidden-only notifications.
- [x] N-05 — Validate a client-local incoming-hostile example using authorized reads; no server briefing API.

### P2. Shared UI and operation lifecycle

- [ ] P2-01 — Register scoped contexts with parent restoration and unregister before native release.
- [ ] P2-02 — Track context/control generations through rebuild/replacement/mark/release/restore/local actions.
- [ ] P2-03 — Implement visible semantic context/view/control reads.
- [ ] P2-04 — Implement open/back/close through native navigation.
- [ ] P2-05 — Implement typed activate/cancel/select/value interactions.
- [ ] P2-06 — Implement legacy/continuous text, limits, cursor, commit and cancel.
- [ ] P2-07 — Implement authorized grid pointing, right-click/help and map interaction.
- [ ] P2-08 — Implement visible lists, row identity, enabled state, selection and scroll.
- [ ] P2-09 — Implement message/timed/warning/confirmation children with rendered text and actual choices.
- [ ] P2-10 — Capture active help without debug controls or raw format memory.
- [ ] P2-11 — Implement bounded operation identity/results, state transitions, deadlines and cancellation.
- [ ] P2-12 — Implement duplicate protection, partial outcomes and unknown-after-disconnect behavior.
- [ ] P2-13 — Implement nonrecursive mutations and owner-consumed nested intents.
- [ ] P2-14 — Preserve input delays; separate animation observation from decision dispatch.
- [ ] P2-15 — Map legitimate function/Alt shortcuts semantically; block raw injection/cheats.
- [ ] P2-16 — Share audited native operations without duplicating rules or authority.

### P3. Strategic screens and actions

- [ ] P3-01 — Add map pan/zoom/focus/selection/navigation and native submit/cancel turn.
- [ ] P3-02 — Add map boxes, visible fleet membership/order, selection/scroll/buttons and merge-relocation.
- [ ] P3-03 — Add colony modes/buildings/satellites/events/jobs/production/displayed derived values.
- [ ] P3-04 — Implement native checked job assignment and gesture handling.
- [ ] P3-05 — Implement same-system transfers with refusals/atomicity.
- [ ] P3-06 — Implement interstellar transfers with questions/jobs/partial outcomes/empty-slot rejection.
- [ ] P3-07 — Implement production/buy/queues/autobuild and displayed cost/turn/eligibility.
- [ ] P3-08 — Add colony transport/relocation/refit lists/scroll and troop children.
- [ ] P3-09 — Implement demolish with native ordering/confirmation/refund/recalculation/nesting safety.
- [ ] P3-10 — Add Colonies list/sort/window/jobs/transfers/buy/navigation.
- [ ] P3-11 — Add Fleets selection models/filter/sort/scroll/scan/relocate/merge.
- [ ] P3-12 — Implement native fleet move/engage/ignore/colonize/outpost/transport/attack/scrap/leader actions.
- [ ] P3-13 — Add Planets filter/sort/send/rows/scroll/ship/ETA and disclosed hover verdicts.
- [ ] P3-14 — Add research/change/group lists and semantic shared-commit selection.
- [ ] P3-15 — Add staged designer fields/derived values/accept/cancel.
- [ ] P3-16 — Add shield/computer/weapon/special pickers with offered choices.
- [ ] P3-17 — Add Leaders views and native hire/reject/assign/dismiss/inspect.
- [ ] P3-18 — Add Races/intelligence and checked spy/mission/agent actions.
- [ ] P3-19 — Add player/AI audiences, messages/menus/chat and native choices.
- [ ] P3-20 — Add treaty/gift/demand/money/technology/system/counteroffers without AI forecasts.
- [ ] P3-21 — Add Info history/technology/statistics/messages/reference navigation.
- [ ] P3-22 — Add distance/command-point/tax/freighter slider/scrap flows.
- [ ] P3-23 — Exercise every strategic input owner, delegated help/lists and early returns.

### P4. Setup, reports and session UI

- [ ] P4-01 — Implement setup views, local slot metadata and Hall of Fame reads.
- [ ] P4-02 — Add menu/animation/credits/new-game/settings/exit contexts.
- [ ] P4-03 — Add race/customization/flag/color and stock-to-custom acceptance.
- [ ] P4-04 — Add game/options/load/save, both slot-list routes, descriptions and confirmations without file access.
- [ ] P4-05 — Add initialized scores through menu and result-entry routes.
- [ ] P4-06 — Add science discovery/reason/progress, initial research and mutation contexts.
- [ ] P4-07 — Add leader offers and promotion/marooned-hero reports.
- [ ] P4-08 — Add colony/outpost/troop targets, discovery/landing/naming/unloading.
- [ ] P4-09 — Add GNN/summary links/report orchestration/Antaran-room presentations.
- [ ] P4-10 — Add council nominees/votes/messages/member ordering and decisions.
- [ ] P4-11 — Add Dragon offer/payment/poor modes and dynamic eligible stars.
- [ ] P4-12 — Implement lobby/session/chat views without query-time network pumping.
- [ ] P4-13 — Implement native host/join/position/ready/chat with multiplayer authority.
- [ ] P4-14 — Implement hotseat setup and secure perspective switch/reauthorization.
- [ ] P4-15 — Exercise every report/setup/lobby owner, keyboard/timed routes, cancel/nesting/restoration.

#### Multiplayer phases

- [ ] MP-01 — MP_SETUP: mode/setup controls.
- [ ] MP-02 — MP_ONLINE_SETUP: endpoint validation.
- [ ] MP-03 — MP_NET_GAME_NAME: name entry/duplicate warning.
- [ ] MP-04 — MP_LOAD_GAME: multiplayer slot list.
- [ ] MP-05 — MP_HOTSEAT_SETUP: human roster/races.
- [ ] MP-06 — MP_HOTSEAT_SELECT: position/reauthorization.
- [ ] MP-07 — MP_HOST_INIT: initialization status.
- [ ] MP-08 — MP_HOST_WAIT_JOINERS: roster/wait/begin.
- [ ] MP-09 — MP_HOST_WAIT_RACES: race-info wait.
- [ ] MP-10 — MP_HOST_SENDING: transfer status.
- [ ] MP-11 — MP_HOST_PICK_POSITION: loaded host position.
- [ ] MP-12 — MP_HOST_WAIT_LOADED: loaded roster/wait/begin.
- [ ] MP-13 — MP_JOIN_INIT: initialization status.
- [ ] MP-14 — MP_JOIN_GAME_LIST: advertised sessions/join choice.
- [ ] MP-15 — MP_JOIN_WAITING: host wait.
- [ ] MP-16 — MP_JOIN_GENERATING_MAP: progress without hidden map data.
- [ ] MP-17 — MP_JOIN_PICK_POSITION: loaded join position.
- [ ] MP-18 — MP_JOIN_GETTING_DATA: receive status without raw state.
- [ ] MP-19 — MP_NET_NEXT_TURN: ready/chat/wait controls.

### P5. Combat and invasion

- [ ] P5-01 — Implement observed battle view/unit/target/result reads.
- [ ] P5-02 — Add scoped battle/actor/turn/selection/viewport/weapon state and children.
- [ ] P5-03 — Authorize units/ordnance/scan/legal masks without hidden-target oracles.
- [ ] P5-04 — Add scan/noncombat detail/name/board/marine/help/options views.
- [ ] P5-05 — Implement CCMD_MOVE with native legal cells and viewport/range safety.
- [ ] P5-06 — Implement CCMD_FIRE with native target/mask/phase/refusal checks.
- [ ] P5-07 — Implement CCMD_FIRE_MISSILE with safe offscreen handling.
- [ ] P5-08 — Implement CCMD_FACE with logical coordinates.
- [ ] P5-09 — Implement CCMD_SELECT with ownership/initiative checks.
- [ ] P5-10 — Implement CCMD_REMOVE_STASIS with eligibility/confirmation.
- [ ] P5-11 — Implement CCMD_BOARD, native second step/refusals/marine/raid choices.
- [ ] P5-12 — Implement done/wait/auto/retreat/options and observed opponent waits.
- [ ] P5-13 — Capture authorized results before cleanup; revoke on battle/reset/perspective changes.
- [ ] P5-14 — Implement bounded ordered streams, baseline/sequence/time and gap handling.
- [ ] P5-15 — Implement invasion views and native bombard/invade/destroy/mind-control/cease-fire/drop-troop choices.
- [ ] P5-16 — Capture displayed ground counts/strength/labels/sprites/beams/explosions/flics.
- [ ] P5-17 — Audit battle/ground visibility, ordering and RNG invariance.

#### Combat events

- [ ] CE-01 — CEV_BEAM_SHOT, including slotless Energy Absorber and ordering before death.
- [ ] CE-02 — CEV_REFLECT.
- [ ] CE-03 — CEV_BOMB.
- [ ] CE-04 — CEV_SPECIAL and authorized following state.
- [ ] CE-05 — CEV_MISSILE_LAUNCH.
- [ ] CE-06 — CEV_MISSILE_MERGE for live missiles only.
- [ ] CE-07 — CEV_MISSILE_HIT.
- [ ] CE-08 — CEV_FIGHTER_PASS.
- [ ] CE-09 — CEV_MISSILE_GONE on actual live removal only.
- [ ] CE-10 — CEV_MOVE at move start, including teleport/cost.
- [ ] CE-11 — CEV_ROTATE.
- [ ] CE-12 — CEV_DESTROY before cleanup.
- [ ] CE-13 — CEV_RETREAT.
- [ ] CE-14 — CEV_BLAST_HIT.
- [ ] CE-15 — CEV_WEB_DAMAGE.
- [ ] CE-16 — CEV_CAPTURE and displayed boarding outcome.
- [ ] CE-17 — CEV_RAID.
- [ ] CE-18 — CEV_COMMAND observability/operation outcomes.
- [ ] CE-19 — CEV_SOUND with logical ticks.
- [ ] CE-20 — CEV_SOUND_CHANGE with handle/fade/loop/pitch semantics.
- [ ] CE-21 — CEV_FIGHTER_BEAM.
- [ ] CE-22 — CEV_BEAM_AIM from chosen endpoints without resetting sound timing.
- [ ] CE-23 — CEV_GYRO_SPIN from native chosen spin.
- [ ] CE-24 — CEV_ABSORB with native discharge/after-action clearing.
- [ ] CE-25 — CEV_CONVERTER_HIT with shield/structure shares distinguished.

### P6. Presentation and companion fixes

- [ ] P6-01 — Implement bounded allowlisted reference/media, not arbitrary files/LBX entries.
- [ ] P6-02 — Implement atomic matching presentation/frame/palette and optional streams.
- [ ] P6-03 — Implement platform-thread visibility/hidden frames without worker renderer access.
- [ ] P6-04 — Implement explicit presentation/audio ownership and local fallback.
- [ ] P6-05 — Deliver sound/change handles without reader-induced native silence.
- [ ] P6-06 — Preserve effect/sound timing, input delays and timed decisions when skipping animation waits.
- [ ] P6-07 — Add film identity/progress/frames and non-field observation pumping.
- [ ] P6-08 — Preserve film left/right/key skip, frame counts/pacing without sticky input.
- [ ] P6-09 — Preserve trusted local language/install precedence/fallback without remote paths.
- [ ] P6-10 — Reuse native speed/VSync and prevent hidden-presentation hangs.
- [ ] P6-11 — Separately audit/test the custom-race old_race companion fix against upstream.
- [ ] P6-12 — Separately audit/test the clipped-bitmap/tractor zero-scale companion guard.
- [ ] P6-13 — Audit research null/selection and colony refusal/recalculation guards against upstream.

### Native screen coverage sign-off

- [ ] S-01 — SCREEN_UNKNOWN: unavailable/no navigation.
- [ ] S-02 — SCREEN_MAIN: map/nested boxes.
- [ ] S-03 — SCREEN_COLONY: colony/children.
- [ ] S-04 — SCREEN_DESIGN: designer/pickers.
- [ ] S-05 — SCREEN_FLEET: selection/orders.
- [ ] S-06 — SCREEN_RACE: contacts/intelligence/diplomacy.
- [ ] S-07 — SCREEN_EXIT: end/cleanup.
- [ ] S-08 — SCREEN_GAME: options/load/save.
- [ ] S-09 — SCREEN_INFO: all subscreens.
- [ ] S-10 — SCREEN_MAIN_MENU: menu/routes.
- [ ] S-11 — SCREEN_UNK2: unused disposition.
- [ ] S-12 — SCREEN_MAIN_NEXT_TURN: processing without premature advancement.
- [ ] S-13 — SCREEN_NEW_GAME: settings/race children.
- [ ] S-14 — SCREEN_HALL_OF_FAME: initialized data/both routes.
- [ ] S-15 — SCREEN_MULTI_PLAYER: setup/online child.
- [ ] S-16 — SCREEN_HOTSEAT: roster/races.
- [ ] S-17 — SCREEN_HOTSEAT_SELECT_PLAYER: authorized switch.
- [ ] S-18 — SCREEN_PLANET_DATA: known details.
- [ ] S-19 — SCREEN_COLONY_SUMMARY: list/operations.
- [ ] S-20 — SCREEN_START_NET: host phases.
- [ ] S-21 — SCREEN_JOIN_NET: join phases.
- [ ] S-22 — SCREEN_QUEUE_POPUP: products/queue/refit.
- [ ] S-23 — SCREEN_DISTANCE_BETWEEN_STARS: known-system selection.
- [ ] S-24 — SCREEN_OFFICERS: leaders/actions/offer children.
- [ ] S-25 — SCREEN_COLONIZATION_IN_MAIN: choice/confirmation/name.
- [ ] S-26 — SCREEN_BUILDING_OUTPOSTS_IN_MAIN: outpost flow.
- [ ] S-27 — SCREEN_PLANET_SUMMARY: known list/send modes.
- [ ] S-28 — SCREEN_COLONY_LANDING: presentation/name/dismiss.
- [ ] S-29 — SCREEN_UNLOADING_TRANSPORTS_IN_MAIN: troop target/confirmation.
- [ ] S-30 — SCREEN_SCRAP_FREIGHTERS: slider/confirmation.
- [ ] S-31 — SCREEN_TECH_CHANGE: research/change/details.
- [ ] S-32 — SCREEN_NET_NEXT_TURN: ready/chat/wait/commit.
- [ ] S-33 — SCREEN_REPORTS: orchestration/children.
- [ ] S-34 — SCREEN_TURN_SUMMARY: pages/links/dismiss.
- [ ] S-35 — SCREEN_LOAD_NET: load/position/host without previous-screen aliasing.
- [ ] S-36 — SCREEN_SHOW_COMMAND_POINTS: authorized detail.
- [ ] S-37 — SCREEN_CREDITS: credits/dismiss.

### Popup coverage sign-off

- [ ] TP-01 — Science reasons/discoveries/modifiers/progress/footer.
- [ ] TP-02 — Summary messages/pages/links.
- [ ] TP-03 — Leader offer/cost/maintenance/choices.
- [ ] TP-04 — Colony/outpost/troop targets and disclosed system/special.
- [ ] TP-05 — Discovered system/special/dismiss.
- [ ] TP-06 — Promotion/marooned hero.
- [ ] TP-07 — Presented GNN text/picture/authorized arguments.
- [ ] TP-08 — Battle target choices/cancel.
- [ ] TP-09 — Landing title/planet/colony/system/name.
- [ ] TP-10 — Invasion choices/counts/explosions.
- [ ] TP-11 — Ground sides/strength/summary/frame.
- [ ] TP-12 — Council votes/state/messages/order/choices.
- [ ] TP-13 — Troops/rebels/summary lines.
- [ ] TP-20 — Slider bounds/current/text/native steps.
- [ ] TP-22 — Presented Antaran message/correct message set.
- [ ] TP-23 — Mutation picks/options/refusals/accept.
- [ ] TP-24 — Dragon offer/payment/poor/eligible stars.
- [ ] TP-25 — Disclosed diplomacy system/planet/surrender choices.

### Tagged-state coverage sign-off

- [ ] B-01 — FSEL: visible fleet members/order/selection.
- [ ] B-02 — FLTS: Fleets view/modes.
- [ ] B-03 — OFFS: Leaders view/modes.
- [ ] B-04 — INFS: authorized chart/message precision.
- [ ] B-05 — DIPL: presented audience/menu.
- [ ] B-06 — COLS: colony/staged modes/settings.
- [ ] B-07 — CBLD: displayed building/satellite layout/identity.
- [ ] B-08 — CEVT: displayed colony events.
- [ ] B-09 — CPRD: displayed cost/turns.
- [ ] B-10 — BLDQ: edited queue/modes.
- [ ] B-11 — BLDL: offered products/refits/numbers.
- [ ] B-12 — DSGN: staged design/derived values.
- [ ] B-13 — DSBX: offered components/selections.
- [ ] B-14 — FMOV: safe native movement verdicts.
- [ ] B-15 — MSGB: box kinds/rendered substitutions.
- [ ] B-16 — TPOP: all popup kinds/dynamic content.
- [ ] B-17 — HOFM: initialized sorted scores.
- [ ] B-18 — MPLY: all multiplayer phases.
- [ ] B-19 — CMBT: authorized battle state.
- [ ] B-20 — CMSL: visible live ordnance.
- [ ] B-21 — CRES: retained observed results.
- [ ] B-22 — CTGT: safe legal cell/target/weapon masks.
- [ ] B-23 — FBSC: actual fleet scroll.
- [ ] B-24 — CMEV: ordered effects/gaps.
- [ ] B-25 — CPOP: scan/board children.
- [ ] B-26 — SHPV: authorized noncombat detail/name.
- [ ] B-27 — COPT: panel/native flags.
- [ ] B-28 — HLPL: active help text/regions.
- [ ] B-29 — RFIT: refit list/window/scroll.
- [ ] B-30 — FBTN: order controls/eligibility.
- [ ] B-31 — INBX: initial/edited/current name/prompt.
- [ ] B-32 — TXTF: continuous editing/committed text.
- [ ] B-33 — SYSW: system window/visible fleet bar.
- [ ] B-34 — PLSM: known planet rows/send/ship/ETA/hover.
- [ ] B-35 — MREL: map merge-relocation distinct from Fleets.
- [ ] B-36 — STLR: transfer progress/stop/question/refusal.
- [ ] B-37 — CSUM: sorted colonies and production/buy values.
- [ ] B-38 — FILM: active film/progress/skip/frames.

### Base payloads, replacements and remaining contexts

- [ ] COV-01 — Map all direction-specific messages to methods/streams or explicit replacements.
- [ ] COV-02 — Cover permitted game/map/settings/new-game temporary values.
- [ ] COV-03 — Replace raw records with DTOs; exclude hidden schedules/opponent internals.
- [ ] COV-04 — Cover icon owners/order/selection without hidden/stale nodes.
- [ ] COV-05 — Cover initial/same-count changed controls without legacy rectangle assumptions.
- [ ] COV-06 — Replace layout tables/generators with contracts and record non-port disposition.
- [ ] COV-07 — Cover all non-HELLO intents and legitimate shortcuts without raw injection.
- [ ] COV-08 — Distinguish setup/research/designer contexts without changing native screen identity.
- [ ] COV-09 — Verify audience/battle/scan/board/noncombat detail/film lifetimes.
- [ ] COV-10 — Cover shared boxes/lists/text/help/colors/transfers/refits/Info/tax/map/score contexts.
- [ ] COV-11 — Classify animation/AI/wait/raw-cinematic polls without unrestricted decisions.
- [ ] COV-12 — Attach evidence/disposition to every runtime commit, including amendments/companion fixes.
- [ ] COV-13 — Reconcile documentation-only commits without publishing obsolete protocols.
- [ ] COV-14 — Reconcile all changed-file dispositions; omit no protocol-related hunk.

### Final validation and release gates

- [ ] T-01 — Pass paired-state noninterference across outputs/IDs/errors/events/revisions.
- [ ] T-02 — Pass fog/scanner/stealth/destination/ETA/scan and knowledge-state tests.
- [ ] T-03 — Match native chart/diplomacy/report/modal/combat/ground data and precision.
- [ ] T-04 — Test replacement/restoration and local/RPC conflicts with reused controls.
- [ ] T-05 — Test world/perspective/reset/reuse revocation through queued publications.
- [ ] T-06 — Test committed turns across cancel/auto-turn/reports/research/network paths; never infer turns from load.
- [ ] T-07 — Test atomic baseline/change/reconnect and pinned pages.
- [ ] T-08 — Test refusal/duplicate/deadline/cancel/disconnect/partial outcomes/nested confirmation.
- [ ] T-09 — Test all battle commands/events, viewports/refusals/ordering/gaps and RNG invariance.
- [ ] T-10 — Test film skip/frame/palette ownership, timing and audio fallback.
- [ ] T-11 — Test malformed/deep/oversized JSON, invalid methods/types and authentication/authorization boundaries.
- [ ] T-12 — Test fragmentation/coalescing/partial output/close/ping/slow readers/floods/multi-client bounds.
- [ ] T-13 — Test bind failure/shutdown/fatal exit without future game polls.
- [ ] T-14 — Pass doctests and native/save/mod comparisons; reads consume no gameplay RNG.
- [ ] T-15 — Profile maximum games/clients/queues/output with bounded work and no game-thread waits.
- [ ] T-16 — Run sanitizers/thread checks and review live-state/platform handoffs.
- [ ] T-17 — Build on/off across macOS/Linux/Windows x86/x64/MinGW; preserve router-only independence.
- [ ] T-18 — Exercise native-only/no-client/readers/controllers/multi-reader/disconnect-fallback modes.
- [ ] T-19 — Exercise renderer/voice/agent clients with policies/briefings client-local.
- [ ] T-20 — Verify all coverage obligations, visibility blockers and evidence without hidden-data bypasses.
