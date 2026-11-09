# Manual: Critical bug fixes

Orion2 Redone includes fixes backported from the 1.50 patch. This page lists fixes that have been verified in the restored project; it is not the complete historical 1.50 fix list.

## General

### “Mark Block Stack Size Exceeded” Crash

The game no longer crashes with this message after repeatedly entering and exiting Race Report in the Races screen. The original problem was caused by allocated memory accumulating each time the report was opened, so it was more likely to occur in long games.

### Star System Pop-up Freeze

The game no longer freezes while preparing the rotating planet animation for a star-system pop-up.

### AI Food Balancing Freeze

The game no longer freezes when an AI without Biomorphic Fungi technology balances population for food on an affected liberated and blockaded non-food planet whose previous owner had that technology.

## Tactical Combat

### Map Edge Retreat Crash

The game no longer crashes if the attacker retreats a frigate from the left edge of the battle map.

### Fighters Resolve on Top-left Corner Crash

The game no longer crashes during the affected fighter attack animation near the top-left corner of the target ship.

### Raid Report Crash

The game no longer crashes when a raid report is too long.

### “Crystal Crash”

The game no longer crashes when combat needs the hit points of a missile or fighter launched by a ship that was subsequently captured, including the affected case where the new owner is the Crystal.

## Networked Multiplayer

### Techfield Desync Bug

Multiplayer combat no longer desynchronizes when a player gets a new combat-related technology on the turn of battle. This may follow a research breakthrough, technology theft, or technology captured during another battle on the same turn.

### Evolutionary Mutation Desync Bug

Multiplayer combat no longer desynchronizes when Evolutionary Mutation is applied on the turn of battle.

### “Monster Bug”

Battles involving identical monsters fighting different players on the same turn resolve independently. One battle no longer rolls back the outcome of another by restoring the monster or lost ships.

### Shuttle Capture when Rotating Desync Bug

Network games no longer desynchronize when a ship rotating in place is captured by telepathic Assault Shuttles.

### Dismissed Leaders Desync Bug

Dismissed leaders receive their level change consistently in network games.

### Artemis System Net Desync Bug

Network games no longer desynchronize in tactical combat because attacking ships passing an Artemis System Net receive different damage results for different players.

### Large Fleet Tactical Combat Network Freeze

Network games can start tactical combat when one or both sides bring more than 99 ships without freezing.