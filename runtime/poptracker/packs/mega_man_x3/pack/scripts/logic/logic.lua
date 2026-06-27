
function has(item, amount)
    local count = Tracker:ProviderCountForCode(item)
    amount = tonumber(amount)
    if not amount then
        return count > 0
    else
        return count >= amount
    end
end
function is_active(item)
    return Tracker:FindObjectForCode(item).Active
end

function can_charge()
    local arms = Tracker:FindObjectForCode("arms").CurrentStage
    return arms >= 2
end

function boss_weaknesses_not_required()
    local setting_weakness = Tracker:FindObjectForCode('setting_weakness').CurrentStage == 1
    return not setting_weakness
end

function boss_other_damage_possible()
    if Tracker:FindObjectForCode("z_saber").Active then return true end
    local strictness = Tracker:FindObjectForCode("boss_weakness_strictness").CurrentStage
    if strictness == 3 then return false end
    if strictness == 2 then
        return can_charge()
    end
    return true
end

function get_weapons_count()
    local weapons = 0
    if Tracker:FindObjectForCode("frost_shield").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("acid_burst").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("tornado_fang").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("triad_thunder").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("spinning_blade").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("ray_splasher").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("gravity_well").Active then weapons = weapons + 1 end
    if Tracker:FindObjectForCode("parasitic_bomb").Active then weapons = weapons + 1 end
    return weapons
end
function get_upgrades_count()
    local upgrades = 0
    upgrades = upgrades + Tracker:FindObjectForCode("third_armor_helmet").CurrentStage
    upgrades = upgrades + Tracker:FindObjectForCode("third_armor_body").CurrentStage
    upgrades = upgrades + Tracker:FindObjectForCode("third_armor_legs").CurrentStage
    local arms = Tracker:FindObjectForCode("arms").CurrentStage
    if Tracker:FindObjectForCode('jammed_buster').CurrentStage == 0 then
        upgrades = upgrades + arms - 1
    else
        upgrades = upgrades + arms
    end
    return upgrades
end

function vile_codes_req_met()
    return Tracker:FindObjectForCode("stage_vile").Active
end
function vile_medals_req_met()
    local mavericks = Tracker:ProviderCountForCode("maverick_medal")
    local mavericks_needed = Tracker:ProviderCountForCode("vile_medal_count")
    return mavericks >= mavericks_needed
end
function vile_weapons_req_met()
    local weapons = get_weapons_count()
    local weapons_needed = Tracker:ProviderCountForCode("vile_weapon_count")
    return weapons >= weapons_needed
end
function vile_upgrade_req_met()
    local upgrades = get_upgrades_count()
    local upgrades_needed = Tracker:ProviderCountForCode("vile_upgrade_count")
    return upgrades >= upgrades_needed
end
function vile_heart_tanks_req_met()
    local heart_tanks = Tracker:ProviderCountForCode("heart_tank")
    local heart_tanks_needed = Tracker:ProviderCountForCode("vile_heart_tank_count")
    return heart_tanks >= heart_tanks_needed
end
function vile_sub_tanks_req_met()
    local sub_tanks = Tracker:ProviderCountForCode("sub_tank")
    local sub_tanks_needed = Tracker:ProviderCountForCode("vile_sub_tank_count")
    return sub_tanks >= sub_tanks_needed
end
function vile_all_req_met()
    return vile_medals_req_met() and vile_weapons_req_met() and vile_upgrade_req_met() and vile_heart_tanks_req_met() and vile_sub_tanks_req_met()
end

function is_vile_open()
    local allreqs = Tracker:ProviderCountForCode("vile_sub_tank_count") + Tracker:ProviderCountForCode("vile_heart_tank_count") + Tracker:ProviderCountForCode("vile_upgrade_count") + Tracker:ProviderCountForCode("vile_weapon_count") + Tracker:ProviderCountForCode("vile_medal_count")
    if allreqs == 0 then
        return vile_codes_req_met()
    end
    return vile_all_req_met()
end

function update_vile_state()
    local vilestate = Tracker:FindObjectForCode('vile_state')
    if Tracker:FindObjectForCode('vile_cleared').Active then
        vilestate.CurrentStage = 2
    elseif is_vile_open() then
        vilestate.CurrentStage = 1
    else
        vilestate.CurrentStage = 0
    end
