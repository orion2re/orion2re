# Observed tactical decision workflow (`orion2.rpc/0`)

This is the complete tactical decision workflow for the standard controller and
an independent reader. It extends the [observer](observer.md),
[control](tactical-control.md), [firing](tactical-firing.md),
[boarding](tactical-boarding.md), and [withdrawal](tactical-withdrawal.md)
contracts. Their implemented operations and final-response lifecycle remain in
use; this contract supersedes their ship-only, scan, options, automation, remote
confirmation, and missed-active-report exclusions.

## Start, inspect, choose, continue, close

Build and configure the optional listener as in [README](README.md). Enter an
observed tactical battle through the game; strategic navigation/initiation is not
an RPC capability. Run both clients, using the hidden key prompt or an owner-only
key file, never a key argument or URL:

```sh
python3 tools/rpc_observer.py --port 17362
python3 tools/rpc_observer.py --port 17362 --controller
```

Either client can connect before or during battle. Subscription precedes current
discovery. Both roles can inspect the same authorized state and input opportunity;
only the controller can submit actions. The reader never reserves control.

Use current references, targets, cells and marine bounds, not the placeholders
below. Each command is a deliberate customer choice:

```text
options
unit <unitId>
condition <unitId>
scan <unitId>
help wait
help weapon 0
help system 0
selection 0=on 1=off
move <x> <y>
face <x> <y>
fire <targetUnitId>
missiles
intercept <missileId>
hits
wait
done
```

After each action or native refresh, inspect the resulting situation and fresh
options. Native ordering can select another actor, offer the same actor again,
run an opponent/AI/network action, enter a strict wait stage, or start an eligible
Time Warp pass. The client does not choose or submit any follow-up action.

Additional native decisions are usable in this same client:

```text
release <stasisTargetUnitId>
confirm no
release <stasisTargetUnitId>
confirm yes
mode options
option impactWarning on
option doubleSpeed on
option legalBoxes on
option shieldSectors on
option grid on
self-destruct
confirm no
mode orders
side-wait
end-side
auto normal
```

Use `auto manual` when the displayed native input is `returnToManual`.
`auto superFast` is offered only where the native super-fast shortcut exists.
These are explicit native automation choices, not client tactics. Return to
manual changes the native auto flags; an already-running automatic ship turn can
still finish. No AI/opponent takeover is provided. The registered but unhandled
native `[` hotkey is not a new command.

Return-to-manual authority follows the existing own-side auto flag, not a
particular AI ship. Its token survives intervening native actor/state changes
while that flag remains enabled. The intent uses the ordinary operation slot and
is consumed only at the existing native auto-input checks (including network
opponent waits). Disabling/re-enabling auto, battle retirement and revocation
cannot redirect an old token. No second queue or turn-driving client policy is
involved. Return-to-manual cannot coexist with another outstanding mutation.

For a native boarding choice already opened locally, use
`choice raid|capture|cancel <marines>` with the displayed bounds. For an ordinary
remote boarding attempt, retain `board <targetUnitId> raid|capture <marines>`.
Use `boarding-ack <reportId>` for a native result report and `ack` for the displayed
tactical information/detail view. No answer, maximum commitment, target or
acknowledgement is chosen automatically. `options` can rediscover the active
native input after connecting mid-question or mid-report.

`mode scan`, `mode boarding`, `mode options`, and `mode orders` explicitly change
the native panel/input mode. Reads never change it. Native mode-specific actions
remain unavailable outside their mode. The native missile warning is not merely
cosmetic: it can interrupt movement or rotation. When its warning control is
offered, `option cycleWarning on` invokes one native warning cycle; inspect the
result before choosing whether to move again. The client never retries movement
to overcome an interruption.

For deliberate withdrawal, use `retreat`, observe pending state/outcomes, and
choose subsequent actions yourself. For end-of-opportunity decisions, `wait`,
`side-wait`, `done`, and `end-side` are distinct. Side Wait is unavailable in
classic mode and the restricted strict wait stage. End-side ends the native
side/pass, not the battle or strategic turn.

