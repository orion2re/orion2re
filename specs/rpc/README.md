# RPC interface

Experimental optional runtime; AI-assisted documentation. The canonical enabled
surface is `methods.json.implementedMethods`, discoverable with `rpc.describe`.
Current reads cover session, exploration, players, own colonies, own research, leaders, relations, own intelligence and visible ships;
all actions and unaudited fields remain unavailable. See
[read-sidecar ownership](read_foundation.md) and the owner-specific field audits.

## Build and connect

RPC is disabled by default at both build time and runtime. Disabled and router-only
builds do not require libwebsockets. For an enabled game build, obtain a clean
libwebsockets checkout at the exact revision in `backend.json`, then configure:

```sh
cmake -S . -B out/build/rpc-debug -DCMAKE_BUILD_TYPE=Debug \
  -DORION2RE_ENABLE_RPC=ON -DORION2RE_BUILD_TESTS=ON \
  -DORION2RE_RPC_BACKEND_DIR=/absolute/path/to/pinned/libwebsockets
cmake --build out/build/rpc-debug --target orion2re test_rpc --parallel 4
ctest --test-dir out/build/rpc-debug -R '^test_rpc' --output-on-failure
```

The backend reuses the project's `uv_a` target. There is no second libuv or router
protocol. Backend notices are copied into `data/licenses/libwebsockets.txt` in the
build tree and included by the existing runtime-data packager. MinGW RPC builds
require the POSIX-thread compiler variant for C++ standard threading support.

Set local, unsaved, unsynchronized preferences in `orion2re.cfg` (or the file
selected with `--config`), then restart:

```text
interface rpc_key = "replace-with-your-private-key";
interface rpc_port = 17362;
interface rpc_enabled = true;
```

An empty key refuses listener startup. Use a private value and protect the local
configuration file. Connect to `ws://127.0.0.1:17362/rpc` and send:

```json
{"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"replace-with-your-private-key"}}
```

The service compares this value to the configured key and returns an authorized
reader result, or a fixed unauthorized error and closes the connection. There are
no per-launch key files, approval dialogs or additional authentication services.
`requestedRole: "controller"` is optional; only one controller can hold the role.
Changing roles requires reconnecting. The default contract is `orion2.rpc/0`.
The key is never returned or logged by RPC; invalid config-value diagnostics for
this setting are redacted. Config strings use native CP850 storage, decoded to
UTF-8 before comparison: 128 CP850 bytes can require up to 384 UTF-8 bytes on wire.

## Reproduce the inventory

From the repository root, with the pinned Git objects available:

```sh
python3 tools/audit_rpc_contract.py
python3 tools/audit_rpc_contract.py --ledger
python3 tools/audit_rpc_contract.py --describe
python3 tools/generate_rpc_schema.py
python3 tools/generate_rpc_schema.py --check
python3 -m unittest discover -s tests/rpc -p 'test_*.py'
```

The ledger command emits deterministic JSON, expanding Appendices F-H of
`planning_baseline.md` to one entry per full commit SHA and changed path. It checks
exact sets, duplicates, documentation/runtime classification, and diff totals against Git;
matching counts alone are insufficient. `coverage.json` persists all 119 commit
identities and 61 paths, ownership and minimum planned test obligations. Each
expanded entry keeps its precise Appendix F-H feature disposition/acceptance.
`testsPlanned` is not an executed test. Runtime entries remain `planned`, with
null implementation and empty executed-test lists. Later per-identity evidence
must include implementation, executed tests and visibility evidence before an
entry can become `implemented` or `replaced`. The audit rejects omissions,
duplicates and orphan evidence; a generic default cannot hide a missing identity.
Implementation references have `{path, sha256}`. Executed-test references have
`{id, artifact: {path, sha256}}`; each JSON report records its matching test ID,
command, `result: passed` and integer `exitCode: 0`. Every planned test must have
a report, and visibility evidence must reference an executed T-01/T-02/T-03/T-20
report. References stay inside the checkout and digests must match. The audit
never executes commands from evidence and cannot attest that a human-authored
report is truthful; release review and actual test execution remain mandatory.

