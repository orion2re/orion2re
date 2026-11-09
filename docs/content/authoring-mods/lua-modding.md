# Authoring Mods: Lua Modding

Lua API version 1 runs one isolated, bounded Lua state for each enabled Lua package. A package executes one fixed `main.lua` during setup, registers supported behavior, and keeps the same state during the game session. Reloading a save creates fresh states from the exact package source and restores only engine-owned persistent data.

Lua is for deterministic simulation behavior. It is not a UI scripting system, an unrestricted file language, or a way to access native records and memory.

## Current scope

API version 1 is the minimal foundation of the Orion2 Redone modding API. It provides isolated package runtimes, deterministic registration, semantic game objects, saved mod storage, migrations, multiplayer identity, and enough specific events and subsystem operations to migrate the Lua scripts shipped with the audited 1.50 patch.

It is not a complete scripting surface for the game. Only a small number of prototype categories, event points, rule hooks, services, and game subsystems are exposed today. More events, rules, prototype families, and subsystem APIs are expected in later revisions. Until a capability appears in the current [events and API reference](events-and-api.md), a mod must not assume that a similarly named object or hook exists.

## Add Lua to a package

Add `main.lua` at package root and matching manifest metadata:

```json5
lua: {
  api: 1,
  storage_version: 1,
  migrations: [],
},
```

All Lua packages participate in simulation identity. Keep the required `config` entry point even when its CFG file is empty.

Minimal `main.lua`:

```lua
events.on("game.initialized", function(event)
    storage.games_started = (storage.games_started or 0) + 1
end)
```

During top-level `main.lua`, `storage` and live game objects are not available. The callback above closes over the global name and resolves `storage` only when the event runs after setup.

## Setup and sealing

During `main.lua`, API version 1 exposes these setup roots:

| Namespace | Purpose |
| --- | --- |
| `events` | Register handlers for implemented semantic events. |
| `rules` | Register modifiers for implemented calculations. |
| `services` | Register a provider for an implemented exclusive service. |
| `mod` | Export or import bounded interfaces between declared dependencies. |
| `prototypes` | Define content through the prototype families currently exposed by the API. |
| `defines` | Read stable enum and definition values. |
| `game` | Access the game-state collections, objects, and shared RNG permitted by the current lifecycle. During top-level setup, only its RNG member is available. |
| `require(name)` | Load a private package module. |

After every enabled `main.lua` returns, the engine validates prototypes and registrations, resolves the exclusive service provider, computes simulation identity, and seals setup. Events, migrations, exports, and callbacks cannot add registrations or modify prototypes later.

`prototypes` is the content-definition namespace, but API version 1 currently exposes only a narrow prototype surface. Do not infer that technologies, weapons, buildings, leaders, events, or other content categories are available merely because they would fit naturally below this namespace. Consult the current API reference before designing a mod around a prototype family. The prototype surface will expand as each game subsystem gains semantic identity, validation, persistence, and multiplayer behavior.

## Private modules

Organize implementation code in package-private modules:

```text
example_mod/
  main.lua
  content/rules.lua
```

Load the module from `main.lua`:

```lua
local rules_module = require("content.rules")
rules_module.register()
```

Each module-name segment must match `[a-z_][a-z0-9_]*`. A dotted name maps to a private `.lua` path in the same package. Absolute paths, separators, traversal, case aliases, native modules, and cross-package source access are rejected. Modules are cached per mod, cycles are rejected, and callbacks may use only modules loaded before the module set was sealed.

Every executed module contributes its canonical path and source hash to compatibility identity.

## Isolation and cross-mod calls

Mods do not share globals, tables, closures, module caches, storage, or proxy objects. To call another mod, declare it as a dependency or optional dependency and use an explicit export:

```lua
-- Provider main.lua
mod.export("scores", 1, {
    current = function()
        return storage.score or 0
    end,
})
```

```lua
-- Dependent main.lua
local scores = mod.import("provider_mod", "scores")
```

