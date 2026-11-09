# Authoring Mods: VFS and Asset Overrides

The virtual file system (VFS) resolves read-only game assets from loose development overrides, enabled package layers, shared data, and the original installation. A mod can replace a complete asset or an individual LBX entry without modifying the original files.

For individual strings in supported text resources, prefer [JSON5 string overrides](string-overrides.md)
instead of replacing binary LBX entries. These sidecars use the same package
`data/` paths and VFS precedence, while retaining unchanged strings from the LBX.

## Public and private package paths

Only files below a package's `data/` directory become public assets:

```text
data/mods/visual_mod/
  mod.json5
  config/main.cfg
  data/
    INTRO.LBX
    music/theme.dat
```

The virtual paths supplied by this package are `INTRO.LBX` and `music/theme.dat`. In a ZIP package, the equivalent entries are also `data/INTRO.LBX` and `data/music/theme.dat`.

Files beside `mod.json5`, including CFG, Lua, modules, and migrations, remain in the package-private namespace. An `INTRO.LBX` placed at package root does not replace the game asset.

## Supported asset reads

The VFS is used for read-only data including:

- LBX resources and streamed LBX-backed audio;
- animations loaded through normal resource loaders;
- Smacker video assets; and
- generic assets opened through the VFS.

It does not redirect save games, `ORION2RE.SET`, `language.ini`, `orioncd.ini`, temporary files, or other write paths. A mod must not expect a file placed under its public `data/` subtree to become a writable save or settings location.

## Resource lifetime

Save loading refreshes shared string tables, fonts, persistent gameplay artwork,
and the graphics and sound-effect caches before entering the destination screen.
Returning to the main menu restores the user's launch configuration and its
resources. Screen-specific artwork continues to load on demand. Resource refresh
does not rename saved players or ships.

Music is not included in this refresh. Live music and animation streams are not
automatically reopened when packages change. In particular, a stream opened from
a mod ZIP can retain references to an archive closed by a package remount; this
stream-lifetime limitation remains unresolved. Avoid runtime switches of packages
providing active streamed assets until stream ownership/reopening is implemented.

## Resolution order

For each virtual asset, Orion2 Redone searches:

1. loose `data/overrides/`;
2. enabled package `data/` projections, highest-priority package first;
3. optional `data/base.zip`;
4. loose files under `data/`;
5. original files in the game directory; and
6. original files in the configured fallback installation path, if present.

The first match wins. A loose development override hides every mod version. The first enabled package wins a collision with later packages. All enabled packages win over shared data and original assets.

`data/base.zip` is the only archive mounted automatically from `data/`. Mod archives belong under `data/mods/` and still require an `enable` statement.

## Whole-file replacement

Reproduce the virtual path below the package's public `data/` directory:

```text
data/mods/visual_mod/data/INTRO.LBX
```

Use the path expected by the game, not the local path of an unrelated installation. Asset matching is case-insensitive, but distribute one canonical spelling. Archive entries that normalize to the same virtual path are invalid.

## Individual LBX entry replacement

To replace only one entry, use the LBX filename as a directory and a zero-padded three-digit entry number:

```text
data/mods/visual_mod/data/COLBLDG0.LBX/000
data/mods/visual_mod/data/COLBLDG0.LBX/012
```

The override file contains only that entry's payload, not an LBX header or table. When entry 12 is requested, the VFS first searches `COLBLDG0.LBX/012` through the normal source order. If it finds no entry override, it finds the highest-priority complete `COLBLDG0.LBX` and reads entry 12 from that file.

An entry override therefore wins for its one entry even when another visible layer provides a complete replacement LBX. Unchanged entries continue to come from the highest-priority complete file.

## Development overrides

`data/overrides/` is useful for discovering paths and testing a replacement quickly. It is not a package:

- it needs no manifest or `enable` statement;
- it has higher priority than all packages;
- it has no package identity, dependencies, version, or save compatibility declaration; and
- it should not be used for distribution.

Before publishing, move every required override into the package's `data/` subtree and remove the loose development copy. Otherwise a local file may make the package appear to work while the ZIP is incomplete.

## ZIP requirements

ZIP assets are read directly without extraction. Use ordinary non-encrypted entries and safe relative paths. Package activation rejects malformed archives, unsafe traversal paths, unsupported or encrypted entries, and duplicate normalized paths.

Archive root should look like:

```text
mod.json5
config/main.cfg
data/INTRO.LBX
data/COLBLDG0.LBX/012
```

Do not archive the containing `visual_mod/` directory itself.

## Collision testing

Test both resource and CFG collisions because they use the same package priority in opposite processing directions: the first package is searched first for assets and applied last for configuration.

When the wrong asset appears:

1. Remove or inspect `data/overrides/`.
2. Confirm the package is enabled.
3. Confirm the asset is below the package's `data/` directory.
4. Confirm the first enabled package is intended to win.
5. Check case-insensitive duplicate paths in the directory or ZIP.
6. For an LBX entry, confirm the three-digit entry number and payload-only format.