`baseline.json` distinguishes the original target from the reviewed checkout.
The audit fails on target drift outside the reviewed P0/P1 integration file set rather than silently
repinning it (including tracked worktree changes). Review such a diff and update
`reviewedTarget` intentionally. P0-only commits do not require a self-referential
baseline SHA update. A shallow
checkout missing the fork or ancestor fails explicitly; the tool does not fetch.

### Target drift reviewed for P0

The fork/ancestor remain unchanged. Between original target `09d4f610` and
reviewed target `e57a55b2`, 48 paths changed. Relevant differences:

- Tech Review categories now hold 256 entries (`src/game/info.h`): do not retain
  stock category limits in future DTOs, fixtures, or budgets.
- Startup can request a one-shot native save-slot load (`mainmenu.cpp`); initial
  publication must follow successful load and perspective authorization, not
  process launch. Intro skipping initializes update/palette/mouse state in
  `mox2.cpp`; do not assume an intro context always runs.
- Exit-message configuration changes normal shutdown presentation, not the
  exception funnel. Score state uses aligned allocation (`score.cpp`); never
  serialize its native layout.
- Monster HP overrides (`initship.cpp`, configuration schema/bindings) reinforce
  that public combat stats must use authorized native derived values, not stock
  lookup tables. Configuration loader/startup and documentation/build changes
  do not add an RPC authorization layer.

No SCREEN enum or fork feature inventory was changed by this target diff.
Appendix source line numbers remain historical leads, not current hook locations.
The audit rechecks exact SCREEN values, actual fork tag writes, message names,
TPOP constructor kinds, multiplayer phases and combat enum inventories against
Appendices A-E of `planning_baseline.md`, and commits/files against F-H. Target
drift does not change these inventories. Semantic runtime equivalence remains a
later implementation gate.

P1 turn-notification test drift: `tests/unit/netmox_test.cpp` now subscribes to the
neutral world-update events during its existing differential snapshot round-trip.
It verifies that begin observes the old stardate and committed observes the fully
decoded stardate. The listener is scoped and removed before fixture destruction;
no packet data, game rules, native APIs or fixture values change. The audit permits
this reviewed test path explicitly rather than allowing arbitrary unit-test drift.

The approved post-refresh ship increment adds only a neutral publication after
`SHIPS::Build_Ship_Icons` completes (`src/game/ships.cpp`). No detection calculations
or gameplay API are extracted or changed. `game_rpc_native_test.cpp` observes the
actual native filter/builder and verifies the sidecar's copied allowlist. This one
additional native hook path is explicitly permitted by the drift audit.

The approved research extraction shares native offered-choice enumeration and
caller-buffer application-name formatting between the native UI and read sidecar.
The drift audit permits `tech.cpp`, `tech.h` and `tech_choices.cpp` for that scope;
it does not authorize research-changing RPC or a general UI-action framework.

Leader reads share native roster, name, ability and result projections. The
focused native-owner publications cover rescue results, troop/ground summaries
and name-bearing event presentations, including interruption and teardown. These
hooks grant no general UI context, navigation or persistent foreign knowledge.

## Own research reads

`game.getResearch` reports the current field (`project`), selected `application`,
native research mode, field RP cost and offered options. An application selection
does not predict all awards for creative or special fields. No selection is
known-empty, not unknown; a completed/breakthrough state is distinct from active
ordinary or hyper research. Options use the native change-research choices,
including the current field and selectable no-application entries.

`game.listResearchOptions` accepts the standard bounded `ListRequest` and returns
`items` with field/application technology references. Resolve those references
with `game.getTechnology` for copied native names and RP costs. Option IDs are
choice references, not technology IDs or action capabilities. `remainingCost` is
the native change-screen RP remainder, not a guaranteed completion threshold.
Descriptions and raw progress remain explicitly unaudited; no prerequisites or
forecasts are inferred.

