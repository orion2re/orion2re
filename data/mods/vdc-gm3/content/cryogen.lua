local function is_unavailable(technologies, application_id)
    return technologies:status(application_id) ==
        defines.technology_status.unavailable
end

local function all_unavailable(technologies, application_ids)
    for _, application_id in ipairs(application_ids) do
        if not is_unavailable(technologies, application_id) then
            return false
        end
    end
    return true
end

local function is_available(technologies, application_id)
    return technologies:status(application_id) ==
        defines.technology_status.available
end

local function set_available(technologies, application_id, available)
    technologies:set_status(application_id, available and
        defines.technology_status.available or
        defines.technology_status.unavailable)
end

local function eligible(player)
    return player.is_human and player:has_trait("uncreative")
end

local function legacy_players()
    local players = game.players:all()
    if #players > 1 then
        local first = table.remove(players, 1)
        table.insert(players, first)
    end
    return players
end

local function repair_primary_choices(player)
    local technologies = player.technologies

    local armor_roll = game.rng:integer(0, 3)
    if all_unavailable(technologies, {
            "zortrium_armor", "neutronium_armor", "adamantium_armor",
        }) then
        if armor_roll == 1 then
            set_available(technologies, "zortrium_armor", true)
            set_available(technologies, "nano_disassemblers", false)
            set_available(technologies, "zeon_missile", false)
        elseif armor_roll == 2 then
            set_available(technologies, "neutronium_armor", true)
            set_available(technologies, "microlite_construction", false)
            set_available(technologies, "uridium_fuel_cells", false)
        else
            set_available(technologies, "adamantium_armor", true)
            set_available(technologies, "thorium_fuel_cells", false)
        end
    end
    if is_unavailable(technologies, "planetary_supercomputer") then
        set_available(technologies, "planetary_supercomputer", true)
        set_available(technologies, "optronic_computer", false)
        set_available(technologies, "virtual_reality_network", false)
    end
    if all_unavailable(technologies, {
            "positronic_computer", "cybertronic_computer",
            "moleculartronic_computer",
        }) then
        set_available(technologies, "cybertronic_computer", true)
        set_available(technologies, "achilles_targeting_unit", false)
    end
    if is_unavailable(technologies, "evolutionary_mutation") then
        set_available(technologies, "evolutionary_mutation", true)
        set_available(technologies, "android_workers", false)
    end
    if is_unavailable(technologies, "anti_matter_bomb") then
        set_available(technologies, "anti_matter_bomb", true)
        set_available(technologies, "neutron_blaster", false)
        set_available(technologies, "neutron_scanner", false)
    end
    if is_available(technologies, "bomber_bays") and
            is_available(technologies, "heavy_fighter_bays") then
        set_available(technologies, "heavy_fighter_bays", false)
        set_available(technologies, "ground_batteries", false)
        set_available(technologies, "powered_armor", true)
    end
    local anti_matter_torpedo =
        technologies:is_available("anti_matter_torpedo")
    local proton_torpedo = technologies:is_available("proton_torpedo")
    local plasma_torpedo = technologies:is_available("plasma_torpedo")
    local torpedoes_unresearched =
        not technologies:is_researched("anti_matter_torpedo") and
        not technologies:is_researched("proton_torpedo") and
        not technologies:is_researched("plasma_torpedo")
    if torpedoes_unresearched and
            ((anti_matter_torpedo and proton_torpedo) or
             (anti_matter_torpedo and plasma_torpedo) or
             (proton_torpedo and plasma_torpedo)) then
        set_available(technologies, "disrupter_cannon", false)
        set_available(technologies, "neutronium_bomb", false)
        set_available(technologies, "particle_beam", false)
        set_available(technologies, "sensors", true)
    end
    if is_unavailable(technologies, "class_v_shield") then
        set_available(technologies, "class_v_shield", true)
        set_available(technologies, "gauss_cannon", false)
        set_available(technologies, "wide_area_jammer", false)
    end
    if is_unavailable(technologies, "hard_shields") then
        set_available(technologies, "hard_shields", true)
        set_available(technologies, "reflection_field", false)
    end
end

