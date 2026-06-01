local bundle_root = assert(arg[1], "bundle_root_missing")
local state_path = assert(arg[2], "state_path_missing")
local output_path = assert(arg[3], "output_path_missing")

local function slurp(path)
    local file = assert(io.open(path, "rb"))
    local text = file:read("*a")
    file:close()
    return text
end

local function trim(text)
    return (text:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function split(text, sep)
    local parts = {}
    local pattern = "([^" .. sep .. "]*)"
    local start = 1
    while true do
        local stop = text:find(sep, start, true)
        if not stop then
            table.insert(parts, text:sub(start))
            break
        end
        table.insert(parts, text:sub(start, stop - 1))
        start = stop + #sep
    end
    return parts
end

local function parse_relaxed_json_text(text, path)
    path = path or "<json>"
    local index = 1
    local length = #text

    local function parse_error(message)
        error("relaxed_json_parse_failed:" .. path .. ":" .. tostring(message))
    end

    local function skip_ignored()
        while index <= length do
            local ch = text:sub(index, index)
            if ch == " " or ch == "\t" or ch == "\r" or ch == "\n" then
                index = index + 1
            elseif ch == "/" and text:sub(index + 1, index + 1) == "/" then
                index = index + 2
                while index <= length do
                    local comment_ch = text:sub(index, index)
                    if comment_ch == "\r" or comment_ch == "\n" then
                        break
                    end
                    index = index + 1
                end
            else
                break
            end
        end
    end

    local parse_value

    local function parse_string()
        if text:sub(index, index) ~= '"' then
            parse_error("expected string at byte " .. tostring(index))
        end
        index = index + 1
        local out = {}
        while index <= length do
            local ch = text:sub(index, index)
            if ch == '"' then
                index = index + 1
                return table.concat(out)
            end
            if ch == "\\" then
                local next_ch = text:sub(index + 1, index + 1)
                if next_ch == "" then
                    parse_error("unterminated escape at byte " .. tostring(index))
                end
                if next_ch == '"' or next_ch == "\\" or next_ch == "/" then
                    table.insert(out, next_ch)
                elseif next_ch == "b" then
                    table.insert(out, "\b")
                elseif next_ch == "f" then
                    table.insert(out, "\f")
                elseif next_ch == "n" then
                    table.insert(out, "\n")
                elseif next_ch == "r" then
                    table.insert(out, "\r")
                elseif next_ch == "t" then
                    table.insert(out, "\t")
                elseif next_ch == "u" then
                    local hex = text:sub(index + 2, index + 5)
                    if #hex ~= 4 or not hex:match("^[0-9a-fA-F]+$") then
                        parse_error("invalid unicode escape at byte " .. tostring(index))
                    end
                    local code = tonumber(hex, 16)
                    if code <= 0x7F then
                        table.insert(out, string.char(code))
                    elseif code <= 0x7FF then
                        table.insert(out, string.char(
                            0xC0 + math.floor(code / 0x40),
                            0x80 + (code % 0x40)))
                    else
                        table.insert(out, string.char(
                            0xE0 + math.floor(code / 0x1000),
                            0x80 + (math.floor(code / 0x40) % 0x40),
                            0x80 + (code % 0x40)))
                    end
                    index = index + 4
                else
                    parse_error("unsupported escape \\" .. next_ch .. " at byte " .. tostring(index))
                end
                index = index + 2
            else
                table.insert(out, ch)
                index = index + 1
            end
        end
        parse_error("unterminated string literal")
    end

    local function parse_number()
        local start_index = index
        local ch = text:sub(index, index)
        if ch == "-" then
            index = index + 1
        end
        while text:sub(index, index):match("%d") do
            index = index + 1
        end
        if text:sub(index, index) == "." then
            index = index + 1
            while text:sub(index, index):match("%d") do
                index = index + 1
            end
        end
        ch = text:sub(index, index)
        if ch == "e" or ch == "E" then
            index = index + 1
            ch = text:sub(index, index)
            if ch == "+" or ch == "-" then
                index = index + 1
            end
            while text:sub(index, index):match("%d") do
                index = index + 1
            end
        end
        local value = tonumber(text:sub(start_index, index - 1))
        if value == nil then
            parse_error("invalid number at byte " .. tostring(start_index))
        end
        return value
    end

    local function parse_array()
        index = index + 1
        local out = {}
        skip_ignored()
        if text:sub(index, index) == "]" then
            index = index + 1
            return out
        end
        while true do
            out[#out + 1] = parse_value()
            skip_ignored()
            local ch = text:sub(index, index)
            if ch == "]" then
                index = index + 1
                return out
            end
            if ch ~= "," then
                parse_error("expected ',' or ']' at byte " .. tostring(index))
            end
            index = index + 1
            skip_ignored()
            if text:sub(index, index) == "]" then
                index = index + 1
                return out
            end
        end
    end

    local function parse_object()
        index = index + 1
        local out = {}
        skip_ignored()
        if text:sub(index, index) == "}" then
            index = index + 1
            return out
        end
        while true do
            skip_ignored()
            local key = parse_string()
            skip_ignored()
            if text:sub(index, index) ~= ":" then
                parse_error("expected ':' after object key at byte " .. tostring(index))
            end
            index = index + 1
            out[key] = parse_value()
            skip_ignored()
            local ch = text:sub(index, index)
            if ch == "}" then
                index = index + 1
                return out
            end
            if ch ~= "," then
                parse_error("expected ',' or '}' at byte " .. tostring(index))
            end
            index = index + 1
            skip_ignored()
            if text:sub(index, index) == "}" then
                index = index + 1
                return out
            end
        end
    end

    parse_value = function()
        skip_ignored()
        local ch = text:sub(index, index)
        if ch == "{" then
            return parse_object()
        end
        if ch == "[" then
            return parse_array()
        end
        if ch == '"' then
            return parse_string()
        end
        if ch == "-" or ch:match("%d") then
            return parse_number()
        end
        if text:sub(index, index + 3) == "true" then
            index = index + 4
            return true
        end
        if text:sub(index, index + 4) == "false" then
            index = index + 5
            return false
        end
        if text:sub(index, index + 3) == "null" then
            index = index + 4
            return nil
        end
        parse_error("unexpected token at byte " .. tostring(index))
    end

    local result = parse_value()
    skip_ignored()
    if index <= length then
        parse_error("trailing content at byte " .. tostring(index))
    end
    return result
end

local function load_relaxed_json(path)
    return parse_relaxed_json_text(slurp(path), path)
end

local function parse_codes(value)
    if not value or value == "" then
        return {}
    end
    local out = {}
    for code in value:gmatch("([^,]+)") do
        code = trim(code)
        if code ~= "" then
            table.insert(out, code)
        end
    end
    return out
end

local objects_by_code = {}
local code_stage_requirements = {}
local code_stage_ambiguous = {}
local objects_by_name = {}
local pack_locations = {}
local pack_locations_by_id = {}
local pack_sections_by_id = {}
local location_checks_by_pack_id = {}
local section_checks_by_id = {}
local mapped_pin_records = {}
local mapped_pin_record_keys = {}
local ap_locations_by_id = {}
local has_pack_location_mapping = false
local checked_location_ids = {}
local checked_location_names = {}
local all_location_names = {}
local location_files = {}
local raw_slot_data = ""

local function new_object(name, kind)
    return {
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
    }
end

local function register_code(code, object, stage_index)
    if code and code ~= "" then
        objects_by_code[code] = object
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

ScriptHost = {
    AddOnFrameHandler = function() end,
    RemoveOnFrameHandler = function() end,
    AddWatchForCode = function() end,
    RemoveWatchForCode = function() end,
    AddOnLocationSectionHandler = function() end,
    RemoveOnLocationSectionHandler = function() end,
    AddOnLocationSectionChangedHandler = function() end,
    RemoveOnLocationSectionChangedHandler = function() end,
    LoadScript = function(path)
        if type(path) == "string" and path ~= "" then
            require(path:gsub("%.lua$", ""))
        end
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
}

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
    if not objects_by_code[code] then
        objects_by_code[code] = new_object(code, "dynamic")
    end
    return objects_by_code[code]
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
    local object = self:FindObjectForCode(code)
    if not object then
        return 0
    end
    local required_stage = code_stage_requirements[code]
    if required_stage ~= nil and object.CurrentStage ~= required_stage then
        return 0
    end
    if object.AcquiredCount and object.AcquiredCount > 0 then
        return object.AcquiredCount
    end
    if object.CurrentStage and object.CurrentStage > 0 then
        return object.CurrentStage
    end
    return object.Active and 1 or 0
end

function Tracker:UiHint() end
function Tracker:AddMaps() end
function Tracker:AddLayouts() end
function Tracker:ReadU() return 0 end

function Tracker:AddItems(path)
    local items = load_relaxed_json(bundle_root .. "/poptracker-adapted/" .. path)
    for _, item in ipairs(items) do
        local object = new_object(item.name, item.type)
        object.MinCount = tonumber(item.min_quantity) or 0
        object.MaxCount = tonumber(item.max_quantity) or 0
        object.Increment = tonumber(item.increment) or 1
        object.AcquiredCount = tonumber(item.initial_quantity) or object.MinCount
        object.CurrentStage = tonumber(item.initial_stage_idx) or tonumber(item.initial_stage) or object.CurrentStage
        object.Active = item.allow_disabled == false or item.initial_active_state == true or
            object.CurrentStage > 0 or object.AcquiredCount > object.MinCount
        objects_by_name[item.name] = object
        register_code(item.name, object)
        for _, code in ipairs(parse_codes(item.codes)) do
            register_code(code, object)
        end
        if (item.type == "toggle" or item.type == "toggle_badged" or item.type == "consumable") and
            item.initial_active_state ~= true and item.allow_disabled ~= false then
            object.Active = false
        end
        if item.stages then
            for stage_number, stage in ipairs(item.stages) do
                local stage_index = stage_number - 1
                for _, code in ipairs(parse_codes(stage.codes)) do
                    register_code(code, object, stage_index)
                end
            end
        end
    end
end

local function copy_rules(rules)
    local out = {}
    for _, ruleset in ipairs(rules or {}) do
        local copied = {}
        for _, part in ipairs(ruleset) do
            table.insert(copied, part)
        end
        table.insert(out, copied)
    end
    return out
end

local function parse_rule_value(value)
    local out = {}
    if type(value) == "string" then
        for _, part in ipairs(split(value, ",")) do
            part = trim(part)
            if part ~= "" then
                table.insert(out, part)
            end
        end
    elseif type(value) == "table" then
        for _, part in ipairs(value) do
            if type(part) == "string" and trim(part) ~= "" then
                table.insert(out, trim(part))
            end
        end
    end
    return out
end

local function parse_rules_field(node, field_name)
    local raw = node and node[field_name]
    local out = {}
    if type(raw) == "string" and raw ~= "" then
        table.insert(out, parse_rule_value(raw))
    elseif type(raw) == "table" then
        for _, value in ipairs(raw) do
            local parsed = parse_rule_value(value)
            if #parsed > 0 then
                table.insert(out, parsed)
            end
        end
    end
    return out
end

local function parse_rules_fields(node, field_names)
    local out = {}
    for _, field_name in ipairs(field_names or {}) do
        for _, ruleset in ipairs(parse_rules_field(node, field_name)) do
            table.insert(out, ruleset)
        end
    end
    return out
end

local function merge_rules(parent_rules, new_rules)
    if not new_rules or #new_rules == 0 then
        return copy_rules(parent_rules)
    end
    if not parent_rules or #parent_rules == 0 then
        return copy_rules(new_rules)
    end
    local out = {}
    for _, new_rule in ipairs(new_rules) do
        for _, old_rule in ipairs(parent_rules) do
            local merged = {}
            for _, part in ipairs(old_rule) do
                table.insert(merged, part)
            end
            for _, part in ipairs(new_rule) do
                table.insert(merged, part)
            end
            table.insert(out, merged)
        end
    end
    return out
end

local function hosted_items_from(value)
    if type(value) ~= "string" or value == "" then
        return {}
    end
    local out = {}
    for _, part in ipairs(split(value, ",")) do
        part = trim(part)
        if part ~= "" then
            table.insert(out, part)
        end
    end
    return out
end

local function collect_rule_targets(rules)
    local out = {}
    local seen = {}
    local function add(target)
        target = trim(target or "")
        if target == "" then
            return
        end
        if target:sub(1, 1) == "@" then
            target = target:sub(2)
        end
        if target ~= "" and not seen[target] then
            seen[target] = true
            table.insert(out, target)
        end
    end
    for _, ruleset in ipairs(rules or {}) do
        for _, rule in ipairs(ruleset) do
            local text = trim(rule)
            if text:sub(1, 1) == "{" and text:sub(-1) == "}" then
                text = trim(text:sub(2, -2))
            end
            if text:sub(1, 1) == "[" and text:sub(-1) == "]" then
                text = trim(text:sub(2, -2))
            end
            if text:sub(1, 2) == "^$" then
                local parts = split(text:sub(3), "|")
                table.remove(parts, 1)
                for _, target in ipairs(parts) do
                    add(target)
                end
            elseif text:sub(1, 1) == "@" then
                add(text)
            end
        end
    end
    return out
end

local function state_field(value)
    value = tostring(value or "")
    value = value:gsub("[\r\n]", " ")
    value = value:gsub("|", "/")
    return value
end

local function add_unique_check(index, key, check)
    if not key or key == "" or not check or not check.id then
        return
    end
    index[key] = index[key] or {}
    for _, existing in ipairs(index[key]) do
        if tostring(existing.id) == tostring(check.id) then
            return
        end
    end
    table.insert(index[key], {
        id = tostring(check.id),
        name = check.name or tostring(check.id),
        group_id = check.group_id or "",
    })
end

local function index_ap_locations(groups)
    ap_locations_by_id = {}
    for _, group in ipairs(groups or {}) do
        for _, location in ipairs(group.locations or {}) do
            local id = tostring(location.location_id or "")
            if id ~= "" then
                ap_locations_by_id[id] = {
                    id = id,
                    name = location.location_name or id,
                    group_id = group.group_id or "",
                }
            end
        end
    end
end

local function mapped_check_for_location_id(location_id)
    local id = tostring(location_id or "")
    local check = ap_locations_by_id[id]
    if check then
        return check
    end
    return {
        id = id,
        name = id,
        group_id = "",
    }
end

local function add_mapped_pin(parent, section, check)
    if not parent or not parent.MapLocations then
        return
    end
    local section_id = section and section.FullID or ""
    for _, map_location in ipairs(parent.MapLocations) do
        local pack_map = map_location.map or ""
        if pack_map ~= "" then
            local x = tonumber(map_location.x) or 0
            local y = tonumber(map_location.y) or 0
            local size = tonumber(map_location.size) or 0
            local pin_key = table.concat({
                tostring(check.id or ""),
                parent.FullID or "",
                section_id,
                pack_map,
                tostring(x),
                tostring(y),
                tostring(size),
            }, "\31")
            if not mapped_pin_record_keys[pin_key] then
                mapped_pin_record_keys[pin_key] = true
                table.insert(mapped_pin_records, {
                    pin_id = table.concat({
                        check.group_id or "",
                        tostring(check.id or ""),
                        parent.FullID or "",
                        section_id,
                        pack_map,
                        tostring(x),
                        tostring(y),
                    }, ":"),
                    group_id = check.group_id or "",
                    pack_location_id = parent.FullID or "",
                    section_id = section_id,
                    parent_id = parent.FullID or "",
                    location_id = tostring(check.id or "0"),
                    location_name = check.name or tostring(check.id or ""),
                    pack_map = pack_map,
                    map_location = map_location,
                    x = x,
                    y = y,
                    size = size,
                })
            end
        end
    end
end

local function find_location(location_id, partial)
    if pack_locations_by_id[location_id] then
        return pack_locations_by_id[location_id]
    end
    if not partial then
        return nil
    end
    if not location_id:find("/", 1, true) then
        for _, location in ipairs(pack_locations) do
            if location.Name == location_id then
                return location
            end
        end
    else
        local suffix = "/" .. location_id
        for _, location in ipairs(pack_locations) do
            if #location.FullID > #suffix and location.FullID:sub(-#suffix) == suffix then
                return location
            end
        end
    end
    return nil
end

local function find_section(section_id)
    if pack_sections_by_id[section_id] then
        return pack_sections_by_id[section_id]
    end
    local slash = section_id:match("^.*()/")
    if not slash then
        return nil
    end
    local location_id = section_id:sub(1, slash - 1)
    local section_name = section_id:sub(slash + 1)
    local location = find_location(location_id, true)
    if not location then
        return nil
    end
    for _, section in ipairs(location.Sections or {}) do
        if section.Name == section_name then
            return section
        end
    end
    return nil
end

local accessibility_cache = {}
local visibility_cache = {}
local accessibility_stale = true
local visibility_stale = true
local building_accessibility = false
local building_visibility = false

local function resolve_rules(rules, visibility_rules, glitched_scoutable_as_glitched)
    local glitched_reachable = false
    local inspect_only_reachable = false
    if not rules or #rules == 0 then
        return AccessibilityLevel.Normal
    end
    for _, ruleset in ipairs(rules) do
        if #ruleset == 0 then
            return AccessibilityLevel.Normal
        end
        local reachable = AccessibilityLevel.Normal
        local inspect_only = false
        for _, rule in ipairs(ruleset) do
            local s = trim(rule or "")
            if s ~= "" then
                if s:sub(1, 1) == "{" then
                    inspect_only = true
                    s = s:sub(2)
                    if s:sub(-1) == "}" then
                        s = s:sub(1, -2)
                    end
                    s = trim(s)
                end
                local optional = false
                if #s > 1 and s:sub(1, 1) == "[" and s:sub(-1) == "]" then
                    optional = true
                    s = trim(s:sub(2, -2))
                end
                if inspect_only and s == "" then
                    s = ""
                end
                if s ~= "" then
                    local is_accessibility_level = s:sub(1, 1) == "^"
                    local is_location_reference = s:sub(1, 1) == "@"
                    local count = 1
                    if not is_accessibility_level and not is_location_reference then
                        local base, raw_count = s:match("^(.-):(%d+)$")
                        if base and base ~= "" then
                            s = base
                            count = tonumber(raw_count) or 1
                        end
                    end
                    if is_accessibility_level then
                        if s:sub(2, 2) ~= "$" then
                            reachable = AccessibilityLevel.None
                            break
                        end
                        s = s:sub(2)
                    end
                    if is_location_reference then
                        local target = s:sub(2)
                        local location = find_location(target, true)
                        local section = nil
                        local sub = AccessibilityLevel.None
                        if location then
                            sub = visibility_rules and (is_location_visible(location) and AccessibilityLevel.Normal or AccessibilityLevel.None)
                                  or is_location_reachable(location)
                        else
                            section = find_section(target)
                            if section then
                                local parent = find_location(section.ParentID, false)
                                sub = visibility_rules and (is_section_visible(parent, section) and AccessibilityLevel.Normal or AccessibilityLevel.None)
                                      or is_section_reachable(section)
                            end
                        end
                        if not inspect_only and sub == AccessibilityLevel.Inspect then
                            sub = AccessibilityLevel.None
                        elseif optional and sub == AccessibilityLevel.None then
                            sub = AccessibilityLevel.SequenceBreak
                        elseif sub == AccessibilityLevel.None then
                            reachable = AccessibilityLevel.None
                        end
                        if sub == AccessibilityLevel.SequenceBreak and reachable ~= AccessibilityLevel.None then
                            reachable = AccessibilityLevel.SequenceBreak
                        end
                        if reachable == AccessibilityLevel.None then
                            break
                        end
                    else
                        local n = Tracker:ProviderCountForCode(s)
                        if is_accessibility_level then
                            local sub = n
                            if not inspect_only and sub == AccessibilityLevel.Inspect then
                                inspect_only = true
                            elseif optional and sub == AccessibilityLevel.None then
                                sub = AccessibilityLevel.SequenceBreak
                            elseif sub == AccessibilityLevel.None then
                                reachable = AccessibilityLevel.None
                            end
                            if sub == AccessibilityLevel.SequenceBreak and reachable ~= AccessibilityLevel.None then
                                reachable = AccessibilityLevel.SequenceBreak
                            end
                            if reachable == AccessibilityLevel.None then
                                break
                            end
                        elseif n < count then
                            if optional then
                                reachable = AccessibilityLevel.SequenceBreak
                            else
                                reachable = AccessibilityLevel.None
                                break
                            end
                        end
                    end
                end
            end
        end
        if reachable == AccessibilityLevel.Normal and not inspect_only then
            return AccessibilityLevel.Normal
        end
        if reachable ~= AccessibilityLevel.None and inspect_only then
            inspect_only_reachable = true
        end
        if reachable == AccessibilityLevel.SequenceBreak then
            glitched_reachable = true
        end
    end
    local glitched_scoutable = glitched_reachable and inspect_only_reachable
    if glitched_scoutable and not glitched_scoutable_as_glitched then
        return AccessibilityLevel.Inspect
    end
    if glitched_reachable then
        return AccessibilityLevel.SequenceBreak
    end
    if inspect_only_reachable then
        return AccessibilityLevel.Inspect
    end
    return AccessibilityLevel.None
end

function is_location_reachable(location)
    if not location then
        return AccessibilityLevel.None
    end
    if building_accessibility then
        return accessibility_cache[location.FullID] or AccessibilityLevel.None
    end
    cache_accessibility()
    return accessibility_cache[location.FullID] or AccessibilityLevel.None
end

function is_section_reachable(section)
    if not section then
        return AccessibilityLevel.None
    end
    local real_section = section.Ref ~= "" and find_section(section.Ref) or section
    if not real_section then
        return AccessibilityLevel.None
    end
    if building_accessibility then
        return accessibility_cache[real_section.FullID] or AccessibilityLevel.None
    end
    cache_accessibility()
    return accessibility_cache[real_section.FullID] or AccessibilityLevel.None
end

function is_location_visible(location)
    if not location then
        return false
    end
    if not location.VisibilityRules or #location.VisibilityRules == 0 then
        return true
    end
    if building_visibility then
        return visibility_cache[location.FullID] or false
    end
    cache_visibility()
    return visibility_cache[location.FullID] or false
end

function is_section_visible(location, section)
    if not section then
        return false
    end
    local real_section = section.Ref ~= "" and find_section(section.Ref) or section
    if not real_section then
        return false
    end
    if not real_section.VisibilityRules or #real_section.VisibilityRules == 0 then
        return true
    end
    if building_visibility then
        return visibility_cache[real_section.FullID] or false
    end
    cache_visibility()
    return visibility_cache[real_section.FullID] or false
end

local function is_map_location_visible(parent, section, map_location)
    if not parent or not map_location then
        return false
    end
    if not is_location_visible(parent) then
        return false
    end
    if section and not is_section_visible(parent, section) then
        return false
    end
    if map_location.invisibility_rules and #map_location.invisibility_rules > 0 and
        resolve_rules(map_location.invisibility_rules, true, false) ~= AccessibilityLevel.None then
        return false
    end
    return resolve_rules(map_location.visibility_rules or {}, true, false) ~= AccessibilityLevel.None
end

function cache_accessibility()
    if not accessibility_stale then
        return
    end
    accessibility_cache = {}
    accessibility_stale = false
    building_accessibility = true
    local done = false
    local guard = 0
    while not done and guard < 64 do
        done = true
        guard = guard + 1
        for _, location in ipairs(pack_locations) do
            local old = accessibility_cache[location.FullID]
            local next_value = resolve_rules(location.AccessRules, false, location.GlitchedScoutableAsGlitched)
            if old ~= next_value then
                accessibility_cache[location.FullID] = next_value
                location.AccessibilityLevel = next_value
                done = false
            end
            for _, section in ipairs(location.Sections or {}) do
                old = accessibility_cache[section.FullID]
                next_value = resolve_rules(section.AccessRules, false, section.GlitchedScoutableAsGlitched)
                if old ~= next_value then
                    accessibility_cache[section.FullID] = next_value
                    section.AccessibilityLevel = next_value
                    done = false
                end
            end
        end
    end
    building_accessibility = false
end

function cache_visibility()
    if not visibility_stale then
        return
    end
    visibility_cache = {}
    visibility_stale = false
    building_visibility = true
    local done = false
    local guard = 0
    while not done and guard < 64 do
        done = true
        guard = guard + 1
        for _, location in ipairs(pack_locations) do
            if location.VisibilityRules and #location.VisibilityRules > 0 then
                local old = visibility_cache[location.FullID]
                local next_value = resolve_rules(location.VisibilityRules, true, false) ~= AccessibilityLevel.None
                if old ~= next_value then
                    visibility_cache[location.FullID] = next_value
                    done = false
                end
            end
            for _, section in ipairs(location.Sections or {}) do
                if section.VisibilityRules and #section.VisibilityRules > 0 then
                    local old = visibility_cache[section.FullID]
                    local next_value = resolve_rules(section.VisibilityRules, true, false) ~= AccessibilityLevel.None
                    if old ~= next_value then
                        visibility_cache[section.FullID] = next_value
                        done = false
                    end
                end
            end
        end
    end
    building_visibility = false
end

local function register_pack_location(location)
    table.insert(pack_locations, location)
    pack_locations_by_id[location.FullID] = location
    register_code("@" .. location.FullID, location)
    register_code(location.FullID, location)
    location.AccessibilityLevel = AccessibilityLevel.None
end

local function register_pack_section(section)
    pack_sections_by_id[section.FullID] = section
    register_code("@" .. section.FullID, section)
    register_code(section.FullID, section)
    section.AccessibilityLevel = AccessibilityLevel.None
end

local function parse_location_node(node, parent_lookup, parent_id, parent_access_rules, parent_visibility_rules,
                                   closed_img, opened_img, overlay_background, glitched_scoutable_as_glitched)
    if type(node) ~= "table" then
        return
    end
    local name = node.name or ""
    local inherited_access = parent_access_rules or {}
    local inherited_visibility = parent_visibility_rules or {}
    if type(node.parent) == "string" and node.parent ~= "" then
        local parent_name = node.parent:sub(1, 1) == "@" and node.parent:sub(2) or node.parent
        local parent = find_location(parent_name, true)
        if parent then
            inherited_access = parent.AccessRules
            inherited_visibility = parent.VisibilityRules
        end
    end
    local access_rules = merge_rules(inherited_access, parse_rules_field(node, "access_rules"))
    local visibility_rules = merge_rules(inherited_visibility, parse_rules_field(node, "visibility_rules"))
    local location_id = parent_id ~= "" and (parent_id .. "/" .. name) or name
    local location = {
        Name = name,
        Kind = "location",
        FullID = location_id,
        AccessRules = access_rules,
        VisibilityRules = visibility_rules,
        GlitchedScoutableAsGlitched = node.inspectable_sequence_break or glitched_scoutable_as_glitched or false,
        MapLocations = {},
        Sections = {},
        ChestCount = tonumber(node.item_count) or 0,
        AvailableChestCount = tonumber(node.item_count) or 0,
        ItemState = { MANUAL_LOCATIONS = { default = {} }, MANUAL_LOCATIONS_ORDER = {} },
        Owner = { ModifiedByUser = false },
        SetOverlayFontSize = function() end,
        SetOverlayAlign = function() end,
    }
    if type(node.map_locations) == "table" then
        for _, map_location in ipairs(node.map_locations) do
            if type(map_location) == "table" then
                table.insert(location.MapLocations, {
                    map = map_location.map or "",
                    x = tonumber(map_location.x) or 0,
                    y = tonumber(map_location.y) or 0,
                    size = tonumber(map_location.size) or 0,
                    visibility_rules = parse_rules_fields(map_location, { "restrict_visibility_rules", "visibility_rules" }),
                    invisibility_rules = parse_rules_fields(map_location, { "force_invisibility_rules", "invisibility_rules" }),
                })
            end
        end
    end
    register_pack_location(location)
    if type(node.sections) == "table" then
        for _, section_node in ipairs(node.sections) do
            if type(section_node) == "table" then
                local hosted_items = hosted_items_from(section_node.hosted_item)
                local ref = section_node.ref or ""
                local item_count = (#hosted_items == 0 and ref == "") and 1 or 0
                if section_node.item_count ~= nil then
                    item_count = tonumber(section_node.item_count) or item_count
                end
                local section_access_rules = merge_rules(access_rules, parse_rules_field(section_node, "access_rules"))
                local section_visibility_rules = merge_rules(visibility_rules, parse_rules_field(section_node, "visibility_rules"))
                local section = {
                    Name = section_node.name or "",
                    Kind = "location-section",
                    ParentID = location.FullID,
                    FullID = location.FullID .. "/" .. (section_node.name or ""),
                    AccessRules = section_access_rules,
                    VisibilityRules = section_visibility_rules,
                    HostedItems = hosted_items,
                    Ref = ref,
                    ChestCount = item_count,
                    AvailableChestCount = item_count,
                    ItemCount = item_count,
                    ItemCleared = 0,
                    GlitchedScoutableAsGlitched = section_node.inspectable_sequence_break or location.GlitchedScoutableAsGlitched,
                    ItemState = { MANUAL_LOCATIONS = { default = {} }, MANUAL_LOCATIONS_ORDER = {} },
                    Owner = { ModifiedByUser = false },
                    SetOverlayFontSize = function() end,
                    SetOverlayAlign = function() end,
                }
                table.insert(location.Sections, section)
                register_pack_section(section)
            end
        end
    end
    if type(node.children) == "table" then
        for _, child in ipairs(node.children) do
            parse_location_node(child, parent_lookup, location.FullID, access_rules, visibility_rules,
                node.chest_unopened_img or closed_img, node.chest_opened_img or opened_img,
                node.overlay_background or overlay_background, location.GlitchedScoutableAsGlitched)
        end
    end
end

function Tracker:AddLocations(path)
    local locations = load_relaxed_json(bundle_root .. "/poptracker-adapted/" .. path)
    if type(locations) ~= "table" then
        return
    end
    for _, location in ipairs(locations) do
        parse_location_node(location, pack_locations_by_id, "", {}, {}, "", "", "", false)
    end
    accessibility_stale = true
    visibility_stale = true
end

local function load_pack_location_mapping()
    local mapping_path = bundle_root .. "/poptracker-adapted/scripts/autotracking/location_mapping.lua"
    local mapping_file = io.open(mapping_path, "rb")
    if not mapping_file then
        return
    end
    mapping_file:close()
    local ok, err = pcall(require, "scripts/autotracking/location_mapping")
    if not ok then
        error("location_mapping_load_failed:" .. tostring(err))
    end
    has_pack_location_mapping = type(LOCATION_MAPPING) == "table"
end

local function build_location_checks_from_pack_mapping()
    if not has_pack_location_mapping then
        return
    end
    location_checks_by_pack_id = {}
    section_checks_by_id = {}
    mapped_pin_records = {}
    mapped_pin_record_keys = {}

    for location_id, mapped_objects in pairs(LOCATION_MAPPING) do
        local check = mapped_check_for_location_id(location_id)
        if check.id ~= "" and type(mapped_objects) == "table" then
            for _, mapped_code in pairs(mapped_objects) do
                if type(mapped_code) == "string" and mapped_code:sub(1, 1) == "@" then
                    local mapped_object = Tracker:FindObjectForCode(mapped_code)
                    if mapped_object and mapped_object.Kind == "location-section" then
                        local parent = pack_locations_by_id[mapped_object.ParentID]
                        if parent then
                            add_unique_check(location_checks_by_pack_id, parent.FullID, check)
                            add_unique_check(section_checks_by_id, mapped_object.FullID, check)
                            add_mapped_pin(parent, mapped_object, check)
                        end
                    elseif mapped_object and mapped_object.Kind == "location" then
                        add_unique_check(location_checks_by_pack_id, mapped_object.FullID, check)
                        add_mapped_pin(mapped_object, nil, check)
                    end
                end
            end
        end
    end
end

local function decode_slot_data()
    if not raw_slot_data or raw_slot_data == "" then
        return nil
    end
    local ok, result = pcall(parse_relaxed_json_text, raw_slot_data, "slot_data")
    if ok and type(result) == "table" then
        return result
    end
    return nil
end

local function load_pack_archipelago_script()
    local script_path = bundle_root .. "/poptracker-adapted/scripts/autotracking/archipelago.lua"
    local script_file = io.open(script_path, "rb")
    if not script_file then
        return false
    end
    script_file:close()
    local ok, err = pcall(require, "scripts/autotracking/archipelago")
    if not ok then
        error("archipelago_script_load_failed:" .. tostring(err))
    end
    if type(EmptyLocationTargets) == "function" and not __sekailink_wrapped_empty_location_targets then
        local original_empty_location_targets = EmptyLocationTargets
        EmptyLocationTargets = function(...)
            local empty_ok, empty_result = pcall(original_empty_location_targets, ...)
            if empty_ok then
                return empty_result
            end
            return nil
        end
        __sekailink_wrapped_empty_location_targets = true
    end
    return type(onClear) == "function"
end

local function apply_pack_autotracking_clear(slot_data)
    if type(slot_data) ~= "table" then
        return
    end
    if not load_pack_archipelago_script() then
        return
    end
    Archipelago.PlayerNumber = Archipelago.PlayerNumber or -1
    Archipelago.TeamNumber = Archipelago.TeamNumber or 0
    Archipelago.Seed = Archipelago.Seed or ""
    Archipelago.MissingLocations = {}
    Archipelago.CheckedLocations = {}
    for location_id, _ in pairs(ap_locations_by_id) do
        if checked_location_ids[tostring(location_id)] then
            table.insert(Archipelago.CheckedLocations, tonumber(location_id) or location_id)
        else
            table.insert(Archipelago.MissingLocations, tonumber(location_id) or location_id)
        end
    end
    local ok, err = pcall(onClear, slot_data)
    if not ok then
        error("archipelago_clear_failed:" .. tostring(err))
    end
    accessibility_stale = true
    visibility_stale = true
end

local function apply_checked_location_state()
    for _, checks in pairs(location_checks_by_pack_id) do
        for _, check in ipairs(checks) do
            all_location_names[check.name] = true
            if checked_location_ids[tostring(check.id)] then
                checked_location_names[check.name] = true
            end
        end
    end
    for _, location in ipairs(pack_locations) do
        local checks = location_checks_by_pack_id[location.FullID] or {}
        local checked = 0
        for _, check in ipairs(checks) do
            if checked_location_ids[tostring(check.id)] or checked_location_names[check.name] then
                checked = checked + 1
            end
        end
        location.MappedCheckCount = #checks
        location.CheckedCheckCount = checked
        if #checks > 0 then
            location.ChestCount = #checks
            location.AvailableChestCount = math.max(0, #checks - checked)
        end
        for _, section in ipairs(location.Sections or {}) do
            local section_checks = section_checks_by_id[section.FullID] or {}
            if #section_checks > 0 then
                local section_checked = 0
                for _, check in ipairs(section_checks) do
                    if checked_location_ids[tostring(check.id)] or checked_location_names[check.name] then
                        section_checked = section_checked + 1
                    end
                end
                section.ChestCount = math.max(section.ItemCount or 0, #section_checks)
                section.AvailableChestCount = math.max(0, section.ChestCount - section_checked)
                section.ItemCleared = section_checked
            else
                local targets = collect_rule_targets(section.AccessRules)
                local target_count = 0
                local target_checked = 0
                for _, target in ipairs(targets) do
                    if all_location_names[target] then
                        target_count = target_count + 1
                        if checked_location_names[target] then
                            target_checked = target_checked + 1
                        end
                    end
                end
                if target_count > 0 then
                    section.ChestCount = math.max(section.ItemCount or 0, target_count)
                    section.AvailableChestCount = math.max(0, section.ChestCount - target_checked)
                    section.ItemCleared = target_checked
                end
            end
        end
    end
end

local function calculate_location_state(location)
    local has_reachable = false
    local has_unreachable = false
    local has_sequence_break = false
    local has_inspect = false
    local has_visible = false
    if location.MappedCheckCount and location.MappedCheckCount > 0 and location.AvailableChestCount == 0 then
        return 0
    end
    for _, original_section in ipairs(location.Sections or {}) do
        local section = original_section.Ref ~= "" and find_section(original_section.Ref) or original_section
        if section and is_section_visible(location, section) then
            has_visible = true
            local item_count = section.ChestCount or section.ItemCount or 0
            local available = section.AvailableChestCount
            if available == nil then
                available = item_count
            end
            local has_missing_hosted = false
            for _, code in ipairs(section.HostedItems or {}) do
                if Tracker:ProviderCountForCode(code) == 0 then
                    has_missing_hosted = true
                    break
                end
            end
            if available > 0 or has_missing_hosted or item_count == 0 then
                local reachable = is_section_reachable(section)
                if reachable == AccessibilityLevel.Normal or reachable == AccessibilityLevel.Cleared then
                    has_reachable = true
                elseif reachable == AccessibilityLevel.None then
                    has_unreachable = true
                elseif reachable == AccessibilityLevel.Inspect or reachable == AccessibilityLevel.Partial then
                    has_inspect = true
                else
                    has_sequence_break = true
                end
            end
        end
    end
    if not has_visible then
        return -1
    end
    local result = 0
    if has_reachable then result = result + 1 end
    if has_unreachable then result = result + 2 end
    if has_sequence_break then result = result + 4 end
    if has_inspect then result = result + 8 end
    return result
end

local function calculate_section_state(location, original_section)
    local section = original_section.Ref ~= "" and find_section(original_section.Ref) or original_section
    if not section or not is_section_visible(location, section) then
        return -1
    end
    local item_count = section.ChestCount or section.ItemCount or 0
    local available = section.AvailableChestCount
    if available == nil then
        available = item_count
    end
    local has_missing_hosted = false
    for _, code in ipairs(section.HostedItems or {}) do
        if Tracker:ProviderCountForCode(code) == 0 then
            has_missing_hosted = true
            break
        end
    end
    if available <= 0 and not has_missing_hosted and item_count > 0 then
        return 0
    end
    local reachable = is_section_reachable(section)
    if reachable == AccessibilityLevel.Normal or reachable == AccessibilityLevel.Cleared then
        return 1
    end
    if reachable == AccessibilityLevel.None then
        return 2
    end
    if reachable == AccessibilityLevel.Inspect or reachable == AccessibilityLevel.Partial then
        return 8
    end
    return 4
end

local function color_for_location_state(state)
    if state == 0 then
        return "black"
    end
    if state < 0 then
        return "hidden"
    end
    if bit.band(state, 1) ~= 0 then
        return "green"
    end
    if bit.band(state, 4) ~= 0 then
        return "yellow"
    end
    if bit.band(state, 8) ~= 0 then
        return "blue"
    end
    return "red"
end

ScriptHost = ScriptHost or {}
ScriptHost.AddOnFrameHandler = ScriptHost.AddOnFrameHandler or function() end
ScriptHost.RemoveOnFrameHandler = ScriptHost.RemoveOnFrameHandler or function() end
ScriptHost.AddWatchForCode = ScriptHost.AddWatchForCode or function() end
ScriptHost.RemoveWatchForCode = ScriptHost.RemoveWatchForCode or function() end
ScriptHost.AddOnLocationSectionChangedHandler = ScriptHost.AddOnLocationSectionChangedHandler or function() end
ScriptHost.RemoveOnLocationSectionChangedHandler = ScriptHost.RemoveOnLocationSectionChangedHandler or function() end
ScriptHost.RemoveOnLocationSectionHandler = ScriptHost.RemoveOnLocationSectionHandler or function() end
ScriptHost.AddMemoryWatch = ScriptHost.AddMemoryWatch or function() end
ScriptHost.RemoveMemoryWatch = ScriptHost.RemoveMemoryWatch or function() end
ScriptHost.PushMarkdownNotification = ScriptHost.PushMarkdownNotification or function() end
ScriptHost.CreateLuaItem = ScriptHost.CreateLuaItem or function() return {} end

Archipelago = Archipelago or {}
Archipelago.PlayerNumber = Archipelago.PlayerNumber or -1
Archipelago.TeamNumber = Archipelago.TeamNumber or 0
Archipelago.MissingLocations = Archipelago.MissingLocations or {}
Archipelago.CheckedLocations = Archipelago.CheckedLocations or {}
Archipelago.AddSetReplyHandler = Archipelago.AddSetReplyHandler or function() end

PopVersion = "0.32.0"

local function is_ident_char(ch)
    return ch:match("[%w_%.:]") ~= nil
end

local function find_operand_left(source, pos)
    local cursor = pos
    while cursor >= 1 and source:sub(cursor, cursor):match("%s") do
        cursor = cursor - 1
    end
    if cursor < 1 then
        return nil
    end
    local last = source:sub(cursor, cursor)
    if last == ")" or last == "]" then
        local close_char = last
        local open_char = last == ")" and "(" or "["
        local depth = 1
        cursor = cursor - 1
        while cursor >= 1 do
            local ch = source:sub(cursor, cursor)
            if ch == close_char then
                depth = depth + 1
            elseif ch == open_char then
                depth = depth - 1
                if depth == 0 then
                    return cursor, pos
                end
            end
            cursor = cursor - 1
        end
        return nil
    end
    local start_pos = cursor
    while start_pos >= 1 and is_ident_char(source:sub(start_pos, start_pos)) do
        start_pos = start_pos - 1
    end
    if start_pos == cursor then
        return nil
    end
    start_pos = start_pos + 1
    while start_pos > 1 do
        local probe = start_pos - 1
        while probe >= 1 and source:sub(probe, probe):match("%s") do
            probe = probe - 1
        end
        if probe < 1 then
            break
        end
        local ch = source:sub(probe, probe)
        if ch == "." or ch == ":" then
            start_pos = probe
        elseif ch == ")" or ch == "]" then
            local close_char = ch
            local open_char = ch == ")" and "(" or "["
            local depth = 1
            probe = probe - 1
            while probe >= 1 do
                local current = source:sub(probe, probe)
                if current == close_char then
                    depth = depth + 1
                elseif current == open_char then
                    depth = depth - 1
                    if depth == 0 then
                        start_pos = probe
                        break
                    end
                end
                probe = probe - 1
            end
        elseif is_ident_char(ch) then
            while probe >= 1 and is_ident_char(source:sub(probe, probe)) do
                probe = probe - 1
            end
            start_pos = probe + 1
        else
            break
        end
    end
    return start_pos, pos
end

local function find_operand_right(source, pos)
    local cursor = pos
    while cursor <= #source and source:sub(cursor, cursor):match("%s") do
        cursor = cursor + 1
    end
    if cursor > #source then
        return nil
    end
    local first = source:sub(cursor, cursor)
    if first == "(" or first == "[" then
        local open_char = first
        local close_char = first == "(" and ")" or "]"
        local depth = 1
        local end_pos = cursor + 1
        while end_pos <= #source do
            local ch = source:sub(end_pos, end_pos)
            if ch == open_char then
                depth = depth + 1
            elseif ch == close_char then
                depth = depth - 1
                if depth == 0 then
                    return cursor, end_pos
                end
            end
            end_pos = end_pos + 1
        end
        return nil
    end
    local end_pos = cursor
    while end_pos <= #source and is_ident_char(source:sub(end_pos, end_pos)) do
        end_pos = end_pos + 1
    end
    if end_pos == cursor then
        return nil
    end
    return cursor, end_pos - 1
end

local function preprocess_lua_source(source)
    local cursor = 1
    while true do
        local marker = source:find("//", cursor, true)
        if not marker then
            return source
        end
        local left_start, left_end = find_operand_left(source, marker - 1)
        local right_start, right_end = find_operand_right(source, marker + 2)
        if not left_start or not right_start then
            cursor = marker + 2
        else
            local left = source:sub(left_start, left_end)
            local right = source:sub(right_start, right_end)
            local replacement = "math.floor((" .. left .. ") / (" .. right .. "))"
            source = source:sub(1, left_start - 1) .. replacement .. source:sub(right_end + 1)
            cursor = left_start + #replacement
        end
    end
end

package.path = bundle_root .. "/poptracker-adapted/?.lua;" ..
    bundle_root .. "/poptracker-adapted/?/init.lua;" ..
    bundle_root .. "/poptracker-adapted/?.lua;" ..
    package.path

local searchers = package.searchers or package.loaders
table.insert(searchers, 1, function(module_name)
    local relative = module_name:gsub("%.", "/")
    for template in package.path:gmatch("[^;]+") do
        local candidate = template:gsub("%?", relative)
        local file = io.open(candidate, "rb")
        if file then
            local source = file:read("*a")
            file:close()
            local chunk, err = load(preprocess_lua_source(source), "@" .. candidate)
            if not chunk then
                error("lua_module_load_failed:" .. candidate .. ":" .. tostring(err))
            end
            return chunk, candidate
        end
    end
    return "\n\tno preprocessed loader found for " .. module_name
end)

local slot_state = {}
local source_state = {}
for line in slurp(state_path):gmatch("[^\r\n]+") do
    local parts = split(line, "|")
    if parts[1] == "meta" then
        if parts[2] == "variant" and parts[3] ~= "" then
            Tracker.ActiveVariantUID = parts[3]
        elseif parts[2] == "slot_data" and parts[3] ~= "" then
            raw_slot_data = parts[3]
        end
    elseif parts[1] == "location_file" then
        if parts[2] and parts[2] ~= "" then
            table.insert(location_files, parts[2])
        end
    elseif parts[1] == "slot" then
        slot_state[parts[2]] = {
            owned = parts[3] == "1",
            stage = tonumber(parts[4]) or 0,
            count = tonumber(parts[5]) or 0,
        }
    elseif parts[1] == "source" then
        source_state[parts[2]] = source_state[parts[2]] or {}
        table.insert(source_state[parts[2]], parts[3])
    elseif parts[1] == "check" then
        checked_location_ids[tostring(parts[2])] = true
    elseif parts[1] == "pin_location" then
        local pack_location_id = parts[2]
        local location_id = parts[3]
        local location_name = parts[4]
        if pack_location_id and pack_location_id ~= "" and location_id and location_name and location_name ~= "" then
            location_checks_by_pack_id[pack_location_id] = location_checks_by_pack_id[pack_location_id] or {}
            local duplicate = false
            for _, check in ipairs(location_checks_by_pack_id[pack_location_id]) do
                if tostring(check.id) == tostring(location_id) then
                    duplicate = true
                    break
                end
            end
            if not duplicate then
                table.insert(location_checks_by_pack_id[pack_location_id], {
                    id = location_id,
                    name = location_name,
                })
            end
        end
    end
end

require("scripts/items_import")
require("scripts/logic/logic_helpers")
require("scripts/logic/logic_main")
require("scripts/logic_import")
do
    local locations_import_path = bundle_root .. "/poptracker-adapted/scripts/locations_import.lua"
    local locations_import = io.open(locations_import_path, "rb")
    if locations_import then
        locations_import:close()
        require("scripts/locations_import")
    else
        for _, location_file in ipairs(location_files) do
            Tracker:AddLocations(location_file)
        end
    end
end

load_pack_location_mapping()
local groups = load_relaxed_json(bundle_root .. "/location-groups.complete.json").groups
index_ap_locations(groups)
build_location_checks_from_pack_mapping()

for _, code in ipairs({ "update", "manual_location_storage", "manual_er_storage", "manual_misc_items_storage" }) do
    local object = Tracker:FindObjectForCode(code)
    object.ItemState = object.ItemState or { MANUAL_LOCATIONS = { default = {} }, MANUAL_LOCATIONS_ORDER = {} }
    object.ItemState.MANUAL_LOCATIONS = object.ItemState.MANUAL_LOCATIONS or { default = {} }
    object.ItemState.MANUAL_LOCATIONS_ORDER = object.ItemState.MANUAL_LOCATIONS_ORDER or {}
end

apply_pack_autotracking_clear(decode_slot_data())

for slot_id, state in pairs(slot_state) do
    local object = Tracker:FindObjectForCode(slot_id)
    if object then
        object.Active = state.owned or state.stage > 0 or state.count > 0
        if state.stage > 0 then
            object.CurrentStage = state.stage
        elseif state.count > 0 then
            object.CurrentStage = state.count
        else
            object.CurrentStage = object.Active and 1 or 0
        end
        object.AcquiredCount = math.max(state.count, object.CurrentStage)
    end
    if source_state[slot_id] then
        for _, source_name in ipairs(source_state[slot_id]) do
            local source_object = objects_by_name[source_name]
            if source_object then
                source_object.Active = true
                source_object.CurrentStage = math.max(1, source_object.CurrentStage)
                source_object.AcquiredCount = math.max(1, source_object.AcquiredCount)
            end
        end
    end
end

for _, group in ipairs(groups or {}) do
    for _, location in ipairs(group.locations or {}) do
        all_location_names[location.location_name] = true
        if checked_location_ids[tostring(location.location_id)] then
            checked_location_names[location.location_name] = true
        end
    end
end

apply_checked_location_state()

if StateChanged then
    local ok, err = pcall(StateChanged)
    if not ok then
        error("state_changed_failed:" .. tostring(err))
    end
end
if EmptyLocationTargets then
    pcall(EmptyLocationTargets)
end

local output = assert(io.open(output_path, "wb"))
output:write("logic_ready|1|1\n")

for _, pin in ipairs(mapped_pin_records) do
    local parent = find_location(pin.parent_id or pin.pack_location_id, false)
    local section = pin.section_id ~= "" and find_section(pin.section_id) or nil
    if is_map_location_visible(parent, section, pin.map_location) then
        output:write(string.format("pin|%s|%s|%s|%s|%s|%s|%s|%.17g|%.17g|%.17g\n",
            state_field(pin.pin_id),
            state_field(pin.group_id),
            state_field(pin.pack_location_id),
            state_field(pin.section_id),
            state_field(pin.location_id),
            state_field(pin.location_name),
            state_field(pin.pack_map),
            tonumber(pin.x) or 0,
            tonumber(pin.y) or 0,
            tonumber(pin.size) or 0))
    end
end

for _, location in ipairs(pack_locations) do
    if location.MapLocations and #location.MapLocations > 0 and location.Sections and #location.Sections > 0 then
        local state = calculate_location_state(location)
        local color = color_for_location_state(state)
        local normal = (state > 0 and bit.band(state, 1) ~= 0) and 1 or 0
        local sequence_break = (state > 0 and bit.band(state, 4) ~= 0) and 1 or 0
        local inspect = (state > 0 and bit.band(state, 8) ~= 0) and 1 or 0
        local none = (state > 0 and bit.band(state, 2) ~= 0) and 1 or 0
        output:write(string.format("pack_location|%s|%s|%d|%d|%d|%d\n",
            location.FullID, color, normal, sequence_break, inspect, none))
    end
    for _, section in ipairs(location.Sections or {}) do
        if section_checks_by_id[section.FullID] and #section_checks_by_id[section.FullID] > 0 then
            local state = calculate_section_state(location, section)
            local color = color_for_location_state(state)
            local normal = (state > 0 and bit.band(state, 1) ~= 0) and 1 or 0
            local sequence_break = (state > 0 and bit.band(state, 4) ~= 0) and 1 or 0
            local inspect = (state > 0 and bit.band(state, 8) ~= 0) and 1 or 0
            local none = (state > 0 and bit.band(state, 2) ~= 0) and 1 or 0
            output:write(string.format("section|%s|%s|%d|%d|%d|%d\n",
                section.FullID, color, normal, sequence_break, inspect, none))
        end
    end
end

for _, group in ipairs(groups) do
    local normal = 0
    local sequence_break = 0
    local inspect = 0
    local none = 0
    for _, location in ipairs(group.locations or {}) do
        local value = 0
        if CanReach then
            value = CanReach(location.location_name) or 0
        end
        if value >= AccessibilityLevel.Normal then
            normal = normal + 1
        elseif value >= AccessibilityLevel.SequenceBreak then
            sequence_break = sequence_break + 1
        elseif value > AccessibilityLevel.None then
            inspect = inspect + 1
        else
            none = none + 1
        end
    end
    local color = "red"
    if normal > 0 then
        color = "green"
    elseif sequence_break > 0 then
        color = "yellow"
    elseif inspect > 0 then
        color = "blue"
    end
    output:write(string.format("group|%s|%s|%d|%d|%d|%d\n",
        group.group_id, color, normal, sequence_break, inspect, none))
end

output:close()
