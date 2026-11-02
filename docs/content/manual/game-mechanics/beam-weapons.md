# Manual: Beam weapons

Beam accuracy depends mainly on the firing ship's Beam Attack, the target's Beam Defense, and distance. Better computers and scanners help you hit; faster targets and longer ranges make hits less likely. Mounts and weapon modifications can change both accuracy and damage, so the same weapon can behave quite differently between designs.

## Accuracy

Several things determine the chance to hit, but it boils down to three factors: Beam Attack, Beam Defense and Range Penalty.

### Beam Attack modifiers

Beam Attack (BA) is a way of rating the firing ship’s beam offense capability. It is calculated based on the type of computer on board, ship systems like battle scanner, crew experience, leaders and racial bonuses. Battlestation and Star Fortress give an additional BA bonus of +10 and +20 respectively to all ships in the defensive fleet. Individual beam weapons get additional bonuses based on mount type (PD +25 BA) and mods (Co +25 BA, AF -20 BA).

### Beam Defense modifiers

Beam Defense (BD) is a rating of the target ship's dodging ability, calculated from 5 times its combat speed plus bonuses from ship systems such as an Inertial Stabilizer, crew experience, leaders, and race. A ship with zero current speed uses -20 as its speed contribution instead of 0, making an immobilized ship easier to hit.

### Beam Defense of missiles

Missile Beam Defense is calculated as follows:

> `Missile Beam Defense = 5 * Speed + MissileBonus`

> `Speed = Missile BaseSpeed + 2 * (FTLlevel - 1)`

BaseSpeed is 12 for Nuclear, Merculite, Pulson, and Zeon missiles. The Fast modification adds 4 to tactical movement speed, but it is not included in this defensive speed calculation.

| Drive | `FTLlevel` | Speed | Missile | `MissileBonus` |
| --- | ---: | ---: | --- | ---: |
| None | 0 | 10 | Nuclear | -10 |
| Nuclear | 1 | 12 | Merculite | 15 |
| Fusion | 2 | 14 | Pulson | 40 |
| Ion | 3 | 16 | Zeon | 70 |
| Anti-Matter | 4 | 18 | - | - |
| Hyper | 5 | 20 | - | - |
| Interphased | 6 | 22 | - | - |

### Beam Defense of fighters

Fighter Beam Defense is calculated as follows:

> `Fighter Beam Defense = 5 * Speed + RacialShipDefenseBonus + FighterPilotBonus + HelmsmanBonus`

> `Speed = Fighter BaseSpeed + 2 * (FTLlevel - 1) + TDBonus`

TDBonus is 4 when the fighter belongs to a Trans-Dimensional race and 0 otherwise. The Fast modification adds 4 to a fighter's tactical movement speed, but it is not included in this defensive speed calculation. Fighter Pilot and Helmsman use the best applicable active fleet-leader bonuses.

| Fighter | `BaseSpeed` |
| --- | ---: |
| Interceptor | 10 |
| Assault Shuttle | 6 |
| Bomber | 8 |
| Heavy Fighter | 8 |

### Range penalty to accuracy

The farther away a target is, the less likely it becomes for a beam weapon to hit it. The range penalty to a beam’s accuracy comes from below table:

| Range | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Penalty | 0 | 0 | 10 | 20 | 30 | 40 | 55 | 70 | 85 |

One range unit equals 3 squares. At range 0 two opponents share the same square, for example when interceptors are on top of the target ship. At range 1 opponents are adjacent to each other or have 1-2 squares between them.

For calculation purposes, Point Defense weapons get a penalty as if range is doubled and for Heavy mount weapons the actual range is halved (and rounded down):

| Range | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Regular (sq) | 0 | 1-3 | 4-6 | 7-9 | 10-12 | 13-15 | 16-18 | 19-21 | 22-24 |
| PD (sq) | 0 | - | 1-3 | - | 4-6 | - | 7-9 | - | 10-12 |
| Hv (sq) | 0-3 | 4-9 | 10-15 | 16-21 | 22-27 | 28-33 | 34-39 | 40-45 | 46-51 |
| Penalty | 0 | 0 | 10 | 20 | 30 | 40 | 55 | 70 | 85 |

Classic Fusion Beam, Plasma Cannon, Mauler Device, and Plasma Breath originally had an inherent 2x range-to-hit penalty. This property doubles the calculated penalty after mount effects and the Rangemaster Unit are applied.

