# Manual: Using mods

Mods are optional packages that change game rules, replace game data, or add scripts. Version 2.00 includes several mods, and other compatible packages can be installed without replacing the original game files.

## Installing a mod

Install a mod as either:

- a directory named `data/mods/<id>/`; or
- a ZIP file named `data/mods/<id>.zip`.

In XDG mode on Linux, you can also install mods in `~/.local/share/orion2re/mods/` and enable them in `~/.config/orion2re/orion2re.cfg`; see [File locations](../overview/file-locations.md).

Use only one form for the same mod ID. The package must be made for Orion2 Redone and include a `mod.json5` manifest. A loose 1.50 configuration, LBX replacement, or Lua script is not a version 2.00 package and should not be copied directly into `data/mods`.

## Enabling a mod

Close the game and add an `enable` line to `data/orion2re.cfg`:

```text
enable v150;
```

The name after `enable` is the package ID, not necessarily the title displayed in the game. IDs use lowercase letters, numbers, underscores, and hyphens. End each line with a semicolon.

Installing a package does not enable it. See [Included game mods](included-mods.md) for the IDs and purposes of packages shipped with version 2.00.

If you want to create or convert a package, see [Authoring Mods](../../authoring-mods/index.md). That section documents manifests, configuration overrides, asset replacement, and the Lua API. The instructions on this page are only for installing and selecting completed mods.

## Disabling or removing a mod

To disable a mod, remove or comment out its `enable` line and restart the game. A `#` starts a comment:

```text
# enable v150;
```

To remove a mod completely, first disable it, then remove its directory or ZIP file from `data/mods`.

Do not remove a package needed by a saved game. Version 2.00 records the required package IDs and versions and may refuse to load the game if a required package is missing or incompatible.

## Dependencies, conflicts, and load order

A mod can require another package. Enabling the mod also requires its dependencies to be installed at compatible versions. For example, `v150i` requires `v150`.

Dependencies are activated automatically. You do not need a separate `enable` line for a package that is already required by the mod you selected.

Some packages conflict because they change the same rules in incompatible ways. The game reports the conflict and stops loading rather than combining them unpredictably.

The order of `enable` lines sets package priority: the first enabled package has the highest priority when packages provide the same game resource. A package may also require another active package to appear before or after it. The game checks this order but does not reorder the lines for you; list packages in the order required by their authors.

## Mods that replace game data

A package can provide replacement graphics, text, sounds, or other game data. These replacements are active only while the package is enabled; the original game files remain unchanged.

If a replacement is missing or the package is malformed, the game may report an error during startup or when the affected resource is first used. Remove the package's `enable` line or reinstall a complete copy of the package.

## Mods that contain scripts

Some mods use scripts to change galaxy setup, technology, game rules, or other state. These scripts run as part of the package; the numbered `MAIN0.LUA` through `MAIN9.LUA` script-launch shortcuts from 1.50 are not supported.

A script error stops the package from loading. Use the exact package version intended for the current game. Script-bearing mods can store their own data in a saved game, and changing or removing their scripts can make that save incompatible.

For multiplayer, all players must use compatible copies of the same script-bearing packages. Version 2.00 compares package, script, and game-configuration information before play.

## Troubleshooting

Configuration and package errors identify the source and usually the affected line or package. Common causes are:

- a missing semicolon or misspelled package ID;
- both the directory and ZIP form of one package being installed;
- a missing or incompatible dependency;
- two enabled packages that conflict;
- incorrect package order; or
- an incomplete package, missing resource, or script failure.

Disable the most recently added package and restart. If the game starts, check that package's instructions and dependencies before enabling it again. If it still does not start, temporarily rename `data/orion2re.cfg` to confirm that the game can start with its built-in defaults.