ScriptHost:LoadScript("scripts/autotracking/item_mapping.lua")
ScriptHost:LoadScript("scripts/autotracking/location_mapping.lua")

CUR_INDEX = -1
SLOT_DATA = nil
LOCAL_ITEMS = {}
GLOBAL_ITEMS = {}

TAB_SWITCH_KEY = ""

TAB_MAPPING = {
    [0] = "Stage Select",
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
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("set_stage_state_unlocked() called for %s", stagecode))
    end
    local state = Tracker:FindObjectForCode(stagecode)
    if state then
        if state.CurrentStage == 0 then state.CurrentStage = 1 end
    end
end

function stage_cleared(robot)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("stage_cleared() called for %s", robot))
    end
    local clearobj = Tracker:FindObjectForCode(robot.."_cleared")
    clearobj.Active = true
    local state = Tracker:FindObjectForCode(robot.."_state")
    state.CurrentStage = 2
end

function get_enabled_locs()
    -- Quick Man E-Tank
    local etank = 0x880203
    -- Quick Man Health Energy
    local energypickup = 0x880207

    local etanks_enabled = 0
    local energypickups_enabled = 0
    for _, i in ipairs(Archipelago.CheckedLocations) do
        if i == etank then
            etanks_enabled = 1
        elseif i == energypickup then
            energypickups_enabled = 1
        end
    end
    for _, i in ipairs(Archipelago.MissingLocations) do
        if i == etank then
            etanks_enabled = 1
        elseif i == energypickup then
            energypickups_enabled = 1
        end
    end
    return etanks_enabled, energypickups_enabled
end

--not implemented for this game
function tab_switch_handler(tab_id)
    if tab_id then
        if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
            print(string.format("tab_switch_handler(), tab_id=%x", tab_id))
        end
        if Tracker:FindObjectForCode('auto_tab_switch').CurrentStage == 1 then
            for str in string.gmatch(TAB_MAPPING[tab_id], "([^/]+)") do
                --print(string.format("On stage %x, switching to tab %s",tab_id,str))
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

    LOCAL_ITEMS = {}
    GLOBAL_ITEMS = {}

    PLAYER_ID = Archipelago.PlayerNumber or -1
	TEAM_NUMBER = Archipelago.TeamNumber or 0

    -- if Archipelago.PlayerNumber>-1 then
	-- 	TAB_SWITCH_KEY="mm2_level_id_"..TEAM_NUMBER.."_"..PLAYER_ID
    --     if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
    --         print(string.format("SET NOTIFY %s",TAB_SWITCH_KEY))
    --     end
	-- 	Archipelago:SetNotify({TAB_SWITCH_KEY})
	-- 	Archipelago:Get({TAB_SWITCH_KEY})
	-- end

    Tracker:FindObjectForCode("etanks").CurrentStage, Tracker:FindObjectForCode("energypickups").CurrentStage = get_enabled_locs()
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

    if item_id == 0x880101 then
        set_stage_state_unlocked("heat_man_state")
    end
    if item_id == 0x880102 then
        set_stage_state_unlocked("air_man_state")
    end
    if item_id == 0x880103 then
        set_stage_state_unlocked("wood_man_state")
    end
    if item_id == 0x880104 then
        set_stage_state_unlocked("bubble_man_state")
    end
    if item_id == 0x880105 then
        set_stage_state_unlocked("quick_man_state")
    end
    if item_id == 0x880106 then
        set_stage_state_unlocked("flash_man_state")
    end
    if item_id == 0x880107 then
        set_stage_state_unlocked("metal_man_state")
    end
    if item_id == 0x880108 then
        set_stage_state_unlocked("crash_man_state")
    end
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
    if location_id == 0x880001 then
        stage_cleared("heat_man")
    end
    if location_id == 0x880002 then
        stage_cleared("air_man")
    end
    if location_id == 0x880003 then
        stage_cleared("wood_man")
    end
    if location_id == 0x880004 then
        stage_cleared("bubble_man")
    end
    if location_id == 0x880005 then
        stage_cleared("quick_man")
    end
    if location_id == 0x880006 then
        stage_cleared("flash_man")
    end
    if location_id == 0x880007 then
        stage_cleared("metal_man")
    end
    if location_id == 0x880008 then
        stage_cleared("crash_man")
    end

    if location_id == 0x880009 then
        Tracker:FindObjectForCode("wily_1_cleared").Active = true
    end
    if location_id == 0x88000A then
        Tracker:FindObjectForCode("wily_2_cleared").Active = true
    end
    if location_id == 0x88000B then
        Tracker:FindObjectForCode("wily_3_cleared").Active = true
    end
    if location_id == 0x88000C then
        Tracker:FindObjectForCode("wily_4_cleared").Active = true
    end
    if location_id == 0x88000D then
        Tracker:FindObjectForCode("wily_5_cleared").Active = true
    end
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
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("onNotify called. key=%s value=%s old_value=%s", key, value, old_value))
    end
    if key == TAB_SWITCH_KEY then
        tab_switch_handler(value)
    end
end

function onNotifyLaunch(key, value)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_AP then
        print(string.format("onNotifyLaunch called. key=%s value=%s", key, value))
    end
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


--Archipelago:AddSetReplyHandler("notify handler", onNotify)
--Archipelago:AddRetrievedHandler("notify launch handler", onNotifyLaunch)


--Archipelago:AddScoutHandler("scout handler", onScout)
--Archipelago:AddBouncedHandler("bounce handler", onBounce)
