# Deliberate native tactical withdrawal

This extends [ship control](tactical-control.md), [firing](tactical-firing.md) and
[boarding](tactical-boarding.md). It is an interface into native tactical state,
not an escape simulator or strategic fleet-order API.

## Launch and commands

Build and configure the optional listener using [README](README.md#build-and-configure).
Launch the executable from your legitimate installation with the private local
`interface rpc_enabled`, `rpc_port` and `rpc_key` settings. RPC cannot navigate
into a battle. Do not deploy test executables over a legitimate installation.

Keep an independent reader and controller running:

```sh
python3 tools/rpc_observer.py --port 17362
python3 tools/rpc_observer.py --port 17362 --controller
```

Use the hidden key prompt or an owner-only `--key-file`; never key arguments or
URLs. The existing event-before-discovery and event-and-pull behavior is unchanged.

```text
options
retreat
```

An example semantic sequence is: options offers retreat; `retreat` returns applied;
the original owned ship remains visible with `retreatPending:true` while another
eligible actor becomes current; explicitly chosen `done`/other ship commands allow
normal native passes to proceed; `combat.retreatResolved` later reports escape,
native cancellation, destruction or interruption. Escape retires the old reference.
Continue with current options for remaining ships, or observe `combat.closed`.
The client never chooses those other actions or advances turns to force escape.

Use `cancel` while the request is still undispatched. Successful cancellation
changes no gameplay and the original request receives `-32013`. Cancellation is
too late after dispatch (`-32014`); there is **no** player operation to cancel an
already-started native retreat. Pressing native Retreat again does not cancel it.

## Current reads and action

With normal native ship control, `combat.getTargetOptions` adds:

```json
{"retreat":{"available":true,"reason":null}}
```

An unavailable retreat has `reason:"noSpeed"` or `"retreatPrevented"`. These
preserve the native human refusal precedence. No source, enemy device reference,
count, design information, escape time or survival forecast is disclosed. A
noncontrollable actor retains the existing `control.available:false` response,
without retreat diagnostics. Ordinary reads never recalculate speed, rebuild
grids, start/cancel retreat or consume gameplay RNG.

Owned ship rows in `combat.getView` and `combat.getUnit` add `retreatPending`.
It is omitted for foreign units and nonship actors. Pending means the native
retreat flag is still set, not that the next pass will escape. Existing owned
condition/speed/stasis detail remains available through `getUnit`.

`combat.retreat` takes exactly the existing mutation context:

```json
{"battleId":"<battle>","actorUnitId":"<actor>","decisionId":"<decision>","operationSeq":21}
```

Its sole final success is `{"operationSeq":21,"state":"applied"}` after the
native scheduling command, next-ship work and normal barriers return. It does
not occupy the global mutation slot across future passes and does not promise
escape. Actor/battle/reference lifetime, current local human, ownership, native
turn/pass, initiative, input phase/delay, network authority, decision and retreat
legality are rechecked at consumption. Existing unavailable, no-control, conflict,
illegal, busy, duplicate, expiry and cancellation errors apply. Fresh options can
explain a current authorized refusal; mutations are never automatically retried.

## Native outcome notification

Select `combat.retreatResolved` along with the existing tactical events:

```json
{"battleId":"<battle>","unitId":"<previously disclosed own reference>","outcome":"escaped"}
```

| Outcome | Meaning |
| --- | --- |
| `escaped` | Native retreat validation succeeded and tactical escape committed |
| `destroyed` | Native destruction committed while withdrawal was pending |
| `cancelled` | Native validation/pass logic cleared pending retreat; no private cause is disclosed |
| `interrupted`, `reason:"captured"` | Native capture ended this owned ship's withdrawal |
| `interrupted`, `reason:"battleEnded"` | The tactical battle ended with withdrawal still pending; this says nothing about strategic fate |

These are native semantic facts, not RPC completion receipts. Producers cover
local, controller, AI and received-network routes for authorized owned ships.
The event identifies the previously disclosed lifetime before its reference is
retired. It never creates a new handle for an already removed unit. Outcome
notifications are ordered, not coalesced; ordinary invalidation/selection and
pulls follow settled native boundaries. Capture, destruction, escape and slot
replacement cannot redirect an old reference. Owned rephasing preserves that
same ship reference; foreign disclosure downgrades still retire references.

No result report, retained history or withdrawal-specific retained storage is added.
A connection that misses an outcome cannot replay it. Mid-sequence readers discover
current pending state by pulling. On closure/revocation the client clears its
context; invalidation clears operational choices. Disconnect before a final
response means the command outcome is **unknown**. Even after an applied response,
a missed later event does not prove escape. Reauthenticate, inspect current state
and decide explicitly; do not silently reconnect, reselect, retry, cancel or
advance turns.

## Native variants and compatibility

Supported actors are existing controllable local-human ships, not planet,
satellite, monster/AI-owned or remotely owned actors. Classic, ordinary initiative,
strict initiative and eligible Time Warp passes retain their native ordering.
Stasis can defer a pending retreat. Speed loss or effective native prevention can
cancel it; capture and destruction are distinct terminal consequences.

Human eligibility uses current native speed, not a new FTL rule. Actual escape
validates native death and calculated speed. Working/damaged engines and specials,
Trans-Dimensional speed, configured tractor thresholds, black holes, interactive
combat statistics and phasing/cloak behavior remain native. A request does not
uncloak or move the ship as an invented preparation step.

Displayed retreat rotates toward the native escape facing. That rotation can
resolve missiles and defensive fire, including fatal damage or capture; it is not
merely cosmetic. Super-fast/headless routes skip the drawing-owned rotation as
before. Native missile retargeting and continuing-effect cleanup occur on escape.
Human sending retains zero-argument `TACTICAL_NET_MSG_RETREAT` and its barriers;
received application does not repeat the sender's refusal checks. AI retains its
own retreat policy and additional state changes. No packet/save format changes.
An end-of-pass refresh with no current actor rebuilds occupancy but does not read
an invalid ship or construct actor-dependent legal moves/mouse images. Valid-actor
costs, rules and RNG are unchanged.

There is no successful-command confirmation/report owner. Local refusal boxes
remain local; remote refusal occurs before opening them. Existing warnings and
boarding reports reached during native resolution retain their owners and
attention/boarding-only acknowledgement rules, without early parent completion.

## Evidence boundary

Automated acceptance uses production gameplay/RNG, actual tactical consumption,
pass processing and the initialized production round/winner loop over fixture-owned
memory. The fixture owns setup and teardown, not strategic writeback. Transparent
sprites, blank glyphs and messages are synthetic media, not original asset parity.
Live WebSocket tests run the standard client alongside an independent reader.
Held-dispatch/queue tests remain supplemental delivery evidence.

Allocation-release assertions are separate from sanitizers with leak detection
disabled. No commercial-save walkthrough, real multiplayer session, other-platform
or interactive asset parity is implied. Strategic destinations/orders/after-battle
data, remote design/device edits, forecasts, auto combat/end-side-turn commands,
generic modal control, receipts, archives and effect/frame streams remain excluded.
