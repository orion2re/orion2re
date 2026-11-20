# Game-state refactor cleanup

## Leaders

- The fixed roster uses its catalog slot as the ID. Array order preserves
  the original traversal order for the oracle. Ship, star, colony, and player
  records, messages, events, special-leader config, Lua, the officer UI and
  incremental multiplayer packets still use narrow slot references. Those
  references would need migration if leaders ever become dynamically allocated.
- Pass `s_game_state` through remaining singleton callers as their entry points
  are migrated: officer rules/UI, combat and ship/colony/turn services, event
  messages, Lua and debug UI, resource reload, and the save/network entry points.
  Remove the temporary singleton swap for staged Lua galaxy generation when
  its callbacks can use the candidate state directly.
- Keep the oracle's v7-to-v8 byte normalization until baselines no longer use
  v7. It inserts only the implicit leader IDs, leaving all other bytes intact.

## Other game records

- Players, stars, ships, colonies, planets, and other state still live in
  legacy globals. Migrate them to ordered ID maps and remove remaining packed
  in-memory records and raw multiplayer transfers as each type is migrated.