Version 1.50 removes the property from Mauler Device and Plasma Breath. Mauler Device always hits except against a Displacement Device, so the removed range penalty would not otherwise affect it. The bundled 1.50 Improved mod also removes the property from Fusion Beam, leaving only Plasma Cannon with it.

### Classic chance-to-hit formula

The game rolls a random number from 1 through 100 and applies these checks in order:

1. A lucky shot hits for maximum damage:

   > `random(100) > 95`

2. Otherwise, overwhelming attack strength hits for maximum damage:

   > `BA + CO - AF - BD >= 99`

3. Otherwise, the normal accuracy check determines whether the beam hits:

   > `random(100) + BA + CO - AF - BD >= hit_threshold`

   > `hit_threshold = min(40 + range_penalty - PD_bonus, 95)`

   The `range_penalty` is doubled for a weapon with the inherent 2x range-to-hit penalty.

In simplified mode, the third check uses the fixed threshold of 40 rather than the range-adjusted `hit_threshold` used by the classic damage calculation below.

Thus, if the random number is greater than 95 (a 5% lucky shot, no matter the odds), or `BA + CO - AF - BD` is larger than 98, the beam strikes the target. Otherwise, `BA + CO - AF - BD` minus the range penalty, plus any PD bonus, must be 39 or higher to ensure a hit.

- Mauler Device and Energy Absorber always hit unless the target ship has a Displacement Device.
- Stellar Converter always hits regardless of the presence of a Displacement Device by default. Setting `combat bonus displacement_device_affects_stellar_converter = true;` lets a working Displacement Device evade each shot using `combat bonus displacement_device_beam_miss_percent` (default 30%). This does not turn Stellar Converter into a beam weapon or apply ordinary beam attack/defense checks.

The value 40 in `hit_threshold` can be changed with `combat hit_threshold_fixed_value = 40;`.

### Optional 1.50 alternative chance-to-hit formula

The chance-to-hit formula contains a major flaw: range penalties, though important in theory, can have little or no effect. Formula [3] caps the threshold at 95, so range penalties above 55 for a non-PD beam or 80 for a PD beam have no additional effect. Formula [2] can also ignore range entirely when BA is high and BD is low. This creates sure-hit-at-any-distance situations, reduces the usefulness of the Rangemaster Unit compared with the Battle Scanner, and weakens the inherent 2x range-to-hit penalty.

Setting `combat simplified_beam_formula = true;` enables a more straightforward way of calculating chance to hit, with distance penalties consistently affecting chance:

1. A lucky shot hits for maximum damage:

   > `random(100) > 95`

2. Otherwise, overwhelming attack strength hits for maximum damage:

   > `BA + CO - AF - BD - range_penalty + PD_bonus >= 99`

3. Otherwise, the normal accuracy check determines whether the beam hits:

   > `BA + CO - AF - BD - range_penalty + PD_bonus + random(100) >= 40`

In both checks, `range_penalty` is doubled for a weapon with the inherent 2x range-to-hit penalty.


## Damage Potential

If a beam hits, it needs to be determined how much damage it does.

There are 2 pairs of beam weapon types, affecting damage distribution:

1. Not reduced by range / Reduced by range.
2. Equal min-max damage / Different min-max damage.

Damage values of both types are later modified with Hv, PD, HEF and Ordnance effects.

### Not reduced by range

For weapons whose damage is not reduced by range, such as Mass Driver, Gauss Cannon, and Disruptor, no dissipation penalty applies. In cases of low accuracy, a Laser Cannon with the NR modification can still deal less than 4 damage.

Example: The lowest damage at range for an NR beam with base damage 10-40 is 10, whereas a regular 10-40 beam can deal as little as 4.

### Reduced by range

For weapons of which the damage is reduced by range (dissipation), a penalty based on the distance the beam must travel to the target is taken into account. This penalty lowers the damage potential. (Round to nearest applies and the minimum damage potential is always 1.) Below table of damage potential by range includes examples for Laser Cannon, Phasor, Mauler and Death Ray:

