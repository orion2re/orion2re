# Authoring Mods: Packages and Manifests

Every Orion2 Redone mod is a directory or ZIP package with a strict `mod.json5` manifest. Package metadata is resolved before CFG files, assets, or Lua are committed, so an invalid package fails instead of being partially activated.

The manifest uses [JSON5](https://json5.org/), a JSON-compatible format that permits conveniences such as comments, unquoted property names, and trailing commas. JSON5 syntax does not relax the manifest schema: properties and values must still follow the requirements below.

## Installation forms

A package must use exactly one of these forms:

```text
data/mods/<id>/
data/mods/<id>.zip
```

Do not install both forms for one ID. The directory name or ZIP filename, the manifest `id`, dependency references, and the `enable` ID must match exactly.

The package ID must match:

```text
[a-z][a-z0-9_-]*
```

Use portable lowercase paths even though public VFS asset matching is case-insensitive.

## Package layout

Directory and ZIP packages have the same internal structure:

```text
<package root>/
  mod.json5
  config/main.cfg
  main.lua                 Optional
  content/                 Optional private Lua modules
  migrations/              Optional declared migration scripts
  data/                    Optional public asset overrides
```

In a ZIP, `mod.json5` must be at archive root. Do not wrap these files in another `<id>/` directory. Public asset paths begin below `data/`; all other paths remain private to the package.

## Complete manifest shape

```json5
{
  id: "example_mod",
  name: "Example Mod",
  description: "Changes selected rules and assets.",
  version: "1.2.0",
  compatible_versions: ">=1.0.0 <1.2.0",

  requires: {
    engine: ">=2.0.0 <3.0.0",
  },

  dependencies: [
    { id: "shared_rules", version: ">=2.0.0 <3.0.0" },
  ],
  optional_dependencies: [
    { id: "portrait_pack", version: ">=1.0.0" },
  ],
  conflicts: ["other_overhaul"],
  load_after: ["shared_rules"],

  config: "config/main.cfg",

  lua: {
    api: 1,
    storage_version: 2,
    migrations: [
      { from: 1, to: 2, script: "migrations/001-002.lua" },
    ],
  },
}
```

The schema is strict. Unknown and duplicate properties are errors. Callback lists, service lists, and script entry-point paths are not manifest properties.

## Required properties

| Property | Requirement |
| --- | --- |
| `id` | Stable package ID matching the directory or ZIP name. |
| `name` | Non-empty display name. |
| `version` | One to three decimal components without leading zeroes. |
| `requires.engine` | Version range accepted by this package. |
| `config` | Safe, non-empty package-relative CFG path. The file must exist. |

`description` is optional display text. It does not affect priority or dependencies.

Versions such as `2`, `2.0`, and `2.0.0` compare as the same version. Ranges are whitespace-separated comparisons, all of which must match:

```text
2.0.0
=2.0.0
>=2.0.0 <3.0.0
```

Supported operators are `=`, `<`, `<=`, `>`, and `>=`. Caret, tilde, wildcard, logical OR, and hyphen ranges are not supported.

## Package relationships

`dependencies` are required packages. A missing package or version mismatch stops activation. Dependencies are activated automatically and loaded once even when several packages require them.

`optional_dependencies` describe integration with another installed package. If the optional package is present, its manifest, version, dependency graph, and configuration must be valid. Do not use optional dependencies for a package without which the mod cannot function.

`conflicts` lists package IDs that may not be active together. Use a conflict when combining the packages would be invalid, not merely because they override the same value or asset.

`load_after` is an ordering assertion, not a dependency and not an automatic sorter. If `patch` declares `load_after: ["overhaul"]`, list `patch` before `overhaul` in `orion2re.cfg`. The engine checks the required relation.

## Priority and application order

The first explicit `enable` has the highest priority:

```text
enable compatibility_patch;
enable overhaul;
enable shared_rules;
```

One priority order controls all package mechanisms:

- VFS searches highest to lowest priority, so the first package supplies a colliding asset.
- CFG files are applied lowest to highest priority, so the first package writes last and wins a colliding setting.
- Lua `main.lua` setup runs lowest to highest priority, so the first package runs last.
- Lua callbacks run in setup order, then registration order within each `main.lua`.
- A package has higher priority than a dependency activated only for it.

Use manifest dependencies for required relationships. An `enable` statement can appear in a package CFG, but it lacks the clear versioned relationship expressed by `dependencies`.

## Save-version substitution

Saves and network setup record exact package versions. `compatible_versions` allows the installed version to stand in for specific older serialized versions of the same package:

```json5
compatible_versions: ">=1.0.0 <1.2.0",
```

Compatibility is directional. A new `1.2.0` package accepting saves from `1.1.0` does not imply that `1.1.0` can load saves made by `1.2.0`. Omit this property unless the newer package has been tested with the older serialized state and, for Lua packages, has a complete storage migration path where needed.

## Lua metadata

Add `lua` only when the package has a package-root `main.lua`:

```json5
lua: {
  api: 1,
  storage_version: 1,
  migrations: [],
},
```

API version 1 is the current supported API. `storage_version` is a positive package-owned schema number. `migrations` must form one ordered, increasing chain ending at the current storage version. Every script path must be safe, package-relative, unique, and present.

All Lua packages participate in synchronized simulation identity.

## Packaging checklist

Before publishing a ZIP, verify:

1. The archive is named `<id>.zip`.
2. `mod.json5` is at archive root and its `id` matches the filename.
3. The configured CFG entry point exists.
4. `main.lua` and `lua` metadata are either both present or both absent.
5. Every declared migration exists and the chain ends at `storage_version`.
6. Public assets are below `data/`.
7. Paths are relative, normalized, and unique after case-insensitive normalization.
8. The ZIP is not encrypted.
9. The directory form is removed before testing the ZIP form.
10. The final package starts, creates a game, saves, reloads, and reports its intended version.