# Basic external tactical ship turn (`orion2.rpc/0`)

The [deliberate firing extension](tactical-firing.md) adds explicit native bank
selection, missile APIs/targeting and game-owned active-battle hit history. Its
scope supersedes the corresponding exclusions below; the mutation lifecycle is
unchanged.

This extends, rather than replaces, the [read-only observer](observer.md).
All new wire methods and notifications have the `combat.` prefix. No raw native
input, caller-selected perspective, remote loadout editing or strategic authority
is provided.

## Run

Build/configure/deploy as in [README](README.md), using a legitimate asset
installation. Keep an independent reader running:

```sh
python3 tools/rpc_observer.py --port 17362
python3 tools/rpc_observer.py --port 17362 --controller
```

Use the hidden key prompt or an owner-only `--key-file`; never put the key in an
argument or URL. Controller contention at hello is distinct from acquiring a
controller connection but having no native authority over the current actor.
Subscribe before initial discovery, including when connecting before a battle.

Controller commands are `options`, `select <unitId>`, `move <x> <y>`,
`face <x> <y>`, `fire <targetUnitId>`, `done`, and `cancel`. Existing `refresh`,
`unit <unitId>` and `quit` remain available. Coordinates are logical grid cells,
not mouse/pixel coordinates. `face` invokes native turning toward that position;
it does not assign a facing index.

After a native event, the controller pulls current state/options. Review the
actor, weapons, movement cells and targets before issuing a command. A stale
decision is discarded, not silently refreshed and resubmitted. Neither role
polls state on a fixed interval. The observer remains usable without a controller.

## Options

`combat.getTargetOptions` accepts omitted parameters, `{}`, or
`{"battleId":"<token>"}`. Explicit battle IDs never follow replacement battles.
An observed safe read returns `battleId` and `control`. `control.available: false`
has `reason: "noNativeControl"` and no decision token or operational lists.
Absent/hidden battles are unavailable, not enemy-state discovery endpoints.

An available response contains:

- `control`: `available`, `actorUnitId`, and opaque `decisionId`.
- `selectableUnitIds`: authorized own functional ship actors allowed by the
  native classic selection path. Empty in initiative modes. Native classic
  selection can reinsert a done ship; no new prohibition is imposed on it.
- `weapons`: up to eight occupied own slots, each with `slot`, native `name`,
  native `type`, installed `count`, `selection` (native active value),
  `shotsAvailable`, `ammoOrRecharge`, `arc`, and `ready`.
  Selection `1` is on, `0` is temporarily off, `-1` is off. Native post-fire
  handling resets `0` slots to `1`. Torpedo ammo is a recharge counter (zero
  is ready), unlike consumable ammunition; negative ammo retains native meaning.
  Shot availability includes the native status-dependent restoration of a bank.
- `preFireEffects`: `absorberSelected` and current own `absorberCharge`.
- `movement.cells`: complete native legal destinations `{x,y,nominalCost}`.
- `facing`: `available`, `turnCost`, and `movementRemaining`.
- `targets`: authorized normal unit references, `eligibleWeaponSlots`, and
  `preFireEffectEligible`. No missile references, phased-contact eligibility,
  enemy design details, hit forecasts or damage predictions.
- `canDone`.

The movement source is the settled actor-bound native legal grid: the same
markers drawn by `CMBTDRW1::Draw_Legal_Moves`, produced by
`COMBAT1::Assign_Combat_Grids`/`Set_Legal_Moves`. Reads do not call these producers,
reconstruct occupancy, identify blockers, or retain masks/calculator inputs.
Costs use `Move_Cost`. Target eligibility uses native bearing/arc, shot/ammo,
`Weapon_In_Range`, `Target_Is_Legal_For_Weapon`, and Energy Absorber pre-fire
conditions. Disclosure filtering precedes those checks and public references,
counts and ordering. Own labels/readiness follow `Draw_Weapon_Status_Display`.
No rendering, impact simulation, operation/rollback or gameplay RNG is used.

Lists are bounded by native grid/slot capacities and the existing 16 MiB response
ceiling. There is no pagination, copied options store or silent truncation.

## Operations

Every operation requires authenticated controller status and selection of the
observer's opened/closed/viewInvalidated events. Every request includes:

```json
{
  "battleId": "<battle token>",
  "actorUnitId": "<owned actor token>",
  "decisionId": "<current decision token>",
  "operationSeq": 1
}
```

| Method | Additional parameter |
| --- | --- |
| `combat.selectUnit` | `unitId` |
| `combat.move` | `destination: {x,y}` |
| `combat.face` | `toward: {x,y}` |
| `combat.fire` | `targetUnitId` |
| `combat.done` | None |

`operationSeq` is a positive JavaScript-exact integer, strictly increasing for
mutation submissions on this connection. Its high-water mark is bounded metadata,
not a receipt archive. A structurally valid controller submission consumes the
sequence even if queue admission fails. Reusing a sequence never executes again;
changing arguments does not make reuse valid. JSON-RPC request IDs still correlate
responses, but are not the mutation duplicate identity. Notifications never act.

There is **one** outstanding mutation globally. There is no admission response,
operation status API, completion topic, retained receipt history, or reconnect
resumption. A final success is:

```json
{"operationSeq":1,"state":"applied"}
```

Applied means the native operation returned, including its effects and normal
network barriers. It is not a promise of movement to the requested endpoint,
full rotation, a hit, survival, or any particular damage. Native warnings,
defensive fire, teleport behavior and automatic consequences remain unchanged.

