# Optional local RPC and tactical decision workflow

The [complete observed tactical workflow](tactical-workflow.md) is the standard
controller/independent-reader entry point. It covers inspection, native commands,
explicit questions/options, native continuation and normal closure. The earlier
contracts below remain the detailed sources for existing operations; their
superseded pilot exclusions are not current capability restrictions.

The listener uses libwebsockets v4.5.4 with the existing pinned project libuv.
The exact backend revision, source URL, license and build options are in
[backend.json](backend.json). It is an external checkout, not downloaded by CMake.
MIT and retained permissive component notices are copied from its `LICENSE` to
the build's `data/licenses/libwebsockets.txt`; include that file in distributions.
TLS, compression, client support and backend test applications are disabled.

## Build and configure

```sh
git clone --branch v4.5.4 https://github.com/warmcat/libwebsockets.git /path/to/libwebsockets
cmake -S . -B out/build/rpc-debug -DCMAKE_BUILD_TYPE=Debug \
  -DORION2RE_BUILD_TESTS=ON -DORION2RE_ENABLE_RPC=ON \
  -DORION2RE_RPC_BACKEND_DIR=/path/to/libwebsockets
cmake --build out/build/rpc-debug --target orion2re test_rpc test_tactical_control \
  rpc-test-server rpc-native-test-server rpc-turn-test-server \
  test_tactical_events test_tactical_view test_combat_stats test_combat_repair \
  test_cmbtfire test_boarding test_absorber_cleanup test_cmbtspec test_missile_defense \
  test_stasis_area_effects test_shield_table test_config_snapshot \
  test_savegame test_multplay test_fields_helpers
```

The backend must match the pinned revision and have no tracked modifications.
RPC is off by default. Disabled and router-only builds do not require or link
libwebsockets, even if the router-only configuration sets `ORION2RE_ENABLE_RPC`.

Enable the listener in a private local configuration:

```text
interface rpc_enabled = true;
interface rpc_port = 17362;
interface rpc_key = "replace-with-your-private-key";
```

An empty key prevents startup. Settings require restart and are excluded from
saves and multiplayer synchronization. Configuration keys are CP850 strings
(maximum 128 bytes), converted to UTF-8 at service startup. The service copies
its startup configuration; runtime reloads do not mutate the listening thread.
Bind failure logs a redacted diagnostic and leaves the game usable.

## Current protocol

Connect to `ws://127.0.0.1:17362/rpc`, optionally selecting subprotocol
`orion2.rpc`. Only loopback Host headers matching the listening port and absent
or HTTP(S) loopback Origins are accepted. Query strings are forbidden. Send
JSON-RPC 2.0 text messages, starting with:

```json
{"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"your-configured-key","requestedRole":"reader"}}
```

`requestedRole` is `reader` (default) or `controller`. Only one connection may
hold the controller role; disconnect releases it. Roles cannot change on an
authenticated connection. Invalid credentials return unauthorized and close.
Authenticate within five seconds. Never put the key in URLs or logs.

Authenticated `rpc.describe` takes no parameters (or `{}`) and advertises
`rpc.hello`, `rpc.describe`, `rpc.subscribe`, `rpc.unsubscribe`, `combat.getView`
and `combat.getUnit`, the basic tactical control methods, tactical event names,
and delivery/read/dispatch limits under provisional contract `orion2.rpc/0`.
Controller status is necessary but never sufficient for native tactical control.

## Run the observer

Configure the game as above, build/deploy the executable and runtime data into
your legitimate game installation, and launch that executable. This repository
does not contain the commercial assets or battle-ready saves. For a separate
installation use `-DORION2RE_GAME_DIR=/path/to/game` and the project's deployment
targets; never copy commercial assets into the repository.

In a terminal, run Python 3.10 or newer (no packages required):

```sh
python3 tools/rpc_observer.py --port 17362
```

Enter the configured key at the hidden prompt. For unattended use, put the key
in an owner-only UTF-8 file (`chmod 600 /private/path/rpc-key`) and use
`--key-file /private/path/rpc-key`. The key itself is never an argument or URL.

The client authenticates as reader, subscribes **before** initial discovery,
and shows the current battle even if it missed `combat.opened`. It prints round,
local side, grid dimensions, opaque unit references and schematic map rows
(side, kind, coordinates, size and disclosed facing), with owned selected-unit
name/condition/movement detail. It does not render proprietary sprites.

