# Manual: Compatibility

Use this page to check whether your game data, saved games, configuration, mods, and multiplayer setup are compatible with version 2.00.

## Operating environment

Version 2.00 runs natively on Windows, OS X/macOS, and Linux. Windows and Linux builds support 32-bit and 64-bit environments; current native macOS builds target 64-bit ARM systems. Availability of a particular build depends on the distribution being used.

## Game data

The original version 1.31 data from Steam or GOG is supported. A CD installation must first be updated with the official 1.31 patch. Version 2.00 does not include or replace these files.

## Saved games

Version 2.00 can load save games created by the original version 1.31 game.

Save games created by the 1.50 patch are not supported. Do not attempt to continue a 1.50 game in version 2.00.

Save-game versions are backward compatible across the 2.x branch: a newer 2.x release can load saves created by earlier 2.x releases. The reverse is not guaranteed, so an older 2.x release may not be able to load a save created by a newer release.

## Configuration

Version 2.00 uses `data/orion2re.cfg`; in XDG mode on Linux, a copy in your configuration directory takes its place (see [File locations](file-locations.md)). Historical 1.50 configuration files and command-line configuration switches are not a supported replacement for this file. Recreate the settings you need using the [current configuration reference](../../generated/configuration/index.md); do not copy a complete 1.50 configuration and assume that names, values, and defaults are unchanged.

## Mods

Version 2.00 mods use packages installed under `data/mods` as directories or ZIP files. A current package identifies its version, engine requirements, dependencies, and conflicts. Historical 1.50 mods are not automatically compatible with this system. The bundled `v150` package enables restored 1.50 behavior; related bundled packages declare their required dependencies.

The active package set is part of save and multiplayer compatibility. Keep the packages required by a game available when returning to it.

## Network Multiplayer

Players should use the same version 2.00 build, enabled packages, and gameplay configuration. Script state must also match. The host shares the required game configuration during setup, and incompatible package or script information prevents a session from starting or a multiplayer save from loading normally. It is recommended that all players use the exact same version.

## Returning from 1.50

Use the version 2.00 executable and configuration workflow rather than the old launcher, command-line switches, or complete 1.50 configuration files. Install current package versions under `data/mods` and enable the bundled `v150` package if the restored 1.50 rules are wanted. Check controls and settings again after moving to version 2.00, and keep backups of older saves before testing them.