| Range (sq) | 0 | 1-3 | 4-6 | 7-9 | 10-12 | 13-15 | 16-18 | 19-21 | 22-24 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Penalty | 0 | 0 | -10 | -20 | -30 | -40 | -50 | -60 | -65 |
| Damage | 100% | 100% | 90% | 80% | 70% | 60% | 50% | 40% | 35% |
| Laser | 1-4 | 1-4 | 1-4 | 1-3 | 1-3 | 1-2 | 1-2 | 1-1 | 1-1 |
| Phasor | 5-20 | 5-20 | 5-18 | 4-16 | 4-14 | 3-12 | 3-10 | 2-8 | 2-7 |
| Mauler | 100 | 100 | 90 | 80 | 70 | 60 | 50 | 40 | 35 |
| Death Ray | 50-100 | 50-100 | 45-90 | 40-80 | 35-70 | 30-60 | 25-50 | 20-40 | 18-35 |

### Equal minimum and maximum damage

For Mauler Device, minimum and maximum damage are equal, so actual damage is known after applying dissipation and the Hv, PD, HEF, and Ordnance effects. If the beam hits, the resulting minimum damage is applied. Weapons with different minimum and maximum damage require an additional calculation.

### Different minimum and maximum damage

With the classic beam formula, actual damage for weapons with different minimum and maximum damage is based on the damage potential modified by the accuracy of the shot:

The uncapped damage is:

> `uncapped_damage = min_dmg + (max_dmg - min_dmg + 1) * A / B`

The formula uses these intermediate values:

> `A = roll_plus_attack - hit_threshold`

> `B = 100 - hit_threshold`

> `roll_plus_attack = min(random(100) + BA + CO - AF - BD, 100)`

> `hit_threshold = min(40 + range_penalty - PD_bonus, 95)`
                \* doubled for inherent 2x range to-hit penalty mod

The overall result is capped at `max_dmg`.


### Examples

1. Death Ray damage at 23 squares, assuming `roll = 85` and `BA + CO - AF - BD - range_penalty = 10`:

   > `18 + (35 - 18 + 1) * A / B`

   > `A = (85 + 10) - (40 - -85) = 95 - 95 = 0`

   > `B = 100 - (40 - -85) = 100 - 95 = 5`

   Thus, actual damage equals `min_dmg`, or 18.

2. If the target is closer, at 11 squares:

   > `35 + (70 - 35 + 1) * A / B`

   > `A = (85 + 10) - (40 - -30) = 95 - 70 = 25`

   > `B = 100 - (40 - -30) = 100 - 70 = 30`

   The actual damage is therefore `min_dmg + ⅚ * (max_dmg - min_dmg + 1)`, or 65.


### Hv, PD, HEF, and Ordnance

These bonuses are percentages of damage of a regular mount beam. (In 1.50 the HEF bonus can be set for each mount separately.) Their interaction is not multiplicative but additive and they work the same way as dissipation range penalty, modifying min&max damage, so these bonuses are not subject to dissipation themselves.

Ordnance works best for regular ship beams because their modified damage is rounded to the nearest whole point, while fighter payloads are truncated. A 10% Ordnance bonus therefore raises a 5-damage ship beam to 6 after rounding, while a 5-9 damage fighter beam or missile still gains no whole point at its lower end.

Examples for a 50-100 beam with a dissipation `range_penalty` of 30 follow.

#### Heavy mount

> `50 * (100 {base} + 50 {Hv} - 30 {range_penalty})% = 60`
>
> `100 * (100 {base} + 50 {Hv} - 30 {range_penalty})% = 120`

#### Heavy mount with High Energy Focus

> `50 * (100 {base} + 50 {Hv} + 50 {HEF} - 30 {range_penalty})% = 85`
>
> `100 * (100 {base} + 50 {Hv} + 50 {HEF} - 30 {range_penalty})% = 170`

#### Point Defense mount

> `50 * (100 {base} - 50 {PD} - 30 {range_penalty})% = 10`
>
> `100 * (100 {base} - 50 {PD} - 30 {range_penalty})% = 20`

For a Point Defense beam whose 2x range penalty also applies to dissipation:

> `50 * (100 {base} - 50 {PD} - 2 * 30 {range_penalty})% = -5`
>
> `100 * (100 {base} - 50 {PD} - 2 * 30 {range_penalty})% = -10`

In this case, both actual damage values become the minimum of 1. At maximum distance, every Point Defense beam reduced by range likewise does only 1 damage without High Energy Focus or Ordnance, regardless of its listed maximum damage.

With High Energy Focus added:

> `50 * (100 {base} - 50 {PD} + 50 {HEF} - 2 * 30 {range_penalty})% = 20`
>
> `100 * (100 {base} - 50 {PD} + 50 {HEF} - 2 * 30 {range_penalty})% = 40`