end


function doppler_codes_req_met()
    return Tracker:FindObjectForCode("stage_doppler_lab").Active
end
function doppler_medals_req_met()
    local mavericks = Tracker:ProviderCountForCode("maverick_medal")
    local mavericks_needed = Tracker:ProviderCountForCode("doppler_medal_count")
    return mavericks >= mavericks_needed
end
function doppler_weapons_req_met()
    local weapons = get_weapons_count()
    local weapons_needed = Tracker:ProviderCountForCode("doppler_weapon_count")
    return weapons >= weapons_needed
end
function doppler_upgrade_req_met()
    local upgrades = get_upgrades_count()
    local upgrades_needed = Tracker:ProviderCountForCode("doppler_upgrade_count")
    return upgrades >= upgrades_needed
end
function doppler_heart_tanks_req_met()
    local heart_tanks = Tracker:ProviderCountForCode("heart_tank")
    local heart_tanks_needed = Tracker:ProviderCountForCode("doppler_heart_tank_count")
    return heart_tanks >= heart_tanks_needed
end
function doppler_sub_tanks_req_met()
    local sub_tanks = Tracker:ProviderCountForCode("sub_tank")
    local sub_tanks_needed = Tracker:ProviderCountForCode("doppler_sub_tank_count")
    return sub_tanks >= sub_tanks_needed
end
function doppler_all_req_met()
    return doppler_medals_req_met() and doppler_weapons_req_met() and doppler_upgrade_req_met() and doppler_heart_tanks_req_met() and doppler_sub_tanks_req_met()
end

function is_doppler_open()
    local allreqs = Tracker:ProviderCountForCode("doppler_sub_tank_count") + Tracker:ProviderCountForCode("doppler_heart_tank_count") + Tracker:ProviderCountForCode("doppler_upgrade_count") + Tracker:ProviderCountForCode("doppler_weapon_count") + Tracker:ProviderCountForCode("doppler_medal_count")

    local vilereq = Tracker:FindObjectForCode("logic_vile_required").CurrentStage
    local vilebeat = Tracker:FindObjectForCode("vile_cleared").Active
    local vile_cleared_met = true
    if vilereq > 0 then
        vile_cleared_met = vilebeat
    end
    if allreqs == 0 then
        return doppler_codes_req_met() and vile_cleared_met
    end
    return doppler_all_req_met() and vile_cleared_met

end
function update_doppler_state()
    local dopplerstate = Tracker:FindObjectForCode('drdoppler_state')
    if Tracker:FindObjectForCode('doppler_1_cleared').Active and Tracker:FindObjectForCode('doppler_2_cleared').Active and Tracker:FindObjectForCode('doppler_3_cleared').Active then
        dopplerstate.CurrentStage = 2
    elseif is_doppler_open() then
        dopplerstate.CurrentStage = 1
    else
        dopplerstate.CurrentStage = 0
    end
end
function are_doppler_two_and_three_open()
    if Tracker:FindObjectForCode('doppler_all_labs').CurrentStage > 0 then
        return Tracker:FindObjectForCode('drdoppler_state').CurrentStage > 0
    end
    return false
end

function doppler_1_cleared()
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 1/Dr. Doppler's Lab 1 Boss").AvailableChestCount == 0 then
        return true
    end
    return false
end
function doppler_2_cleared()
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 2/Dr. Doppler's Lab 2 Boss").AvailableChestCount == 0 then
        return true
    end
    return false
end

function is_bit_open()
    local mavericks = Tracker:ProviderCountForCode("maverick_medal")
    local mavericks_needed = Tracker:ProviderCountForCode("bit_medal_count")
    return mavericks >= mavericks_needed
end
function is_byte_open()
    local mavericks = Tracker:ProviderCountForCode("maverick_medal")
    local mavericks_needed = Tracker:ProviderCountForCode("byte_medal_count")
    return mavericks >= mavericks_needed
