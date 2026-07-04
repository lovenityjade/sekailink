ScriptHost:LoadScript("scripts/autotracking/item_mapping.lua")
ScriptHost:LoadScript("scripts/autotracking/location_mapping.lua")

CUR_INDEX = -1
SLOT_DATA = nil
LOCAL_ITEMS = {}
GLOBAL_ITEMS = {}

TAB_SWITCH_KEY = ""

TAB_MAPPING = {
    [00] = "Stage Select",
    [01] = "Hunter Base",
    [02] = "Maverick Stages/Blast Hornet",
    [03] = "Maverick Stages/Blizzard Buffalo",
    [04] = "Maverick Stages/Gravity Beetle",
    [05] = "Maverick Stages/Toxic Seahorse",
    [06] = "Maverick Stages/Volt Catfish",
    [07] = "Maverick Stages/Crush Crawfish",
    [08] = "Maverick Stages/Tunnel Rhino",
    [09] = "Maverick Stages/Neon Tiger",
    [10] = "Vile's Stage",
    [11] = "Dr. Doppler's Lab/Dr. Doppler's Lab 1",
    [12] = "Dr. Doppler's Lab/Dr. Doppler's Lab 2",
    [13] = "Dr. Doppler's Lab/Dr. Doppler's Lab 3",
    [14] = "Dr. Doppler's Lab/Dr. Doppler's Lab 4",
    [17] = "", --weapon get?
}

function onSetReply(key, value, old)
    return
end

function set_if_exists(slot_data, slotname)
    if slot_data[slotname] then
        Tracker:FindObjectForCode(slotname).AcquiredCount = slot_data[slotname]
    end
end
function enable_if_exists(slot_data, slotname)
    if slot_data[slotname] then
        obj = Tracker:FindObjectForCode(slotname)
        if slot_data[slotname] == 0 then
            obj.Active = false
        else
            obj.Active = true
        end
    end
end
function enable_progressive_if_exists(slot_data, slotname)
    if slot_data[slotname] then
        obj = Tracker:FindObjectForCode(slotname)
        if slot_data[slotname] == 0 then
            obj.CurrentStage = 0
        else
            obj.CurrentStage = 1
        end
    end
end
function set_stage_state_unlocked(stagecode)
    local state = Tracker:FindObjectForCode(stagecode)
    if state then
        if state.CurrentStage == 0 then state.CurrentStage = 1 end
    else
        print(string.format("set_stage_state_unlocked called with %s, invalid object"), stagecode)
    end
end

function zero_item(itemstr)
    Tracker:FindObjectForCode(itemstr).AcquiredCount = 0
end


function set_ap_doppler_access(slot_data)
    --option_medals = 1
    --option_weapons = 2
    --option_armor_upgrades = 4
    --option_heart_tanks = 8
    --option_sub_tanks = 16
    --option_all = 31

    if (slot_data['doppler_open']) then
        local so = slot_data['doppler_open']
        Tracker:FindObjectForCode("doppler_open").AcquiredCount = so
        if (so & 1) > 0 then
            set_if_exists(slot_data, 'doppler_medal_count')
        else
            zero_item('doppler_medal_count')
        end

        if (so & 2) > 0 then
            set_if_exists(slot_data, 'doppler_weapon_count')
        else
            zero_item('doppler_weapon_count')
        end

        if (so & 4) > 0 then
            set_if_exists(slot_data, 'doppler_upgrade_count')
        else
            zero_item('doppler_upgrade_count')
        end

        if (so & 8) > 0 then
            set_if_exists(slot_data, 'doppler_heart_tank_count')
        else
            zero_item('doppler_heart_tank_count')
        end

        if (so & 16) > 0 then
            set_if_exists(slot_data, 'doppler_sub_tank_count')
        else
            zero_item('doppler_sub_tank_count')
        end
    end
end
function set_ap_vile_access(slot_data)
    if (slot_data['vile_open']) then
        local so = slot_data['vile_open']
        Tracker:FindObjectForCode("vile_open").AcquiredCount = so
        if (so & 1) > 0 then
            set_if_exists(slot_data, 'vile_medal_count')
        else
            zero_item('vile_medal_count')
        end

        if (so & 2) > 0 then
            set_if_exists(slot_data, 'vile_weapon_count')
        else
            zero_item('vile_weapon_count')
        end

        if (so & 4) > 0 then
            set_if_exists(slot_data, 'vile_upgrade_count')
        else
            zero_item('vile_upgrade_count')
        end

        if (so & 8) > 0 then
            set_if_exists(slot_data, 'vile_heart_tank_count')
        else
            zero_item('vile_heart_tank_count')
        end
        if (so & 16) > 0 then
            set_if_exists(slot_data, 'vile_sub_tank_count')
        else
            zero_item('vile_sub_tank_count')
        end
    end
