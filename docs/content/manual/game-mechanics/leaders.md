# Manual: Leaders

## Receiving leader offers

Leader offers become more likely as turns pass without an offer. Keeping more colony leaders and ship officers reduces the chance of receiving another offer, while Charismatic and Famous improve it. Repulsive reduces it.

By default, no leader can offer to join during the first five turns. After that, the game calculates offer-chance points as follows:

1. Start with the number of turns since the last offer, plus one. Before the first offer, use the number of turns since the beginning of the game, plus one.
2. By default, add 5 for Charismatic or subtract 10 for Repulsive.
3. For each owned Famous leader who is active or available for assignment, add the leader's level plus one. Improved Famous instead adds one and a half times that amount, rounded down.
4. Divide the result by the total number of colony leaders and ship officers in your service or for hire, plus one.

If the final value is positive, the game rolls from 1 through 100 and makes an offer when the roll is no greater than that value. Values of 100 or more therefore guarantee an offer.

Rejecting a new offer leaves the leader available for hire from the Leaders screen for 30 turns. After 30 turns, the leader returns to the (invisible) candidate pool and gains a level; the leader may or may not return again later. You can also dismiss a leader sooner with the same effect. Dismissing a leader (in service or for hire) reduces the divisor for later rolls, improving the chance of another offer.

By default, the offer restarts the wait for another offer. Set `leaders rejected_offer_keeps_timer = true;` to preserve the elapsed wait when declining instead. Accepting a leader into the officer pool still restarts the wait. The setting does not change offers to AI players or dismissals from the Leaders screen.

## Which leaders can appear

With the default settings, the game selects candidates from a pool that expands as the game continues:

```text
candidate pool = players + 10 + floor(turns elapsed / 5)
```

Charismatic adds 10 candidates. Repulsive instead halves the pool, rounding down. The result cannot exceed the 67-entry leader table; two special entries are excluded from ordinary random offers.

Only leaders within the current pool who are still available and whose type has room can be selected. By default, an empire can have up to four colony leaders and four ship officers. If one type is full, new offers can only select the other type.

The initial delay, offer modifiers, and candidate-pool values can be changed by configuration. See the [Leaders and officers settings](../../generated/configuration/leaders-and-officers.md) for current choices and defaults.

## Hiring cost

A leader's hiring cost depends on the total value of the leader's skills and the leader's level:

```text
hiring cost = skill value x (level + 1) x 10
```

Megawealth doubles this cost under the default rules. If you already employ leaders with Famous or Improved Famous, the largest applicable Famous bonus is subtracted from the offer. A result below 1 costs no BC.

Technologies brought by a leader are included in the skill value, so a leader offering an expensive technology can cost more than another leader with otherwise similar skills. Base tech value is 10 for technologies worth less than 1000 Research Points (RP), 30 for technologies from 1000 to 5000 RP, and 40 from 5000 RP upwards.

## Experience

Leaders in service, who are employed or waiting for assignment, gain 1 experience point per turn.

After a tactical victory, surviving ship officers gain additional experience based on the total hull sizes lost by the opposing side. Each lost Frigate through Doom Star contributes 1 through 6 points respectively. The total is halved and rounded down, with a minimum award of 1 experience point.

The experience needed for each level is configurable. The built-in thresholds are listed in the [Leaders and officers settings](../../generated/configuration/leaders-and-officers.md).
