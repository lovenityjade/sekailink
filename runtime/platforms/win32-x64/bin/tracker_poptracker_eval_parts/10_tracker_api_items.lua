
local function mark_logic_stale()
    accessibility_stale = true
    visibility_stale = true
end

local object_mt = {}

function object_mt:__newindex(key, value)
    rawset(self, key, value)
    if key == "Name" and type(value) == "string" and value ~= "" then
        objects_by_name[value] = self
        register_code(value, self)
    elseif key == "Active" or key == "CurrentStage" or key == "AcquiredCount" or
           key == "AvailableChestCount" or key == "ItemCleared" or key == "BadgeText" or
           key == "Highlight" or key == "ItemState" then
        mark_logic_stale()
    end
end

local function new_object(name, kind)
    local object = {
        Name = name,
        Kind = kind or "generic",
        Type = kind or "generic",
        Active = false,
        CurrentStage = 0,
        AcquiredCount = 0,
        MinCount = 0,
        MaxCount = 0,
        Increment = 1,
        BadgeText = "",
        ItemState = { MANUAL_LOCATIONS = { default = {} }, MANUAL_LOCATIONS_ORDER = {} },
        Owner = { ModifiedByUser = false },
        ChestCount = 1,
        AvailableChestCount = 1,
        AccessibilityLevel = 0,
        FullID = name,
        SetOverlayFontSize = function() end,
        SetOverlayAlign = function() end,
        SetOverlay = function(self, text) self.BadgeText = tostring(text or "") end,
        SetOverlayBackground = function() end,
        SetOverlayColor = function() end,
    }
    setmetatable(object, object_mt)
    return object
end

function register_code(code, object, stage_index)
    if code and code ~= "" then
        if not objects_by_code[code] then
            objects_by_code[code] = object
        end
        local lower_code = code:lower()
        if not objects_by_code_ci[lower_code] then
            objects_by_code_ci[lower_code] = object
        end
        objects_for_code[code] = objects_for_code[code] or {}
        local duplicate = false
        for _, existing in ipairs(objects_for_code[code]) do
            if existing == object then
                duplicate = true
                break
            end
        end
        if not duplicate then
            table.insert(objects_for_code[code], object)
        end
        if stage_index ~= nil then
            if not code_stage_ambiguous[code] then
                if code_stage_requirements[code] ~= nil and code_stage_requirements[code] ~= stage_index then
                    code_stage_requirements[code] = nil
                    code_stage_ambiguous[code] = true
                elseif code_stage_requirements[code] == nil then
                    code_stage_requirements[code] = stage_index
                end
            end
        else
            code_stage_requirements[code] = nil
            code_stage_ambiguous[code] = true
        end
    end
end

local function new_lua_item()
    local object = new_object("", "custom")
    rawset(object, "Name", nil)
    object.Type = "custom"
    object.ItemState = nil
    object.Icon = ""
    object.IconMods = ""
    object.OnLeftClickFunc = nil
    object.OnRightClickFunc = nil
    object.OnMiddleClickFunc = nil
    object.CanProvideCodeFunc = nil
    object.ProvidesCodeFunc = nil
    object.AdvanceToCodeFunc = nil
    object.SaveFunc = nil
    object.LoadFunc = nil
    object.PropertyChangedFunc = nil
    table.insert(lua_items, object)
    table.insert(all_items, object)
    return object
end

AccessibilityLevel = {
    None = 0,
    Partial = 1,
    Inspect = 3,
    SequenceBreak = 5,
    Normal = 6,
    Cleared = 7,
}

Highlight = {
    Unspecified = 0,
    NoPriority = 1,
    Avoid = 2,
    Priority = 3,
    None = 4,
}

Tracker = {
    ActiveVariantUID = "Map Tracker - AP",
    BulkUpdate = false,
}

local script_host_watchers = {}
local script_host_frame_handlers = {}
local script_host_location_section_handlers = {}
local archipelago_clear_handlers = {}
local archipelago_item_handlers = {}
local archipelago_location_handlers = {}
local archipelago_set_reply_handlers = {}
local archipelago_retrieved_handlers = {}

local function add_script_host_watch(_, name, code, handler)
    if type(handler) ~= "function" then
        return
    end
    table.insert(script_host_watchers, {
        name = tostring(name or ""),
        code = tostring(code or ""),
        handler = handler,
    })
end

local function remove_script_host_watch(_, name)
    name = tostring(name or "")
    for index = #script_host_watchers, 1, -1 do
        if script_host_watchers[index].name == name then
            table.remove(script_host_watchers, index)
        end
    end
end

local function add_script_host_frame_handler(_, name, handler)
    if type(handler) ~= "function" then
        return
    end
    table.insert(script_host_frame_handlers, {
        name = tostring(name or ""),
        handler = handler,
    })
