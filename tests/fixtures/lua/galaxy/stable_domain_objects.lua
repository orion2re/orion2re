services.register("galaxy_generator", 1, function()
  if game.map_width ~= 506 or game.map_height ~= 400 then
    error("wrong map dimensions")
  end

  local players = game.players:all()
  if #players ~= 2 or players[1] ~= game.players:get(0) or
      players[2] ~= game.players:get(1) or
      game.players:get(-1) ~= nil or game.players:get(2) ~= nil then
    error("player collection is not stable and ordered")
  end
  if players[1].home_planet ~= game.planets:get(0) or
      players[1].home_system ~= game.systems:get(0) or
      players[2].home_planet ~= nil or players[2].home_system ~= nil then
    error("wrong player home relationships")
  end
  players[2]:set_home_planet(game.planets:get(1))
  if players[2].home_planet ~= game.planets:get(1) or
      players[2].home_system ~= game.systems:get(1) then
    error("player home assignment failed")
  end

  local systems = game.systems:all()
  local first, second = systems[1], systems[2]
  if #systems ~= 2 or first ~= game.systems:get(0) or
      second ~= game.systems:get(1) or game.systems:get(2) ~= nil then
    error("system collection is not stable and ordered")
  end
  first.x, first.y = 11, 21
  first.spectral_class = 2
  game.planets:get(2):set_special(defines.planet_special.none)
  first.special = 0
  first.visited = 3
  if first.x ~= 11 or first.y ~= 21 or
      first.spectral_class ~= 2 or first.special ~= 0 or
      first.size ~= 4 or first.picture ~= 5 or first.visited ~= 3 or
      first.wormhole ~= nil then
    error("wrong system properties")
  end
  local orbit_planets = first.planets:all()
  if #orbit_planets ~= 2 or orbit_planets[1] ~= game.planets:get(0) or
      orbit_planets[2] ~= game.planets:get(2) then
    error("system planets are not in orbit order")
  end
  first:set_wormhole(second)
  if first.wormhole ~= second or second.wormhole ~= first then
    error("wormhole is not reciprocal")
  end
  first:set_wormhole(nil)
  if first.wormhole ~= nil or second.wormhole ~= nil then
    error("wormhole unlink failed")
  end

  local planets = game.planets:all()
  local home, remote, moving = planets[1], planets[2], planets[3]
  if #planets ~= 3 or home ~= game.planets:get(0) or
      remote ~= game.planets:get(1) or moving ~= game.planets:get(2) or
      game.planets:get(3) ~= nil then
    error("planet collection is not stable and ordered")
  end
  for _, invalid_id in ipairs({-1, 65536, 4294967295, 4294967296}) do
    if game.planets:get(invalid_id) ~= nil then
      error("out-of-range planet ID aliased a live planet")
    end
  end
  if home.system ~= first or home.orbit ~= 0 or
      home.planet_type ~= defines.planet_type.planet or
      home.size ~= defines.planet_size.medium or
      home.gravity ~= defines.gravity.normal or
      home.climate ~= defines.climate.terran or
      home.minerals ~= defines.minerals.abundant or
      home.special ~= defines.planet_special.none then
    error("wrong planet properties")
  end
  home.size = defines.planet_size.huge
  home.gravity = defines.gravity.heavy
  home.climate = defines.climate.gaia
  home.minerals = defines.minerals.ultra_rich
  moving:set_special(defines.planet_special.none)
  home:set_special(defines.planet_special.gold_deposits)
  if home.size ~= defines.planet_size.huge or
      home.gravity ~= defines.gravity.heavy or
      home.climate ~= defines.climate.gaia or
      home.minerals ~= defines.minerals.ultra_rich or
      home.special ~= defines.planet_special.gold_deposits or
      first.special ~= defines.planet_special.gold_deposits then
    error("planet mutation failed")
  end
  moving:set_orbit(second, 1)
  if moving.valid then
    error("moved planet retained old identity")
  end
  moving = game.planets:get(2)
  if moving.system ~= second or moving.orbit ~= 1 then
    error("planet movement failed")
  end

  local ships = game.ships:all()
  local ship = ships[1]
  if #ships ~= 2 or ship ~= game.ships:get(0) or
      ships[2] ~= game.ships:get(1) or game.ships:get(2) ~= nil or
      ship.owner ~= players[1] or ship.owner_id ~= 0 or
      ship.status ~= 1 or ship.system ~= first or
      ship.x ~= 10 or ship.y ~= 20 then
    error("wrong stable ship collection or properties")
  end
  ship:set_system(second)
  if ship.system ~= second or ship.x ~= second.x or ship.y ~= second.y then
    error("ship movement failed")
  end

  local leaders = game.leaders:all()
  local source, unused = leaders[1], leaders[2]
  if #leaders ~= 67 or source ~= game.leaders:get(0) or
      unused ~= game.leaders:get(1) or game.leaders:get(67) ~= nil or
      source.kind ~= "leader" or source.id ~= 0 or
      source.name ~= "Pathfinder" or source.status ~= 5 or
      source.system ~= first then
    error("wrong stable leader state")
  end
  local placed = game.leaders:copy(source, unused, second)
  if unused.valid or placed ~= game.leaders:get(1) or
      placed.name ~= source.name or placed.status ~= 5 or
      placed.system ~= second then
    error("leader placement failed")
  end

  local first_nebula = game.nebulas:create(30, 40, 1)
  local second_nebula = game.nebulas:create(50, 60, 2)
  first_nebula.x, first_nebula.y, first_nebula.type = 31, 41, 3
  local nebulas = game.nebulas:all()
  if #nebulas ~= 2 or nebulas[1] ~= first_nebula or
      nebulas[2] ~= second_nebula or
      game.nebulas:get(0) ~= first_nebula or
      first_nebula.kind ~= "nebula" or first_nebula.id ~= 0 or
      first_nebula.x ~= 31 or first_nebula.y ~= 41 or
      first_nebula.type ~= 3 then
    error("wrong stable nebula state")
  end
  first_nebula:delete()
  local replacement = game.nebulas:create(70, 80, 4)
  if first_nebula.valid or replacement.id ~= 0 or
      replacement == first_nebula or replacement ~= game.nebulas:get(0) then
    error("nebula slot reuse retargeted an old handle")
  end
  nebulas = game.nebulas:all()
  if #nebulas ~= 2 or nebulas[1].id ~= 0 or nebulas[2].id ~= 1 then
    error("nebula ordering changed after slot reuse")
  end
end)