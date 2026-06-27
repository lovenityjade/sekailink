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

local building_accessibility = false
local building_visibility = false

