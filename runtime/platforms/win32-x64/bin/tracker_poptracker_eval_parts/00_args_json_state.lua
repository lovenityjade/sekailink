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
local objects_by_code_ci = {}
local objects_for_code = {}
local code_stage_requirements = {}
local code_stage_ambiguous = {}
local objects_by_name = {}
local all_items = {}
local lua_items = {}
local register_code
local accessibility_cache = {}
local visibility_cache = {}
local accessibility_stale = true
local visibility_stale = true
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
local runtime_slot_id = ""
local runtime_seed_id = ""
local runtime_room_id = ""
local ap_item_events = {}
