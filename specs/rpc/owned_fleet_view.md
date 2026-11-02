# Owned fleet, ship and committed-design inspection

The enabled list/get ship, fleet and design families form one read-only workflow.
Comparison, warnings and recommendations belong in the client. Movement/design
options, foreign scans, tactical inspection and gameplay actions are not enabled.
Q-07, Q-08 and Q-10 remain partial; these handles are not durable identities.

## Collections and references

Ambient `owned-inspection` captures include the bound player's active ships
(native status 0..2), their fleet groups and committed configurations. They do
not require native navigation. Fleet membership shares native owner/location/
position grouping and member type/size/picture order; current combat/support
filters, selection and scroll are not offscreen authority. Fleet collections are
publicly sorted by position and authorized membership, not by rebuilt stack IDs.

`Ship.name` is always the canonical ship name. `inspection.type` is a separate
native hull/type label. A captured ship retains its actual configuration and the
native previous-owner-dependent structure calculation; this grants no foreign
player record or durable captured-ship identity.

`Design.source` distinguishes:

| Source | Native meaning |
| --- | --- |
| `slot` | Stored committed player design slot, not the designer's staged copy |
| `ship` | An active owned ship's current configuration, linked by `Ship.inspection.configuration` |
| `queued` | Independently inspected committed ship record in an owned full colony's queue |
| `refit` | Independently inspected committed refit configuration in that queue |

Names or equal equipment never imply that a ship uses a particular current slot.
Special/noncombat configurations do not become player design slots. Pending
refits, staged designer changes, generated candidates and queue-created records
not yet committed are excluded.

Queued/refit configurations use `Design`, not phantom map-positioned `Ship`
records. Their native production detail discloses stored equipment/cost, but not
an active condition/crew dossier. Hull labels, arcs, postfix modifications,
current weapon counts and damage remain unknown where that presentation does not
disclose them. `constructionColony` links only to an independently authorized
colony in the same capture. A colony queue label alone never creates a record.

All ship/design/location/colony references resolve only through that capture's
authorized projections. A system handle does not imply a known system name.
Pinned pages remain explicitly dated; get methods resolve current observation
handles, so an older page's handle may no longer resolve after a public change.

## Condition and presentation precision

Condition values use the shared native display producers, not effectiveness or
combat-outcome formulas. Structure/armor and subsystem pairs are integer hit
points. Native signed exceptional structure values and armor conversions are
preserved; no repair or damage clamping is invented. A damaged shield's current
capacity retains native upward rounding; the separately displayed theoretical
maximum retains its wider integer precision.

Combat speed and drive speed use their native displayed units. Damage scaling,
trans-dimensional bonuses, disabled/absent drive behavior and special-device
damage follow native presentation. Beam attack/defense are signed whole ratings;
missile evasion is a whole percentage, not a calculated combat probability.
Crew experience is EP for combat ships; noncombat EP is unknown. Combat crew text
uses the native 120-byte Fleets quality/EP presentation; clipped-away EP is unknown,
not an independently disclosed scalar. Noncombat detail uses its native KENTEXT
quality presentation with the same parenthetical suppression, not a substituted
Fleets label.

Noncombat and strategic-combat layouts suppress subsystem hit ratios and the
separate shield maximum. Their native displayed zero speeds and strategic 0/0
armor are preserved; those zeros do not mean unknown equipment or a zero ETA.
Undefined zero-maximum shield inputs are unavailable, not fabricated zero values.

Equipment uses native localized component names, production/refit weapon labels
and the native 128-byte arc/modification formatters where authorized. Type-zero
weapon slots are absent; nonzero type with zero count remains a displayed slot.
Active noncombat detail suppresses weapons independently of stored bytes. Active
special-system inclusion respects the native regeneration presentation switch.
Cost is stored **production points**, not BC or a newly calculated build/purchase
price; refit records store their refit-specific cost. Cargo remains unknown: no
manifest, troop load or logistics amount is inferred from ownership/type.

Current automatic strategic equipment is a pure shared projection used by the
native updater as well as inspection. The updater still mutates only at its
original native call sites. Reads never call that updater, initialize a ship,
calculate a staged design or generate a candidate. Arc and noncombat crew labels
initialize/reload through native strings and clear on resource unload; capture
never loads resources or pumps UI.

## Coherence and lifetime

Use one state/subscription baseline for fleets, ships, designs and empire topics.
All selected topics and nested stamps belong to one immutable capture. A later
map-only or presentation-only capture never enriches an older combined baseline.
Ambient owned captures do not retain foreign observations; surviving foreign
reads remain limited to the existing native refresh/context paths in
[ship_refresh.md](ship_refresh.md). No persistent foreign dossier is created.

Route and ETA authority remains independent of equipment inspection. Lower-route
cached ETA belongs only to the native ordered head, including unsigned-byte
values 0 and 255. Upper-route ETA still requires the approved actual fleet-box
presentation. No ETA calculation or special-destination handle is manufactured.

Ship/fleet/design handles are read observations. Equal public collections may
retain them; changed collections replace them. Native generations are private
retirement/reuse checks, not public identity or ordering. Retirement, template
replacement, ownership/storage loss, reset and perspective changes revoke
dependent handles, links, pages and leased output. Started revoked output closes.
Independently authorized ambient topics remain readable from coherent retained
captures; missing inspection storage is busy, not an empty fleet/design list.

Existing colony queue/population staging and original-player guards apply to
every fresh inspection capture. Settled map/colony/Fleets points and completed
native commits publish coherent data; queries never commit edits. Resolution is
busy until coherent publication, not a new turn merely because a counter changed.
Contextual route leases still end on owner exit/interruption/replacement and need
fresh native presentation on return. Ambient owned detail is independent of them.

Native-function fixtures and the `test_native_owned_*` WebSocket workflows provide
synthetic producer/transport evidence. They do not certify interactive saves,
multiplayer, real mod/language assets or other-platform parity. Reproduction gates
are in [README verification](README.md#verification).