local function repair_weapons(player)
    local technologies = player.technologies
    if all_unavailable(technologies, {
            "disrupter_cannon", "death_ray", "mauler_device",
        }) then
        set_available(technologies, "mauler_device", true)
        set_available(technologies, "phase_shifter", false)
        set_available(technologies, "death_spores", false)
    end
    local merculite_missile =
        technologies:is_available("merculite_missile")
    local pulson_missile = technologies:is_available("pulson_missile")
    local zeon_missile = technologies:is_available("zeon_missile")
    local missiles_unresearched =
        not technologies:is_researched("merculite_missile") and
        not technologies:is_researched("pulson_missile") and
        not technologies:is_researched("zeon_missile")
    if missiles_unresearched and zeon_missile and
            (merculite_missile or pulson_missile) then
        set_available(technologies, "anti_matter_bomb", false)
        set_available(technologies, "neutron_blaster", false)
        set_available(technologies, "neutron_scanner", true)
    elseif missiles_unresearched and merculite_missile and
            pulson_missile and is_unavailable(technologies, "zeon_missile") then
        set_available(technologies, "anti_matter_bomb", false)
        set_available(technologies, "neutron_blaster", false)
        set_available(technologies, "neutron_scanner", true)
        set_available(technologies, "fusion_bomb", true)
        set_available(technologies, "fusion_beam", false)
        set_available(technologies, "fusion_rifle", false)
    end
end

local function repair_government(player)
    local technologies = player.technologies
    if player.government <= defines.government.federation then
        if is_unavailable(technologies, "holo_simulator") then
            set_available(technologies, "holo_simulator", true)
            set_available(technologies, "dauntless_guidance_system", false)
            set_available(technologies, "neural_scanner", false)
        end
        if is_unavailable(technologies, "heightened_intelligence") then
            set_available(technologies, "heightened_intelligence", true)
            set_available(technologies, "android_farmers", false)
            set_available(technologies, "android_scientists", false)
        end
    end
end

local function repair_cloning(player)
    local technologies = player.technologies
    if is_unavailable(technologies, "cloning_center") then
        set_available(technologies, "cloning_center", true)
        set_available(technologies, "subterranean_farms", false)
    end
end

local function repair_spaceport(player)
    local technologies = player.technologies
    if is_unavailable(technologies, "spaceport") then
        set_available(technologies, "spaceport", true)
        set_available(technologies, "alien_management_center", false)
    end
end

local function repair_gravity(player)
    local technologies = player.technologies
    local gravity_roll = game.rng:integer(0, 2)
    if is_available(technologies, "planetary_gravity_generator") then
        set_available(technologies, "planetary_gravity_generator", false)
        set_available(technologies, "anti_grav_harness", gravity_roll == 1)
        set_available(technologies, "graviton_beam", gravity_roll == 2)
    end
end

local function repair_pollution(player)
    local technologies = player.technologies
    if not player:has_trait("tolerant") and
            all_unavailable(technologies, {
                "pollution_processor", "atmospheric_renewer",
                "nano_disassemblers",
            }) then
        set_available(technologies, "core_waste_dumps", true)
        set_available(technologies, "battlestation", false)
        set_available(technologies, "titan_construction", false)
    end
end

local function repair_farming(player)
    local technologies = player.technologies
    if not player:has_trait("lithovore") and
            all_unavailable(technologies, {
                "soil_enrichment", "weather_control_system",
                "android_farmers",
            }) then
        set_available(technologies, "soil_enrichment", true)
        set_available(technologies, "microbiotics", false)
    end
end

local function repair_lithovore(player)
    local technologies = player.technologies
    if player:has_trait("lithovore") then
        local weather_control =
            technologies:is_available("weather_control_system")
        local universal_antidote =
            technologies:is_available("universal_antidote")
        if (weather_control and
                is_unavailable(technologies, "universal_antidote")) or
                (is_unavailable(technologies, "weather_control_system") and
                 universal_antidote) then
            set_available(technologies, "weather_control_system", false)
            set_available(technologies, "psionics", true)
            set_available(technologies, "universal_antidote", false)
        end
    end
end

return function(event)
    local players = game.players:all()
    if event.player_id ~= #players - 1 then return end
    local seed = game.rng:get_seed()
    local succeeded, diagnostic = pcall(function()
        players = legacy_players()
        for _, repair in ipairs({
                repair_primary_choices,
                repair_weapons,
                repair_government,
                repair_cloning,
                repair_spaceport,
            }) do
            for _, player in ipairs(players) do
                if eligible(player) then repair(player) end
            end
        end
        for _, player in ipairs(players) do
            if eligible(player) and player:has_trait("high_gravity") then
                repair_gravity(player)
            end
        end
        for _, repair in ipairs({
                repair_pollution, repair_farming, repair_lithovore,
            }) do
            for _, player in ipairs(players) do
                if eligible(player) then repair(player) end
            end
        end
    end)
    game.rng:set_seed(seed)
    if not succeeded then error(diagnostic, 0) end
end

