# Authoring Mods: Migrating a 1.50 Mod

Orion2 Redone preserves important 1.50 gameplay use cases, but it does not reproduce the 1.50 loader or raw Lua API. Migration requires a package manifest and source changes. Loose CFG, LBX, and Lua files from 1.50 are not automatically compatible.

## Structural comparison

| Area | Version 1.50 | Orion2 Redone |
| --- | --- | --- |
| Mod identity | Configuration metadata and paths without one enforced package boundary | Stable package ID, version, engine range, and strict `mod.json5` |
| Installation | Loose profiles, scripts, and replacement files | `data/mods/<id>/` or `data/mods/<id>.zip` |
| Activation | Launcher/profile selection, path accumulation, and loader settings | Explicit `enable <id>;` plus manifest dependencies |
| Ordering | Textual CFG order and `mod_order` | Explicit enable priority, dependency graph, `load_after`, and registration order |
| Configuration | Flattened binary-era names and offsets | Typed hierarchical CFG paths generated from semantic schema |
| Assets and LBX | Per-file substitution parameters and DOS paths | Package `data/` projection, whole-file VFS replacement, or `NAME.LBX/NNN` |
| Lua entry points | Configurable paths, pregame/postprocessor lists, `MAIN0.LUA` through `MAIN9.LUA` | One package-root `main.lua` with private modules and semantic registrations |
| Lua state | Fresh state per script invocation | One isolated persistent state per active Lua package and session |
| Lua data model | Flat globals and copied raw native tables | Versioned roots, stable proxies, immutable payloads, and semantic methods |
| Mutation | Broad `get_*`, `set_*`, `rget_*`, and raw table replacement | Validated event, rule, service, prototype, and object operations |
| Persistence | Results mainly written directly into native state; no per-mod storage schema | Engine-owned per-mod `storage`, explicit schema versions, and migrations |
| RNG | `random`, `get_seed`, and `set_seed`, including scripts that checkpoint and restore the native seed | Shared engine RNG through `game.rng`; preserve intentional checkpoint/restore behavior around migrated random work |
| Failure | Earlier raw writes could survive a later script error | Strict staged activation; lifecycle failure stops the owner, with owner-specific staging where implemented |
| Multiplayer | Script timing around legacy autosave and synchronization | Exact source/order negotiation and an explicit network hook allowlist |

## Convert the package first

1. Choose a stable lowercase ID.
2. Create `data/mods/<id>/mod.json5` and `config/main.cfg`.
3. Move translated CFG assignments into the package CFG and includes.
4. Move public assets below the package's `data/` directory.
5. Add `main.lua` and `lua` metadata only if behavior still requires scripting.
6. Express required package relationships as dependencies or conflicts.
7. Enable the package explicitly and test it as a directory before creating a ZIP.

Do not retain absolute DOS paths. Package CFG, Lua modules, migrations, and public assets all use safe package-relative paths.

## Convert 1.50 configuration

Do not mechanically rename every old variable. Search the current [configuration reference](../generated/configuration/index.md) for the player-visible rule or data concept. Orion2 Redone uses hierarchical names, records, arrays, enums, and masks rather than preserving flattened 1.50 spellings.

Examples of intentional structural replacements include:

- `mod_id`, `mod_name`, and `mod_desc` -> manifest `id`, `name`, and `description`;
- `mod_class` and `mod_order` -> dependencies, conflicts, `load_after`, and explicit priority;
- `scan_mods` -> explicit `enable` statements; and
- per-LBX substitution variables -> VFS files or LBX entry overrides.

If no current configuration path represents a legacy behavior, do not invent an alias. Confirm whether the behavior belongs to CFG, Lua, native UI, or a missing semantic engine feature.

## Convert Lua entry points

| 1.50 mechanism | Orion2 Redone direction |
| --- | --- |
| `pregame_script` | Setup-time prototype work or a supported semantic new-game event. |
| `newgame_postprocessor_script` | Supported generated-object events or `galaxy_generator`. |
| Semicolon-separated paths | One `main.lua`, private modules, dependencies, and registrations. |
| `MAIN0.LUA` through `MAIN9.LUA` | No Lua migration; presentation and reports belong to native UI. |
| Configurable `main.lua` filename template | Fixed package-root `main.lua`. |
| `LUA` full-access cheat | No mod equivalent. Use supported lifecycle capabilities or separate developer tools. |

Move top-level one-shot behavior into the semantic event that owns it:

```lua
events.on("new_game.player_setup", function(event)
    local player = game.players:get(event.player_id)
    -- Apply supported initial player changes.
end)
```

