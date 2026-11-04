# Live read-only tactical observer (`orion2.rpc/0`)

## Requests

All examples omit the usual `jsonrpc: "2.0"` and request `id` envelope members.
Hello and selection of `combat.opened`, `combat.closed`, and
`combat.viewInvalidated` are prerequisites. Reader/controller authority is equal.

| Method | Parameters | Result |
| --- | --- | --- |
| `combat.getView` | Omitted, `{}`, or `{ "battleId": "<token>" }` | `{ "battle": null }`, or `{ "battle": { ... } }` |
| `combat.getUnit` | `{ "battleId": "<token>", "unitId": "<token>" }` | `{ "battleId": "<token>", "unit": { ... } }` |

Omitting battleId performs initial discovery of the **currently observable**
battle, including mid-battle connection. Supplying it pins the read: an old token
never silently follows a replacement battle. Hidden/uninitialized battles and
no battle have the same discovery result. Unknown parameters, paging and a
full-details list flag are not supported. Tokens are 32 lowercase hexadecimal
characters. No caller-selected perspective is accepted.

Battle fields:

- `battleId`: opaque observed-battle lifetime, not a native index/action handle.
- `localSide`: `attacker` or `defender`.
- `round`: one-based current native round (`combat_turn + 1`).
- `initiative`: native initiative versus classic side-turn mode.
- `grid`: `width`, `height` in tactical grid cells, not pixels/camera coordinates.
- `selectedUnitId`: authorized current actor reference, or null; never hover.
- `visibleUnitCount`: length of the authorized `units` list, not native allocation.
- `units`: the **whole short map list**, ordered by opaque public reference.

Each short row has `unitId`, `disclosure` (`unit` or `contact`), `kind` (`ship`,
`planet`, `satellite`, `monster`, `contact`), `side`, `owned`, `position: {x,y}`,
`mapSize` (`small`, `medium`, `large`, `huge`), and disclosed `facing` (native
sixteen-direction index, 0–15). A contact has no facing; a planet omits both
mapSize and facing, using a distinct schematic planet marker. Map size is the native
reduced-map size grouping, not a design/hull dossier. Side supports schematic
attacker/defender colors; there are no original sprite/resource identifiers.

`getUnit` returns the current short row plus the native combat `name` when not a
contact. An owned nonplanet unit additionally has `ownDetails`:

```json
{
  "structureRemaining": 80,
  "armorRemaining": 50,
  "driveRemaining": 10,
  "shieldSystemRemaining": 0,
  "computerRemaining": 0,
  "speed": 12,
  "movementRemaining": 7,
  "inStasis": true
}
```

These are current native integer values, not rounded enemy damage bars or a
copied calculator. Drive/shield condition is zero for an absent system.
`ownDetails` is omitted for foreign units and planets. Absence means withheld or
outside this field set, not known zero. Names preserve native combat naming and
are converted from CP850 to UTF-8, never substituted with hull labels.

Errors: `-32001` unauthorized; `-32008` tactical selection required; `-32602`
invalid parameters; `-32004` unavailable (indistinguishable unknown, stale,
destroyed, hidden or revoked reference); `-32006` busy/expired; `-32005` response
limit exceeded. Errors do not echo references or request payloads.

## Native field/disclosure policy

The shared game-owned `tactical_view` helpers perform reads; the WebSocket layer
does not reproduce combat rules or call rendering/input/calculation functions.

| Enabled disclosure | Native authority/presentation anchor |
| --- | --- |
| Local human, initialized participating battle only | `COMBINIT::Local_Tactical_Observer`, observed `Tactical_Combat` scope after deployment/initial synchronization |
| Live unit presence, side, position, map-size grouping across the grid | `CMBTDRW1::Draw_Reduced_Map`; main-screen live-unit traversal; no inactive records |
| Normal/ordinary-cloaked unit presentation, facing, combat name | `CMBTDRW1::Draw_Ship`, main-screen selected/hover names and `COMBAT1::Assign_Combat_Grids` |
| Enemy phasing-cloak contact | Reduced-map blip only for its supported size classes; no name/facing/design. Conditional outline/scan disclosure is deliberately outside this contract |
| Owned nonplanet condition, speed, movement | `CMBTDRW1::Draw_Internals_Display`; actual owner independently checked |
| Owned stasis condition | Native `stasis_source_idx != COMBAT_SHIP_NONE`, rendered stasis effect; no source reference disclosed |
| Current actor/round/mode | Committed native turn selection, `combat_turn`, `combat_init`; not raw stale selection globals during setup |

Participation does not authorize an enemy design dossier. Ordinary cloak's
darkening is not invisibility. A phased enemy is conservatively contact-only
even where a native outline/hover/scan might permit more. Its native map side is
public; `owned: false` follows the already-bound local side, not extra intelligence.

