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

