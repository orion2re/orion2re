# Authoring Mods: Events and API Reference

This page lists the Lua API version 1 behavior implemented by Orion2 Redone. Unknown event, rule, and service names fail during package activation. Event payloads and rule contexts are immutable even when the current lifecycle permits mutation through ordinary `game` objects.

## Registration and ordering

Register behavior only from `main.lua` or a module loaded by it:

```lua
events.on("planet.generated", function(event)
    local planet = game.planets:get(event.planet_id)
    -- Validated planet operations are available in this lifecycle.
end)
```

Packages set up from lowest to highest package priority. Callbacks dispatch in that package setup order, followed by source registration order within each package. Later callbacks observe successful writes made by earlier callbacks.

## Events

| Event | Immutable payload | Mutation and execution |
| --- | --- | --- |
| `game.initialized` | Empty | May initialize `storage` and use permitted live game operations. Runs once after new-game setup. |
| `game.loaded` | Empty | Read-only. Rebuild transient Lua caches; do not change game state, storage, prototypes, registrations, services, or exports. |
| `game.configuration_changed` | `reason`, `changes`, `changed_groups` | Storage migration/configuration lifecycle. See [persistence](persistence.md). |
| `new_game.technology_setup` | `setup_sequence` | May change supported global technology source state before player technology assignment. Not run when loading a save. |
| `new_game.player_setup` | `player_id` | May change supported initial player technology state. Runs once per player. Not run on load. |
| `new_game.home_system_generated` | `player_id`, `system_id` | May change supported staged home-system planet state. Not run on load. |
| `planet.generated` | `planet_id` | May change supported staged planet fields. Runs once per generated planet and is not rerun on load. |
| `ship.destroyed` | `destroyed_ship`, optional `killer`, `phase`, `sequence` | May use supported mutable game operations. Current network games reject this event. |

`destroyed_ship` is an immutable historical record with `id`, `owner`, `cost`, `cause`, and `name`. `killer`, when present, is a live ship proxy. The record remains usable even though the destroyed entity no longer exists.

For `game.configuration_changed`, `reason` is `package_version_changed` or `configuration_reloaded`. `changes` is an ordered sequence whose records contain `package_id`, `old_package_version`, `new_package_version`, and, for Lua packages, `old_storage_version` and `new_storage_version`. `changed_groups` lists changed configuration groups for a reload.

## Rules

Rule modifiers receive an immutable context and the current value, and must return a replacement integer:

```lua
rules.modify("population.growth", function(context, growth)
    if context.climate == defines.climate.gaia then
        return growth * 120 // 100
    end
    return growth
end)
```

Rules compose as:

```text
native base -> package A -> package B -> validated final value
```

Rule callbacks cannot mutate game state or storage.

### `population.growth`

Context fields are:

```text
colony_id
planet_id
owner_id
race_id
race_population
total_population
population_limit
climate
housing
population_boom
plague
```

The result must fit a signed 16-bit integer. In multiplayer, this rule runs only during the elected gameplay host's authoritative strategic turn; clients receive the resulting state.

### `combat.beam_damage`

Context fields are:

```text
attacker_player_id
target_player_id
weapon_id
beam_ordinal
beam_count
```

The result must be an integer from 0 through 32767. It modifies successful strategic beam damage after native weapon and attacker bonuses and before target defenses. Current network games reject this rule.

## Exclusive service

API version 1 supports one exclusive service:

```lua
services.register("galaxy_generator", 1, function()
    -- Inspect or transform the generated candidate through game objects.
end)
```

The callback takes no arguments. It runs after native new-game construction and can use the topology operations listed below. Only one active package may register the service. The engine validates and atomically installs the completed candidate; callback or topology validation failure discards it, although completed RNG draws remain consumed. The service is not invoked when loading a save.

In multiplayer, the host runs the provider and clients install the authoritative result.

## `game` collections and objects

Most collections provide `get(id)` and `all()`. `all()` is a deterministic one-based sequence. Mutating properties and methods remain subject to lifecycle permissions even when listed here.

### Players

`game.players:get(id)` and `game.players:all()` return player proxies with:

