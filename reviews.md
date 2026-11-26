# Code review lessons

Use this checklist to review a change across its callers, stored state, and
format boundaries. Track unfinished migrations separately in [cleanup.md](cleanup.md).

## Identity and lookup

- **Trace the value from selection to use.** A typed ID at the chooser is not
  enough if a pending offer or AI plan stores a slot. Keep the key across turns,
  prompts, or sync. At use, verify that the original entity still exists and
  that the required relationship still holds. Test a reorder, a deletion, and
  replacement in the same slot. A replacement with the same owner is still the
  wrong target for a pending system gift.
- **Check the domain, not just the bounds.** A slot can be in range but vacant;
  a colony index says nothing about its owner; a population group is not always
  a player. Test with different numeric values for the index, key, and owner so
  that accidentally comparing them cannot pass.
- **Distinguish special cases from missing data.** Antares is a real destination
  outside the star roster. Leader zero is real even where a legacy packet also
  uses a zero byte for absence. Test each variant at the consumer, including
  fallback branches such as a tagged map icon with no display name.

## State ownership and lifecycle

- **A state-taking API must not mix worlds.** Follow the supplied world into
  pricing, queue refunds, tech grants, and recalculation, not just the top-level
  mutation. Singleton access is fine for explicitly live-only operations.
  Separate session effects from world mutations. Test with two different worlds
  and assert that the correct one changed and the other did not.
- **Test the transitions, not just ready-to-play state.** Empty menu state, a
  staged draft, a loaded world, and a rollback checkpoint can have different
  invariants. Finish draft-only work (such as revealing the starting area)
  before moving the draft. Initialize typed fields through their defaults rather
  than zeroing memory. Test loading over an empty world and rolling back a
  failed load without assuming a leader catalog is already installed.

## Compatibility and persistence

- **Compare what the producer accepts with what the reader restores.** Check
  signedness, width, sentinels, and actual value ranges, including editor and
  debug paths. For example, `previous_owner = -1` became `255` through an
  unsigned byte codec. For a projected slot, use the roster that belongs to the
  data being decoded; a full snapshot may place references before the roster.
  Test sparse keys, valid extrema, and absence through the actual codecs.
- **Account for auxiliary state beyond record counts.** Audit disk, full
  snapshots, and rollback separately. For each omitted field, name the inputs
  used to reconstruct it and when that happens. A ship reservation beyond the
  live ship extent does not persist merely because the ship block round-trips.
  Test that the first consumer after load sees usable reservations, without
  growing the live table or overwriting the saved prefix.
- **Keep compatibility tied to an explicit contract.** Runtime representation,
  save version, and multiplayer handshake are separate. Check the exact agreed
  token and layout, incompatible-peer rejection, and actual historical save
  imports. Don't add a new format marker just to distinguish interim builds.

## Behavior and verification

- **Preserve traversal and failure behavior deliberately.** Check sort ties,
  first matches, RNG order, and side effects on failure. Test ties when roster
  order differs from key order. Check an actual legacy failure case: the AI
  historically spent a colony ship after an attempt without a reservation,
  even though no colony was founded. A new early return changed that behavior.
- **Use focused tests alongside replay.** A passing turn oracle covers the
  histories it ran, not detached worlds, editors, or every load path. Rebuild
  the actual game executable before replaying unchanged baselines. If there is
  a mismatch, locate the field in the normalized stream before attributing it
  to gameplay. Only normalize a proven equivalence, and test that unrelated
  differences still fail.
- **Validate the affected build configurations.** A macOS pass does not check
  MSVC overload resolution or a configuration that enables debug editor code.
  Report which configurations were exercised and which were not.

Record each finding as **trigger → incorrect behavior → violated contract →
regression test**. Distinguish a reproduced failure from a verified code trace
or a hypothesis, and keep rules here only when they apply beyond one fix.
