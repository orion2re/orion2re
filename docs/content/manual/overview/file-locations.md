# Manual: File locations

Orion2 Redone keeps the original game files, its own data, and the files it writes for you in one game directory by default. On Linux it can instead keep your files in your home directory.

## Portable mode (default)

Everything stays in the game directory, as with the original game: saved games, the Hall of Fame, settings, `data/orion2re.cfg`, and mods under `data/mods`. This is how the game works on Windows and macOS, and on Linux unless XDG mode is selected.

## XDG mode (Linux)

In XDG mode the game follows the [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/latest/). Files you create or change are kept in your home directory, so the installation can be read-only and shared by several users.

XDG mode is used when any of the following is true:

- the environment variable `ORION2RE_XDG` is set to `1`;
- a file named `xdg` exists next to the executable;
- the directory containing the executable is not writable, as with a system-wide installation.

Setting `ORION2RE_XDG=0` forces portable mode in every case.

| What | Location | Default |
|---|---|---|
| Configuration (`orion2re.cfg`, `ORION2RE.SET`, `language.ini`) | `$XDG_CONFIG_HOME/orion2re/` | `~/.config/orion2re/` |
| Saved games (`SAVE1.GAM` to `SAVE10.GAM`), Hall of Fame (`HOF.M2`), last custom race (`ORION2RE.RAC`) | `$XDG_DATA_HOME/orion2re/` | `~/.local/share/orion2re/` |
| Your own mods | `$XDG_DATA_HOME/orion2re/mods/` | `~/.local/share/orion2re/mods/` |
| Debug and sanitizer logs | `$XDG_STATE_HOME/orion2re/logs/` | `~/.local/state/orion2re/logs/` |
| Temporary files | `$XDG_CACHE_HOME/orion2re/` | `~/.cache/orion2re/` |

The game looks for its own data (`data/base.zip`, the bundled mods, and the default `data/orion2re.cfg`) next to the executable, whatever directory it is started from.

### Original game files

The original version 1.31 files, such as `ORION2.LBX` and `ORIONCD.INI`, are searched for in this order:

1. the directory containing the executable;
2. `$XDG_DATA_HOME/orion2re/game/` (by default `~/.local/share/orion2re/game/`);
3. `orion2re/game/` under each directory in `$XDG_DATA_DIRS` (by default `/usr/local/share/orion2re/game/` and `/usr/share/orion2re/game/`);
4. the CD path named in `ORIONCD.INI`.

Copying your Steam or GOG installation to `~/.local/share/orion2re/game/` is usually the simplest setup. A `language.ini` shipped with the original game is used until you change the language in the game, which writes your own copy to the configuration directory.

### Configuration

If `~/.config/orion2re/orion2re.cfg` exists, it is used instead of the installed `data/orion2re.cfg`. To customize the game, copy the installed file there and edit the copy. Configuration files it includes are looked up in the same directory first, then in the installation.

### Mods

A package in `~/.local/share/orion2re/mods/` is used in place of an installed package with the same ID. As in the installation, use either the directory or the ZIP form of a package, not both. Enable packages in your own `orion2re.cfg` as described in [Using mods](../legacy-modding/index.md).

### Switching an existing game to XDG mode

Files are not moved between the two modes automatically. To keep your progress when switching to XDG mode, copy `SAVE*.GAM`, `HOF.M2`, and `ORION2RE.RAC` to `~/.local/share/orion2re/`, and `ORION2RE.SET` and any customized `data/orion2re.cfg` to `~/.config/orion2re/`.
