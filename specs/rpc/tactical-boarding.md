# External native tactical boarding

This extends the [ship-turn](tactical-control.md) and [firing](tactical-firing.md)
pilot. Build, enable and launch using [README](README.md). Authentication remains
a hidden prompt or an owner-only key file. Keep an independent reader running:

```sh
python3 tools/rpc_observer.py --port 17362
python3 tools/rpc_observer.py --port 17362 --controller
```

## Standard-client workflow

```text
boarding
boarding-cancel
boarding
board <targetUnitId> capture 4
boarding-report
boarding-ack <reportId>
options
done
```

Use actual references and bounds from current options. `boarding` is an alias for
`options`; `board <targetUnitId> raid 3` deliberately raids with three marines.
The client never chooses a mode, maximum commitment, raid component or report
answer for you.
`boarding-cancel` discards unsubmitted choices without a game mutation, or requests
pre-dispatch cancellation of the outstanding operation. Existing `cancel` remains
available. Neither can undo an executing commitment.

The client prints detailed native boarding events and final action results.
`boarding-report` displays the last result received by this client, not a server
history query. A non-null `reportId` permits explicit acknowledgement while the
parent action is still outstanding. Native report closure clears that authority;
battle closure/revocation clears the client context. No reconnect, reselect,
recommit, acknowledgement or mutation retry is automatic.

## Options and commitment

`combat.getTargetOptions` adds `boarding` when normal native control is available:

```json
{
  "ownMarines":12,
  "alreadyBoarded":false,
  "minimumMarines":0,
  "maximumMarines":6,
  "modes":["raid","capture"],
  "targetUnitIds":["<authorized eligible target>"]
}
```

The step is one. Bounds come from the same native helper as the local slider:
`0..(marine_count + 1)/2`, inclusive. Byte value 255 retains upper bound 128;
`ownMarines` is null for the native `0xFF` marine-display suppression. A zero
commitment is **not cancel**: native boarded/cloak handling still occurs.

Only legitimate, normal-disclosure foreign targets are listed. No foreign marine
count, device, shield value, boarding strength or forecast is returned. The shared
human predicate includes the checks beyond `BOARDING::Filter_Boarding`. Queries
do not open a popup, change modes/values, rebuild grids or consume gameplay RNG.

`combat.board` requires the existing controller role, tactical subscription and
current native ship decision:

```json
{
  "battleId":"<battle>",
  "actorUnitId":"<actor>",
  "decisionId":"<decision>",
  "operationSeq":21,
  "targetUnitId":"<target>",
  "mode":"capture",
  "marines":4
}
```

Authority, actor/target lifetime, turn/pass, local network ownership, decision,
already-boarded state, legality and bounds are checked at native consumption.
Refusals precede marine deduction, packet send and RNG use. Existing error codes
apply: unavailable references, no native control, stale decision, illegal action,
busy, duplicate sequence, cancelled and expired. A legal failed attempt is an
applied result, not a refusal or a promise that no marines were consumed.

Supported actors are the existing controllable local-human ships, in classic or
initiative modes and eligible native passes. Targets include ships, Antaran ships,
satellites and native recaptures where legal. Planets, monsters, own ships, stasis
and phased targets remain forbidden by native rules. Ordinary boarding and working
Transporters preserve configured range/speed, facing-shield, Hard Shields and
Damper checks, damaged devices and black-hole configuration. Boarding never grants
control over an AI/network-owned actor or planet/satellite actor.

## Detailed native result and event

A final successful request has `operationSeq`, `state:"applied"` and `boarding`:

```json
{
  "mode":"capture",
  "outcome":"captured",
  "targetName":"Opponent",
  "marinesCommitted":4,
  "marinesArrived":4,
  "transporterLosses":0,
  "attackerLost":0,
  "attackerSurvived":4,
  "raidersReturned":0,
  "defenderStarted":2,
  "defenderLost":2,
  "defenderSurvived":0,
  "attackerStrength":200,
  "defenderStrength":0,
  "targetOwned":true,
  "targetCaptured":true,
  "capturedMarines":4,
  "reportId":null,
  "reportTimed":false
}
```

Values above illustrate a controlled fixture outcome, not a capture guarantee.
Capture outcomes are `captured`, `failed` or `destroyed`; raid outcomes are
`raided`, `noDamage` or `destroyed`, following the native report branches. Raid
adds complete native result `text` and `contentAvailable`. No enemy input dossier
is disclosed before resolution. Foreign raids disclose reported strengths/losses,
not the foreign party's starting/surviving counts. Pre-transporter commitment,
transporter losses and returned-raider counts are supplied only for an owned
boarding party. Commitment/loss fields are omitted when no explicit native
commitment is known, such as an assault-shuttle impact; launch counts are not
reconstructed. Full defender counts are capture-report fields only. Stored
garrison/captured-flag detail is supplied only for a currently owned target.
Capture counts preserve the native report's
combat survivors; `capturedMarines` separately records the stored capture garrison
after native capacity handling. Ownership/eligibility remains native, including
telepathy, captured flags, officer consequences and Quantum Detonator destruction.