Commands are `unit <unitId>` to inspect a row, `refresh` for an explicit current
read, and `quit` (or Ctrl-C). After closure it clears battle state and waits for
the next observed opening. Disconnect/revocation clears state and exits; rerun
to perform a fresh hello. Busy refreshes get at most three backoff retries
(0.1/0.3/0.9 seconds), then wait for another native event or manual refresh.
There is no fixed-interval state polling or automatic reconnect loop. Other
authenticated/subscribed clients may poll within the advertised bounds.
Demanded CLI reads are paced to at most eight per second, consuming/coalescing
notifications while waiting; idle time never creates read demand.

The [tactical observer contract](observer.md) defines fields, authorization,
safe points, references, notifications and the interactive acceptance walkthrough.

## Complete a basic ship turn

Run `python3 tools/rpc_observer.py --port 17362 --controller` alongside an
independent reader. Use `options`, `select <unitId>`, `move <x> <y>`,
`face <x> <y>`, `fire <targetUnitId>`, `wait`, `done`, and `cancel`. All new wire
methods/events use `combat.`. See [tactical control](tactical-control.md) for
current-selection firing, native authority, local-attention events, bounded
single-operation dispatch, cancellation, errors and unknown-outcome recovery.
No mutation is automatically retried.

`options` displays native Wait availability. `wait` deliberately yields with
only the current ship, without marking it Done. Native ordering may offer that
same ship again; inspect refreshed options and choose the next command explicitly.
Strict initiative retains its movement cost and disables Wait in its wait stage.
The independent reader sees resulting state through the existing events and reads.

For deliberate bank selection, whole-battle missile/fighter/torpedo inspection,
normal interception and native battle-scoped hit history, see
[tactical firing](tactical-firing.md). The client adds `selection`, `missiles`,
`missile`, `intercept` and `hits`. History belongs to the tactical module and its
allocated storage is released between combats; RPC retains no combat history.
Acceptance for this firing boundary is automated: production native-function
tests plus live WebSocket/standard-client tests. A real-battle walkthrough is
out of scope, and no real multiplayer or other-platform parity is implied.

For deliberate native raid/capture, marine bounds, detailed result events and
explicit result-report acknowledgement, see [tactical boarding](tactical-boarding.md).
The standard client adds `boarding`, `board <targetUnitId> raid|capture <marines>`,
`boarding-cancel`, `boarding-report` and `boarding-ack <reportId>`. A parent action
remains outstanding while its native manual report awaits dismissal; only its
explicit boarding-report acknowledgement may coexist with it. No answer or action
is automatically chosen or retried.

## Tactical lifecycle events

For deliberate withdrawal, see [tactical withdrawal](tactical-withdrawal.md).
The same client adds `retreat`; `options` shows native availability/refusals,
owned rows show pending withdrawal, and `combat.retreatResolved` reports native
outcomes. Existing `cancel` applies only before dispatch. Applied scheduling is
not escape; the client never advances turns or retries to force an outcome.

After hello, select future notifications with:

```json
{"jsonrpc":"2.0","id":2,"method":"rpc.subscribe","params":{"events":["combat.opened","combat.closed","combat.viewInvalidated","combat.unitSelected"]}}
```

There is one selection per connection. Subscribe replaces that selection;
`events: []` or `rpc.unsubscribe` with no parameters (or `{}`) clears it.
Unknown event names, duplicates and extra parameters are rejected without
changing the selection. Results return the selected `events` array. Both reader
and controller roles may subscribe. Notifications cannot change selections.

Native `COMBINIT::Tactical_Combat` publishes open after deployment, controls and
initial network synchronization, and close after normal teardown. Aborted setup
emits neither. Only the current, non-eliminated local human participating in
that initialized tactical context is an observer. AI-only, nonparticipant and
all-AI contexts emit nothing, regardless of host calculation or omniscience.
Observer loss or unfinished teardown revokes delivery instead of inventing a
normal completion. This does not change native rules or multiplayer authority.

```json
{"jsonrpc":"2.0","method":"combat.opened","params":{"battleId":"0123456789abcdef0123456789abcdef"}}
{"jsonrpc":"2.0","method":"combat.closed","params":{"battleId":"0123456789abcdef0123456789abcdef"}}
```