Include `research` in `game.getState` or `rpc.subscribe` for a coherent baseline
and `state.changed` invalidations, including offered-choice changes. The nested
research stamp matches the enclosing state stamp. Research changes expire old
technology lookup handles; pinned option pages remain explicitly dated until
eviction. Reset/perspective changes revoke both. Re-read a baseline after fresh
authentication rather than carrying references between empires. The complete
synthetic live client flow is exercised by
`tests/rpc/test_transport.py::test_own_research_client_workflow_and_revocation`;
native-function fixtures live in `tests/unit/game_rpc_native_test.cpp`.

## Leader reads

`game.listLeaders` and `game.getLeader` expose the bound player's ambient roster:
hired leaders, actual pending offers, and the personal hiring pool retained after
declining an offer. Generated candidates and undiscovered marooned leaders are
not roster entries. Roles, localized rank/name/title, roster XP and native
displayed ability names/values are copied without opening a screen. Ability
values are presentation text, not a new numeric scoring or effectiveness API.

Assignments distinguish unassigned, limbo, assigned and travelling leaders.
Colony leaders are assigned to **systems**, not colonies. A displayed assignment
includes its native name; its target handle is current only when it resolves
through the same capture's authorized system/own-ship projection. Otherwise the
target is unknown, not known-empty. Unclassified native states report
`unavailable` rather than an invented assignment. Positive arrival ETA is cached
whole turns; no transfer calculation runs. Costs are displayed BC and upkeep is
BC/turn. Only hiring-pool entries have an audited remaining-offer countdown;
pending offers have no inferred expiry. Offer benefits use native popup prose.
Screen-owned help descriptions remain explicitly unaudited.

Include `leaders` with other state/subscription topics to establish one coherent
baseline. A later leader-only capture never enriches an older ship capture.
Pagination pins the original capture; `state.changed` means re-read, and changed
leader projections expire old lookup handles. Offer withdrawal, dismissal/death
or replacement revoke the existing view, including pinned pages and queued
output, requiring fresh authentication and a new baseline. Reset/perspective
changes also revoke handles. Missing initialized storage is unavailable, not an
empty roster. The complete ambient client flow is exercised by
`tests/rpc/test_transport.py::test_leader_client_workflow_assignments_coherence_and_revocation`.

Additional native disclosures use partial records, distinguished by `disclosure`:

| Disclosure | Known fields beyond the opaque handle |
| --- | --- |
| `leader-result` | Promotion/rescue presentation's ranked name/title, role, rank, displayed abilities and any positive portrait arrival ETA, including a failed rescue after deassignment |
| `troop-summary`, `ground-summary` | Displayed ranked-name string and `contextBonus` from that summary |
| `message-name` | Name actually substituted into the presented recipient message |

For partial records, `name` is the native displayed string rather than a separate
unranked roster name. Abilities use a knowledge tag: unknown is not an empty
ability list. XP, status, assignment, costs and upkeep remain explicitly
unknown unless independently authorized by an ambient roster entry. An absent
portrait ETA overlay is unknown, not known-empty. A summary
bonus is contextual, not a new ability/effectiveness score. When an entry is
already ambient, its full roster fields remain available and a displayed summary
bonus may accompany them. A private event argument or a formatter call without
the corresponding initialized presentation never grants a leader handle. The
audited name-bearing event is the leader-bearing Freak Accident report variant.

Presentation authority ends before native owner cleanup, on withdrawal or
replacement, or when interrupted by a child. Parent return requires fresh native
presentation. Dependent handles, pages and queued output expire through the same
context leases as other native observations; ambient reads remain independent.
No presentation knowledge is retained as a foreign-leader dossier.