Calls are synchronous after setup seals. Nil, Boolean, integer, string, and bounded acyclic tables with Boolean, integer, or string keys can cross by validated copy. Functions inside the exported interface remain in the provider state, but arbitrary functions, userdata, threads, metatables, aliases, and cyclic tables do not cross as arguments or results. An export inherits the caller's lifecycle permissions and cannot grant broader game-state access.

## Integer-only language profile

The runtime uses signed 64-bit Lua integers and rejects floating-point gameplay arithmetic:

- floating-point literals are rejected;
- `/` and `^` are rejected;
- use `//` for integer floor division;
- native field conversions are range-checked; and
- external Lua bytecode is rejected.

Audit negative division carefully because `//` floors rather than truncating toward zero.

The allowed standard library omits debug, IO, OS, unrestricted package loading, native dynamic modules, sockets, environment access, process control, wall-clock access in deterministic behavior, and runtime source compilation.

## Stable objects and collections

Live game entities are engine-owned proxy objects, not copied native tables. Collections use `get(stable_id)` and `all()`. `all()` returns a deterministic one-based sequence suitable for `ipairs`:

```lua
for _, planet in ipairs(game.planets:all()) do
    local id = planet.id
end
```

Every access validates object identity, lifetime, current lifecycle, and permissions. Retained objects can expire; check `object.valid` where the API supplies it. Structural methods, writable properties, and collection creation are available only in lifecycle contexts that own the necessary mutable state.

## Definitions

`defines` exposes immutable semantic values. Current namespaces are:

```text
defines.climate
defines.planet_size
defines.planet_type
defines.gravity
defines.minerals
defines.government
defines.planet_special
defines.star_class
defines.leader_status
defines.ship_special
defines.technology_status
```

Use names from these tables instead of copying numeric values. A value absent from the current namespace is not part of API version 1.

## Random numbers

Use the engine-owned deterministic stream:

```lua
local roll = game.rng:integer(1, 100)
```

For `integer(minimum, maximum)`, `minimum` must be less than `maximum`; the implemented native-compatible result range is `minimum + 1` through `maximum`. Draws occur in package and callback order and are not rolled back when a later callback or validation step fails.

The runtime also exposes the current unsigned 32-bit seed:

```lua
local seed = game.rng:get_seed()
game.rng:set_seed(seed)
```

These methods read or replace the shared engine stream, not a private stream for the calling package. Reseeding changes every later native and Lua draw and therefore changes deterministic simulation identity. Use seed access only when a feature must deliberately checkpoint and restore the shared stream, and restore it on both success and error paths. Prefer `integer` for ordinary random choices.

## Lua and configuration

Lua API version 1 does not provide generic reads or writes by CFG path. Declarative mod settings should currently be expressed in the package's CFG files so they remain typed, validated, ordered, saveable, and predictable before simulation begins.

Whether Lua mods should be able to define or mutate dynamic configuration remains an open API-design question. Such support could be useful for mod-owned options and behavior that changes during a game, but unrestricted configuration mutation would make package order, reload behavior, persistence, multiplayer synchronization, validation, and derived-state refresh harder to reason about. Any future interface should preserve the configuration system's typed and transactional guarantees, distinguish setup values from safe runtime changes, and give each changed subsystem an explicit validation and synchronization owner. Until such an interface is documented, use CFG for exposed settings, semantic Lua objects for supported runtime behavior, and `storage` for mod-private saved state.

## Errors and resource limits

Every mod has bounded source, module, memory, stack, recursion, instruction, registration, cross-mod call, result, and storage resources. A diagnostic identifies the package, callback kind and semantic name, source location where available, traceback, API operation, and exceeded limit.

A deterministic callback error stops its owning operation. Successful writes and RNG draws made before the error are not generally rolled back. Some native construction owners, including galaxy generation, use their own staged candidate and can discard that candidate. Test failure paths rather than depending on rollback.