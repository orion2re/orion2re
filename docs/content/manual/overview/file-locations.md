# Manual: File locations

The native game supports portable installations on Windows, macOS, and Linux. Linux also supports system-wide installations with per-user files following the [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/latest/).

## Portable mode

By default, the game keeps saves, settings, the Hall of Fame, and temporary files in the directory containing the executable. Configuration and mods live under `data/`. The game uses this directory regardless of the working directory of the launcher.

On Linux, `ORION2RE_XDG=0` forces portable mode, even for a read-only installation. No XDG directories are consulted or created in that mode; the executable directory must be writable to save progress or settings. Windows and macOS keep their existing portable behavior and ignore the XDG mode controls.

## XDG mode on Linux

XDG mode is selected when:

- `ORION2RE_XDG=1` is set (any nonempty value other than `0` enables it);
- an `xdg` marker exists next to the executable; or
- the executable directory is not writable, as in a system-wide installation.

The environment override takes precedence over the marker and automatic detection.

| Files | Location | Default |
|---|---|---|
| Configuration: `ORION2RE.SET`, `language.ini`, `user.cfg`, user `orion2re.cfg` | `$XDG_CONFIG_HOME/orion2re/` | `~/.config/orion2re/` |
| Saves, `HOF.M2`, last custom race `ORION2RE.RAC` | `$XDG_DATA_HOME/orion2re/` | `~/.local/share/orion2re/` |
| User mods | `$XDG_DATA_HOME/orion2re/mods/` | `~/.local/share/orion2re/mods/` |
| Debug and sanitizer logs | `$XDG_STATE_HOME/orion2re/logs/` | `~/.local/state/orion2re/logs/` |
| Temporary file `TEMP.TMP` | `$XDG_CACHE_HOME/orion2re/` | `~/.cache/orion2re/` |

Empty or relative XDG home-directory values fall back to the defaults under `$HOME`. Relative entries in `$XDG_DATA_DIRS` are ignored. New user directories are created with private permissions (`0700`); existing directory permissions are not changed.

### Configuration and mods

Put personal settings in `user.cfg` in your configuration directory. It is loaded automatically after the main configuration if present, so you do not need to copy the installed `data/orion2re.cfg`. A user copy of `orion2re.cfg`, if present, still takes precedence over the installed main configuration. Both `user.cfg` and included `.cfg` files are searched in the user configuration directory first; private mod configuration stays inside its package.

Install user mods as `mods/<id>/` or `mods/<id>.zip` under the user data directory. A user package replaces an installed package with the same ID; other installed packages remain available. Do not install both directory and ZIP forms of the same ID. Enable packages in your configuration as described in [Using mods](../legacy-modding/index.md).

### Original game files

Provide the original version 1.31 game files from your own legitimate copy. For loose original files, the search order is:

1. the resolved runtime-data directory (normally the executable directory, or the system data directory for an installed game);
2. the executable directory, if different;
3. `$XDG_DATA_HOME/orion2re/game/`;
4. `$XDG_DATA_HOME/orion2re/` as a convenience fallback;
5. `orion2re/game/` under each absolute `$XDG_DATA_DIRS` entry (default `/usr/local/share` and `/usr/share`);
6. the CD path supplied by `ORIONCD.INI`.

The recommended location is `~/.local/share/orion2re/game/`. `ORIONCD.INI` and the initial `language.ini` are found beside the executable, in the installed data directory, or alongside original files in the XDG locations. Once you change the language, your configuration-directory copy takes precedence.

### Switching modes

Files are not moved automatically. Before switching to XDG mode, copy `SAVE*.GAM`, `HOF.M2`, and `ORION2RE.RAC` into the user data directory, and `ORION2RE.SET`, `language.ini`, `user.cfg`, and any customized `orion2re.cfg` into the user configuration directory. Copy them back to their portable locations to switch back. Keep backups when switching.

## Native Linux installation

CMake installs the executable and router under the configured `bindir`, and project runtime assets under `<datadir>/orion2re/data/`. Original proprietary game files are never included. Build before installing:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --target orion2re orion2re-router --parallel
cmake --install build --component Runtime
```

Use `DESTDIR` for package staging. The installation prefix is recorded at configure time; changing only `cmake --install --prefix` does not update the executable's data path. A router-only build installs only the router.

Runtime assets are loaded beside the executable when `data/base.zip` is present, otherwise from the configured system data directory. The game does **not** automatically select a runtime tree from the user's home directory, even in XDG mode. Use the configuration and mod layers for user customization instead.