`native-leader-presentation` stamps distinguish these captures. During resolution,
only the initialized owner's leader projection (optionally with session metadata)
is readable, anchored to the last committed turn; it cannot announce a new turn.
Other combined requests remain busy. Outside resolution, disclosures can accompany
a fresh coherent ambient capture. A request including ships may instead select an
older complete, explicitly stamped capture: newer leader observations are never
spliced into it. Use a leader/session subscription to observe current transient
disclosures independently of a retained combined baseline.

The leader workflow test above covers the full roster and partial-disclosure
client flow. Its native result producers are in `tests/support/rpc_native_leaders.cpp`;
native-function and formatter fixtures also live in `game_rpc_native_test.cpp`
and `events_test.cpp`. This is synthetic native-function/live-transport evidence,
not interactive saved-game or multiplayer parity.

## Relations and own intelligence

`game.getRelations` and `game.getIntelligence` support an offscreen diplomatic/
intelligence dashboard. They share native read projections with the existing
Races and Race Report presentations; no query opens a screen, commits staged spy
edits, generates a proposal or consumes RNG. Canonical relationship text is
initialized by the native language/mod string lifecycle, not by opening Races.

Relations reference the same capture's known foreign players. Living contacts
have native relationship bucket labels, the native 26-position relationship
indicator offset, treaty labels, ignoring status, current trade/research levels
and directional tribute. Research levels below zero are
BC/turn; nonnegative levels are RP/turn. Trade is signed BC/turn. Tribute is the
displayed 5% or 10%, not a calculated BC payment or a read of foreign treasury.
Native treaty labels clamp the internal war subtypes; exact attitude scores,
agreement goals, expiry timers and AI intentions remain unavailable.
The indicator is a logical-pixel Y offset relative to its bar top, not an exact
attitude score. Eliminated or uncontacted roster inclusion—including
omniscience/Lore—is not a field grant.
Missing agreements are `known-empty` only for a currently authorized contact.

Intelligence reports home defensive agents, displayed spy/agent bonus percentages,
and per-contact spy counts and selected missions: `espionage`, `sabotage`, `hide`.
Counts are native 0..63. Zero spies is a known count and can retain a selected
mission; unknown target assignments are not zero. Unclassified mission bits are
unknown. There is no invented total across undisclosed targets, opponent
organization, staged drag state or success forecast. Missing initialized leader
storage makes bonuses unknown without hiding an independently available home pool.

Use one subscription baseline, for example:

```json
{"jsonrpc":"2.0","id":3,"method":"rpc.subscribe","params":{"topics":["session","players","relations","intelligence","systems","colonies","ships","research","leaders"],"events":["state.changed"]}}
```

After authentication and `rpc.describe`, resolve each relation/allocation `player`
through `game.getPlayer`. On `state.changed`, reread the same selected topics with
`game.getState`; nested intelligence and the enclosing state share a stamp.
All relations/intelligence data requests are busy during resolution. A combined
request can select an older complete ship capture; it never receives newer
relationship or intelligence fields spliced into that capture.

Additional disclosures are presentation-scoped. The Races incoming-spy overlay
grants a positive count only when its configured text actually substitutes the
count. An absent/disabled/literal overlay does not establish zero. The enabled
selected-contact Race Report grants its displayed incoming count (including zero),
personality/objective labels and alliance/war race-name lists. Label suffixes
clipped by the native combined-text buffer are not exposed; an objective clipped
away entirely is unknown, not known-empty. An unlisted empire
in such a list has a displayed name but an unknown player reference; the report
does not expand the roster or grant other foreign fields. These fields are unknown
offscreen, not a persistent dossier. They do not gate ambient treaty or own-
intelligence reads behind UI navigation.

Owner exit, replacement, interruption and world/resolution boundaries revoke
context-dependent output before cleanup; parent restoration needs fresh native
presentation. Independently authorized ambient projections remain readable.
Contact/roster authority loss, elimination and replacement use existing whole-view
revocation to expire retained pages and queued output; reconnect and establish a
new baseline. Reset/perspective changes likewise invalidate old references.