Normal completion is `combat.closed`, after native battle writeback/teardown in
the game. Both clients clear tactical references and await another observed
opening. There is no final tactical report, duplicate finished event, or archive.

## Current inspection and available choices

`combat.getView` retains its short map and adds `sequenceMode`
(`classic`, `initiative`, `strict`), `pass` (`normal`, `timeWarp`),
`strictWaitStage`, and `inNebula`. These describe current native context, not a
predicted next actor or schedule. Ship, planet, satellite/base, monster and contact
rows retain their existing disclosure distinctions.

`combat.getScan {battleId,unitId}` returns `battleId` and `scan`, using the native
tactical scan layout. It includes the canonical name, attack/defense/evade values,
crew/marine presentation, captured state, speed, structure, applicable armor,
drive/computer/shield values, installed weapons/modifiers/ammunition and
installed/damaged systems. Native absence/suppression is preserved: marine 255,
monster layouts, planet defenses, absent drive/shield systems and subsystem-hit
availability are not normalized into a universal ship dossier. Shield strength
uses the native weakest-arc scan presentation, not four invented scan fields.

Scan requires an initialized local-human input opportunity, native scan enabled,
an active subject and the native tactical subject route. It does not require the
reader to obtain controller status or to toggle scan mode. A phased contact is
still contact-only in ordinary `getUnit`; separately authorized native scan can
reveal the information the scan branch actually shows. Scan authority is not
ownership, contact visibility alone, host privilege, or a strategic design query.

`combat.getUnit {battleId,unitId,detail:"status"}` is the explicit status variant
used by `condition <unitId>`. It adds the native grey damage-bar presentation at
its native 44-pixel half-width, including its rounding/minimum widths, rather than
raw foreign hit points or component totals. It requires the current local-human
tactical information opportunity but does not require scan enabled. Phased
contacts do not acquire condition detail. Ordinary `getUnit` remains unchanged.
For nonplanet units it also describes native sprite darkening, the visible
phase/stasis/web/black-hole/absorber marker in native presentation precedence, and the
officer-present indicator. It does not expose effect sources, timers, pending
damage, private links, or an enemy officer dossier.

`combat.getTacticalHelp {battleId,topic,slot?}` reads the native localized help
chain, bounded by the native nine-record limit. Topics include `internals`,
`auto`, `scan`, `boarding`, `retreat`, `wait`, `done`, `selfDestruct`,
`impactWarning`, `doubleSpeed`, `legalBoxes`, `shieldSectors`, `grid`, `options`,
`weapons`, `map`, and `warning`.
`weapon` and `system` require the current actor's native help slot, 0–7. There is
no arbitrary help index, file path or asset download parameter.

Missile detail adds `targetName` only when that displayed label is independently
authorized. No private target link/reference is created. Current own systems and
their damaged/active presentation are also available in target options; Specials
is an information view, not a native device-toggle command.

`combat.getTargetOptions` retains existing options and adds current `mode`,
`canWaitSide`, `canEndSideTurn`, `canSelfDestruct`, `canSuperFast`,
`canAutoCombat`, `scanEnabled`, `canEnterBoarding`, `missileWarning`,
`tacticalOptions`, `systems`, and `stasisTargetUnitIds`.
Lists and availability follow the native mode. Normal actor authority includes
functional human-owned planet defenses and satellites/bases, not just ships.
Native zero-speed, defense, boarding, retreat and self-destruct restrictions still
apply. Initiative never gains arbitrary reselection. Monsters, ordinary NPCs,
foreign/network actors and launched ordnance do not gain human actor control.
Noncombat fleet craft are not independently piloted tactical actors.

All inspection is on demand and non-consuming: no render/modal, UI scratch copy,
selection, order/grid rebuild, gameplay RNG or retained projection prepares it.
Authorization precedes fields, references, counts, ordering and errors. Responses
use current native precision and CP850-to-UTF-8 conversion. Context-dependent
undelivered output is invalidated when native authority changes.

## Mutations and explicit children

The following use the existing `battleId`, `actorUnitId`, `decisionId`, positive
increasing `operationSeq` context and final operation response:

| Method | Additional parameters |
| --- | --- |
| `combat.removeStasis` | `targetUnitId` from current stasis choices |
| `combat.selfDestruct` | None; native options panel required |
| `combat.waitSide` | None |
| `combat.endSideTurn` | None |
| `combat.setInputMode` | `mode`: `orders`, `scan`, `boarding`, `options` |
| `combat.setTacticalOptions` | `option` and boolean `value`; one native control per call |
| `combat.setAutoCombat` | `mode`: `normal` or `superFast` |

The tactical option names match the CLI above. The five panel flags require the
native options panel. Warning cycling requires the native warning control.
Changing speed/presentation never silently skips native consequences or barriers.

A concrete native question is shown by `combat.localAttentionChanged` with
`required:true` and `input`. It is also readable through `getTargetOptions` while
that owner is active. Input has `kind`, opaque `inputId`, native `text` where
applicable, and actual choices/bounds. A boarding result instead contains its
existing `reportId` and report content. During unsafe nested handling, ordinary
battlefield reads remain busy; only current owner-input inspection is serviced.
Local marine edits refresh this information with a fresh decision token. Closing
a child may restore return-to-manual input in `required:false`; attention changes
and result-known events are not parent-operation completion responses.

| Child method | Parameters in addition to `battleId`, `operationSeq` |
| --- | --- |
| `combat.answerConfirmation` | `inputId`, `choice`: `yes` or `no` |
| `combat.answerBoardingChoice` | `inputId`, `choice`: `raid`, `capture`, `cancel`; explicit `marines` |
| `combat.acknowledgeInformation` | `inputId` |
| `combat.setAutoCombat` (return route) | `inputId`, `mode:"manual"` |
| `combat.acknowledgeBoardingReport` | Existing `reportId` contract |

These are concrete tactical owners, not a generic dialog API. Cancellation in a
boarding choice preserves its native uncommitted value and is distinct from
committing zero marines. Declining stasis release does not fire; accepting releases
stasis only. Stasis choices follow the native enemy-click branch before its
phasing refusal: an eligible contact can be released without gaining a name,
firing eligibility, scan permission or other contact detail.
Self-destruct preserves the native
engine/destruction effects and next-actor handling. Native local answers, input
delays, cleanup, timed boarding reports and received-network routing remain intact.

The existing single operation slot and its single bounded acknowledgement child
are reused. Only an owner-compatible answer from the executing parent's connection
may coexist with that parent. With a locally initiated parent, the answer uses the
ordinary slot. An answer's final success means that child owner consumed it and
completed its cleanup; it does not claim completion of the enclosing local action.
An RPC parent remains executing through all native effects and normal barriers.
Ordinary success stays `{operationSeq,state:"applied"}`, not queued success.

Native authorization, reference lifetime, current decision, phase, availability,
input ownership and choice bounds are rechecked at consumption. Local conflicts,
retirement/reuse and owner replacement invalidate dependent choices. Completed
outcomes are not rewritten into refusals by retirement or unsubscription.
Existing duplicate, two-second pre-dispatch expiry, cancellation, output limits
and unknown-after-disconnect rules apply. Cancellation does not answer an already
open question. Disconnect does not undo gameplay or automatically dismiss it;
native local input remains usable. Never automatically retry a mutation.

## Evidence and remaining parity limits

The production native turn/round owners, gameplay/RNG, concrete question owners,
live WebSockets, standard CLI and independent reader are used for automated
workflow acceptance. Fixture-owned initialization/teardown and synthetic transparent
sprites, glyphs and text are identified as such. Standalone transport/budget tests
are supplemental, not battle evidence. Local/shared and received-helper loopback
checks are not real multiplayer-session evidence.

Commercial-asset/save walkthroughs, real multiplayer, leak verification where
unsupported, other platforms and full interactive media parity require separate
validation. Strategic gameplay/navigation/turn management, invasion/ground combat,
effect streams, exact sprites/frames/audio, replay/archives and invented final
reports remain outside this tactical decision capability.
Help is read without opening its renderer/modal. Generic UI/help navigation,
developer config reloads, and cosmetic camera/tab gestures are not remote tactical
decisions; no raw input injection or general popup controller is provided.