Authorization/filtering precedes name/detail copying, reference creation, count,
public ordering and errors. Hidden native slots never allocate public references
or affect public ordering. A reference is generated from OS randomness only after
disclosure, without gameplay RNG. Only slot/token lifetime metadata is retained;
query-local rows/encoded responses are never used to answer a later query.

Destruction, retreat, capture, slot reinitialization and disclosure downgrade
retire affected references. Battle retirement disables reads **before** native
teardown/allocation reset. Closure/replacement, load, menu/perspective changes and
observer loss revoke their dependent work/output. Perspective revocation closes
authenticated connections and requires fresh configured-key hello.

## Execution, events and limits

The transport copies validated requests to an eight-slot handoff. The game thread
executes at most two per explicitly classified safe visit: settled tactical input,
completed local action, completed AI turn/next selection, completed network action
after its barrier, or completed pass/round maintenance. Initialization, effects,
damage resolution, recalculation, nested dialogs, network barriers and teardown
are not safe merely because an input pump ran. Generic input points can complete
no-battle discovery without reading combat allocations; during an observed unsafe
phase they leave requests queued.

There are at most two outstanding reads per connection and a two-second lifetime
from receipt. Queue saturation returns busy immediately; unsafe delays expire as
busy. A 50 ms transport deadline timer only cancels work. Disconnect, unsubscribe
from required events, revocation and shutdown cancel pending work without another
game poll. Unsubscribe cancellation produces unavailable results; already queued
ordered output precedes the selection acknowledgement. A racing old completion
cannot deliver state after that acknowledgement, even after resubscription.

Each read is coherent; separate reads may differ. There are no pinned/historical
pages, snapshots, state baselines, change hashes, timers reconstructing state,
or stale-state fallbacks. Clients may poll, but the shipped client pulls on events.

`combat.viewInvalidated` contains only battleId. Native semantic operations mark
the view dirty; the hint is flushed only at a completed safe boundary. It covers
local, AI and network-applied operations, visible movement/facing/damage/lifetimes,
selection and round transitions. A hint is permission to pull, not proof that a
particular field changed. Idle input, redraws, animation frames, hover, camera
scroll and hidden-only changes do not emit it.

Optional `combat.unitSelected` contains battleId and the authorized unitId, or null
when a previously disclosed actor clears. It follows settled native actor
selection, not cursor focus. Hidden-to-hidden selections produce no wire event.
Only adjacent complete unsent refresh hints coalesce; selection/lifecycle events,
responses and partial messages keep their order.

The current full list is bounded by the native 1,010-record capacity and filtered
before counting; it is not silently truncated to fit a transport message. Limits
are 16 MiB per encoded response, 32 MiB/16 output messages per connection, four
connections and 64 KiB input. Pending completion bytes have an independent 128 MiB
cap. These ceilings are not preallocated. Overflow disconnects affected clients
rather than dropping ordered output. If a dependent response/selection becomes
stale while pending or partially sent, the connection closes rather than finishing
that message; bytes already handed to the backend cannot be recalled.

## Interactive acceptance walkthrough

Use a legitimate asset installation and a save/session that can enter tactical
combat. Do not use AI-only or nonparticipating host calculation as observation.

1. Start the RPC-enabled game and observer using the README commands. Before
   battle, expect "No observable battle" and no repeated state requests.
2. Enter a tactical battle as the current participating local human. Expect an
   opening, current map rows, and owned selected-unit name/condition/movement.
3. Start a second observer mid-battle. It must discover the same battle and
   references without needing an opening event.
4. Use `unit <unitId>` for an owned ship, an enemy ship and any phased contact.
   Compare to native disclosures; no foreign ownDetails/design catalog is allowed.
5. Move/face/fire, wait/done, release stasis where applicable, and let the opponent
   act. Expect event-triggered current reads after completed actions. Repeat in
   classic and initiative modes, with AI and a real network opponent when available.
6. Leave the game idle and redraw/scroll/hover. Expect no refresh traffic or view
   reconstruction until a query or meaningful native change.
7. Finish the battle. Expect safe closure and cleared client references. A slow
   client with stale output may disconnect and must reconnect rather than use it.
8. Exercise load/hotseat/perspective revocation. Old connections must close; a
   fresh hello/selection/discovery cannot reuse old battle or unit references.

Automated evidence distinguishes production native-function tests and live-socket
fixture/CLI tests from this real-game walkthrough. Commercial assets, interactive
saved-game parity, real multiplayer sessions and other supported platforms require
separate execution; an asset-free server is not evidence they were exercised.

Excluded: actions/navigation, targeting/options, enemy scan/design dossiers,
weapon catalogs, ordnance/effect/audio streams, exact sprites/media, planet
population/economy/defense detail, results/history, strategic queries, pagination,
full-detail list flags and a universal UI/event framework.
