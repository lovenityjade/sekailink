local function script_dir()
    local source = debug.getinfo(1, "S").source or ""
    if source:sub(1, 1) == "@" then
        source = source:sub(2)
    end
    source = source:gsub("\\", "/")
    local directory = source:match("^(.*)/[^/]*$")
    return directory or "."
end

local parts = {
    "00_args_json_state.lua",
    "10_tracker_api_items.lua",
    "20_rules.lua",
    "30_locations_index.lua",
    "31_accessibility.lua",
    "32_location_loader.lua",
    "40_callbacks_and_states.lua",
    "50_lua_loader_preprocess.lua",
    "60_run_and_output.lua",
}

local root = script_dir() .. "/tracker_poptracker_eval_parts"
local chunks = {}

for _, part in ipairs(parts) do
    local path = root .. "/" .. part
    local file = assert(io.open(path, "rb"), "tracker_eval_part_missing:" .. path)
    chunks[#chunks + 1] = "\n-- sekailink tracker eval part: " .. part .. "\n"
    chunks[#chunks + 1] = file:read("*a")
    file:close()
end

local assembled = table.concat(chunks, "\n")
local chunk, err = loadstring(assembled, "@tracker_poptracker_eval_assembled.lua")
if not chunk then
    error("tracker_eval_assemble_failed:" .. tostring(err))
end

return chunk()
