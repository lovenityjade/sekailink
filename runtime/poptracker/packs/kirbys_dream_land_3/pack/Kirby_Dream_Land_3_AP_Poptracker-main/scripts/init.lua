-- entry point for all lua code of the pack
-- more info on the lua API: https://github.com/black-sliver/PopTracker/blob/master/doc/PACKS.md#lua-interface
ENABLE_DEBUG_LOG = true
-- get current variant
local variant = Tracker.ActiveVariantUID
-- check variant info
IS_ITEMS_ONLY = variant:find("itemsonly")

print("-- Example Tracker --")
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
Tracker:AddItems("items/items.json")

if not IS_ITEMS_ONLY then -- <--- use variant info to optimize loading
    -- Maps
    Tracker:AddMaps("maps/maps.json")    
    -- Locations
    Tracker:AddLocations("locations/Level1.json")
    Tracker:AddLocations("locations/Level2.json")
    Tracker:AddLocations("locations/Level3.json")
    Tracker:AddLocations("locations/Level4.json")
    Tracker:AddLocations("locations/Level5.json")
    Tracker:AddLocations("locations/1-1.json")
    Tracker:AddLocations("locations/1-2.json")
    Tracker:AddLocations("locations/1-3.json")
    Tracker:AddLocations("locations/1-4.json")
    Tracker:AddLocations("locations/1-5.json")
    Tracker:AddLocations("locations/1-6.json")
    Tracker:AddLocations("locations/2-1.json")
    Tracker:AddLocations("locations/2-2.json")
    Tracker:AddLocations("locations/2-3.json")
    Tracker:AddLocations("locations/2-4.json")
    Tracker:AddLocations("locations/2-5.json")
    Tracker:AddLocations("locations/2-6.json")
    Tracker:AddLocations("locations/3-1.json")
    Tracker:AddLocations("locations/3-2.json")
    Tracker:AddLocations("locations/3-3.json")
    Tracker:AddLocations("locations/3-4.json")
    Tracker:AddLocations("locations/3-5.json")
    Tracker:AddLocations("locations/3-6.json")
    Tracker:AddLocations("locations/4-1.json")
    Tracker:AddLocations("locations/4-2.json")
    Tracker:AddLocations("locations/4-3.json")
    Tracker:AddLocations("locations/4-4.json")
    Tracker:AddLocations("locations/4-5.json")
    Tracker:AddLocations("locations/4-6.json")
    Tracker:AddLocations("locations/5-1.json")
    Tracker:AddLocations("locations/5-2.json")
    Tracker:AddLocations("locations/5-3.json")
    Tracker:AddLocations("locations/5-4.json")
    Tracker:AddLocations("locations/5-5.json")
    Tracker:AddLocations("locations/5-6.json")
end

-- Layout
Tracker:AddLayouts("layouts/items.json")
Tracker:AddLayouts("layouts/worlds.json")
Tracker:AddLayouts("layouts/tabbedlayout.json")
Tracker:AddLayouts("layouts/tracker.json")
Tracker:AddLayouts("layouts/broadcast.json")

-- AutoTracking for Poptracker
if PopVersion and PopVersion >= "0.18.0" then
    ScriptHost:LoadScript("scripts/autotracking.lua")
end