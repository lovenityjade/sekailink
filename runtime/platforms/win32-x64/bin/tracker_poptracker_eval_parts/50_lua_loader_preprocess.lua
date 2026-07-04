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
Archipelago.AddClearHandler = Archipelago.AddClearHandler or function() end
Archipelago.AddItemHandler = Archipelago.AddItemHandler or function() end
Archipelago.AddLocationHandler = Archipelago.AddLocationHandler or function() end
Archipelago.AddSetReplyHandler = Archipelago.AddSetReplyHandler or function() end
Archipelago.AddRetrievedHandler = Archipelago.AddRetrievedHandler or function() end

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
                    local start_pos = cursor
                    local probe = start_pos - 1
                    while probe >= 1 and source:sub(probe, probe):match("%s") do
                        probe = probe - 1
                    end
                    if probe >= 1 and is_ident_char(source:sub(probe, probe)) then
                        while probe >= 1 and is_ident_char(source:sub(probe, probe)) do
                            probe = probe - 1
                        end
                        start_pos = probe + 1
                    end
                    return start_pos, pos
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

local function protect_lua_long_strings(source)
    local placeholders = {}
    local next_index = 0
    local protected = source:gsub("(%[=*%[.-%]=*%])", function(text)
        next_index = next_index + 1
        local token = "__SEKAILINK_LONG_STRING_" .. tostring(next_index) .. "__"
        placeholders[token] = text
        return token
    end)
    return protected, function(text)
        for token, original in pairs(placeholders) do
            text = text:gsub(token, function()
                return original
            end)
        end
        return text
    end
end

local function preprocess_binary_operator(source, marker, replacement)
    local cursor = 1
    while true do
        local marker_pos = source:find(marker, cursor, true)
        if not marker_pos then
            return source
        end
        local left_start, left_end = find_operand_left(source, marker_pos - 1)
        local right_start, right_end = find_operand_right(source, marker_pos + #marker)
        if not left_start or not right_start then
            cursor = marker_pos + #marker
        else
            local left = source:sub(left_start, left_end)
            local right = source:sub(right_start, right_end)
            local rewritten
            if replacement == "__sekailink_floor_div" then
                rewritten = "math.floor((" .. left .. ") / (" .. right .. "))"
            else
                rewritten = replacement .. "((" .. left .. "), (" .. right .. "))"
            end
            source = source:sub(1, left_start - 1) .. rewritten .. source:sub(right_end + 1)
            cursor = left_start + #rewritten
        end
    end
end

local function preprocess_lua_source(source)
    local restore_long_strings
    source, restore_long_strings = protect_lua_long_strings(source)
    source = preprocess_binary_operator(source, "//", "__sekailink_floor_div")
    source = preprocess_binary_operator(source, "<<", "bit.lshift")
    source = preprocess_binary_operator(source, ">>", "bit.rshift")
    source = preprocess_binary_operator(source, "&", "bit.band")
    source = preprocess_binary_operator(source, "|", "bit.bor")
    return restore_long_strings(source)
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

package.preload["os"] = package.preload["os"] or function()
    return os
end
package.preload["math"] = package.preload["math"] or function()
    return math
end
package.preload["string"] = package.preload["string"] or function()
    return string
end
package.preload["table"] = package.preload["table"] or function()
    return table
end
package.preload["bit"] = package.preload["bit"] or function()
    return bit
end