| Member | Meaning |
| --- | --- |
| `kind`, `id`, `valid` | Stable identity and validity. |
| `is_human`, `government` | Read-only player properties. |
| `home_planet`, `home_system` | Stable related objects, or nil when absent. |
| `technologies` | Player technology interface. |
| `has_trait(id)` | Test a built-in or custom trait. |
| `add_research(amount)` | Add research when the current mutable lifecycle permits it. |
| `set_home_planet(planet)` | Galaxy-generator operation requiring an owned colony. |

`player.technologies` provides:

```text
status(application_id)
is_available(application_id)
is_researched(application_id)
field_of(application_id)
alternatives(field_id)
set_available(application_id, available)
set_status(application_id, status)
replace_available(removed_id, first_id, second_id, choose_first)
set_field_application(field_id, application_id)
```

Mutation requires a compatible mutable lifecycle. `replace_available` is restricted to `new_game.player_setup`. Canonical string IDs and `defines.technology_status` replace raw status bytes and table positions.

### Ships

`game.ships:get(id)` and `game.ships:all()` return proxies with `kind`, `id`, `valid`, `owner`, `owner_id`, `status`, `system`, `x`, and `y`. `set_system(system)` and `delete()` are structural galaxy-generator operations. `game.ships:copy(source, system)` creates a candidate generated ship during that service.

### Planets

`game.planets:get(id)` and `game.planets:all()` return proxies with:

```text
kind, id, valid
system, orbit, planet_type
size, gravity, climate, minerals, special
```

Validated assignment to `size`, `gravity`, `climate`, and `minerals` is available in planet-mutable generation lifecycles. Use `set_special(value)` for specials. `set_orbit(system, orbit)` and `delete()` are structural galaxy-generator operations. `game.planets:copy(source, system, orbit)` creates an uncolonized candidate planet during that service.

### Systems

`game.systems:get(id)` and `game.systems:all()` return proxies with:

```text
kind, id, valid
x, y, spectral_class, special, size, picture, visited
wormhole, planets
```

`system.planets:all()` returns planets in orbit order. During `galaxy_generator`, validated assignment is supported for `x`, `y`, `spectral_class`, `special`, and `visited`; use `set_wormhole(system_or_nil)` for reciprocal links and `delete()` for an eligible non-home system. `game.systems:copy(source)` creates a candidate system.

`game.map_width` and `game.map_height` are read-only map bounds.

### Global technology

`game.technology` contains `applications`, `fields`, and `starting_fields` collections. Application and field collections provide `get(id)` and `all()`; `starting_fields:all()` returns its ordered source list.

Applications expose `id` and `field`. Fields expose `id`, `cost`, `previous`, `next`, and ordered `applications`. During `new_game.technology_setup`, assign `application.field`, `field.previous`, or `field.next` to change the source graph. The engine validates membership, capacity, reciprocal links, branch coverage, cycles, and required starting fields after all callbacks.

### Leaders and nebulas

`game.leaders:get(id)` and `game.leaders:all()` return leader proxies with `kind`, `id`, `valid`, `name`, `status`, and `system` where applicable. `game.leaders:copy(source, unused_target, system)` places a copied marooned leader during `galaxy_generator`.

`game.nebulas:get(id)` and `game.nebulas:all()` return proxies with `kind`, `id`, `valid`, `x`, `y`, and `type`. During `galaxy_generator`, `x`, `y`, and `type` are writable, `delete()` removes a candidate nebula, and `game.nebulas:create(x, y, type)` creates one.

## Capability and lifetime rules

- Event payloads and rule contexts reject assignment.
- A mutable event permits only operations implemented for its current native owner.
- Rule callbacks are read-only regardless of the ordinary object's wider API.
- Topology creation, deletion, movement, and link changes are restricted to `galaxy_generator`.
- Setup technology mutations are restricted to `new_game.technology_setup`.
- Handles from a disposable galaxy candidate expire when that candidate is installed or discarded.
- Stored player references remain stable; stored ship references use a slot and generation so reuse expires old references.
- Unknown properties and operations fail rather than exposing native data accidentally.