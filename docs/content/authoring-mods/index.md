# Authoring Mods: Introduction

Orion2 Redone mods are explicit packages. A package can change exposed configuration values, replace read-only game assets, run Lua code through the versioned modding API, or combine all three. This section documents the package format and the author-facing interfaces implemented by the current engine.

This is separate from [Using mods](../manual/legacy-modding/index.md), which explains installation and activation for players. The pages here assume that you can edit plain-text files, inspect a ZIP archive, and read short CFG or Lua examples. They do not require knowledge of the restored C++ source or the original executable.

## Choose the narrowest mechanism

| What the mod needs to do | Use |
| --- | --- |
| Change an exposed rule, table entry, display option, or other generated setting | [CFG overrides](configuration-overrides.md) |
| Change individual strings in supported LBX resources | [JSON5 string overrides](string-overrides.md) |
| Replace graphics, sound, video, a complete text asset or LBX file, or one LBX entry | [VFS asset overrides](vfs-and-assets.md) |
| Edit a text resource that uses inline layout, substitution, or style commands | [Text formatting](text-formatting.md) |
| React to a supported game event or change a supported calculation | [Lua events and rules](events-and-api.md) |
| Transform the generated galaxy through the supported topology API | Lua `galaxy_generator` service |
| Keep mod-private state in saves | Lua `storage` and [persistence](persistence.md) |
| Add behavior for which no semantic API exists | Request a new semantic event, rule, service, or object operation; do not depend on native memory layout |

Prefer CFG for declarative values already listed in the [configuration reference](../generated/configuration/index.md). Prefer JSON5 string overrides for supported text resources, and VFS replacement for other assets whose virtual paths are known. Use Lua only when behavior depends on lifecycle, game state, deterministic calculation, or mod-private saved state.

## Minimal package

Create this directory relative to the game executable:

```text
data/
  orion2re.cfg
  mods/
    example_mod/
      mod.json5
      config/
        main.cfg
```

Put this manifest in `data/mods/example_mod/mod.json5`:

```json5
{
  id: "example_mod",
  name: "Example Mod",
  version: "1.0.0",
  requires: {
    engine: ">=2.0.0 <3.0.0",
  },
  config: "config/main.cfg",
}
```

Put at least an empty file at `data/mods/example_mod/config/main.cfg`. The current loader requires the manifest to name a non-empty configuration entry point, including for a package that otherwise contains only assets or Lua.

Enable the package in `data/orion2re.cfg`:

```text
enable example_mod;
```

Installing a package does not activate it. The engine resolves packages named by `enable` and their declared dependencies; it does not scan `data/mods` and enable everything it finds.

## Add configuration, assets, or Lua

A combined development layout can contain:

```text
example_mod/
  mod.json5
  config/main.cfg
  main.lua
  content/helpers.lua
  migrations/001-002.lua
  data/
    INTRO.LBX
    COLBLDG0.LBX/012
```

- `config/main.cfg` is the manifest-selected CFG entry point.
- `main.lua` is the only Lua entry point recognized at package root.
- `content/helpers.lua` is a private module that `main.lua` can load with restricted `require`.
- `migrations/` contains scripts named explicitly by the manifest.
- Only the package's `data/` subtree is projected into the public asset namespace.

The manifest, CFG, Lua sources, and migrations are private package files. They do not replace game assets merely because they exist beside `mod.json5`.

## Development workflow

1. Pick a stable lowercase package ID.
2. Develop the package as a directory under `data/mods/<id>/`.
3. Add the smallest CFG, JSON5 string, VFS asset, or Lua change that implements the feature.
4. Enable the package explicitly and test startup diagnostics.
5. Test interactions with dependencies and packages that touch the same configuration or assets.
6. Start a new game when the mod changes setup data or scripts.
7. Test save, load, and any declared storage migration.
8. Test multiplayer only with hooks supported there and with identical package sources on every peer.
9. Package the directory contents as `data/mods/<id>.zip`, with `mod.json5` at archive root.

The package ID, version, enabled order, synchronized configuration, Lua source, registrations, and selected service providers can all affect save or multiplayer compatibility. Change the package version when distributing changed content, even when the visible feature appears small.

## Current boundaries

Orion2 Redone does not provide a 1.50 compatibility mode for loose mods. It does not execute `MAIN0.LUA` through `MAIN9.LUA`, import `pregame_script` or `newgame_postprocessor_script` paths, expose the 1.50 raw global API, or treat old per-file LBX substitution settings as packages. Use the [1.50 migration guide](migrating-150.md) to classify and convert each part of an older mod.

Lua API version 1 is intentionally a minimal semantic foundation rather than complete coverage of the game. It is sufficient to recreate the behavior of the Lua scripts shipped with the audited 1.50 patch, but it currently exposes only a small set of prototypes, lifecycle events, rule hooks, services, and game subsystems. Broader modding coverage is expected as more semantic extension points are designed and implemented. The [events and API reference](events-and-api.md) lists the current surface; an event, prototype family, rule, service, or object operation not listed there is not currently available.