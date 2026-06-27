-- entry point for all lua code of the pack
-- more info on the lua API: https://github.com/black-sliver/PopTracker/blob/master/doc/PACKS.md#lua-interface
ENABLE_DEBUG_LOG = true
-- get current variant
local variant = Tracker.ActiveVariantUID
-- check variant info
IS_ITEMS_ONLY = variant:find("itemsonly")

print("-- SMZ3 Archipelago --")
print("Loaded variant: ", variant)
if ENABLE_DEBUG_LOG then
    print("Debug logging is enabled!")
end

-- Utility Script for helper functions etc.
ScriptHost:LoadScript("scripts/utils.lua")

-- Logic
ScriptHost:LoadScript("scripts/logic/logic.lua")

-- Custom Items
ScriptHost:LoadScript("scripts/custom_items/class.lua")
ScriptHost:LoadScript("scripts/custom_items/progressiveTogglePlus.lua")
ScriptHost:LoadScript("scripts/custom_items/progressiveTogglePlusWrapper.lua")

-- Items
Tracker:AddItems("items/sm/boss_tokens.json")
Tracker:AddItems("items/sm/equipment.json")
Tracker:AddItems("items/sm/keys.json")
Tracker:AddItems("items/z3/dungeon_info.json")
Tracker:AddItems("items/z3/equipment.json")
Tracker:AddItems("items/z3/keys.json")
Tracker:AddItems("items/z3/prizes.json")
Tracker:AddItems("items/settings.json")
Tracker:AddItems("items/labels.json")

-- Locations (load SM cards first for portal logic)
if not IS_ITEMS_ONLY then -- <--- use variant info to optimize loading
    -- Maps
    Tracker:AddMaps("maps/maps.json")
    -- Locations
    Tracker:AddLocations("locations/sm/cards.json")
    Tracker:AddLocations("locations/logic.json")
    Tracker:AddLocations("locations/z3/lightworld.json")
    Tracker:AddLocations("locations/z3/darkworld.json")
    Tracker:AddLocations("locations/z3/bothworlds.json")
    Tracker:AddLocations("locations/z3/hyrulecastle.json")
    Tracker:AddLocations("locations/z3/castletower.json")
    Tracker:AddLocations("locations/z3/easternpalace.json")
    Tracker:AddLocations("locations/z3/desertpalace.json")
    Tracker:AddLocations("locations/z3/towerofhera.json")
    Tracker:AddLocations("locations/z3/palaceofdarkness.json")
    Tracker:AddLocations("locations/z3/swamppalace.json")
    Tracker:AddLocations("locations/z3/skullwoods.json")
    Tracker:AddLocations("locations/z3/thievestown.json")
    Tracker:AddLocations("locations/z3/icepalace.json")
    Tracker:AddLocations("locations/z3/miserymire.json")
    Tracker:AddLocations("locations/z3/turtlerock.json")
    Tracker:AddLocations("locations/z3/ganonstower.json")
    Tracker:AddLocations("locations/z3/itempulls.json")
    Tracker:AddLocations("locations/sm/doors.json")
    Tracker:AddLocations("locations/sm/wreckedship.json")
    Tracker:AddLocations("locations/sm/crateria.json")
    Tracker:AddLocations("locations/sm/brinstar.json")
    Tracker:AddLocations("locations/sm/norfairupper.json")
    Tracker:AddLocations("locations/sm/norfairlower.json")
    Tracker:AddLocations("locations/sm/maridia.json")
    Tracker:AddLocations("locations/sm/tourian.json")
end

-- Layout
Tracker:AddLayouts("layouts/alttp_item_grid.json")
Tracker:AddLayouts("layouts/boss_tokens_grid.json")
Tracker:AddLayouts("layouts/bottom_bar.json")
Tracker:AddLayouts("layouts/broadcast.json")
Tracker:AddLayouts("layouts/maps.json")
Tracker:AddLayouts("layouts/prizepacks.json")
Tracker:AddLayouts("layouts/settings.json")
Tracker:AddLayouts("layouts/sm_item_grid.json")
Tracker:AddLayouts("layouts/tracker.json")

-- AutoTracking for Poptracker
if PopVersion and PopVersion >= "0.18.0" then
    ScriptHost:LoadScript("scripts/autotracking.lua")
end
