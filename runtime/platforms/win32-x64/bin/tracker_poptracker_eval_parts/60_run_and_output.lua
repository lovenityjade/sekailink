local slot_state = {}
local source_state = {}
for line in slurp(state_path):gmatch("[^\r\n]+") do
    local parts = split(line, "|")
    if parts[1] == "meta" then
        if parts[2] == "variant" and parts[3] ~= "" then
            Tracker.ActiveVariantUID = parts[3]
        elseif parts[2] == "slot_data" and parts[3] ~= "" then
            raw_slot_data = parts[3]
        elseif parts[2] == "slot_id" and parts[3] ~= "" then
            runtime_slot_id = parts[3]
        elseif parts[2] == "seed_id" and parts[3] ~= "" then
            runtime_seed_id = parts[3]
        elseif parts[2] == "room_id" and parts[3] ~= "" then
            runtime_room_id = parts[3]
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
    elseif parts[1] == "ap_item" then
        table.insert(ap_item_events, {
            delivery_id = parts[2] or "",
            index = tonumber(parts[3]) or #ap_item_events,
            item_id = tonumber(parts[4]) or 0,
            player_number = tonumber(parts[5]) or tonumber(runtime_slot_id) or 1,
            item_name = parts[6] or "",
        })
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

if pack_relative_file_exists("scripts/init.lua") then
    local init_ok, init_err = pcall(require, "scripts/init")
    if not init_ok then
        error("pack_init_failed:" .. tostring(init_err))
    end
    wrap_pack_runtime_guards()
    run_script_host_frame_handlers()
else
    require("scripts/items_import")
    require("scripts/logic/logic_helpers")
    require("scripts/logic/logic_main")
    require("scripts/logic_import")
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
    if object then
        object.ItemState = object.ItemState or { MANUAL_LOCATIONS = { default = {} }, MANUAL_LOCATIONS_ORDER = {} }
        object.ItemState.MANUAL_LOCATIONS = object.ItemState.MANUAL_LOCATIONS or { default = {} }
        object.ItemState.MANUAL_LOCATIONS_ORDER = object.ItemState.MANUAL_LOCATIONS_ORDER or {}
    end
end

local decoded_slot_data = decode_slot_data()
if type(decoded_slot_data) == "table" then
    fire_archipelago_clear(decoded_slot_data)
    fire_archipelago_item_events()
    run_script_host_frame_handlers()
end

local watch_state_before = capture_watch_object_state()

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
mark_logic_stale()

local changed_watch_codes = collect_changed_watch_codes(watch_state_before)
for slot_id, _ in pairs(slot_state) do
    mark_changed_code(changed_watch_codes, slot_id)
end
for slot_id, _ in pairs(source_state) do
    mark_changed_code(changed_watch_codes, slot_id)
end
run_script_host_watchers(changed_watch_codes)

for _, group in ipairs(groups or {}) do
    for _, location in ipairs(group.locations or {}) do
        all_location_names[location.location_name] = true
        if checked_location_ids[tostring(location.location_id)] then
            checked_location_names[location.location_name] = true
        end
    end
end

fire_archipelago_checked_locations()
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
mark_logic_stale()

local output = assert(io.open(output_path, "wb"))
output:write("logic_ready|1|1\n")

local written_item_codes = {}
for code, object in pairs(objects_by_code) do
    if not written_item_codes[code] and object then
        written_item_codes[code] = true
        local active = object.Active and 1 or 0
        local stage = tonumber(object.CurrentStage) or 0
        local count = tonumber(object.AcquiredCount) or 0
        output:write(string.format("item_state|%s|%d|%d|%d\n",
            state_field(code), active, stage, count))
    end
end

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