end

function tab_switch_handler(tab_id)
    if tab_id then
        if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
            print(string.format("tab_switch_handler(), tab_id=%d", tab_id))
        end
        if Tracker:FindObjectForCode('auto_tab_switch').CurrentStage == 1 then
            if TAB_MAPPING[tab_id] == "" then return end
            for str in string.gmatch(TAB_MAPPING[tab_id], "([^/]+)") do
                print(string.format("On stage %x, switching to tab %s",tab_id,str))
                Tracker:UiHint("ActivateTab", str)
            end
        end
    end
end

function onClear(slot_data)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("called onClear, slot_data:\n%s", dump_table(slot_data)))
    end
    SLOT_DATA = slot_data
    CUR_INDEX = -1
    -- reset locations
    for _, v in pairs(LOCATION_MAPPING) do
        if v[1] then
            if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
                print(string.format("onClear: clearing location %s", v[1]))
            end
            local obj = Tracker:FindObjectForCode(v[1])
            if obj then
                if v[1]:sub(1, 1) == "@" then
                    obj.AvailableChestCount = obj.ChestCount
                else
                    obj.Active = false
                end
            elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
                print(string.format("onClear: could not find object for code %s", v[1]))
            end
        end
    end
    -- reset items
    for _, v in pairs(ITEM_MAPPING) do
        if v[1] and v[2] then
            if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
                print(string.format("onClear: clearing item %s of type %s", v[1], v[2]))
            end
            local obj = Tracker:FindObjectForCode(v[1])
            if obj then
                if v[2] == "toggle" then
                    obj.Active = false
                elseif v[2] == "progressive" then
                    obj.CurrentStage = 0
                    obj.Active = false
                elseif v[2] == "consumable" then
                    obj.AcquiredCount = 0
                elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
                    print(string.format("onClear: unknown item type %s for code %s", v[2], v[1]))
                end
            elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
                print(string.format("onClear: could not find object for code %s", v[1]))
            end
        end
    end

    enable_progressive_if_exists(slot_data, 'pickupsanity')
    enable_progressive_if_exists(slot_data, 'jammed_buster')

    set_ap_doppler_access(slot_data)
    set_ap_vile_access(slot_data)

    enable_progressive_if_exists(slot_data, 'doppler_all_labs')
    Tracker:FindObjectForCode('boss_weakness_strictness').CurrentStage = slot_data['boss_weakness_strictness']
    enable_if_exists(slot_data, 'logic_boss_weakness')

    if Tracker:FindObjectForCode('logic_boss_weakness').Active then
        Tracker:FindObjectForCode('setting_weakness').CurrentStage = 1
    end
    enable_progressive_if_exists(slot_data, 'logic_vile_required')
    set_if_exists(slot_data, 'bit_medal_count')
    set_if_exists(slot_data, 'byte_medal_count')
    set_if_exists(slot_data, 'doppler_lab_3_boss_rematch_count')

    if slot_data['bit_medal_count'] and slot_data['byte_medal_count'] then
        if slot_data['bit_medal_count'] == 0 and slot_data['byte_medal_count'] == 0 then
            Tracker:FindObjectForCode('byte_medal_count').AcquiredCount = 1
        end
        if slot_data['bit_medal_count'] >= slot_data['byte_medal_count'] then
            if slot_data['bit_medal_count'] == 7 then
                Tracker:FindObjectForCode('bit_medal_count').AcquiredCount = 6
            end
            Tracker:FindObjectForCode('byte_medal_count').AcquiredCount = slot_data['bit_medal_count'] + 1
        end
    end

    if slot_data['jammed_buster'] > 0 then
        Tracker:FindObjectForCode('arms').CurrentStage = 0
    end

    LOCAL_ITEMS = {}
    GLOBAL_ITEMS = {}

    PLAYER_ID = Archipelago.PlayerNumber or -1
	TEAM_NUMBER = Archipelago.TeamNumber or 0

    if Archipelago.PlayerNumber>-1 then
		TAB_SWITCH_KEY="mmx3_level_id_"..TEAM_NUMBER.."_"..PLAYER_ID
        if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
            print(string.format("SET NOTIFY %s",TAB_SWITCH_KEY))
        end
		Archipelago:SetNotify({TAB_SWITCH_KEY})
		Archipelago:Get({TAB_SWITCH_KEY})
	end

    BOSS_WEAKNESSES = slot_data['boss_weaknesses']
