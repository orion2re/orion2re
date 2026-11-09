# Authoring Mods: Persistence, Saves, and Multiplayer

Lua source, globals, closures, and virtual-machine internals are not saved. Orion2 Redone recreates each Lua state from verified package source, reruns `main.lua` to rebuild code and registrations, restores engine-owned persistent state, and then resumes the game.

## What persists

Use the persistence owner that matches the data:

| Data | Persistence owner |
| --- | --- |
| Exposed engine configuration and save-backed prototype source values | Normal game configuration snapshot |
| Final generated planets, systems, technology choices, and other native game state | Normal game save state |
| Mod-private counters, histories, decisions, and records | Per-mod Lua `storage` |
| Cached lookups, aliases, and derived session data | Ordinary Lua globals rebuilt after load |

Do not copy a save-backed engine table into `storage` merely to preserve it. Do not keep gameplay history only in a global or closure if it must survive save/load or multiplayer authority changes.

## Using `storage`

Each Lua package receives a distinct `storage` root after setup:

```lua
events.on("game.initialized", function(event)
    storage.encounters = {}
    storage.total_rewards = 0
end)
```

Assignments take effect immediately; no explicit commit is required. Supported values are:

- nil by absence;
- Boolean;
- signed integer;
- valid UTF-8 string;
- tables with Boolean, integer, or string keys;
- shared and cyclic table graphs;
- stable player references; and
- generational ship references.

Functions, threads, closures, stacks, arbitrary userdata, executable metatables, and weak-table semantics are not saved. Planet, system, colony, fleet, leader, nebula, and prototype references are not currently supported in storage.

The current per-mod limits are 4 MiB of canonical storage, 65,536 graph nodes, 262,144 table entries, 1 MiB per string, and 256 active traversal frames. Unsupported values or exceeded limits fail the save; values are not silently discarded.

## References and expiry

Stored player references use stable player identity. Stored ship references include an engine-owned generation as well as the ship slot. If a ship is destroyed and its slot is reused, the old reference becomes invalid rather than silently pointing to the new ship.

An expired restored reference keeps readable `kind`, `id`, and `valid` information for cleanup. Accessing live-only properties or methods fails. Replace or remove expired references during a permitted migration or simulation callback.

## New-game and load lifecycle

For a new game:

```text
load exact package source
-> execute main.lua and seal setup
-> attach empty storage
-> run new-game services and setup events
-> dispatch game.initialized
-> begin simulation
```

For an exact-version load:

```text
load exact package source
-> execute main.lua and seal setup
-> restore save-backed engine state
-> restore storage
-> dispatch restricted game.loaded
-> continue simulation
```

Use `game.loaded` only to rebuild transient Lua data:

```lua
local cached_total

events.on("game.loaded", function(event)
    cached_total = storage.total_rewards or 0
end)
```

The callback cannot mutate storage, synchronized game objects, prototypes, registrations, exports, or service selection. The current API still exposes the shared engine RNG in this lifecycle; drawing or setting its seed changes synchronized state and should not be used while rebuilding a transient cache. All persistent state must already be valid before `game.loaded` runs.

## Storage versions and migrations

Increment `lua.storage_version` when a release changes the meaning or shape of saved `storage`. Declare every supported transition in one ordered chain:

```json5
lua: {
  api: 1,
  storage_version: 3,
  migrations: [
    { from: 1, to: 2, script: "migrations/001-002.lua" },
    { from: 2, to: 3, script: "migrations/002-003.lua" },
  ],
},
```

A migration script receives restored migration `storage` and immutable `migration.from` and `migration.to` values. It may transform supported storage and use package-private pure helpers. It cannot access live game proxies, mutable prototypes, active services, or inter-mod exports. Migrations run once in declared order before normal callbacks resume.

After a compatible package-version change and storage migration, `game.configuration_changed` is dispatched before `game.loaded`. Use it only for documented synchronized migration work that cannot be expressed as a storage-only transformation.

An absent edge, invalid graph, migration error, or unsupported version transition fails closed. The engine does not reset storage silently.

## Package compatibility and saves

A save records package IDs and versions, synchronized configuration, selected service providers, API and storage versions, executed Lua source identities, persistent storage, and other simulation identity. Exact installed versions qualify automatically. A different package version qualifies only when the installed manifest's directional `compatible_versions` accepts the saved version and all other required identity and migration checks succeed.

Changing Lua source without changing the package version does not make it compatible. Source hashes are checked independently and can reject a save or network peer even when IDs and versions look equal.

## Multiplayer identity

Before a Lua-enabled session starts or loads, peers compare at least:

- engine and Lua runtime/API profiles;
- ordered package IDs and versions;
- executed `main.lua`, migration, and private-module source identities;
- synchronized configuration;
- storage versions;
- finalized prototype and registration identity; and
- selected service providers.

Package order is significant. The host does not distribute missing or mismatched mod source.

## Current multiplayer hook support

The current network allowlist supports:

- lifecycle and new-game setup events other than `ship.destroyed`;
- `planet.generated`;
- the `population.growth` rule on the authoritative gameplay host; and
- `galaxy_generator` version 1 on the setup host.

`ship.destroyed` and `combat.beam_damage` are currently rejected in network games. Future events are not multiplayer-safe merely because they are deterministic in single-player; their native owner and complete state-transfer path must also be implemented.

Strategic callbacks run where the authoritative native strategic operation runs. Clients install transferred results rather than synthesizing host callbacks. Lua globals and VM internals are never transferred, so persistent history must remain in `storage` or engine-owned state.

## Release discipline

For every release that can affect saved games:

1. Change the package version.
2. Decide whether old saves are accepted directionally.
3. Increment `storage_version` if storage semantics changed.
4. Provide and test every required migration edge.
5. Test exact-version load and supported older-version load.
6. Test that unsupported versions fail with a useful diagnostic.
7. For multiplayer, test identical packages on at least two peers and test a deliberate source or order mismatch.