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
- **Existence is not eligibility.** A retired hull can still exist by key for
  historical reports but cannot complete a construction or refit order. Check
  status and required links at import and again before acting, using the same
  eligibility rule for legacy slots and keyed references.
- **Distinguish special cases from missing data.** Antares is a real destination
  outside the star roster. Leader zero is real even where a legacy packet also
  uses a zero byte for absence. Test each variant at the consumer, including
  fallback branches such as a tagged map icon with no display name.

## State ownership and lifecycle

- **A state-taking API must not mix worlds.** Follow the supplied world into
  pricing, queue refunds, tech grants, and recalculation, not just the top-level
  mutation. Singleton access is fine for explicitly live-only operations.
  Check overloads that silently supply the singleton. Test with the same keys
  in two worlds containing different records; verify both sides of a relationship
  changed in the supplied world and the live world stayed unchanged.
- **Use the record already resolved for the operation.** A downstream helper
  should price the supplied hull or queued ship, not look it up again in the
  live world. Test the exact price with a different live record and with the
  live record or editor absent. For affordability, use a budget between the
  two prices and assert the resulting decision.
- **Derived gameplay state is not a session effect.** A tech grant is incomplete
  if it sets the research flag but leaves range, ship designs, or colony totals
  stale. Keep UI and live-session hooks separate from gameplay recalculation.
  Assert the resulting values in the supplied world, not just the input flag.
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
- **Resolve legacy references once at import.** Bind a valid historical slot
  against the roster being loaded. If it cannot be resolved, cancel or expire
  the active operation, reject the input, or keep history explicitly unavailable,
  as appropriate. Test load → slot reuse → save → reload → act or render.
  Verify that a valid notice can still name the original hull and an unavailable
  notice never names the replacement.
- **Cancel the order, not just the reference.** When import removes an invalid
  order, apply the normal cancellation rules to its linked state. For a paid
  queue head, refund once, clear the payment, preserve earned production and
  queue order, and test that the next item progresses on the first turn.
- **Edit tagged values through typed controls or validated temporaries.** A
  scalar widget must not write directly over a tag and payload. Construct the
  intended variant only after a valid edit. Test setting, clearing, rejecting
  invalid input without altering the old value, and save/reload.
- **Account for auxiliary state beyond record counts.** Audit disk, full
  snapshots, and rollback separately. For each omitted field, name the inputs
  used to reconstruct it and when that happens. A ship reservation beyond the
  live ship extent does not persist merely because the ship block round-trips.
  Check each supported load format, including any fixed legacy block. Assert
  the exact reserved owner and exercise the first allocation before the normal
  turn refresh can mask the omission. Reconstruction must not grow the live
  table or overwrite the saved prefix; count only actual eligible reservations
  in that prefix toward the quota.
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
  GCC or MSVC overload resolution, or a configuration that enables debug editor
  code. Report which configurations were exercised and which were not.
- **Test the call, not the address of an overload set.** `decltype(&T::Store)`
  can fail to resolve a constrained overload set before `is_invocable` checks
  the arguments. To check the public call contract, use a dependent `requires`
  expression and assert both that the intended call works and that the wrong
  ID type is rejected. Use an explicitly typed pointer only when testing a
  particular overload's signature.

Record each finding as **trigger → incorrect behavior → violated contract →
regression test**. Distinguish a reproduced failure from a verified code trace
or a hypothesis, and keep rules here only when they apply beyond one fix.
