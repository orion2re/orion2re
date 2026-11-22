local PADDING_X = 50
local PADDING_Y = 50
local SCALE = 90
local SHIP_STATUS_AT_STAR = 0

local function abs(value)
  if value < 0 then
    return -value
  end
  return value
end

local function trunc_div(numerator, denominator)
  return numerator // denominator
end

local function extend_bounds(bounds, system)
  if bounds == nil then
    return {
      min_x = system.x,
      min_y = system.y,
      max_x = system.x,
      max_y = system.y,
    }
  end
  bounds.min_x = math.min(bounds.min_x, system.x)
  bounds.min_y = math.min(bounds.min_y, system.y)
  bounds.max_x = math.max(bounds.max_x, system.x)
  bounds.max_y = math.max(bounds.max_y, system.y)
  return bounds
end

local function remove_random(values)
  if #values == 0 then
    error("mirror source has no available destination")
  end
  return table.remove(values, game.rng:integer(0, #values))
end

local function transformed(value, translate, transform)
  return transform(value) + translate
end

services.register("galaxy_generator", 1, function()
  local players = game.players:all()
  if #players == 0 then
    return
  end

  local systems = game.systems:all()
  local home_by_system = {}
  local homes = {}
  for player_index, player in ipairs(players) do
    local home = player.home_system
    if home == nil then
      error("mirror player has no home system")
    end
    local record = {
      player_index = player_index,
      system = home,
    }
    home_by_system[home.id] = record
    homes[#homes + 1] = record
  end

  for _, system in ipairs(systems) do
    local home = home_by_system[system.id]
    if home == nil then
      system.visited = 0
    else
      system.visited = 1 << (home.player_index - 1)
    end
  end

  local slice_columns
  local slice_rows
  if #homes == 2 then
    slice_columns, slice_rows = 2, 1
  elseif #homes <= 4 then
    slice_columns, slice_rows = 2, 2
  elseif #homes <= 6 then
    slice_columns, slice_rows = 3, 2
  else
    slice_columns, slice_rows = 4, 2
  end
  local slice_count = slice_columns * slice_rows
  local slice_width = (game.map_width - 2 * PADDING_X) // slice_columns
  local slice_height = (game.map_height - 2 * PADDING_Y) // slice_rows
  local maximum_slice_systems =
      (#systems + slice_count - 1) // slice_count

  local non_home_systems = {}
  for _, system in ipairs(systems) do
    if home_by_system[system.id] == nil then
      non_home_systems[#non_home_systems + 1] = system
    end
  end

  local slices = {}
  for _, home in ipairs(homes) do
    local home_system = home.system
    table.sort(non_home_systems, function(left, right)
      local left_distance = math.max(
          abs(left.x - home_system.x), abs(left.y - home_system.y))
      local right_distance = math.max(
          abs(right.x - home_system.x), abs(right.y - home_system.y))
      return left_distance < right_distance or
          (left_distance == right_distance and left.id < right.id)
    end)

    local source_systems = {home_system}
    local source_ids = {[home_system.id] = true}
    local bounds = extend_bounds(nil, home_system)
    for _, system in ipairs(non_home_systems) do
      if abs(bounds.min_x - system.x) <= slice_width and
          abs(bounds.max_x - system.x) <= slice_width and
          abs(bounds.min_y - system.y) <= slice_height and
          abs(bounds.max_y - system.y) <= slice_height then
        bounds = extend_bounds(bounds, system)
        source_systems[#source_systems + 1] = system
        source_ids[system.id] = true
        if #source_systems + 1 >= maximum_slice_systems then
          break
        end
      end
    end
    slices[#slices + 1] = {
      bounds = bounds,
      home = home,
      source_ids = source_ids,
      systems = source_systems,
    }
  end

  local best_index
  local best_value
  for index, slice in ipairs(slices) do
    local width = slice.bounds.max_x - slice.bounds.min_x
    local height = slice.bounds.max_y - slice.bounds.min_y
    if width == 0 then
      error("mirror source slice has zero width")
    end
    local value = abs(trunc_div(slice_height * 100, slice_width) -
        trunc_div(height * 100, width))
    if best_value == nil or
        #slices[best_index].systems < #slice.systems or
        value < best_value then
      best_index = index
      best_value = value
    end
  end

  local source_slice = slices[best_index]
  local source_home = source_slice.home.system
  local center_x = PADDING_X + slice_width // 2
  local center_y = PADDING_Y + slice_height // 2
  local move_x = center_x -
      trunc_div(source_slice.bounds.max_x + source_slice.bounds.min_x, 2)
  local move_y = center_y -
      trunc_div(source_slice.bounds.max_y + source_slice.bounds.min_y, 2)
  for _, system in ipairs(source_slice.systems) do
    system.x = system.x + move_x
    system.y = system.y + move_y
  end

  local function identity(value)
    return value
  end
  local function flip_x(value)
    return 2 * PADDING_X + slice_width - value
  end
  local function flip_y(value)
    return 2 * PADDING_Y + slice_height - value
  end

  local source_x_transform = identity
  local source_y_transform = identity
  if source_home.x > center_x then
    source_x_transform = flip_x
  end
  if source_home.y > center_y then
    source_y_transform = flip_y
  end
  for _, system in ipairs(source_slice.systems) do
    system.x = source_x_transform(system.x)
    system.y = source_y_transform(system.y)
    if SCALE ~= 100 then
      system.x = center_x + trunc_div((system.x - center_x) * SCALE, 100)
      system.y = center_y + trunc_div((system.y - center_y) * SCALE, 100)
    end
  end

  local source_non_home_systems = {}
  local source_planets = {}
  for _, system in ipairs(source_slice.systems) do
    if system ~= source_home then
      source_non_home_systems[#source_non_home_systems + 1] = system
      source_planets[system.id] = system.planets:all()
    end
  end

  for _, system in ipairs(systems) do
    if home_by_system[system.id] == nil and
        source_slice.source_ids[system.id] == nil then
      system:delete()
    end
  end

  local source_wormholes = {}
  for _, system in ipairs(source_non_home_systems) do
    local destination = system.wormhole
    if destination ~= nil and system.id < destination.id and
        source_slice.source_ids[destination.id] and
        home_by_system[destination.id] == nil then
      source_wormholes[#source_wormholes + 1] = {system, destination}
    end
  end

  local marooned_by_system = {}
  local unused_leaders = {}
  for _, leader in ipairs(game.leaders:all()) do
    if leader.status == defines.leader_status.marooned then
      marooned_by_system[leader.system.id] = leader
    elseif leader.status == defines.leader_status.unowned then
      unused_leaders[#unused_leaders + 1] = leader
    end
  end

  local source_monsters = {}
  for _, ship in ipairs(game.ships:all()) do
    if ship.status == SHIP_STATUS_AT_STAR and ship.owner_id >= 8 and
        ship.system ~= nil and
        source_slice.source_ids[ship.system.id] and
        home_by_system[ship.system.id] == nil then
      source_monsters[#source_monsters + 1] = ship
    end
  end

  local destinations = {}
  for index, home in ipairs(homes) do
    if index ~= best_index then
      destinations[#destinations + 1] = home.system
    end
  end

  local images = {}
  local slice_index = 1
  while #destinations > 0 do
    local destination_home = remove_random(destinations)
    local image = {}
    images[#images + 1] = image
    local translate_x = (slice_index % slice_columns) * slice_width
    local translate_y = (slice_index // slice_columns) * slice_height
    local x_transform = identity
    local y_transform = identity
    if slice_count == 2 then
      if translate_x > 0 or translate_y > 0 then
        x_transform, y_transform = flip_x, flip_y
      end
    elseif slice_count == 4 then
      if translate_x > 0 then
        x_transform = flip_x
      end
      if translate_y > 0 then
        y_transform = flip_y
      end
    elseif translate_y > 0 then
      x_transform, y_transform = flip_x, flip_y
    end

    for _, source in ipairs(source_non_home_systems) do
      local copy = game.systems:copy(source)
      copy.x = transformed(source.x, translate_x, x_transform)
      copy.y = transformed(source.y, translate_y, y_transform)
      image[source.id] = copy
      for _, planet in ipairs(source_planets[source.id]) do
        game.planets:copy(planet, copy, planet.orbit)
      end
      if source.special == defines.planet_special.marooned_hero then
        local leader = marooned_by_system[source.id]
        if leader == nil then
          error("mirror source system " .. source.id ..
              " has a marooned-hero special without a leader")
        end
        game.leaders:copy(leader, remove_random(unused_leaders), copy)
      end
    end

    destination_home.x = transformed(
        source_home.x, translate_x, x_transform)
    destination_home.y = transformed(
        source_home.y, translate_y, y_transform)
    destination_home.spectral_class = source_home.spectral_class
    slice_index = slice_index + 1
  end

  for _, image in ipairs(images) do
    for _, wormhole in ipairs(source_wormholes) do
      image[wormhole[1].id]:set_wormhole(image[wormhole[2].id])
    end
    for _, monster in ipairs(source_monsters) do
      game.ships:copy(monster, image[monster.system.id])
    end
  end

  local nebulas = game.nebulas:all()
  local nebula_width = 180
  local nebula_height = 180
  local usable_width = game.map_width - 2 * PADDING_X
  local usable_height = game.map_height - 2 * PADDING_Y
  local middle_x = usable_width // 2
  local middle_y = usable_height // 2
  local function move_nebula(index, x, y)
    nebulas[index].x = x + PADDING_X - nebula_width // 2
    nebulas[index].y = y + PADDING_Y - nebula_height // 2
  end
  local function twin_nebulas(index)
    local x = game.rng:integer(0, usable_width - nebula_width) +
        nebula_width // 2
    local y = game.rng:integer(0, usable_height - nebula_height) +
        nebula_height // 2
    move_nebula(index, x, y)
    move_nebula(index + 1, usable_width - x, usable_height - y)
  end
  local function horizontal_nebulas(index)
    local x = game.rng:integer(0, usable_width - nebula_width) +
        nebula_width // 2
    move_nebula(index, x, middle_y)
    move_nebula(index + 1, usable_width - x, middle_y)
  end
  local function vertical_nebulas(index)
    local y = game.rng:integer(0, usable_height - nebula_height) +
        nebula_height // 2
    move_nebula(index, middle_x, y)
    move_nebula(index + 1, middle_x, usable_height - y)
  end

  if #nebulas == 1 then
    move_nebula(1, middle_x, middle_y)
  elseif slice_count == 2 then
    if #nebulas == 2 then
      twin_nebulas(1)
    elseif #nebulas == 3 then
      move_nebula(1, middle_x, middle_y)
      twin_nebulas(2)
    elseif #nebulas == 4 then
      twin_nebulas(1)
      twin_nebulas(3)
    end
  elseif slice_count == 4 then
    if #nebulas == 2 then
      horizontal_nebulas(1)
    elseif #nebulas == 3 then
      move_nebula(1, middle_x, middle_y)
      horizontal_nebulas(2)
    elseif #nebulas == 4 then
      horizontal_nebulas(1)
      vertical_nebulas(3)
    end
  elseif slice_count == 6 then
    if #nebulas == 2 then
      move_nebula(1, slice_width, slice_height)
      move_nebula(2, 2 * slice_width, slice_height)
    elseif #nebulas == 3 then
      move_nebula(1, slice_width // 2, slice_height)
      move_nebula(2, slice_width // 2 + slice_width, slice_height)
      move_nebula(3, slice_width // 2 + 2 * slice_width, slice_height)
    elseif #nebulas == 4 then
      move_nebula(1, slice_width, slice_height // 2)
      move_nebula(2, slice_width, slice_height // 2)
      move_nebula(3, 2 * slice_width, slice_height + slice_height // 2)
      move_nebula(4, 2 * slice_width, slice_height + slice_height // 2)
    end
  elseif slice_count == 8 then
    if #nebulas == 2 then
      move_nebula(1, slice_width, slice_height)
      move_nebula(2, 3 * slice_width, slice_height)
    elseif #nebulas == 3 then
      move_nebula(1, slice_width, slice_height)
      move_nebula(2, 2 * slice_width, slice_height)
      move_nebula(3, 3 * slice_width, slice_height)
    elseif #nebulas == 4 then
      move_nebula(1, slice_width // 2, slice_height)
      move_nebula(2, slice_width // 2 + slice_width, slice_height)
      move_nebula(3, slice_width // 2 + 2 * slice_width, slice_height)
      move_nebula(4, slice_width // 2 + 3 * slice_width, slice_height)
    end
  end
end)