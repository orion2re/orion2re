# Deliberate tactical firing and native hit reports

This extends [basic tactical control](tactical-control.md) as a local,
out-of-process accessibility/voice-over interface to the tactical module's own
state and operations. RPC does not own a second combat model. Reader connections
remain independent of the single controller. Authentication, native input
authority, the simple mutation lifecycle and transport limits are unchanged.

## Run the pilot

Build/configure using [README](README.md). Enter tactical combat locally in a
legitimate installation or isolated runtime with copied saves. RPC cannot load
or navigate into a battle.

```sh
python3 tools/rpc_observer.py --port 17362
python3 tools/rpc_observer.py --port 17362 --controller
```

Use the hidden prompt or an owner-only `--key-file`; never a key argument or URL.
An example sequence, substituting references and legal cells from current reads:

```text
options
selection 0=on 1=off 2=temporaryOff
options
fire <unitId>
hits
selection 0=off 1=on 2=off
options
intercept <missileId>
missiles
hits
done
```

`missiles`, `missile <missileId>` and `hits` also work in reader mode. Selecting
the missiles/history display refreshes it on native events, not on a timer.
Closure/revocation clears that display selection and stale operational choices.
An invalidation clears the controller's decision; operations are never silently
reselected or repeated. Native authority loss, contention, local attention,
refusals and unknown-after-disconnect outcomes retain the basic contract.

## Select native weapon banks

`combat.setFireSelection` has the same `battleId`, `actorUnitId`, `decisionId`
and positive increasing `operationSeq` as other mutations, plus:

```json
{"banks":[{"slot":0,"state":"on"},{"slot":1,"state":"off"},{"slot":2,"state":"temporaryOff"}]}
```

There must be 1–8 unique occupied slots, numbered 0–7. Omitted slots are unchanged.
The game validates the whole request before changing any selection. It then
uses the shared native `on -> temporaryOff -> off -> on` cycle, including native
network toggle messages and barriers. This is one outstanding operation and one
final response, not a new network transaction/rollback protocol.

Temporary-off (native 0) resets to on (1) through native post-fire/turn handling;
off (-1) persists through those resets. Automatic defensive-fire exceptions stay
native. This does not edit designs, installed weapons, shots or ammunition and
does not select a firing policy. Special weapons in banks are included;
special-device readiness is separate and is not editable here. In particular,
turning all banks off does not disable a selected Energy Absorber pre-fire effect.

`getTargetOptions.weapons` retains its fields and adds `modifiers`, the installed
native weapon-modification flags. The CLI names those modifiers and distinguishes
ammunition, unlimited ammunition and torpedo recharge. This is current owned-bank
information, not a component catalog or an enemy design dossier.

## Inspect and target missiles

The source's `CMBTMIS` / `s_missile` terminology is used on the wire. These records
also represent native fighter and torpedo groups.

| Method | Parameters | Result |
| --- | --- | --- |
| `combat.listMissiles` | Omitted, `{}`, or `{ "battleId": "<token>" }` | `battleId`, `visibleMissileCount`, complete `missiles` array |
| `combat.getMissile` | `{ "battleId": "<token>", "missileId": "<token>" }` | `battleId`, `missile` |

Rows contain `missileId`, `kind` (`missile`, `fighter`, `torpedo`), native hover
`name`, `owned`, `quantity` and logical-grid `position: {x,y}`. The native reduced
map is global; moving the viewport neither grants nor revokes these ordinary
tactical records. Names/quantities follow native missile status presentation.
No launcher, owner-player ID, target link, targeting chain, modifications,
trajectories, history of positions or forecasts are included.

The participating local-human guard and settled read boundaries apply before
enumeration, counts, references and presentation. A short-lived inactive/dead
allocation is not a public row. Lists cover the native 5,000-slot domain without
silent truncation; response limits still fail explicitly.

`combat.getTargetOptions` additionally returns `missileTargets`: authorized enemy
rows plus `eligibleWeaponSlots` and `canFire`. Ineligible targets have an empty
slot list and `canFire: false`. Eligibility uses the same native selection,
shots/restoration, ammunition/recharge, bearing/arc, `Weapon_In_Range` and
`Target_Is_Legal_For_Weapon` as normal firing—not AI damage/threat estimates.