end
function rematch_quota_met()
    local quota = Tracker:ProviderCountForCode("doppler_lab_3_boss_rematch_count")
    --local count = Tracker:ProviderCountForCode("rematch_fights")
    local count = 0
    --print(string.format("refight quota: %i, refights done: %i", quota, count))
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Blizzard Buffalo (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Toxic Seahorse (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Tunnel Rhino (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Volt Catfish (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Crush Crawfish (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Neon Tiger (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Gravity Beetle (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    if Tracker:FindObjectForCode("@Stages/Dr. Doppler's Lab 3/Blast Hornet (Rematch)").AvailableChestCount == 0 then
        count = count + 1
    end
    --print("found refights done: ", count)
    if count >= quota then
        return true
    else
        return false
    end
end

function refights_required()
    return Tracker:ProviderCountForCode("doppler_lab_3_boss_rematch_count") > 0
end

function print_debug_doppler()
    print("get_weapons_count(): ", get_weapons_count())
    print("get_upgrades_count(): ", get_upgrades_count())
    print("doppler_codes_req_met(): ", doppler_codes_req_met())
    print("doppler_medals_req_met(): ", doppler_medals_req_met())
    print("doppler_weapons_req_met(): ", doppler_weapons_req_met())
    print("doppler_upgrade_req_met(): ", doppler_upgrade_req_met())
    print("doppler_heart_tanks_req_met(): ", doppler_heart_tanks_req_met())
    print("doppler_sub_tanks_req_met(): ", doppler_sub_tanks_req_met())
    print("doppler_all_req_met(): ", doppler_all_req_met())
    print("is_doppler_open(): ", is_doppler_open())
    print("doppler_open object: ", Tracker:ProviderCountForCode("doppler_open"))
end

function print_debug_vile()
    print("get_weapons_count(): ", get_weapons_count())
    print("get_upgrades_count(): ", get_upgrades_count())
    print("vile_codes_req_met(): ", vile_codes_req_met())
    print("vile_medals_req_met(): ", vile_medals_req_met())
    print("vile_weapons_req_met(): ", vile_weapons_req_met())
    print("vile_upgrade_req_met(): ", vile_upgrade_req_met())
    print("vile_heart_tanks_req_met(): ", vile_heart_tanks_req_met())
    print("vile_sub_tanks_req_met(): ", vile_sub_tanks_req_met())
    print("vile_all_req_met(): ", vile_all_req_met())
    print("is_vile_open(): ", is_vile_open())
    print("vile_open object: ", Tracker:ProviderCountForCode("vile_open"))
end

WEAPON_CHECKS = {
    [0x00] = function() return true end, --Lemon
    [0x01] = function() return Tracker:FindObjectForCode("arms").CurrentStage >= 1 end, --Charged Shot (Level 1)
    [0x02] = function() return is_active("z_saber") end, --Z-Saber (Slash)
    [0x03] = function() return Tracker:FindObjectForCode("arms").CurrentStage >= 1 end, --Charged Shot (Level 2)
    [0x04] = function() return is_active("z_saber") end, --Z-Saber (Beam)
    [0x05] = function() return is_active("z_saber") end, --Z-Saber (Beam slashes)
    [0x06] = function() return true end, --Lemon (Dash)
    [0x07] = function() return is_active("acid_burst") end, --Uncharged Acid Burst
    [0x08] = function() return is_active("parasitic_bomb") end, --Uncharged Parasitic Bomb
    [0x09] = function() return is_active("triad_thunder") end, --Uncharged Triad Thunder (Contact)
    [0x0A] = function() return is_active("spinning_blade") end, --Uncharged Spinning Blade
    [0x0C] = function() return is_active("gravity_well") end, --Gravity Well
    [0x0D] = function() return is_active("frost_shield") end, --Uncharged Frost Shield
    [0x0E] = function() return is_active("tornado_fang") end, --Uncharged Tornado Fang
    [0x10] = function() return can_charge() and is_active("acid_burst") end, --Charged Acid Burst
    [0x11] = function() return can_charge() and is_active("parasitic_bomb") end, --Charged Parasitic Bomb
    [0x12] = function() return can_charge() and is_active("triad_thunder") end, --Charged Triad Thunder
    [0x13] = function() return can_charge() and is_active("spinning_blade") end, --Charged Spinning Blade
    [0x15] = function() return is_active("gravity_well") end, --Also Gravity Well
    [0x16] = function() return can_charge() and is_active("frost_shield") end, --Charged Frost Shield (On hand)
    [0x17] = function() return can_charge() and is_active("tornado_fang") end, --Charged Tornado Fang
    [0x18] = function() return is_active("acid_burst") end, --Acid Burst (Small uncharged bubbles)
    [0x1B] = function() return is_active("triad_thunder") end, --Uncharged Triad Thunder (Thunder)
    [0x1C] = function() return is_active("ray_splasher") end, --Ray Splasher
    [0x1D] = function() return can_charge() end, --Charged Shot (Level 3)
    [0x1F] = function() return Tracker:FindObjectForCode("arms").CurrentStage >= 3 end, --Charged Shot (Level 4, Main projectile)
    [0x20] = function() return Tracker:FindObjectForCode("arms").CurrentStage >= 3 end, --Charged Shot (Level 4, Secondary projectile)
    [0x21] = function() return can_charge() and is_active("frost_shield") end, --Charged Frost Shield (Lotus)
}

--vanilla weaknesses
BOSS_WEAKNESSES  = {
    ["Blizzard Buffalo"] = {[1] = 8,[2] = 17,},
    ["Sigma"] = {[1] = 13,[2] = 22,[3] = 33,[4] = 10,[5] = 18,},
    ["Bit"] = {[1] = 9,[2] = 18,[3] = 27,[4] = 13,[5] = 22,[6] = 33,},
    ["Vile"] = {[1] = 28,[2] = 10,[3] = 18,},
    ["Shurikein"] = {[1] = 0,[2] = 1,[3] = 3,[4] = 6,[5] = 29,[6] = 31,[7] = 32,},
    ["Worm Seeker-R"] = {[1] = 0,[2] = 1,[3] = 3,[4] = 6,[5] = 29,[6] = 31,[7] = 32,},
    ["Volt Kurageil"] = {[1] = 9,[2] = 18,[3] = 27,[4] = 13,[5] = 22,[6] = 33,},
    ["Hell Crusher"] = {[1] = 0,[2] = 1,[3] = 3,[4] = 6,[5] = 29,[6] = 31,[7] = 32,},
    ["Volt Catfish"] = {[1] = 14,[2] = 23,},
    ["Dr. Doppler's Lab 2 Boss"] = {[1] = 9,[2] = 18,[3] = 27,[4] = 13,[5] = 22,[6] = 33,},
    ["Tunnel Rhino"] = {[1] = 7,[2] = 16,[3] = 24,},
    ["Crush Crawfish"] = {[1] = 9,[2] = 18,[3] = 27,},
    ["Toxic Seahorse"] = {[1] = 13,[2] = 22,[3] = 33,},
    ["Vile Goliath"] = {[1] = 8,[2] = 17,[3] = 14,[4] = 23,},
    ["Gravity Beetle"] = {[1] = 28,},
    ["Godkarmachine"] = {[1] = 28,},
    ["Neon Tiger"] = {[1] = 10,[2] = 18,},
    ["Kaiser Sigma"] = {[1] = 0,[2] = 1,[3] = 3,[4] = 6,[5] = 29,[6] = 31,[7] = 32,},
    ["Hotareeca"] = {[1] = 0,[2] = 1,[3] = 3,[4] = 6,[5] = 29,[6] = 31,[7] = 32,},
    ["Doppler"] = {[1] = 7,[2] = 16,[3] = 24,},
    ["Byte"] = {[1] = 14,[2] = 23,[3] = 28,},
    ["Blast Hornet"] = {[1] = 12,[2] = 12,[3] = 21,},
    ["Press Disposer"] = {[1] = 14,[2] = 23,[3] = 28,},
}

function has_weakness_for(bossname)
    --print(string.format("Checking weaknesses for %s", bossname))
    for _,weapon in ipairs(BOSS_WEAKNESSES[bossname]) do
        local fn = WEAPON_CHECKS[weapon]
        --print(string.format("has weakness for weapon 0x%x: %s", weapon, fn()))
        if fn() then return true end
    end
    --print("Player does not have weakness")
    return false
end
