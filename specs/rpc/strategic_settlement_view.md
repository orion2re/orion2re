# Strategic settlement awareness

The existing colony list/get methods and `colonies` state/subscription topic
combine committed owned operations with authorized non-owned colonies and
outposts. Non-owned does not mean hostile. This is a strategic observation view,
not a foreign operational dossier, historical intelligence database or action API.

## Inclusion and field authority

Owned full colonies retain [their completed operations](colony_view.md).
Other ambient settlement observations follow SYS's visited/native-omniscient,
non-black-hole system presentation. Contact, an ownership label, a fleet route,
or an outpost ship alone does not grant a settlement record. Actual initialized
strategic presentations can disclose a narrower contextual observation outside
the ambient collection. Destroyed/inactive records are not settlements.

| Field | Meaning |
| --- | --- |
| `kind`, `owned` | Separate colony/outpost variant and relationship to the viewer; `unclassified` when the actual presentation establishes occupancy but not the native variant |
| `systemId`, `orbit`, `planetId` | Same-capture location; physical orbit 1..5, not name ordinal. Planet reference is unknown without an independently authorized planet projection |
| `ownerLabel`, `owner` | Native displayed race label versus independently resolvable player handle. A label never expands the roster; ambiguous banner identities stay unresolved |
| `population` | Full ordinary colony's resident whole-unit `n_pops`, 0..42, only when its initialized native format actually discloses that count. Not fractional growth, species, jobs, inbound population or a population limit. Unknown for outposts |
| `environment` | Native size/effective-climate line, with its 80-byte CP850 buffer before UTF-8 conversion. Distinct from the planet projection's base climate |
| `defenses` | Native SYS defense labels, separately authorized by a viewer full colony, local active owned ship, or native omniscience trait. Missing authority/resources is unknown, not an empty list |
| `presentation` | Non-owned full-colony scoped strategic text, identified by source. Offscreen or suppressed is context-required, not last-known; owned operations and outpost/unclassified summaries do not acquire this schema |

Defense order is Star Fortress, Star Base, Battlestation, Missile Base, Ground
Batteries, Stellar Converter, Fighter Garrison. Preserve native names and all
present candidates: SYS does not apply the owned operations satellite-redundancy
filter. This is not an inventory of ordinary buildings or a measure of military
effectiveness. Local ship presence is a shared pure predicate, not a query-time
icon rebuild or a settlement inclusion grant.

Outposts never inherit workforce, population, queues, production, garrison or
full-colony operations. Nonplanet presentations do not gain ordinary-planet
detail. Zero disclosed resident population is a count, not destruction or known
absence. A known-empty defense list requires its independent disclosure gate.
An explored planet without a settlement has a known-empty `colony` link only
when that capture has complete settlement coverage; missing coverage is unknown.
Planetary specials and monster labels alone do not create settlement records.
Unexplored colony-screen rows can establish an occupied location through their
actually substituted owner header without classifying colony versus outpost.
These context-only records are `unclassified`: no SYS population, environment,
defense, garrison or operational schema is imported from host storage. They expire
with their producer. Labels and references still have independent authorization.
A generic description's race header alone leaves its owner reference unknown;
owner-colored summary rows use the same-capture unambiguous-banner rule.

Literal, unrecognized or clipped-away population substitutions are unknown, not
raw resident-count grants. The simple native integer/string header forms are
audited explicitly; unfamiliar format extensions fail closed. Native prose still
preserves its actual formatter output for independently classified full colonies.

## Contextual strategic presentations

For non-owned full colonies, `presentation` preserves actual native text and its
native formatter buffer. Owned full colonies retain their completed operations
rather than acquiring another contextual operational schema; outpost summaries
remain unavailable.

- `system-detail`: SYS's population/maximum line (80 CP850 bytes).
- `planets-summary`: the displayed native summary maximum-population label.
- `colony-system-summary`: the colony screen's system-row summary (500 bytes).
- `colony-summary`: the Colony/Colonies description or matching initialized help
  text (2,000 bytes), including native cached per-worker presentations where drawn.

RPC never invokes a foreign population-limit calculator or the description
builder. Population-limit results are copied only from actual native producers;
cached per-worker text is not a newly derived operating total, economic simulator
or effectiveness model. Formatting calls without initialized owner/frame evidence
grant nothing. Labels, precision, units, suppression and truncation remain native.
No species, technology, design or full-player record is inferred from prose.

The native main-map system and fleet boxes replace one another. Empty settlement
frames do not replace a freshly presented fleet capture. Unchanged owned records
retain their references when a foreign settlement gains contextual detail.

Exit, interruption, empty/suppressed frames, replacement and resource/storage
loss revoke contextual output before cleanup. Parent return requires fresh native
presentation. Colony staging and the original-player guard remain mandatory;
the temporary owner-as-player override grants no opponent private fields.
Resolution cannot publish a fresh settlement presentation or announce a turn.

## Coverage, coherence and lifetime

Every stamp has independent `settlementCoverage`:

- `complete`: the authorized owned plus strategic settlement collection is
  covered, including a genuinely empty collection.
- `owned-only`: explicitly partial legacy/owned capture coverage; no claim that
  the foreign observation collection is empty.
- `unavailable`: no settlement coverage in that capture; settlement queries need
  another eligible coherent capture or return busy.

`fleetCoverage` retains its existing meaning. All selected topics, references and
nested stamps come from one immutable capture. A retained complete fleet baseline
is never enriched with newer settlement observations or owned operations.
Within that fleet-coverage constraint, settlement requests prefer an authorized
current presentation capture covering all selected topics, rather than replacing
its disclosed text with a later ambient-only observation. Other topic-only reads
can select a newer capture; their stamps must not be combined.
`native-settlement-presentation` identifies fresh scoped presentation captures.
Pages pin their original capture; clients must not merge pages across stamps.

Observation handles are not durable identities, native slots or action tokens.
Replacement publications identify only callback-local affected locations, allowing
revocation of observed settlements without disclosing hidden replacements belonging
to the same empire. Dependent handles, links, pages and bridge/service-owned output
expire before mutation. Reduced independent projections receive new stamps.
Started revoked output closes; socket-owned bytes cannot be recalled.
Independent ambient topics remain readable.
Foreign-only replacement preserves independently authorized owned configurations;
expired construction-colony links become unknown under fresh observation handles.
Existing contact/exploration grant, world/reset and perspective revocation rules still apply; fresh authentication may
be required. Retained DTOs never manufacture historical settlement intelligence.

## Client workflow and evidence

Authenticate, discover the existing enabled methods, then subscribe once to
session, players, systems, planets, colonies, ships, fleets, designs, research,
leaders, relations, intelligence, history and messages. Inspect both coverage
fields, resolve only references present in that capture, and reread the same topic
selection after `state.changed`. Comparisons, alerts and recommendations stay in
the client. After authority loss or reset, discard stale handles and establish a
new baseline; a location or label is not permission to recover a withheld record.

Native-function fixtures in `tests/support/rpc_native_colonies.cpp` and
`game_rpc_native_test.cpp` feed the normal native capture adapters. The
`test_native_*settlement*` WebSocket workflows cover the mixed dashboard,
noninterference, contextual revocation/restoration, coverage, references, pages,
reuse and backpressured output. Evidence is synthetic native-function/live
transport, not interactive saved-game, multiplayer, real language/mod assets or
platform-parity certification. Broader Q-06 and P1 remain partial.
