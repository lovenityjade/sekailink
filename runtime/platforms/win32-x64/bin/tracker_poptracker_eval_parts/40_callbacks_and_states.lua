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

local function load_pack_watch_script()
    local script_path = bundle_root .. "/poptracker-adapted/scripts/watches.lua"
    local script_file = io.open(script_path, "rb")
    if not script_file then
        return false
    end
    script_file:close()
    local ok, err = pcall(require, "scripts/watches")
    if not ok then
        error("watch_script_load_failed:" .. tostring(err))
    end
    return true
end

local function wrap_pack_runtime_guards()
    if type(EmptyLocationTargets) == "function" and not __sekailink_wrapped_empty_location_targets then
        local original_empty_location_targets = EmptyLocationTargets
        EmptyLocationTargets = function(...)
            local ok, result = pcall(original_empty_location_targets, ...)
            if ok then
                return result
            end
            return nil
        end
        __sekailink_wrapped_empty_location_targets = true
    end
end

local function watch_object_signature(code)
    if type(code) ~= "string" or code == "" or code == "*" then
        return nil
    end
    local object = objects_by_code[code] or objects_by_name[code]
    if not object then
        return nil
    end
    return table.concat({
        tostring(object.Active == true),
        tostring(object.CurrentStage or 0),
        tostring(object.AcquiredCount or 0),
        tostring(object.BadgeText or ""),
    }, "|")
end

local function capture_watch_object_state()
    local state = {}
    for _, watcher in ipairs(script_host_watchers) do
        if watcher.code ~= "" and watcher.code ~= "*" then
            state[watcher.code] = watch_object_signature(watcher.code)
        end
    end
    return state
end

local function collect_changed_watch_codes(before)
    local changed = {}
    for _, watcher in ipairs(script_host_watchers) do
        if watcher.code ~= "" and watcher.code ~= "*" then
            local after = watch_object_signature(watcher.code)
            if before[watcher.code] ~= after then
                changed[watcher.code] = true
            end
        end
    end
    return changed
end

local function mark_changed_code(changed, code)
    if type(code) == "string" and code ~= "" then
        changed[code] = true
    end
end

local function run_script_host_watchers(changed_codes)
    local has_changed_code = false
    for _, _ in pairs(changed_codes or {}) do
        has_changed_code = true
        break
    end
    if not has_changed_code then
        return
    end
    local watchers = {}
    for _, watcher in ipairs(script_host_watchers) do
        table.insert(watchers, watcher)
    end
    for _, watcher in ipairs(watchers) do
        if watcher.code == "*" or changed_codes[watcher.code] then
            local ok, err = pcall(watcher.handler, watcher.code)
            if not ok then
                error("watch_handler_failed:" .. watcher.name .. ":" .. tostring(err))
            end
        end
    end
end

local function run_script_host_frame_handlers()
    local guard = 0
    while #script_host_frame_handlers > 0 and guard < 16 do
        guard = guard + 1
        local handlers = {}
        for _, handler in ipairs(script_host_frame_handlers) do
            table.insert(handlers, handler)
        end
        for _, handler in ipairs(handlers) do
            local still_registered = false
            for _, current in ipairs(script_host_frame_handlers) do
                if current.name == handler.name and current.handler == handler.handler then
                    still_registered = true
                    break
                end
            end
            if still_registered then
                local ok, err = pcall(handler.handler)
                if not ok then
                    error("frame_handler_failed:" .. handler.name .. ":" .. tostring(err))
                end
            end
        end
        if guard == 1 then
            break
        end
    end
end

local function configure_archipelago_location_lists()
    Archipelago.MissingLocations = {}
    Archipelago.CheckedLocations = {}
    Archipelago.Seed = runtime_seed_id ~= "" and runtime_seed_id or Archipelago.Seed
    local player_number = tonumber(runtime_slot_id)
    if player_number then
        Archipelago.PlayerNumber = player_number
    end
    for location_id, _ in pairs(ap_locations_by_id) do
        local parsed = tonumber(location_id) or location_id
        if checked_location_ids[tostring(location_id)] then
            table.insert(Archipelago.CheckedLocations, parsed)
        else
            table.insert(Archipelago.MissingLocations, parsed)
        end
    end
end

local function fire_archipelago_clear(slot_data)
    if type(slot_data) ~= "table" then
        return
    end
    configure_archipelago_location_lists()
    for name, handler in pairs(archipelago_clear_handlers) do
        local ok, err = pcall(handler, slot_data)
        if not ok then
            error("archipelago_clear_failed:" .. tostring(name) .. ":" .. tostring(err))
        end
    end
    mark_logic_stale()
end

local function fire_archipelago_item_events()
    table.sort(ap_item_events, function(left, right)
        return (tonumber(left.index) or 0) < (tonumber(right.index) or 0)
    end)
    for _, item in ipairs(ap_item_events) do
        local index = tonumber(item.index) or 0
        local item_id = tonumber(item.item_id) or 0
        local player_number = tonumber(item.player_number) or Archipelago.PlayerNumber or 1
        local item_name = item.item_name or tostring(item_id)
        for name, handler in pairs(archipelago_item_handlers) do
            local ok, err = pcall(handler, index, item_id, item_name, player_number)
            if not ok then
                error("archipelago_item_failed:" .. tostring(name) .. ":" .. tostring(err))
            end
        end
    end
    if #ap_item_events > 0 then
        mark_logic_stale()
    end
end

local function fire_archipelago_checked_locations()
    for location_id, _ in pairs(checked_location_ids) do
        local check = ap_locations_by_id[tostring(location_id)]
        local location_name = check and check.name or tostring(location_id)
        for name, handler in pairs(archipelago_location_handlers) do
            local ok, err = pcall(handler, tonumber(location_id) or location_id, location_name)
            if not ok then
                error("archipelago_location_failed:" .. tostring(name) .. ":" .. tostring(err))
            end
        end
    end
    mark_logic_stale()
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
