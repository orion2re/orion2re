# Manual: Ship refitting

Refitting an existing ship costs production according to the old design, the new design, and the hull used by the new design. Making a small change still has a minimum cost, while replacing equipment with a more expensive design increases the cost.

In a Pre-warp game, refitting becomes available with ship construction after Chemistry and Nuclear Fission; Cold Fusion is not required.

## Standard refit cost

First calculate the signed difference between the two complete design costs:

```text
difference = new design cost - old design cost
minimum cost = new hull's standard cost / 4, rounded down
refit cost = greater of (2 x difference) or minimum cost
```

The difference is not made positive. Refitting to a cheaper design therefore costs the minimum rather than twice the size of the reduction.

Feudal and Confederation governments apply their normal ship-cost reduction to the minimum portion only. Other reductions already included in the displayed design costs affect the difference through those costs.

## Alternative refit cost

Some mods enable the alternative formula:

```text
difference = new design cost - old design cost
refit cost = greater of difference or 0, plus minimum cost
```

This charges the full minimum in addition to any increase in design cost. The standard formula is the Orion2 Redone default; a mod can change it for that game.