Q-13 traceability: shared disclosure/presentation is in
`src/game/relations_read.cpp`; the native adapter is `src/rpc/relations_rpc.cpp`.
Native-function fixtures in `game_rpc_native_test.cpp` and
`tests/support/rpc_native_relations.cpp` feed the live workflows
`test_native_relations_intelligence_dashboard_workflow_and_context_revocation`
and `test_native_relations_contact_loss_elimination_reset_perspective_and_slot_reuse`
in `tests/rpc/test_transport.py`. This is synthetic native-function/live-transport
evidence, not interactive saved-game, multiplayer or other-platform parity.

## Documentation ownership

`plan.md` owns working rules, bounded milestones and TODOs. `planning_baseline.md`
owns the pinned coverage inventories/dispositions consumed by the audit.
`methods.json`, `contracts.json` and `field_policies.json` own public semantics,
native authorization anchors and generation inputs. Tests provide executable
behavioral evidence. Owner documents retain only unique current audit/architecture
constraints, not per-increment narratives or repeated test counts:

- [Exploration](exploration_view.md), [players](player_view.md),
  [own colonies](colony_view.md), [ships](ship_refresh.md).
- [Read ownership](read_foundation.md), [backend constraints](p1.md),
  [native lifecycle audit](lifecycle.md), [remaining gates](evidence.md).
- [Client-side incoming-threat example](incoming_threats.md).

## Verification

Use focused tests during development; run this integration gate at capability
completion, adding affected native-owner targets when needed. Configure both
build trees as above; for `rpc-asan` additionally set
`-DORION2RE_ENABLE_ASAN=ON` (ASAN/UBSAN on Clang/GCC).

```sh
cmake --build out/build/rpc-debug --target orion2re test_rpc test_game_rpc_native rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload test_lua_mod_runtime test_shipmove test_plntsum_filters test_colony_pop_growth test_map_scale --parallel 4
ctest --test-dir out/build/rpc-debug -R '^test_(rpc|rpc_transport|game_rpc_native|game_events|fields|fleetpop|mainscr_reload|lua_mod_runtime|shipmove|plntsum_filters|colony_pop_growth|map_scale)$' --output-on-failure
cmake --build out/build/rpc-asan --target orion2re test_rpc test_game_rpc_native rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload test_lua_mod_runtime test_shipmove test_plntsum_filters test_colony_pop_growth test_map_scale --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir out/build/rpc-asan -R '^test_(rpc|rpc_transport|game_rpc_native|game_events|fields|fleetpop|mainscr_reload|lua_mod_runtime|shipmove|plntsum_filters|colony_pop_growth|map_scale)$' --output-on-failure
ORION2RE_RPC_TEST_SERVER=out/build/rpc-debug/rpc-test-server python3 -m unittest discover -s tests/rpc -p 'test_*.py'
python3 tools/generate_rpc_schema.py --check
python3 tools/audit_rpc_contract.py
bash tools/run-tests.sh
git diff --check
```

Also check affected C++ formatting against the change's starting revision, without
reformatting unrelated native code. `detect_leaks=0` is for the unsupported local
macOS LeakSanitizer runtime only; use leak checking where supported. ASAN/UBSAN
remain enabled. Synthetic native-function and live transport tests do not certify
interactive saved-game/multiplayer or other-platform parity.

For research changes, also build and select `test_tech_selection`, `test_techinit`
and `test_techdata` in both normal and sanitizer trees.

For leader changes, also build and select `test_ground_commando`,
`test_mainpups_leader_technology`, `test_report`, `test_colgcbt` and `test_events`
in both trees.

For relations/intelligence changes, also build and select `test_spy`,
`test_diplomac`, `test_info`, `test_text_format` and `test_resource_reload` in both
trees.