Subscribe to **`combat.boardingResolved`**. Parameters are `battleId` and the same
`boarding` result. Native producers cover local, RPC, AI, received-network and
assault-shuttle resolution without an RPC publication step. Names are historical
presentation labels, not reusable target references. Capture retires the old unit
reference; pull the settled view for a fresh authorized reference and ownership.

The event means **native result known**, not parent-operation completion. For a
displayed report it is sent only after the actual textbox is initialized, with a
non-null `reportId` and native `reportTimed`. Raid marine writeback/returns still
follow report dismissal. Ordinary reads during this unsafe child owner remain
queued or expire busy; they never observe a partially committed combat view.
After native completion, existing invalidation/selection producers enable current
pulls. The final action result has no active report reference.

The module owns only its latest authorized result, not an operation receipt log
or archive. Retained raid text is limited to 1 MiB; overflow explicitly returns
`contentAvailable:false, contentUnavailableReason:"limitExceeded"`, never a partial
body. Replacement, retirement, closure, revocation and scope destruction release
storage. Hit history remains separate with its existing retention/cleanup rules.
Super-fast mode still suppresses the native textbox, but native result facts are
published without manufacturing a report owner.

## Explicit report acknowledgement

`combat.acknowledgeBoardingReport` takes:

```json
{"battleId":"<battle>","reportId":"<initialized report>","operationSeq":22}
```

It requires controller status and the actual local native report owner, not ship
control manufactured after capture or actor loss. Its final response is the usual
`{operationSeq,state:"applied"}` after owner-consumed dismissal and textbox cleanup.
The boarding presentation owner is `CMBTDRW1::Present_Boarding_Result`. Generic
textbox code only opens/draws/closes a box; it has no boarding events, intent
consumption or tactical dependencies.
It cannot answer warnings, confirmations, the boarding slider or other textboxes.
Manual input, mouse-release waits, input delays and timed report expiry remain
native. A local dismissal or expiry racing a queued acknowledgement makes it
unavailable; it cannot apply to the next report.

There remains one global outstanding gameplay mutation. The sole exception is one
bounded report-acknowledgement child from the **same connection** as an executing
parent, bound to the active boarding report. The parent stays executing through
dismissal, remaining native effects and barriers. Only the report owner consumes
the child, outside synchronous observation callbacks. No recursive gameplay
dispatch, early parent completion, admission response, status API or receipt store
is introduced. With no RPC parent, the acknowledgement uses the normal global slot.
This also supports boarding reports reached during existing firing operations.

Sequence protection and two-second pre-dispatch expiry/cancellation apply to the
child too. Once consumed, it has no false completion timeout. Disconnect does not
undo boarding or automatically dismiss a report; another controller cannot take
over an executing parent's child. Dismiss locally if necessary. If the result
event was missed (for example reconnecting during a manual report), this interface
does not replay it or fabricate a new report handle: finish that report locally,
then inspect current state. A disconnect before the final response leaves the
action outcome unknown; never automatically retry.

## Native compatibility and evidence boundary

Native marine arithmetic, RNG, cloak changes, casualties, returned raiders,
capture-capacity/troop-pod and telepathic configuration are preserved. Local human
boarding deducts the selected pre-Damper commitment but sends post-Damper marines.
Known commitment is passed explicitly into native resolution and result helpers;
there is no ambient attempt pointer or set/reset guard. An unprovided commitment
remains unknown, including assault-shuttle resolution during other native work.
The receiver deducts only that packet count and does not repeat attrition. AI keeps
its own commitment policy and zero-survivor behavior. Events reflect the values
known on the native route; no missing packet argument is invented to normalize
sender/receiver marine deductions.

Automated acceptance uses production gameplay/RNG, shared local/received helpers,
actual result textbox consumption and live WebSocket/standard-client sequences
with independent readers. Temporary transparent sprites, blank glyphs and test
messages are synthetic media, not original assets or interactive parity. Queue
unit tests and held-dispatch/budget setups are supplemental, not gameplay proof.
Explicit allocation-release assertions are separate from sanitizers run with leak
detection disabled. Real battles, commercial saves, real multiplayer sessions,
other platforms, strategic navigation and broader tactical/modal catalogs are not
claimed or required by this boundary.
