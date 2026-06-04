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