Documentation-only changes need contract/schema/audit tests, local link checks
and whitespace checks, not the game-build/transport gate above.

## Static method descriptor source

`methods.json` contains every explicitly named method in plan section 4, without
the illustrative `(include)` signature suffix. Owner names are **proposed
ownership labels**, not claims that those C++ adapters already exist. Shared UI
actions dispatch through the active registered owner, not a global input injector.

Runtime is opt-in; `implementedMethods` lists the enabled methods.
`contracts.json` supplies concrete
request/result types for every named method; `field_policies.json` supplies native
source anchors, predicates and required test IDs. These are contract definitions,
**not** permission to serialize native records. Enabling handlers requires a
separate reviewed change and runtime field-visibility tests. The unnamed
operation families in plan section 4 are not implicitly registered.

`Validate_Value` consumes these types for offline fixtures; the generated C++ table
and type data drive runtime parameter checks and supported-method discovery.
`--describe` emits the
same method/type bundle explicitly. There is no second hand-maintained discovery
catalog. This CLI is not a JSON-RPC response or runtime dispatcher. Eventual
runtime discovery lists enabled contracts only; ordinary state/view responses
contain no schemas.

### Generated schema and documentation (P0-11)

The canonical source bundle is `methods.json`, `contracts.json` and
`field_policies.json`. `tools/generate_rpc_schema.py` generates:

- `generated/schema.json`: JSON Schema 2020-12 with local `$defs`, distinct
  per-method request/success-response entry points, server notifications, fixed
  errors, ownership/permission/lifecycle metadata, field policies and units.
- `generated/reference.md`: human-readable reference rendered **only from the
  serialized schema**, not another list of methods or source DTOs.
- `src/rpc/generated/rpc_contract_data.inc`: C++ descriptors, limits, fixed errors
  and parameter type data used by the optional runtime.

The outputs are versioned, carry their canonical source notice and source-bundle
SHA-256, and must be regenerated together after a contract change. Do not edit
generated output by hand. `--check` detects drift without writing files; the CI
schema/renderer tests run it and test deliberate stale-output rejection.
Generation/checking needs only the source checkout and Python standard library,
not fork Git objects, game data, backend libraries or a network connection.

The root schema validates acknowledged client request shapes. Response validation
selects the per-method response definition by the outstanding request's method;
server notifications and errors have separate definitions. The contract is still
pre-release (`orion2.rpc/0`); the implemented-method annotation distinguishes the
implemented subset from planned gameplay contracts. Byte/depth limits,
lexical integer tokens, duplicate-key rejection, counter bounds and visibility
rules are retained as explicit `x-*` annotations when they cannot be expressed
as standard JSON Schema assertions. The protocol guards remain required; a
generic JSON Schema validator alone does not authorize or secure a request.

Availability labels are necessary conditions, never grants of authority:

| Label | Required state |
| --- | --- |
| `transport` | Live connection; only `rpc.hello` may precede authentication |
| `authorized` | Authenticated and explicitly authorized local perspective; no silent hotseat follow |
| `committed-game` | Authorized loaded game at coherent committed capture point, otherwise busy |
| `active-context` | Initialized matching context/generation; field-specific visibility still required |
| `observed-battle` | Active battle legitimately observable by the bound player; scan/targets further filtered |
| `retained-visible-result` | Authorized retained outcome for this game/perspective, cleared on reset/revocation |

Reader/controller describes the minimum role; a controller can read, but gains no
additional knowledge. Ownership, native phase and multiplayer authority are
rechecked on the game thread. No method accepts a viewer override.

### Typed DTO and field-evidence semantics

The declarative type vocabulary is `string`, `integer`, `boolean`, `enum`,
`object`, `array` and a closed `union`. There is no `any`, raw record, reflection,
implicit conversion or recursive object graph. Objects reject unknown keys;
all fields are required except their explicit `optional` list. All types reject
null. Strings have UTF-8 byte bounds, integers have units/ranges, arrays have
explicit caps, and enums define their complete values. `ListRequest.limit`
defaults to 64; cursors pin an immutable authorized snapshot and do not expose
global offsets. No arbitrary filter/search/sort expressions are registered.

