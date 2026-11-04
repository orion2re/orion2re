# Manual: Ships and fleets

## Range and contact

Ship travel range and the range used to establish contact round distance consistently. A fleet that can reach an enemy colony can establish contact there.

The default scanner ranges are 2 parsecs for Space Scanner, 4 for Tachyon Scanner, 6 for Neutron Scanner, and 9 for Sensors. The matching communication ranges are none, 3, 6, and effectively unlimited. Movement direction near the boundary can affect the displayed scanner distance.

## Navigators

A Navigator's speed and black-hole travel effects apply only to the fleet carrying that leader. Moving, assigning, levelling, or separating a Navigator updates fleet travel immediately. If an order would slow a fleet or leave it unable to survive a black hole, the game warns before committing the change.

## Phase Shifter strategic speed

By default, Phase Shifters do not change strategic travel speed. Mods can set `movement phase_shifter_speed_multiplier = 2/1;` to double the base speed of equipped ships, or use `1/2` to halve it. The default is `1/1`. Combat speed is unaffected.

A departing fleet uses the slowest adjusted ship speed, then adds Navigator and Jump Gate bonuses. Positive ratios round down, with a minimum speed of one for ships that can already move. Modified travel speed is capped at 254; wormholes and Star Gates retain their normal travel time. Player fleets already in flight retain their stored departure speed.

## Automatic ship design

Automatic designs use the same core calculations as manual designs. Empty space affects combat speed; Megafluxers, shields, weapon modifications, arcs, Battle Pods, Extended Fuel Tanks, and the individual sizes of fighter-bay types all consume or provide their configured space. A heavy beam is used only when that weapon supports the heavy modification.

New Warlord ships receive their crew-level bonus immediately. A ship without a computer receives no computer subsystem hit points, and a destroyed stealth device no longer contributes strategic-map stealth.

## Fleet maintenance and landing ships

If debt forces automatic scrapping, only enough ships are removed to cover the deficit. When an en-route ship carrying a leader is scrapped, the game first attempts to move that leader to an unoccupied ship in the same fleet; otherwise the leader returns to the officer pool.

Landing a Colony Ship or Outpost Ship updates command-point use and maintenance immediately. When both ship types are present, each can still perform its valid colony or outpost action. Queued landing ships use the best armor currently available.

A Dimensional Portal can be used only by its owner. Each player can have at most one portal, including queued construction, in a star system.