The token is generated from OS randomness only for an observed opening and shared
by its closing event. It is not a native slot, durable identity or action handle.
The lifecycle events contain no player, location, outcome, ship or damage data.
There is no retained battle archive or event replay. Selecting during a battle
can produce a close without a preceding open. Discover current state using
`combat.getView`; clients may ignore closure of an unknown token.

`combat.viewInvalidated` contains only `battleId`, and means "pull if needed",
not an object diff. Optional `combat.unitSelected` contains `battleId` and the
authorized `unitId` (or null when a previously disclosed actor clears). This
means native current actor selected for its turn, not hover. Hidden-to-hidden
selection changes disclose nothing. Reads require selection of opened, closed
and viewInvalidated; unitSelected is optional.

Selection changes apply to future queued native events. Previously queued output
precedes the selection acknowledgement; after an unsubscribe result no further
events from that selection are sent. A new subscription cannot receive native
events that were queued before it. WebSocket delivery preserves event order.

The native publisher invokes a registered RPC forwarder synchronously on the
game thread. The forwarder queues owned delivery data in a 64-entry bounded
queue and calls `uv_async_send`; socket handling stays on the service thread.
Queue overflow disconnects affected subscribers, and output overflow disconnects
the slow connection, rather than silently dropping ordered lifecycle events.

Menu entry, save loading, hotseat selection and native combat perspective changes
revoke event delivery. Revocation clears pending events and closes previously
authenticated connections before further queued output is written; reconnect
and perform a fresh hello/selection. A subscriber must not silently follow the
next local human. Already transmitted bytes cannot be recalled.

Requests use a nonempty string ID up to 64 bytes or a JSON integer within the
JavaScript exact-integer range; null and fractional IDs and batches are rejected.
Duplicate keys, unknown envelope/parameter members and excessive nesting are
rejected. Valid notifications are ignored without authentication or side effects.
Errors never include request payloads or credentials.

Limits: four connections, 64 KiB input per message, depth 32, 32 messages and
128 receive callbacks per second per connection. Each connection has at most
16 queued messages totaling 32 MiB, with a 16 MiB individual response limit.
These ceilings are not preallocated. Pending read completions have a separate
128 MiB byte cap; connection output plus pending completion payloads are bounded
to 256 MiB, excluding transient encoding/backend overhead.
Oversize input, binary, invalid UTF-8, flooding
or output overflow closes the connection. Fragmented input and ordered output
are handled on the service thread.

The service owns a libuv loop and attaches libwebsockets to it. Its only periodic
application timer checks authentication/read/pre-dispatch deadlines, never game state.
Event/response delivery is async-woken. Application composition subscribes the forwarder
before the worker starts, and detaches it after the worker joins and before
stopping the service. Native read hooks service only pending reads; the local
tactical owner separately consumes pending intents. Idle hooks never build a
view. There is no world cache, state watch or fingerprint.

## Verification

```sh
ctest --test-dir out/build/rpc-debug --output-on-failure \
  -R '^(test_rpc|rpc_transport|rpc_tactical_hooks|rpc_observer|rpc_turn|test_tactical_control|test_tactical_events|test_tactical_view|test_combat_stats|test_combat_repair|test_cmbtfire|test_boarding|test_absorber_cleanup|test_cmbtspec|test_stasis_area_effects|test_missile_defense|test_config_snapshot|test_shield_table|test_savegame|test_multplay|test_fields_helpers)$'
cmake --build out/build/rpc-debug --target config-generated-check
```

`rpc_transport` uses a standalone asset-free test server. `rpc_observer` uses
live WebSockets, production native reads and a real native stasis operation over
fixture-owned native memory, and runs the actual CLI. Large-payload tests separately
synthesize response bytes for delivery/budget checks, not native parity. Native
tests also execute real turn selection. `test_tactical_control` and `rpc_turn`
execute full production movement/facing/firing/done and current options, including
the actual controller CLI and a concurrent reader. The held-dispatch delivery race
is synthetic, separately from those gameplay paths. These are not interactive tactical battle
or multiplayer-session evidence. Repeat with `ORION2RE_ENABLE_ASAN=ON` for
AddressSanitizer/UBSan checks. Also build with RPC disabled and router-only.
On hosts without LeakSanitizer, use `ASAN_OPTIONS=detect_leaks=0`; this does not
verify leaks.
Cross-platform builds and interactive game startup require separate validation.
