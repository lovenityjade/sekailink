-- Configuration --------------------------------------
AUTOTRACKER_ENABLE_DEBUG_LOGGING = true or ENABLE_DEBUG_LOG
AUTOTRACKER_ENABLE_ITEM_TRACKING = true
AUTOTRACKER_ENABLE_LOCATION_TRACKING = true and not IS_ITEMS_ONLY
-------------------------------------------------------

print("")
print("Active Auto-Tracker Configuration")
print("---------------------------------------------------------------------")
print("Enable Item Tracking:        ", AUTOTRACKER_ENABLE_ITEM_TRACKING)
print("Enable Location Tracking:    ", AUTOTRACKER_ENABLE_LOCATION_TRACKING)
if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
    print("Enable Debug Logging:        ", "true")
end
print("---------------------------------------------------------------------")
print("")
CUR_INDEX = -1
SLOT_DATA = nil
ScriptHost:LoadScript("scripts/autotracking/item_mapping.lua")
ScriptHost:LoadScript("scripts/autotracking/location_mapping.lua")

function onClear(slot_data)
	SLOT_DATA = slot_data
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
        print(string.format("called onClear"))
    end
    CUR_INDEX = -1
    for _, v in pairs(LOCATION_MAPPING) do
        if v[1] then
            if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
                print(string.format("onClear: clearing location %s", v[1]))
            end
            local obj = Tracker:FindObjectForCode(v[1])
            if obj then
                if v[1]:sub(1, 1) == "@" then
                    obj.AvailableChestCount = obj.ChestCount
                else
                   obj.Active = false 
                end
            elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING then
                print(string.format("onClear: could not find object for code %s", v[1]))
            end
        end
    end
    for _, v in pairs(ITEM_MAPPING) do
        if v[1] and v[2] then
            if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
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
                elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING then
                    print(string.format("onClear: unknown item type %s for code %s", v[2], v[1]))
                end
            elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING then
                print(string.format("onClear: could not find object for code %s", v[1]))
            end
        end
    end
	-- handle slot_data
	if SLOT_DATA == nil then
        return
    end
	
	PLAYER_ID = Archipelago.PlayerNumber or -1
	TEAM_NUMBER = Archipelago.TeamNumber or 0
    
    local starting_areas = {
        [0] = "normalstart",
        [1] = "onettstart",
        [2] = "twosonstart",
        [3] = "happyhappystart",
        [4] = "threedstart",
        [5] = "saturnvalleystart",
        [6] = "foursidestart",
        [7] = "wintersstart",
        [8] = "summersstart",
        [9] = "dalaamstart",
        [10] = "scarabastart",
        [11] = "deepdarknessstart",
        [12] = "tendavillagestart",
        [13] = "understart",
        [14] = "magicantstart"
    }

    local monkey_mode = {
        [0] = 0,
        [1] = 0,
        [2] = 1,
        [3] = 1,
    }

    local free_sancs = {
        [0] = "freesancoff",
        [1] = "freesancon",
    }

    local shopsanity = {
        [0] = 0,
        [1] = 0,
        [2] = 1,
    }

    print("SLOT_DATA['shopsanity'] =", SLOT_DATA["shopsanity"])

    Tracker:FindObjectForCode("normalstart").CurrentStage = SLOT_DATA["starting_area"]
    Tracker:FindObjectForCode("monkeyhunt").CurrentStage = monkey_mode[SLOT_DATA["pizza_logic"]]
    Tracker:FindObjectForCode("freesancoff").CurrentStage = SLOT_DATA["free_sancs"]
    Tracker:FindObjectForCode("shopsoff").CurrentStage = shopsanity[SLOT_DATA["shopsanity"]]

    Melody_status= TEAM_NUMBER.."_"..PLAYER_ID.."_melody_status"
    EarthPower_flag = TEAM_NUMBER.."_"..PLAYER_ID.."_earthpower"
    Archipelago:SetNotify({Melody_status})
    Archipelago:SetNotify({EarthPower_flag})
    Archipelago:Get(data_storage_list)
end

function onSetReply(key, value, _)
	local PLAYER_ID = Archipelago.PlayerNumber or -1
	local TEAM_NUMBER = Archipelago.TeamNumber or 0
    if key == TEAM_NUMBER.."_"..PLAYER_ID.."_melody_status" then
        melody_1_status = value & 32
        melody_2_status = value & 64
        melody_3_status = value & 128
        melody_4_status = value & 256
        melody_5_status = value & 512
        melody_6_status = value & 1024
        melody_7_status = value & 2048
        melody_8_status = value & 4096

        if melody_1_status ~= 0 then
            Tracker:FindObjectForCode("giantstep").Active = true
        end

        if melody_2_status ~= 0 then
            Tracker:FindObjectForCode("lilliputsteps").Active = true
        end

        if melody_4_status ~= 0 then
            Tracker:FindObjectForCode("milkywell").Active = true
        end

        if melody_3_status ~= 0 then
            Tracker:FindObjectForCode("rainycircle").Active = true
        end

        if melody_5_status ~= 0 then
            Tracker:FindObjectForCode("magnethill").Active = true
        end

        if melody_6_status ~= 0 then
            Tracker:FindObjectForCode("pinkcloud").Active = true
        end

        if melody_7_status ~= 0 then
            Tracker:FindObjectForCode("luminehall").Active = true
        end

        if melody_8_status ~= 0 then
            Tracker:FindObjectForCode("firespring").Active = true
        end
    end

    if key == TEAM_NUMBER.."_"..PLAYER_ID.."_earthpower" then
        earth_power_flag = value & 32
        if earth_power_flag ~= 0 then
            Tracker:FindObjectForCode("earth").Active = true
        else
            Tracker:FindObjectForCode("earth").Active = false
        end
    end
end

function onItem(index, item_id, item_name)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
        print(string.format("called onItem: %s, %s, %s, %s", index, item_id, item_name, CUR_INDEX))
    end
    if index <= CUR_INDEX then return end
    CUR_INDEX = index;

	
    local v = ITEM_MAPPING[item_id]
    if not v then
        if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
            print(string.format("onItem: could not find item mapping for id %s", item_id))
        end
        return
    end
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
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
            obj.AcquiredCount = obj.AcquiredCount + 1
        elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING then
            print(string.format("onItem: unknown item type %s for code %s", v[2], v[1]))
        end
    elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING then
        print(string.format("onItem: could not find object for code %s", v[1]))
    end
end

function onLocation(location_id, location_name)
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING then
        print(string.format("called onLocation: %s, %s", location_id, location_name))
    end
    local v = LOCATION_MAPPING[location_id]
    if not v and AUTOTRACKER_ENABLE_DEBUG_LOGGING then
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
    elseif AUTOTRACKER_ENABLE_DEBUG_LOGGING then
        print(string.format("onLocation: could not find object for code %s", v[1]))
    end
end

Archipelago:AddClearHandler("clear handler", onClear)
Archipelago:AddItemHandler("item handler", onItem)
Archipelago:AddLocationHandler("location handler", onLocation)
Archipelago:AddSetReplyHandler("set reply handler", onSetReply)
-- Archipelago:AddScoutHandler("scout handler", onScout)
-- Archipelago:AddBouncedHandler("bounce handler", onBounce)
