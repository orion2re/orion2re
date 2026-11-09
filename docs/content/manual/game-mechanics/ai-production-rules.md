# Manual: AI production rules

Orion2 Redone exposes the rules used to choose production for computer-controlled empires. These settings are intended for mods that want to adjust AI priorities without replacing the production code.

## Two production systems

`ai autobuild_rules` controls the colony-level selector. It chooses among buildings, Housing, and Trade Goods for AI colonies. The same rules are used by the legacy human Autobuild option.

`ai strategic_production` controls AI-only empire policy. It can assign emergency defenses, respond to an economic emergency, build for expansion, or choose among combat ships, freighters, refits, spies, transports, and deferring to the colony-level selector.

These systems are separate from the queue-driven `colony autobuild` option, which assigns a configured build-queue preset to a human colony.

Queue-driven human Autobuild reapplies the selected preset when the player submits the next turn, before turn processing begins. Applying the preset does not itself recalculate colony output. See [Build queue management](../user-facing-changes/build-queue.md#autobuild).

## Normal construction rules still apply

Colony-autobuild candidates and emergency-defense products still pass the normal construction checks for technology, existing buildings and upgrades, colony placement, race and government restrictions, and other build requirements.

Strategic production uses the existing availability checks for each strategic action. Changing a weight does not provide missing technology, create a ship design, or supply a target for expansion. Test strategic changes carefully because not every action uses the same colony-level buildability checks.

## How weighted choices work

The colony selector and general strategic allocation use weighted choices:

1. Remove products that cannot be built.
2. Apply the candidate's configured eligibility conditions.
3. Calculate its weight from the base value, current game facts, and conditional additions or penalties.
4. Apply any limits on individual terms and the final candidate weight.
5. Apply the configured relative cutoff.
6. Select randomly from the remaining candidates according to their weights.

A higher weight makes a candidate more likely to be chosen; it does not guarantee selection. A disabled or ineligible candidate receives no weight. Keep calculated weights nonnegative and within the documented range. Negative or overflowing values may be converted into unexpectedly large selection weights.

### Eligibility

Eligibility can test facts such as whether research is enabled, whether a colony is threatened, or whether food and pollution buildings are useful. It can also compare values such as population, free capacity, food, industry, treasury, command points, and strategic need.

Boolean conditions can be `required`, `forbidden`, or `ignored`. Conditions in one group must all pass. Alternative groups allow any one complete group to pass. `ignored` is useful for removing a condition inherited from another package.

### Weights and limits

A candidate weight can start with a base value and add terms derived from the current colony or empire. A term may apply only under a condition. Limits can prevent one term or the final result from becoming too large or too small.

For example, this makes Automated Factory more desirable without changing its eligibility:

```text
ai autobuild_rules candidates automated_factory weight =
    base: 13;
```

Because Automated Factory uses the default `legacy` formula, this adds `13` to its built-in situational weight; it does not replace that weight with `13`.

This prevents the selector from choosing Research Laboratory:

```text
ai autobuild_rules candidates research_lab =
    enabled: false;
```

This requires at least 20 industry before Research Laboratory is considered:

```text
ai autobuild_rules candidates research_lab eligibility thresholds industry =
    operator: ge
    value: 20;
```

### Relative cutoff

A relative cutoff removes low-weight candidates before the random choice. For colony autobuild, the cutoff multiplier varies by difficulty and has a separate value for human legacy Autobuild. Lower multipliers keep choices closer to the highest current weight; disabling the cutoff skips this relative filtering. Strategic allocation can also use a cutoff, but its current multiplier does not vary by difficulty.

Candidates use one of three cutoff stages:

- `normal` candidates participate in the cutoff calculation and may be removed by it;
- `protected` candidates participate but are not removed; and
- `after` candidates are added after ordinary cutoff filtering.

Housing normally uses the `after` stage so it remains a fallback without lowering the cutoff applied to ordinary buildings.

## Colony-level examples

The unmodified game does not use a fixed building order. Every ordinary building has a configured base weight of `0`, then receives the effective weight shown below from the colony and empire situation. A result of `0` means that the product is not considered. Normal construction requirements still apply before these rules are evaluated.

The tables use the following short terms:

- **Population** is the colony's current population; **capacity** is its maximum population.
- **Treasury** is a bonus from `0` to `10` that becomes available when the empire has at least 1,500 BC and depends on its current income.
- **Threat** combines immediate danger with the presence of enemy, neutral, allied, and out-of-range empires in the system. Military objectives add a small bonus to a nonzero threat score.
- **Building focus** means that the colony is still being steered toward a basic factory or government-required barracks. Optional projects are often held back during this period, but defenses remain available when the colony is threatened.
- An objective bonus is `1` when the AI has the named objective and `0` otherwise. Square roots are rounded down.

### Industry, research, and commerce

| Product | Effective default weight | Main conditions |
| --- | --- | --- |
| Astro University | `5` | None beyond normal construction requirements. |
| Autolab | `11 + 4 x Technologist` | Research must be active; not during building focus. |
| Automated Factory | `population + 13 + 2 x Industrialist` | None beyond normal construction requirements. |
| Capitol | `100` | Home world only; not available to Unification governments. |
| Deep Core Mine | `population + 12 + 4 x Industrialist` | None beyond normal construction requirements. |
| Galactic Cybernet | `11` | Research must be active; not during building focus. |
| Galactic Currency Exchange | `0` | Not selected by colony autobuild in the unmodified game. |
| Planetary Stock Exchange | `(capacity + population + Industrialist) / 3` | Population above 3; not during building focus. |
| Planetary Supercomputer | `8 + 3 x Technologist` | Research must be active; not during building focus. |
| Recyclotron | `(capacity + 2 x population) / 3`, plus ecology bonuses | Adds `2` each when pollution controls apply or the objective is Industrialist or Ecologist. |
| Research Laboratory | `5 + 2 x Technologist` | Research must be active; not during building focus. |
| Robo-Miner Plant | `population + 5 + 2 x Industrialist` | None beyond normal construction requirements. |
| Robotic Factory | `12 + 2 x Industrialist` | None beyond normal construction requirements. |
| Space Academy | `sqrt(industry - 15)` | Industry above 16, population above 4, or a treasury bonus; not during building focus. |
| Spaceport | `(capacity + population + Industrialist) / 3` | Population above 3; not during building focus. |

### Food, population, and environment

| Product | Effective default weight | Main conditions |
| --- | --- | --- |
| Atmosphere Renewer | `Ecologist + sqrt(pollution)` | Pollution controls must apply and pollution must exceed 5; the square-root term starts at 11 pollution. |
| Biospheres | `18 + Ecologist` | Not during building focus. |
| Cloning Center | Up to `Ecologist + treasury / 2` | Not during building focus; the objective bonus applies only to a race with a growth penalty. |
| Core Waste Dumps | `Ecologist + sqrt(pollution)` | Same pollution thresholds as Atmosphere Renewer; not during building focus. |
| Food Replicators | `4 + Ecologist`, or `8 + Ecologist` during a food shortage | Food buildings must be useful to the colony. |
| Gaia Transformation | `treasury + Ecologist` | Receives no weight without either bonus. |
| Holo Simulator | `10` | Population above 2, or above 1 with a treasury bonus; not available to Unification governments. |
| Hydroponic Farm | `6` to `12 + 4 x Ecologist`, increased by the food shortage | Food buildings must be useful; lower food output per farmer produces the higher starting weight. |
| Planetary Gravity Generator | `3 + Ecologist`, or `6 + Ecologist` for a severe mismatch | Only when the planet's gravity does not suit the race. |
| Pleasure Dome | `16` | Population above 2, or above 1 with a treasury bonus; not available to Unification governments. |
| Pollution Processor | `Ecologist + sqrt(pollution)` | Same pollution thresholds as Atmosphere Renewer. |
| Soil Enrichment | `3 + 2 x Ecologist`, or `5 + 2 x Ecologist` during a food shortage | Food buildings must be useful and farmers must produce food; not during building focus. |
| Subterranean Farms | `7` to `13 + 3 x Ecologist`, increased by the food shortage | Food buildings must be useful; lower food output per farmer produces the higher starting weight; not during building focus. |
| Terraforming | `treasury + climate value + 3 x Ecologist` | Climate value is `1` to `6` for a useful transformation; not during building focus. |
| Weather Controller | `5 + 2 x Ecologist`, or `10 + 2 x Ecologist` during a food shortage | Food buildings must be useful and farmers must produce food; not during building focus. |

### Military and defense

| Product | Effective default weight | Main conditions |
| --- | --- | --- |
| Alien Control Center | `0` | Not selected by colony autobuild in the unmodified game. |
| Armor Barracks | Security threat, government, conquest, and half-treasury bonuses | Its threat term requires population above 2 or a treasury bonus. |
| Artemis System Net | Defense threat plus half the treasury bonus | Threat weight is omitted if the system already has a Warp Interdictor; deferred during a safe building focus. |
| Barrier Shield | Defense threat, radiated-world Ecologist bonus, and treasury | Deferred during a safe building focus. |
| Battlestation | Defense threat, command-point need, and treasury | Deferred during a safe building focus. |
| Fighter Garrison | Defense threat and treasury | Deferred during a safe building focus. |
| Flux Shield | Defense threat, radiated-world Ecologist bonus, and treasury | Deferred during a safe building focus. |
| Ground Batteries | Defense threat and treasury | Deferred during a safe building focus. |
| Marine Barracks | Security threat, government, conquest, and treasury bonuses | Its threat term requires population above 1 or a treasury bonus. |
| Missile Base | Defense threat and treasury | Deferred during a safe building focus. |
| Radiation Shield | Defense threat, radiated-world Ecologist bonus, and treasury | Deferred during a safe building focus. |
| Star Base | Defense threat, command-point need, and treasury | Deferred during a safe building focus. |
| Star Fortress | Defense threat, command-point need, and treasury | Deferred during a safe building focus. |
| Stellar Converter | Defense threat and treasury | Deferred during a safe building focus. |
| Warp Interdictor | Defense threat plus half the treasury bonus | Threat weight is omitted if the system already has one; deferred during a safe building focus. |

### Special products and fallbacks

| Product | Effective default weight | Main conditions | Cutoff stage |
| --- | --- | --- | --- |
| Colony Base | `0` | Assigned by strategic expansion instead of colony autobuild. | Normal |
| Dimensional Portal | `0` | Not selected by colony autobuild in the unmodified game. | Normal |
| Artificial Planet | `0` | Not selected by colony autobuild in the unmodified game. | Normal |
| Housing | `sqrt(3 x free population capacity)` | Current population must be no more than one third of capacity. | After |
| Trade Goods | `1` | Available as the ordinary economic fallback. | Normal |

All buildings in the first three tables use the `normal` cutoff stage. Housing uses `after`, so it neither raises the cutoff nor gets removed by it.

The default relative cutoff compares each normal candidate with the highest normal weight. A candidate is removed when its weight multiplied by the value below is still less than that highest weight:

| Difficulty or use | Multiplier |
| --- | ---: |
| Tutor | 6 |
| Easy | 5 |
| Average | 4 |
| Hard | 3 |
| Impossible | 2 |
| Legacy human Autobuild | 2 |

For example, if the highest weight is `18`, Average keeps normal candidates with weight `5` or more, while Impossible keeps candidates with weight `9` or more. Housing remains available after this comparison.

These are the defaults for the unmodified game. A mod can replace the legacy effective-weight calculation with explicit base values and terms. The [AI and diplomacy configuration reference](../../generated/configuration/ai-and-diplomacy.md#ai-autobuild-rules) lists every accepted field and the syntax for overriding these rules.

### Changing default weights

An override only needs to specify the fields it changes. This gives Automated Factory a modest bonus while retaining its built-in calculation:

```text
ai autobuild_rules candidates automated_factory weight =
    base: 5;
```

Its effective weight becomes the table value plus `5`. The default eligibility and cutoff stage remain unchanged.

To replace the built-in calculation, select the `additive` formula and define the new weight. This example gives Automated Factory a base weight of `10`, adds one and a half times the colony population, and adds another `5` for an Industrialist AI:

```text
ai autobuild_rules candidates automated_factory weight =
    formula: additive
    base: 10;
ai autobuild_rules candidates automated_factory weight metric_terms 1 =
    metric: population
    transform: linear
    factor: 3/2;
ai autobuild_rules candidates automated_factory weight value_terms 1 =
    value: 5
    when:
        flags:
            objective_industrialist: required;
```

For a colony with population `6`, the resulting weight is `10 + 9 = 19`, or `24` when the AI has the Industrialist objective. Fractions are rounded down.

Each weight has three numbered metric-term slots and three numbered value-term slots. The slots are numbered `1` through `3`. Changing `formula` or `base` does not erase existing terms, which is useful when several packages build on the same rule. To remove inherited terms explicitly, make each unwanted slot inactive:

```text
ai autobuild_rules candidates automated_factory weight metric_terms 1 =
    metric: none;
ai autobuild_rules candidates automated_factory weight value_terms 1 =
    value: 0;
```

Eligibility can be changed independently of weight. This keeps Research Laboratory out of consideration until the colony produces at least `20` industry:

```text
ai autobuild_rules candidates research_lab eligibility thresholds industry =
    operator: ge
    value: 20;
```

To remove it from colony autobuild entirely:

```text
ai autobuild_rules candidates research_lab =
    enabled: false;
```

### Configuring the relative cutoff

The unmodified game uses this mode:

```text
ai autobuild_rules cutoff =
    mode: relative;
```

For each selection, let `R` be the highest weight among candidates in the `normal` or `protected` stages, `W` be a normal candidate's weight, and `M` be the multiplier for the current difficulty. The candidate is removed when:

```text
M x W < R
```

Equality survives. For example, if the highest weight is `18` on Average difficulty, the default multiplier is `4`. Weights `5` and above survive because `4 x 5` is at least `18`; weights `4` and below are removed. A larger multiplier keeps more alternatives, while a smaller multiplier concentrates production on the highest-weight choices.

The multiplier names are `tutor`, `easy`, `average`, `hard`, `impossible`, and `human`. This makes Average less selective than its default of `4` and makes legacy human Autobuild more selective than its default of `2`:

```text
ai autobuild_rules cutoff multiplier =
    average: 6
    human: 1;
```

The three candidate stages can also be assigned directly:

```text
ai autobuild_rules candidates trade_goods =
    cutoff_stage: protected;
ai autobuild_rules candidates housing =
    cutoff_stage: after;
ai autobuild_rules candidates automated_factory =
    cutoff_stage: normal;
```

| Stage | Contributes to `R` | Can be removed | Typical use |
| --- | --- | --- | --- |
| `normal` | Yes | Yes | Ordinary competing choices. |
| `protected` | Yes | No | A choice that must remain available but should still influence the cutoff. |
| `after` | No | No | A fallback that must not make ordinary choices fail the cutoff. |

Housing uses `after` by default. All other enabled candidates use `normal` in the unmodified game.

To turn relative filtering off without changing any weights or stages:

```text
ai autobuild_rules cutoff =
    mode: disabled;
```

Do not use a multiplier of `0` to disable the cutoff. In relative mode, `0` removes every normal candidate whenever `R` is positive; protected and after-stage candidates remain. If every relevant weight is `0`, the cutoff changes nothing, but there is still no positive weight to select.

## Strategic AI production

Strategic production runs before ordinary colony selection and can reserve a colony for an empire-wide need.

### Emergency defense

Emergency defense is an ordered choice rather than a weighted one. When the policy applies, the AI checks the configured defense list in order and selects the first product that the colony can build. A mod can change the order:

```text
ai strategic_production emergency_defense order =
    1: missile_base
    2: ground_batteries;
```

### Economic emergency

The economic-emergency policy can force Trade Goods when the AI is low on money, has negative income, and the colony produces more than its industry maintenance. The current product choice contains only Trade Goods:

```text
ai strategic_production economic_emergency =
    enabled: true
    product: trade_goods;
```

### Expansion

Expansion policy assigns Colony Bases and Colony Ships when suitable targets and sufficiently productive colonies are available. Its industry thresholds can be adjusted:

```text
ai strategic_production expansion =
    colony_base_minimum_industry: 13
    colony_ship_minimum_industry: 20;
```

### General allocation

If no higher-priority policy takes over, the AI uses a weighted allocation among combat production, freighters, refits, spies, transports, and deferring to colony autobuild. These choices use current strategic needs and can be enabled, disabled, filtered, or reweighted in the same way as colony candidates.

The [AI strategic production reference](../../generated/configuration/ai-and-diplomacy.md#ai-strategic-production) lists the current fields and short examples. As with colony autobuild rules, the full built-in defaults are omitted because they are too large to serve as a useful player reference.

## Saves and multiplayer

AI production settings affect game decisions and random-number use. They are stored with saved games and synchronized for multiplayer. Players continuing the same game should use compatible versions of the mod that supplied these rules.

When experimenting, change a small number of values at a time and test with a copied save. Keep the original package and configuration so the game can be restored if an override causes unwanted AI behavior.