Do not choose an event merely because its timing looks close to the old hook. Choose it because its documented payload, mutation capability, save behavior, and multiplayer ownership match the feature.

## Convert raw APIs

| 1.50 API pattern | Orion2 Redone direction |
| --- | --- |
| `get_conf` | Generated CFG reference, `defines`, or a domain-owned semantic object; no arbitrary Lua CFG path access |
| `get_game` | `game` roots and immutable callback payloads |
| `get_players`, `get_planets`, `get_stars`, `get_ships`, `get_leaders` | Stable collections using `get(id)` and deterministic `all()` |
| `set_*`, `add_*`, `del_*` | Validated properties and semantic methods in a lifecycle that permits them |
| `rget_*`, `rset_*`, `radd_*`, `rdel_*` | No raw-layout equivalent |
| `get_current_player_id` | No simulation Lua equivalent; process-local UI identity is not simulation state |
| `msgbox` | No simulation Lua equivalent; presentation belongs to native UI |
| `print` | Use structured diagnostics where available rather than player UI |
| `random`, `set_seed`, and `get_seed` | Use `game.rng:integer`, `game.rng:set_seed`, and `game.rng:get_seed`. The unsigned 32-bit seed belongs to the shared native stream. Preserve intentional checkpoint/restore behavior used by legacy mods such as VDC, including restoring the seed after errors, rather than replacing it with a mod-private stream. |

Legacy tables often use sparse, zero-based native IDs. Review every `pairs`, `ipairs`, `rawlen`, length operator, and numeric index. Orion2 Redone collections return deterministic one-based sequences from `all()` and use semantic IDs for `get(id)`.

Legacy mutation:

```lua
local planets = get_planets()
planets[planet_id].minerals = 4
set_planets(planets)
```

Semantic mutation:

```lua
local planet = game.planets:get(planet_id)
planet.minerals = defines.minerals.rich
```

The new operation validates identity, lifecycle, domain, and native invariants. There is no generic partial-table setter.

## Convert language assumptions

1. Replace `/` with `//` after checking intended rounding.
2. Review negative division because `//` floors.
3. Remove floating-point literals and exponentiation.
4. Review overflow assumptions: 1.50 used signed 32-bit numbers; API version 1 uses signed 64-bit Lua integers and checked native conversions.
5. Remove debug, IO, OS, clock, native module, and dynamic source-loading assumptions.
6. Convert source text and literals to UTF-8 where necessary.
7. Replace accidental table iteration order with a documented sequence.

## Convert state and ordering

Move persistent mod-private history from globals into `storage`. Leave saved engine values in their engine-owned state instead of duplicating them. Add a storage schema version and migration chain before changing distributed storage structure.

Convert script-path and filename order into:

- declared package dependencies;
- explicit package priority;
- source registration order within `main.lua`; and
- deterministic iteration supplied by the API.

Do not rely on archive order, filesystem enumeration, hash-table order, or concatenated path order.

## Known shipped-script dispositions

The version 2.00 API was checked against all Lua files shipped with the audited 1.50 patch:

- deposit adjustment uses `planet.generated` and stable planet objects;
- player technology repairs use `new_game.player_setup` and semantic technology methods;
- homeworld changes use `new_game.home_system_generated`;
- RandomTech-style global tree changes use `new_game.technology_setup`;
- mirror variants use the exclusive `galaxy_generator` service; and
- interactive `MAIN*.LUA` reports remain native-UI concerns rather than simulation Lua.

This mapping proves those categories can be migrated; it does not make the old source API-compatible.

The current Lua API was built first to cover these audited 1.50 use cases through safer semantic interfaces. It remains a minimal foundation. Mods that need unrelated game phases or subsystems may require new events, rules, services, prototypes, or stable object operations before they can be migrated cleanly.

## Migration checklist

1. Classify each feature as CFG data, VFS asset, prototype, event, rule, service, storage, or native UI.
2. Create a strict package and explicit relationships.
3. Translate configuration by semantics, not old spelling.
4. Replace substitution paths with public VFS paths.
5. Consolidate Lua setup into `main.lua` and private modules.
6. Replace raw globals and tables with the implemented semantic API.
7. Convert arithmetic and encoding assumptions.
8. Move persistent history into versioned `storage`.
9. Make ordering explicit.
10. Test new game, save/load, migration, failure, package collisions, and multiplayer allowlisting.
11. If an operation is absent, request a semantic API extension instead of exposing native layout.