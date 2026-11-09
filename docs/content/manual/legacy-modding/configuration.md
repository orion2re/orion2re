# Manual: Configuration

The `data/orion2re.cfg` file contains player settings and the list of enabled mods. It is a plain text file and can be edited with a text editor while the game is closed.

If the file does not exist, Orion2 Redone uses its built-in defaults. The game does not rewrite the file. To restore all defaults, close the game and temporarily rename or remove `data/orion2re.cfg`.

## Startup options

Use `/` before option names on Windows and a single `-` on Linux and macOS:

| Windows | Linux / macOS | Effect |
| --- | --- | --- |
| `/config configs/test.cfg` | `-config configs/test.cfg` | Load this VFS configuration path instead of `orion2re.cfg`. |
| `/skip-intro` | `-skip-intro` | Skip the logos and intro movie. |
| `/load-save 10` | `-load-save 10` | Load `SAVE10.GAM` on the first main-menu pass. Save numbers are 1 through 10. |

Options with values also accept `=`, for example `/config=configs/test.cfg` or `-load-save=1`. Quote an entire path argument if it contains spaces. Unknown options, repeated options, missing values, and out-of-range save numbers are errors.

Configuration paths use forward slashes on all platforms and are VFS paths, not absolute filesystem paths; for example, `configs/test.cfg` normally selects `data/configs/test.cfg`. An explicitly selected configuration must exist. Only the implicit `orion2re.cfg` falls back to defaults when missing. Configuration selection does not create separate saved games or `ORION2RE.SET` files. Reloading configuration uses the selected file.

Automatic save loading is attempted once and follows the normal Continue path, including its interactive error messages and multiplayer-save restrictions. A failed attempt returns to the main menu after any error is dismissed; it is not a headless test mode. Combine it with `skip-intro` to reach a saved game faster.

## Changing a setting

Each assignment ends with a semicolon. For example, this enables vertical synchronization and selects a presentation scaler:

```text
interface vsync_enabled = true;
interface scaler = pixelart;
```

The [configuration reference](../../generated/configuration/index.md) lists current setting names, effects, accepted values, and built-in defaults. Copy the configuration path and example from the relevant entry rather than relying on a 1.50 parameter name.

Mod authors who need records, sequential assignments, package-relative includes, and validation rules should use the [configuration override guide](../../authoring-mods/configuration-overrides.md).

A `#` starts a comment and can be used to leave a note or temporarily disable a line:

```text
# interface scaler = mmpx4;
interface scaler = pixelart;
```

## Enabling a mod

Use the package ID in an `enable` command:

```text
enable v150;
```

See [Using mods](index.md) for installation, dependencies, conflicts, and package order.

## Including another configuration file

An `include` command reads settings from another required file:

```text
include preferences/display.cfg;
```

Relative include paths are resolved from the file containing the command. Startup fails if the included file is missing, contains an error, creates an include cycle, or exceeds the supported nesting depth. Includes are useful for keeping personal display settings separate from game-rule changes, but are not required for normal use.

## Configuration errors

The configuration must parse and validate completely before the game starts. An error normally reports the file and the affected line and column. Check for:

- a missing semicolon;
- a misspelled setting or accepted value;
- a number outside the allowed range;
- a missing included file; or
- an invalid `enable` line or mod package.

If the error is unclear, close the game, rename `data/orion2re.cfg`, and start with built-in defaults. Restore the file and add its settings back in small groups until the invalid entry is found.

## Reloading settings

On the Main, Build, and Tactical Combat screens, hold Alt and type R then E to reload the full configuration. Full reload is not allowed during multiplayer games. Hold Alt and type R then I to reload interface options; this is allowed during multiplayer games.

Restarting the game is safer after changing enabled packages or gameplay rules. Reloading does not make an incompatible mod safe for an existing saved game.

## Saves and multiplayer

Saved games record the game configuration and required package IDs and versions. Loading a save restores the configuration used by that game. Keep its required packages installed.

Before testing a save with another build or package set, copy the save and preserve the configuration and mods used to create it. Do not overwrite the only copy.

In multiplayer, gameplay settings and package information must be compatible between players. Local interface settings may differ, but changing gameplay rules, package contents, or scripts can prevent a session or multiplayer save from loading.