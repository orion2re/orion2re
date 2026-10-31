# P0 query, action and lifetime decisions

Static audit at the reviewed target, not runtime hook certification. All sites
default to **no RPC work** until an owner registers initialized state. The
Appendix B input-owner checklist is the adapter inventory; this audit specifies
the common integration boundaries for all those owners. P2/P4 route tests remain
mandatory and do not become complete because a function name was audited.

## Pump classes

| Site | Observation | Mutation |
| --- | --- | --- |
| `MOX2::Screen_Control` | Lifecycle/revocation only; no borrowed screen view exists between calls | None. Do not synthesize a screen number or call an arbitrary screen function. |
| `fields::Get_Input`, `Scan_Input`, `Scan_Field` | Only the current initialized owner view; a registered coherent global snapshot if resolution is not busy | No generic action dispatcher here. It also runs during animation, input delay and nested boxes. |
| Each actual human decision loop listed in Appendix B | Capture after native initialization/refresh, before release or replacement | At most one owner-approved semantic intent in place of local input, then follow the same native operation path. |
| `fields::Get_List_Field` | Copy only current argument-backed visible rows/title/enabled state inside the function lifetime | A list-item token belonging to this child context; consume through this loop, not its parent. |
| `GENDRAW::Confirmation_Box` | After its Y/N fields, formatted text and auto-function are initialized | Child-only yes/no continuation consumed by the existing return value. Do not call the outer dispatcher recursively. |
| `TEXTBOX::Do_Text_Box` / `Text_Box_Get_Input` | Initialized rendered content after hidden dismissal field is added | Dismiss only where the native loop permits it; timed wait remains timed. |
| `fields::Timed_Wait_For_Input`, `timer::Release_Time` | Heartbeats belong to the service thread. Context observation only at an explicitly classified caller | None by default; no generic shortening or mutation pump in a timer. |
| `COMBAT1::Get_Tactical_Input` / `Do_Combat_Turn` | Only active observed battle with permitted scan detail | Consume combat intent at the actual human acting-unit decision, not every tactical input call. Config-reload handling can open nested warning/message boxes. |
| `beams::Draw_Shield_Hit`, AI/auto-combat and opponent-wait input polls | Authorized animation/event observation only | None. Polling input does not grant a combat decision. |
| `SMACK::Play_Cinematic` | Copy frame+palette after presentation as one generation; its raw keyboard/mouse loop does not call `Get_Input` | Only native skip, if currently allowed; preserve left/right/key behavior and film pacing. No other game action. |
| Turn calculation, network decode, allocation/mutable list rebuild | Last dated immutable snapshot or explicit busy, never a live global capture | None outside an explicitly registered nested player decision. |

Every action needs authenticated control ownership, game/perspective identity,
top context ID, generation, current semantic control identity, native phase and
local/multiplayer authority. Recheck at execution, not only at queue admission.
Local input wins if it has already changed the context: cancel the stale intent.
An accepted queue entry is not an applied action.

## Field replacement and parent restoration

Verified sources: `fields::Clear_Fields`, `Clear_Fields_Above`, `Mark_Fields`,
`Release_Fields`, `FIELDSAV::Save_Field_Stats` / `Restore_Field_Stats`,
`GENDRAW::Message_Box_Startup` / `Message_Box_Cleanup`,
`TEXTBOX::Text_Box_Startup` / `Text_Box_Cleanup`.

1. Register only after native fields, argument buffers and selected rows exist.
   Registration owns no native memory; only copied values cross the thread boundary.
2. Treat clear/truncate/rebuild and in-place control meaning/content changes as
   generation changes even when field count, index and rectangle are unchanged.
   Dynamic list contents need their own content revision.
3. `Mark_Fields` switches the field pointer and calls `Clear_Fields`;
   `Release_Fields` restores a single saved set of field globals. This is not an
   RPC context stack. Push a distinct child identity; never infer nesting from
   native field indices or assume every legacy save mechanism is arbitrarily
   reentrant.
4. Suspend parent actions while the child is active. Restore the parent on exit
   only after rebuilding/revalidating its controls, with a fresh generation.
5. Unregister a child **before** native field restore/pop/swap and before its
   borrowed strings, arrays or stack buffers die. On exception/unwind, revoke
   callbacks using the RPC registration scope; do not depend on native cleanup
   statements following a loop.

Examples: `COLSUM::Colony_Summary_Screen` keeps its sorted list on the stack;
`CMBTDRW1::Detailed_View_Ship` has a local editable name; list and name popups
retain caller-backed text. Never read a surviving global pointer after its scope.
Report display routines may consume reports or open dialogs, so a global read
must capture already authorized data, not invoke the display routine.

Tests required: T-04 (identical field geometry/new meaning, truncation, mark and
restore), T-05 (revocation/load/slot reuse), T-08 (nested confirmation, duplicate,
refusal/partial result), T-10 (delay/animation/timed box/film), T-16 (thread handoff).

## Committed state / turn publication

`NEXTTURN::Next_Turn_Calc` records history and increments `MOX::stardate` near its
end. `Next_Turn` then handles end-game presentation, clears player done flags and
resets old-system state. Reports/research choices can run independently afterward.
Do not publish from turn submission, the increment alone or `Get_Input`.

- Single-player/hotseat: publish after successful `Next_Turn` completion and
  session/perspective/end-game checks; while it runs global queries are busy.
- Host: do not publish inside `Next_Turn` as well as the host path. Complete
  host elimination/roster adjustments, authoritative broadcast preparation,
  ready-state and between-turn icon refresh, then call the same publication
  routine once with the committed game/turn identity.
- Client: after successful `Decode_Broadcast_Game_Data_Differential`, allocation
  restoration, ready-state and icon refresh, then call that publication routine.
  Abort/eliminated/failed decode paths do not announce an unapplied new world.
- Deduplicate by game identity and committed turn, not screen/revision. Load/new
  emits session reset and gets a new game identity, never a false turn event.
  Revoke the old hotseat perspective before any new publication.

Native source evidence: `src/game/nextturn.cpp` functions above;
`NETTURN::Host_Next_Turn`, `Client_Next_Turn`, `Net_Next_Turn`;
`MULTPLAY::Hotseat_Select_Player`. Test T-06 must exercise native interruption,
cancel, auto-turn and host/client paths; this audit is not a replacement for it.

## Exit and exception ordering

`platform::Check_Exit` throws; `MOX2::thunk_main2` funnels normal return and all
exceptions into sound/resource/network teardown. RPC shutdown must happen in
that funnel before dependent native state dies, not only on normal screen exit:

1. Revoke authorization, close admission and discard unpublished live-view work.
2. Cancel unstarted requests; no game-thread callback can run after unregister.
3. Signal service shutdown and wake its event loop. Do not wait for a game poll.
4. Drain/cancel bounded socket work, destroy backend contexts on their owner
   thread, complete asynchronous close callbacks and final context destruction.
5. Join service thread; only then destroy shared publications/bridge resources
   and proceed with existing native resource/network teardown. Make stop idempotent.

The native backend probe demonstrates this candidate backend's foreign-libuv
context needs a final destroy call after close callbacks; merely polling until
`pcontext` becomes null is insufficient at the selected pin. This does not prove
shutdown with live sockets, slow readers or outstanding game work (T-12/T-13).