Fire is one normal native `Fire_Ship` invocation using the current native
selection. It retains Energy Absorber pre-fire discharge, eight-slot ordering,
beams, missiles, torpedoes, fighters, bombs, specials, cloak changes, damage,
RNG, ammunition and post-fire bank-selection resets. Eligible slots describe
the current opportunity, not a new firing policy or a promised salvo. A prior
effect can stop later firing. Assault-shuttle launches retain automatic native
consequences; this does not enable remote boarding/raid commands.

Done marks this ship done and follows native next-actor/pass/turn progression;
it is not the end-side-turn command.

## Authority, conflicts and consumption

Game-owned decision metadata binds the current native input opportunity to its
actor and generation; opaque public tokens do not expose an internal counter.
Observation handles alone grant no action authority. Local semantic input,
weapon selection edits, actor/authority changes, visible native changes and
dependent lifetime retirement invalidate stale decisions. Idle/redraw/camera
activity does not create revisions or rebuild options.

Only the game thread resolves references, reads options or invokes operations.
The active local-human tactical owner consumes a deferred intent, outside the
synchronous observation callbacks, then rechecks native authority and legality.
The local and corresponding received network paths share semantic operations,
preserving their existing send/apply/barrier differences. AI retains native paths.
Generic input pumps, animation, dialogs, AI execution, recalculation and network
barriers never dispatch mutations. Native input delays still apply.

Control requires the current participating local human, live owned functional
ship actor, valid current turn-order entry and native pass, normal tactical input
mode, and local network input ownership. Auto control, captured/stasis/retreating
actors, planet/satellite actors and scan/boarding modes do not gain remote control.
Classic friendly selection follows native turn-order reinsertion; initiative
never permits arbitrary reselection. Time Warp participation stays native.
Hotseat/load/perspective changes revoke connections rather than following a human.

Operations use the same native refresh producers as local actions. Existing
readers see resulting current state without an RPC publication step. Completed
native selection/invalidation events precede the operation's final response;
later owner acquisition may produce another meaningful refresh. Complete unsent
refresh hints alone coalesce; results, lifecycle/selection events and partial
messages preserve ordering.

## Expiry, cancellation and uncertain completion

The dispatch deadline is two seconds from receipt. Queue saturation rejects
immediately. Cancellation/expiry/disconnect/shutdown and the transition to
dispatch synchronize on the single slot. Expired or successfully cancelled
undispatched work cannot execute later. There is no post-dispatch timeout.

`combat.cancelOperation {operationSeq}` cancels only the issuer's queued work.
Success returns `{"cancelled":true}` and the original request receives cancelled.
Otherwise it reports not pending/already dispatched; it cannot undo gameplay.
Unsubscription cancels queued work but cannot rewrite a started operation's final
outcome. Retirement cancels dependent queued work; perspective revocation closes
the connection. Disconnect does not roll back started work, and another controller
cannot dispatch while it is still executing. Shutdown cancellation needs no
future game poll; the native worker must finish/unwind before sidecar destruction.

If the connection closes or the client exits before a final result, the outcome
is **unknown**, even if the client did not receive any acknowledgement. Reconnect,
inspect current state and decide explicitly what to do. Never automatically retry
a mutation after busy, expiry, reconnect or uncertain completion. The shipped
client imposes no false completion timeout on a started operation.

| Error | Meaning |
| --- | --- |
| `-32004` | Unavailable reference/context; unknown, stale, hidden, foreign and reused references are indistinguishable |
| `-32006` | Not admitted: busy |
| `-32007` | Controller required/contention at hello |
| `-32008` | Required tactical event selection missing |
| `-32009` | No native control |
| `-32010` | Authorized decision conflicts with current native opportunity |
| `-32011` | Currently illegal operation |
| `-32012` | Duplicate/old operation sequence; never executed again |
| `-32013` | Cancelled before dispatch |
| `-32014` | Cancellation too late or operation no longer pending |
| `-32015` | Expired before dispatch |
| `-32016` | Unsupported decision path requires local attention |

Transport limits remain 16 MiB per response, 32 MiB/16 output messages per
connection, four connections and existing read/event/completion byte bounds.
Overflow disconnects, never silently loses an outcome and continues the session.

## Local attention

Subscribe to `combat.localAttentionChanged`:

```json
{"battleId":"<token>","required":true}
```

`required:false` marks return from the instrumented native dialog. Events carry
no unaudited dialog text, enemy details or remote answer capability. Native stasis
release confirmation and the Energy Absorber overload information boundary are
instrumented. Remote fire at a stasis target rejects before network send or
mutation; it does not open/answer the confirmation. An informational warning
arising during resolution stays local and leaves the started operation pending
until native resolution returns. No dialog is silently skipped.

## Validation and exclusions

`test_tactical_control` and `rpc_turn` link production gameplay and RNG functions
over fixture-owned memory. The live test runs the actual CLI and an independent
reader. Synthetic button geometry is not original media. Fixtures are not proof
of interactive assets, real network-session synchronization, or other platforms.
Existing observer/transport tests remain independent regression coverage.

For interactive acceptance, connect a reader before battle and a controller
during battle, inspect/choose/select/move/face/fire/done, compare native costs and
effects, introduce local conflicts, allow opponent/AI/network turns, exercise
local attention and closure, and verify idle silence. Repeat classic/initiative
and network ownership cases where sessions are available.

Excluded: strategic queries/actions, missile/fighter targeting, loadout editing,
planet/satellite actors, boarding/raids, retreat/auto/wait/end-side commands,
remote confirmations, enemy dossiers, effect/audio/frame/media streams, history,
replay, retained combat projections and universal UI/operation frameworks.