Every object field inherits its object's policy unless `fieldPolicies` selects
a more restrictive field policy. Authorization is the conjunction of enclosing
object, field and nested policies, never an override that weakens the parent.
Policies classify protocol-only metadata, statically reviewed native evidence,
or explicitly disabled fields. Reviewed is not runtime-tested. The public fixture
validator rejects disabled objects and rejects disabled field values except an
explicit `unaudited` marker where the declared tagged union permits it.

The initial field set is deliberately conservative. Additional detailed fields
listed in the plan are not silently enabled by a generic serializer: they need
new concrete fields, native evidence and runtime tests. Source anchors are checked
at the pinned target. Disabled policies record the precise missing audit rather
than inventing display units, enemy knowledge or historical knowledge.

- Use opaque game/perspective-scoped strings for entities, contexts and cursors;
  never native indices, pointers, slot counts or allocation generations.
- Encode revision/sequence counters as decimal strings to avoid JavaScript
  precision loss. Game turns are committed native turn numbers, not UI polls.
- Unknown entities are omitted. For a known object, unavailable authorized-schema
  fields use `{ "availability": "unknown" }` or
  `{ "availability": "context-required" }`; known values use
  `{ "availability": "current", "value": ... }`. `last-known` is allowed only
  with native historical evidence and an observation turn. No last-known variant
  is currently defined or approved. Never use `null`, zero, or empty arrays as an
  ambiguous substitute for unavailable knowledge.
- Coordinates, ETA, currency, percentages, ticks and scaled native values need
  per-field units/conversion evidence. No blanket native-integer conversion.
- Filters, ordering, pagination, totals, revisions and error details operate on
  authorized projections. Unknown and nonexistent IDs produce the same error.

## JSON-RPC / yyjson profile

One complete UTF-8 WebSocket **text message** contains one JSON-RPC 2.0 request.
WebSocket fragmentation is reassembled under a total message-byte cap before
parsing. Reject binary messages; negotiate no compression initially. Ping/close
are transport controls, not JSON messages. No legacy binary TCP compatibility.

Use the existing yyjson dependency with `YYJSON_READ_NOFLAG`, explicit byte
length, and a bounded allocator. Do not reuse configuration JSON5 flags. Reject
invalid UTF-8, BOM, comments, trailing commas, NaN/Infinity, trailing non-whitespace,
and duplicate member names at **every** nesting level (compare decoded names,
including escaped aliases). yyjson strict parsing alone does not reject duplicate
keys. Apply an input depth guard before allocating an unbounded tree: the pinned
header defaults `YYJSON_READER_DEPTH_LIMIT` to zero. Do not change the shared
configuration parser's behavior to implement the RPC limit.

Envelope rules:

- Root must be an object; batch arrays are deliberately unsupported (`-32600`).
- Exact keys: `jsonrpc`, `method`, optional `id`, optional `params`. Version must
  be string `"2.0"`; method must be an exact allowlisted name. Reject extra keys.
- Supported IDs: strings of 1-64 UTF-8 bytes, or integers within
  `[-9007199254740991, 9007199254740991]`. Reject null, booleans, fractional numbers,
  or out-of-range IDs (`-32600`, response id null). Preserve ID type/value.
  Numeric IDs must be lexical integer tokens, not `1.0` or `1e0`.
  This is a documented restricted ID profile, not all JSON-RPC ID forms.
- `params`, when present, must be an object. Omission means an empty object, not
  null. Per-method typed validation rejects unknown members and coercions.
- Client notifications (missing ID) receive no response and never dispatch game
  queries/actions/control work. A client must send requests for acknowledgments.
  `rpc.hello` must carry an ID; a notification cannot authenticate a connection.
  Invalid objects that cannot be identified as notifications receive an invalid
  request error with id null; parse errors always use id null.
