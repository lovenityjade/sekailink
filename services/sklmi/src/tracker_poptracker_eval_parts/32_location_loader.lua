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
    if not pack_relative_file_exists(path) then
        return
    end
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