end


-- called when an item gets collected
function onItem(index, item_id, item_name, player_number)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("called onItem: %s, %s, %s, %s, %s", index, item_id, item_name, player_number, CUR_INDEX))
    end
    if not AUTOTRACKER_ENABLE_ITEM_TRACKING then
        return
    end
    if index <= CUR_INDEX then
        return
    end
    local is_local = player_number == Archipelago.PlayerNumber
    CUR_INDEX = index;
    local v = ITEM_MAPPING[item_id]
    if not v then
        if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
            print(string.format("onItem: could not find item mapping for id %s", item_id))
        end
        return
    end
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("onItem: code: %s, type %s", v[1], v[2]))
    end
    if not v[1] then
        return
    end
    local obj = Tracker:FindObjectForCode(v[1])
    if obj then
        if v[2] == "toggle" then
            obj.Active = true
        elseif v[2] == "progressive" then
            if obj.Active then
                obj.CurrentStage = obj.CurrentStage + 1
            else
                obj.Active = true
            end
        elseif v[2] == "consumable" then
            obj.AcquiredCount = obj.AcquiredCount + obj.Increment
        elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
            print(string.format("onItem: unknown item type %s for code %s", v[2], v[1]))
        end
    elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("onItem: could not find object for code %s", v[1]))
    end
    -- track local items via snes interface
    if is_local then
        if LOCAL_ITEMS[v[1]] then
            LOCAL_ITEMS[v[1]] = LOCAL_ITEMS[v[1]] + 1
        else
            LOCAL_ITEMS[v[1]] = 1
        end
    else
        if GLOBAL_ITEMS[v[1]] then
            GLOBAL_ITEMS[v[1]] = GLOBAL_ITEMS[v[1]] + 1
        else
            GLOBAL_ITEMS[v[1]] = 1
        end
    end
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("local items: %s", dump_table(LOCAL_ITEMS)))
        print(string.format("global items: %s", dump_table(GLOBAL_ITEMS)))
    end

    if item_id == 12386306 then
        set_stage_state_unlocked("toxic_seahorse_state")
    end
    if item_id == 12386307 then
        set_stage_state_unlocked("volt_catfish_state")
    end
    if item_id == 12386308 then
        set_stage_state_unlocked("tunnel_rhino_state")
    end
    if item_id == 12386309 then
        set_stage_state_unlocked("blizzard_buffalo_state")
    end
    if item_id == 12386310 then
        set_stage_state_unlocked("crush_crawfish_state")
    end
    if item_id == 12386311 then
        set_stage_state_unlocked("neon_tiger_state")
    end
    if item_id == 12386312 then
        set_stage_state_unlocked("gravity_beetle_state")
    end
    if item_id == 12386313 then
        set_stage_state_unlocked("blast_hornet_state")
    end
    --if is_doppler_open() then
    --    Tracker:FindObjectForCode('stage_doppler_lab').Active = true
    --end
    --if is_vile_open() then
    --    Tracker:FindObjectForCode('stage_vile').Active = true
    --end
    if is_bit_open() then
        set_stage_state_unlocked('bit_state')
    end
    if is_byte_open() then
        set_stage_state_unlocked('byte_state')
    end
    if item_id == 12386334 then
        local arms = Tracker:FindObjectForCode("arms")
        if arms then
            arms.CurrentStage = arms.CurrentStage + 1
        end
    end
    update_vile_state()
    update_doppler_state()

    print(string.format("boss_other_damage_possible: %s",boss_other_damage_possible()))
    print(string.format("boss_weaknesses_not_required: %s",boss_weaknesses_not_required()))
end