end

local function remove_script_host_frame_handler(_, name)
    name = tostring(name or "")
    for index = #script_host_frame_handlers, 1, -1 do
        if script_host_frame_handlers[index].name == name then
            table.remove(script_host_frame_handlers, index)
        end
    end
end

local function add_location_section_handler(_, name, handler)
    if type(handler) ~= "function" then
        return
    end
    table.insert(script_host_location_section_handlers, {
        name = tostring(name or ""),
        handler = handler,
    })
end

local function remove_location_section_handler(_, name)
    name = tostring(name or "")
    for index = #script_host_location_section_handlers, 1, -1 do
        if script_host_location_section_handlers[index].name == name then
            table.remove(script_host_location_section_handlers, index)
        end
    end
end

ScriptHost = {
    AddOnFrameHandler = add_script_host_frame_handler,
    RemoveOnFrameHandler = remove_script_host_frame_handler,
    AddWatchForCode = add_script_host_watch,
    RemoveWatchForCode = remove_script_host_watch,
    AddOnLocationSectionHandler = add_location_section_handler,
    RemoveOnLocationSectionHandler = remove_location_section_handler,
    AddOnLocationSectionChangedHandler = add_location_section_handler,
    RemoveOnLocationSectionChangedHandler = remove_location_section_handler,
    AddMemoryWatch = function() end,
    RemoveMemoryWatch = function() end,
    CreateLuaItem = function()
        return new_lua_item()
    end,
    LoadScript = function(path)
        if type(path) == "string" and path ~= "" then
            require(path:gsub("%.lua$", ""))
        end
    end,
}

ImageReference = ImageReference or {
    FromPackRelativePath = function(_, path)
        return tostring(path or "")
    end,
}

Archipelago = {
    PlayerNumber = -1,
    TeamNumber = 0,
    Seed = nil,
    MissingLocations = {},
    CheckedLocations = {},
    GetPlayerAlias = function(_, player)
        return tostring(player or "")
    end,
    SetNotify = function() end,
    Get = function() end,
    AddClearHandler = function(_, name, handler)
        if type(handler) == "function" then
            archipelago_clear_handlers[tostring(name or "")] = handler
        end
    end,
    AddItemHandler = function(_, name, handler)
        if type(handler) == "function" then
            archipelago_item_handlers[tostring(name or "")] = handler
        end
    end,
    AddLocationHandler = function(_, name, handler)
        if type(handler) == "function" then
            archipelago_location_handlers[tostring(name or "")] = handler
        end
    end,
    AddSetReplyHandler = function(_, name, handler)
        if type(handler) == "function" then
            archipelago_set_reply_handlers[tostring(name or "")] = handler
        end
    end,
    AddRetrievedHandler = function(_, name, handler)
        if type(handler) == "function" then
            archipelago_retrieved_handlers[tostring(name or "")] = handler
        end
    end,
}

local function object_has_code(object, code)
    if not object or not code or code == "" then
        return false
    end
    if object.CanProvideCodeFunc then
        local ok, result = pcall(object.CanProvideCodeFunc, object, code)
        if ok then
            return result and true or false
        end
    end
    if object.Codes then
        for _, candidate in ipairs(object.Codes) do
            if candidate == code or candidate:lower() == code:lower() then
                return true
            end
        end
    end
    if object.Type ~= "composite_toggle" and object.Stages then
        for _, stage in ipairs(object.Stages) do
            for _, candidate in ipairs(stage.codes or {}) do
                if candidate == code or candidate:lower() == code:lower() then
                    return true
                end
            end
        end
    end
    return object.Name == code
end

local function object_stage_index_for_provider(object)
    local current = tonumber(object.CurrentStage) or 0
    if object.Type == "progressive" and object.AllowDisabled ~= false then
        if not object.Active or current <= 0 then
            return nil
        end
        return current - 1
    end
    return current
end