`combat.fire` accepts exactly one of `targetUnitId` and `targetMissileId`.
`intercept` is a CLI name for the latter, not a separate native firing policy.
Both use the shared normal operation and preserve Energy Absorber behavior,
weapon-class effects, native slot ordering, launches, damage, RNG, cloak changes,
ammunition and post-fire selection handling. Existing native refusals apply;
not all weapons can intercept and torpedo-class targets remain ineligible under
the native predicate. No missile retargeting or movement command is exposed.

Missile references and unit references are distinct. Native removal/merge-away
retires a missile reference before slot clearing/reuse. Movement and other
authorized targeting changes invalidate conflicting decisions. Retarget links
are private and are not published as data. Battle retirement, load and perspective
revocation retire dependent references/output. Observation handles alone are
never action authority.

## Native hit history

`combat.getHitHistory` takes the same optional `battleId` as list reads. It returns:

```json
{
  "battleId":"<token>",
  "firstSequence":1,
  "nextSequence":3,
  "retentionGap":false,
  "hits":[
    {"sequence":1,"round":1,"kind":"unitDamage","targetName":"Opponent","amount":8,"secondaryAmount":0},
    {"sequence":2,"round":1,"kind":"missilesDestroyed","targetName":"Nuclear Missile","amount":2,"secondaryAmount":0}
  ]
}
```

This is the tactical module's own battle-scoped report, produced without a
client/RPC dependency. It retains at most 4,096 authorized native attack-result
entries. `firstSequence > 1` and `retentionGap: true` explicitly report eviction;
the returned array is the complete retained report, not a silently truncated page.
Reads are coherent, non-consuming and consume no gameplay RNG.

Entries are resolved result groupings, not animation frames, individual rendered
projectiles, every miss, damage forecasts or operation receipts. Amount channels
preserve native report/display arithmetic. `unitDamage` has primary and secondary
damage channels; `missilesDestroyed` is a native reported interception count;
`biologicalHits` is the native biological-bomb hit result, not a population count.
No raw foreign current/max HP, components or hidden attack/source links are added.
Native calculation/presentation quirks are not silently normalized or repaired.
The approved bounds-only crash guards avoid ship-array reads for missile indices
outside the ship allocation. Existing in-range missile/ship-slot aliasing,
including its Displacement Device RNG behavior, is deliberately preserved.

Authorization precedes recording and public sequence allocation. Normal unit
labels and missile status labels are retained as historical presentation facts;
undisclosed/phased enemy identity is not copied. A label is not a live object or
action reference, so destruction and slot reuse cannot redirect a historical hit.
This is not an RPC event-subscriber log, state-difference calculator, battle
snapshot, mutation receipt store or retransmission/retry mechanism.

The native animation queue is unchanged and remains transient. The hit report
explicitly releases its allocation at battle retirement, closure and revocation;
scope destruction also releases it on exceptional/aborted exits. The next battle
starts empty with sequence 1. Closed/replaced battle IDs cannot query old reports.
There is no accumulation across combats, save serialization or after-battle archive.

Native producers mark the existing `combat.viewInvalidated` hint; settled native
boundaries flush it. Reads do not depend on an RPC publication step. Idle redraws
and queries create no hits or refresh traffic. Local, AI and network-applied
resolution use the same report producers. Capture-result dialogs reached through
normal assault-shuttle effects stay local and use `combat.localAttentionChanged`.

## Verification boundary

Full-production-memory fixtures cover deliberate selection, normal firing,
native interception/refusal, history and repeated combat cleanup. Live WebSocket
tests run the standard client with selection/unit fire/interception/history/done
and concurrent independent reads. Existing synthetic held-dispatch tests remain
delivery evidence only. Real-asset battles, next-actor drawing, real multiplayer
sessions and other platforms are not claimed by these tests. The real-battle
walkthrough is explicitly outside this bounded delivery's acceptance scope;
broader interactive/multiplayer/platform parity remains separate work.

Strategic navigation, design editing, remote confirmations/boarding/retreat,
enemy dossiers, forecasts and effect/audio/frame streams remain excluded.
