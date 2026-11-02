# Manual: Special systems

Special systems can change targeting, movement, damage, boarding, and strategic detection. The effects below describe the default rules; where several systems affect the same attack, their order can matter.

## Achilles Targeting Unit and Structural Analyzer

- Affects beams, Stellar Converter and Energy Absorber fire.


## High Energy Focus

- Affects beams and Energy Absorber fire, but not Stellar Converter (which is not considered a beam but a special weapon).


## Displacement Device

- Works against beams, missiles, torpedoes and Energy Absorber fire.
- There is no effect against fighters, bombs or special weapons.
- Optional exception: `combat bonus displacement_device_affects_stellar_converter = true;` applies the configured beam miss chance to Stellar Converter shots, without changing the weapon type. The flag defaults to `false`.


## Energy Absorber

- Reduces damage before damper field or shields apply.
- Damage reduction is rounded down, but a working absorber blocks at least 1 point from a damaging hit.
- Against incoming Emissions Guidance fire, the Energy Absorber reduces the damage before the remaining damage is directed to the drive.


## Energy Absorber Fire

- Energy fired from the absorber always hits, unless the target has a displacement device.
- Damage of fired energy is reduced by range. It behaves like a beam for dissipation and follows the normal reduced by range table.
- Energy Absorber is a 'pre-weapon' special, meaning the ship won't decloak if you fire the stored energy.
- If the Energy Absorber special is destroyed, the stored energy is not lost and can still be fired.
- Firing requires stored energy, a ready absorber, and a ship target within range 8. Stored energy cannot be fired at planets.


## Inertial Nullifier

- Allows the ship to change direction without movement cost. The only caveat is that the ship needs to have at least 1 remaining movement point. If no movement points are left, the ship cannot turn.
- By default, it adds 100 beam defense and 50 missile defense.
- Turning in place does not advance incoming missiles.


## Sub-Space Teleporter

- Teleporter range does not increase with ship speed.
- Teleporting beyond a Tractor Beam's range breaks the beam.
- The normal destination area is square. `combat round_teleporter` changes it to an octagonal area.
- A ship can teleport once per turn. A Phase Shifter uses the same teleport movement.


## Reflection Field

- A Displacement Device can make the original beam miss before reflection is tested. A reflected shot is applied directly to the firing ship, without another hit or Displacement roll.
- Reflected damage is not reduced by range.
- Reflected damage retains natural ESD, ION, and MAR effects. Installed AP, SP, and ENV modifications do not carry over. Natural or installed SP does not bypass shields, and natural or installed ENV does not make reflected damage strike every side.
- Auto Fire fires three separate shots, each with its own hit and reflection test. Reflected portions are accumulated and applied together as one damage application to the firing ship.

---


## Troop Pods

- Working Troop Pods double the ship's marines. They also increase the number of marines that can remain aboard after capturing an enemy ship.
- If this special system is destroyed in combat, the extra marines will not be killed-off. However if the ship is not repaired before next combat starts, the additional marines will be gone.


## Indestructible special systems

The following special systems cannot be destroyed:

- Reinforced Hull
- Heavy Armor
- Extended Fuel Tanks
- Battle pods
- Quantum Detonator


## Miniaturization

The following special systems cannot be miniaturized:

- Reinforced Hull
- Heavy Armor
- Extended Fuel Tanks
- Battle Pods
- Augmented Engines
- Troop Pods

The production cost of these systems will still decrease, except for Battle Pods.


## Non-combat effects

Most of the special systems are combat oriented, but some have non-combat effects:

| System | Non-combat effect |
| --- | --- |
| Extended Fuel Tanks | Adds 50% to a ship's range. |
| Battle Scanner | Increases ship scanning range by 2 parsecs. |
| Scout Lab | Contributes research points. |
| Stealth Field, Cloaking Device, and Phasing Cloak | Add together to make a ship harder to detect with long-range scanners; they do not guarantee that it remains hidden. |
