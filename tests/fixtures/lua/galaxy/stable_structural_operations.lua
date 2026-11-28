services.register("galaxy_generator", 1, function()
  local systems = game.systems:all()
  local source = systems[1]
  local removed = systems[2]
  local removed_planets = removed.planets
  local removed_members = removed_planets:all()
  removed:delete()
  if removed.valid then
    error("deleted system remained valid")
  end

  local copy = game.systems:copy(source)
  if removed.valid or removed == copy then
    error("system slot retargeted")
  end
  if pcall(function() return removed_planets:all() end) then
    error("deleted system's planet collection retargeted")
  end
  for _, member in ipairs(removed_members) do
    if member.valid then error("deleted system's planet remained valid") end
  end
  if not copy.valid or copy.x ~= source.x or copy.y ~= source.y or
      copy.spectral_class ~= source.spectral_class or
      copy.size ~= source.size or copy.picture ~= source.picture or
      copy.special ~= source.special or copy.visited ~= source.visited then
    error("system copy did not preserve scalar properties")
  end
  copy.x = 70
  copy.y = 80
  source:set_wormhole(copy)
  if source.wormhole ~= copy or copy.wormhole ~= source then
    error("wormhole was not reciprocal")
  end

  local source_planet = game.planets:get(0)
  local planet = game.planets:copy(source_planet, copy, 1)
  if planet.system ~= copy or planet.orbit ~= 1 or
      planet.planet_type ~= source_planet.planet_type or
      planet.size ~= source_planet.size or
      planet.gravity ~= source_planet.gravity or
      planet.climate ~= source_planet.climate or
      planet.minerals ~= source_planet.minerals or
      planet.special ~= source_planet.special then
    error("planet copy did not preserve scalar properties")
  end
  planet:delete()
  if planet.valid then
    error("deleted planet remained valid")
  end
  local replacement = game.planets:copy(source_planet, copy, 1)
  if planet.valid or planet == replacement then
    error("planet slot retargeted")
  end
  if not replacement.valid then
    error("new planet handle is invalid")
  end

  local source_ship = game.ships:get(0)
  local ship = game.ships:copy(source_ship, copy)
  if ship.owner_id ~= source_ship.owner_id or
      ship.status ~= source_ship.status or ship.system ~= copy or
      ship.x ~= copy.x or ship.y ~= copy.y then
    error("ship copy did not preserve placement properties")
  end
  ship:delete()
  if ship.valid then
    error("deleted ship remained valid")
  end

  local nebula = game.nebulas:create(30, 40, 1)
  nebula:delete()
  if nebula.valid then
    error("deleted nebula remained valid")
  end
  local new_nebula = game.nebulas:create(50, 60, 2)
  if nebula.valid or nebula == new_nebula or not new_nebula.valid then
    error("nebula slot retargeted")
  end
end)