-- called when a location gets cleared
function onLocation(location_id, location_name)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("called onLocation: %s, %s", location_id, location_name))
    end
    if not AUTOTRACKER_ENABLE_LOCATION_TRACKING then
        return
    end
    local v = LOCATION_MAPPING[location_id]
    if not v and AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("onLocation: could not find location mapping for id %s", location_id))
    end
    if not v[1] then
        return
    end
    local obj = Tracker:FindObjectForCode(v[1])
    if obj then
        if v[1]:sub(1, 1) == "@" then
            obj.AvailableChestCount = obj.AvailableChestCount - 1
        else
            obj.Active = true
        end
    elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("onLocation: could not find object for code %s", v[1]))
    end

    --handle stage clear events
    if location_id == 12451968 then
        local obj = Tracker:FindObjectForCode("blizzard_buffalo_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("blizzard_buffalo_state")
        state.CurrentStage = 2
    end
    if location_id == 12451969 then
        local obj = Tracker:FindObjectForCode("toxic_seahorse_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("toxic_seahorse_state")
        state.CurrentStage = 2
    end
    if location_id == 12451970 then
        local obj = Tracker:FindObjectForCode("tunnel_rhino_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("tunnel_rhino_state")
        state.CurrentStage = 2
    end
    
    if location_id == 12451971 then
        local obj = Tracker:FindObjectForCode("volt_catfish_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("volt_catfish_state")
        state.CurrentStage = 2
    end
    
    if location_id == 12451972 then
        local obj = Tracker:FindObjectForCode("crush_crawfish_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("crush_crawfish_state")
        state.CurrentStage = 2
    end
    if location_id == 12451973 then
        local obj = Tracker:FindObjectForCode("neon_tiger_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("neon_tiger_state")
        state.CurrentStage = 2
    end
    if location_id == 12451974 then
        local obj = Tracker:FindObjectForCode("gravity_beetle_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("gravity_beetle_state")
        state.CurrentStage = 2
    end
    if location_id == 12451975 then
        local obj = Tracker:FindObjectForCode("blast_hornet_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("blast_hornet_state")
        state.CurrentStage = 2
    end
    if location_id == 12451854 then
        local obj = Tracker:FindObjectForCode("doppler_1_cleared")
        obj.Active = true
    end
    if location_id == 12451856 then
        local obj = Tracker:FindObjectForCode("doppler_2_cleared")
        obj.Active = true
    end
    if location_id == 12451858 then
        local obj = Tracker:FindObjectForCode("doppler_3_cleared")
        obj.Active = true
    end
    if location_id == 12451870 then
        local obj = Tracker:FindObjectForCode("bit_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("bit_state")
        state.CurrentStage = 2
    end
    if location_id == 12451871 then
        local obj = Tracker:FindObjectForCode("byte_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("byte_state")
        state.CurrentStage = 2
    end
    if location_id == 12451869 then
        local obj = Tracker:FindObjectForCode("vile_cleared")
        obj.Active = true
        local state = Tracker:FindObjectForCode("vile_state")
        state.CurrentStage = 2
    end

    --refresh access rules logic
    update_doppler_state()
    local o = Tracker:FindObjectForCode("refresh")
    if o then o.Active = not o.Active end
end

-- called when a locations is scouted
function onScout(location_id, location_name, item_id, item_name, item_player)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("called onScout: %s, %s, %s, %s, %s", location_id, location_name, item_id, item_name,
            item_player))
    end
    -- not implemented yet :(
end

-- called when a bounce message is received 
function onBounce(json)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("called onBounce: %s", dump_table(json)))
    end
    -- your code goes here
end

function onNotify(key, value, old_value)
    print(string.format("onNotify called. key=%s value=%s old_value=%s", key, value, old_value))
    if key == TAB_SWITCH_KEY then
        tab_switch_handler(value)
    end
end

function onNotifyLaunch(key, value)
    print(string.format("onNotifyLaunch called. key=%s value=%s", key, value))
    if key == TAB_SWITCH_KEY then
        tab_switch_handler(value)
    end
end

-- add AP callbacks
-- un-/comment as needed
Archipelago:AddClearHandler("clear handler", onClear)
if AUTOTRACKER_ENABLE_ITEM_TRACKING then
    Archipelago:AddItemHandler("item handler", onItem)
end
if AUTOTRACKER_ENABLE_LOCATION_TRACKING then
    Archipelago:AddLocationHandler("location handler", onLocation)
end
--Archipelago:AddSetReplyHandler("set reply handler", onSetReply)

Archipelago:AddSetReplyHandler("notify handler", onNotify)
Archipelago:AddRetrievedHandler("notify launch handler", onNotifyLaunch)

--Archipelago:AddScoutHandler("scout handler", onScout)
--Archipelago:AddBouncedHandler("bounce handler", onBounce)
