# Manual: Ship limits

The command-point total normally limits how large a fleet you can support, but separate limits control how many ships can enter one tactical battle and how many ship records can exist across the game.

## Command-point usage

Under the default rules, each active combat ship consumes command points according to its hull size:

| Hull | Frigate | Destroyer | Cruiser | Battleship | Titan | Doom Star |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Command points | 1 | 2 | 3 | 4 | 5 | 6 |

Mods can change these values. Colony ships, outpost ships, and transports use their configured effective hull size when command use is calculated.

## Ship-design limits

A ship design can contain up to eight weapon slots and eight special-system slots. An installed weapon slot can contain from 1 to 99 weapons, although available hull space normally imposes a lower practical limit.

Standard marine capacity depends on hull size:

| Hull | Frigate | Destroyer | Cruiser | Battleship | Titan | Doom Star |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Marines | 5 | 8 | 12 | 20 | 30 | 50 |

Operational Troop Pods double this capacity. These design limits remain the same as version 1.50.

## Tactical combat limit

By default, each side can bring up to 100 combat ships into a tactical battle, matching version 1.50. A defending orbital platform and planet are loaded separately and do not count against this fleet limit.

`combat max_ships_per_side` configures the fleet limit from 1 through 500. When a fleet exceeds the configured limit, the game loads larger hulls first. The remaining smaller ships stay out of that battle. Non-combat support ships do not enter tactical combat. The defensive Antaran fleet uses the same admission cap, excluding its fortress.

Battles with more than 100 ships on either side expand the battlefield vertically in proportion to the larger admitted fleet: 500 versus 500 uses an 81 x 340 grid instead of 81 x 68. Horizontal fleet separation and the 32 x 18 viewport remain unchanged. Deployment lanes, scrolling, legal movement, and the tactical overview use the expanded dimensions. Smaller battles retain the classic grid even when the configured ship limit is higher.

## Tactical flyer limits

Missiles, torpedoes, fighters, bombers, and assault shuttles share a pool of active flyer groups. Each group can contain multiple projectiles or craft; these caps count groups, not their quantities. Planetary and orbital-defense launches count toward their owner's quota.

- `combat max_flyers_per_side`: defaults to 300, configurable from 1 through 2,500.
- `combat max_flyers_total`: defaults to 300 shared by both sides, configurable from 1 through 5,000.

Both caps apply independently. The defaults preserve the classic shared 300-group limit without adding a per-side restriction. Destroyed or merged groups free slots for later launches. As in classic combat, a launch with no available slot is dropped; it is not deferred.

For expanded battles, configure all three limits:

```text
combat max_ships_per_side = 500;
combat max_flyers_per_side = 2500;
combat max_flyers_total = 5000;
```

These gameplay settings are saved and synchronized with the game's configuration. They are independent of the empire ship limit below.

## Empire ship limit

Orion2 Redone expands the game-wide ship roster from version 1.50's limit of 500 to 10,000 entries. Hull size does not matter: a Frigate and a Doom Star each occupy one entry.

The following count toward the limit:

- Player combat ships in active duty, or in the build queue either as refit or new to-build ships;

- Player support ships (colony ships, outpost ships and transports) in active duty, or in the build queue as new to-build ships;

- The Guardian;
- The Avenger ship, but only after the Guardian has been defeated;
- System monsters;
- Invading space monsters, but only after they have entered the galaxy;
- The offensive Antaran fleet (these ships already ‘exist’ in the roster before they invade the galaxy).

Not counting towards the limit:

- Star bases and planet defenses;
- Freighter fleets;
- The defensive Antaran fleet;

The broader Orion2 Redone roster prevents the original 500-ship ceiling from constraining large galaxies while preserving the default command-point and tactical-combat rules.