- Server notifications omit ID and use method-specific copied params. Clients
  cannot invoke server event names. No unsolicited state before authentication.
- A response has exactly one of `result` or `error`. Errors contain numeric
  `code`, fixed public `message`, optional allowlisted `data`; never parser input,
  secret, hidden entity detail, format arguments, filesystem paths or native data.

Fixed error codes/messages come from `contracts.json.errors`; see the
[generated error table](generated/reference.md#errors). Disabled/unregistered
methods use method-not-found; unknown/nonexistent/expired entity handles share
object-unavailable. No error-data fields are approved in the initial schema.

Authenticate before discovery/lookup/parameter diagnostics; unauthenticated
connections get no method/entity oracle. Repeated auth failures close the
connection. Native rule refusals are typed action results, not internal errors.
Request acceptance/queueing is not success. An operation ticket is scoped to the
live authorized control session; duplicate handling, bounded retention and partial
outcomes must be defined before actions are enabled. No disconnect exactly-once
guarantee. `operationId` is separate from envelope ID, scoped to the live control
session and immutable request payload. It is a monotonically increasing decimal
counter, not a native/entity ID. Keep an admission high-water mark for that live
control session even after result eviction. Same ID/same payload returns its
retained result; same ID/different payload is invalid params. An older admitted
ID without a retained result is expired, never executed again. A queued ticket must be
resolved as applied/refused/partial/cancelled/expired; never silently discarded.
The bounded bridge implements copied queue admission, unstarted expiration,
epoch/client revocation and immutable completion. Native dispatch and operation
retention for actual gameplay actions remain later implementation work.

## Local authentication contract

Per user direction, authentication is a simple configured-key hello, not the P0
per-launch-secret/approval proposal. No key in URLs or HTTP authentication headers.
An acknowledged hello must authenticate within five seconds of upgrade; no other
method is callable before it. Matching is
exact and case-sensitive; an empty configured key never authenticates anybody.
Repeating hello with the same key and role is idempotent; role changes need a new
connection. Possession of the key grants the requested reader/controller role,
but never authorizes hidden game fields, caller-selected viewers or debug access.

The current listener binds only `127.0.0.1`, never a wildcard. Upgrade requires
`/rpc` with no URL arguments and a Host of `127.0.0.1:<port>` or `localhost:<port>`.
Browser Origins must be HTTP(S) loopback origins (`localhost`, `127.0.0.1` or
`[::1]`, with an optional valid port); foreign and `null` Origins are rejected.
Originless native clients still need the same key. Separate frontend loopback
ports are allowed. Backend IPv6 support is compiled, but this initial listener
does not open an IPv6 socket.

The bridge revokes pending/results and invalidates in-flight work on epoch or
client revocation; the service closes old connections. Neutral new/load/update,
hotseat and thread lifecycle publications now drive the sidecar's reset/busy/view
handling. A fresh key handshake binds the next authorized perspective; old
connections never silently follow it. The game contains no RPC calls or types.

## Remaining gates

See `lifecycle.md` for the source-audited pump/lifetime/publication/shutdown
decisions. `protocol_cases.json` and `Check_Request` are an offline conformance
oracle. `src/rpc/rpc_protocol.cpp` implements the strict C++ yyjson boundary for the
published surface; unsupported methods remain method-not-found. The initial
projection and query dispatcher are owned by RPC-side observers, not the engine.
`contracts.json.limits` supplies explicit provisional admission/queue/output/
retention/polling limits. They are not a measured native maximum-game budget.

See `evidence.md`, `backend.json` and `native_baseline.json` for performed checks
and their limits. P0-06 remains open for the missing platform matrix; P0-09 needs
a native capture path and maximum-game fixture; P0-10 needs real local game
interaction/save/mod/RNG recordings. Synthetic tests cannot close these gates.