local function object_stage_provides_code(object, code)
    if object.Type == "composite_toggle" then
        return false
    end
    if not object.Stages or #object.Stages == 0 then
        return false
    end
    local stage_index = object_stage_index_for_provider(object)
    if stage_index == nil then
        return false
    end
    for index = math.min(stage_index, #object.Stages - 1), 0, -1 do
        local stage = object.Stages[index + 1]
        if stage then
            for _, candidate in ipairs(stage.codes or {}) do
                if candidate == code or candidate:lower() == code:lower() then
                    return true
                end
            end
            if stage.inherit_codes == false then
                break
            end
        end
    end
    return false
end

local function object_provides_code(object, code)
    if not object_has_code(object, code) then
        return 0
    end
    if object.ProvidesCodeFunc then
        local ok, result = pcall(object.ProvidesCodeFunc, object, code)
        if ok then
            if type(result) == "boolean" then
                return result and 1 or 0
            end
            return tonumber(result) or 0
        end
    end
    if object.Type == "consumable" then
        return tonumber(object.AcquiredCount) or 0
    end
    if object.Type == "static" or object.AllowDisabled == false then
        return object_has_code(object, code) and 1 or 0
    end
    if object_stage_provides_code(object, code) then
        return object.Active == false and 0 or 1
    end
    if object.Codes then
        for _, candidate in ipairs(object.Codes) do
            if candidate == code or candidate:lower() == code:lower() then
                if object.Type == "composite_toggle" then
                    return tonumber(object.CurrentStage) or 0
                end
                return object.Active and 1 or 0
            end
        end
    end
    return object.Active and 1 or 0
end

function Tracker:FindObjectForCode(code)
    if not code or code == "" then
        return nil
    end
    if code:sub(1, 1) == "@" then
        local target = code:sub(2)
        if pack_locations_by_id[target] then
            return pack_locations_by_id[target]
        end
        if pack_sections_by_id[target] then
            return pack_sections_by_id[target]
        end
        local slash = target:match("^.*()/")
        if slash then
            local loc_id = target:sub(1, slash - 1)
            local section_name = target:sub(slash + 1)
            local location = pack_locations_by_id[loc_id]
            if location and location.Sections then
                for _, section in ipairs(location.Sections) do
                    if section.Name == section_name then
                        return section
                    end
                end
            end
        end
    end
    return objects_by_code[code] or objects_by_code_ci[code:lower()]
end

function Tracker:ProviderCountForCode(code)
    if code and code:sub(1, 1) == "$" then
        local parts = split(code:sub(2), "|")
        local function_name = table.remove(parts, 1)
        local fn = _G[function_name]
        if type(fn) ~= "function" then
            return 0
        end
        local ok, result = pcall(fn, unpack(parts))
        if not ok then
            return 0
        end
        if type(result) == "boolean" then
            return result and 1 or 0
        end
        return tonumber(result) or 0
    end
    local total = 0
    local providers = objects_for_code[code]
    local counted_lua_items = {}
    if providers then
        for _, object in ipairs(providers) do
            total = total + object_provides_code(object, code)
            if object.Type == "custom" then
                counted_lua_items[object] = true
            end
        end
    end
    for _, object in ipairs(lua_items) do
        if not counted_lua_items[object] and object_has_code(object, code) then
            total = total + object_provides_code(object, code)
        end
    end
    return total
end

function Tracker:UiHint() end
function Tracker:AddMaps() end
function Tracker:AddLayouts() end
function Tracker:ReadU() return 0 end

local function pack_relative_file_exists(path)
    if type(path) ~= "string" or path == "" then
        return false
    end
    local file = io.open(bundle_root .. "/poptracker-adapted/" .. path, "rb")
    if not file then
        return false
    end
    file:close()
    return true
end

function Tracker:AddItems(path)
    if not pack_relative_file_exists(path) then
        return
    end
    local items = load_relaxed_json(bundle_root .. "/poptracker-adapted/" .. path)
    for _, item in ipairs(items) do
        local object = new_object(item.name, item.type)
        object.Codes = parse_codes(item.codes)
        object.Stages = {}
        object.AllowDisabled = item.allow_disabled
        if object.AllowDisabled == nil then
            object.AllowDisabled = true
        end
        object.Loop = item.loop == true
        object.MinCount = tonumber(item.min_quantity) or 0
        object.MaxCount = tonumber(item.max_quantity) or 0
        object.Increment = tonumber(item.increment) or 1
        object.AcquiredCount = tonumber(item.initial_quantity) or object.MinCount
        object.CurrentStage = tonumber(item.initial_stage_idx) or tonumber(item.initial_stage) or object.CurrentStage
        object.Active = item.allow_disabled == false or item.type == "static" or item.initial_active_state == true or
            object.CurrentStage > 0 or object.AcquiredCount > object.MinCount
        objects_by_name[item.name] = object
        table.insert(all_items, object)
        register_code(item.name, object)
        for _, code in ipairs(object.Codes) do
            register_code(code, object)
        end
        if (item.type == "toggle" or item.type == "toggle_badged" or item.type == "consumable") and
            item.initial_active_state ~= true and item.allow_disabled ~= false then
            object.Active = false
        end
        if item.stages then
            for stage_number, stage in ipairs(item.stages) do
                local stage_index = stage_number - 1
                local parsed_stage = {
                    codes = parse_codes(stage.codes),
                    secondary_codes = parse_codes(stage.secondary_codes),
                    inherit_codes = stage.inherit_codes ~= false,
                }
                table.insert(object.Stages, parsed_stage)
                for _, code in ipairs(parsed_stage.codes) do
                    register_code(code, object, stage_index)
                end
            end
        end
    end
    mark_logic_stale()
end
