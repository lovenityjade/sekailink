-- Items
Tracker:AddItems("items/items.json")
Tracker:AddItems("items/settings.json")

-- Logic
ScriptHost:LoadScript("scripts/logic/logic.lua")
ScriptHost:LoadScript("scripts/logic/logic_helper.lua")
ScriptHost:LoadScript("scripts/logic/logic_main.lua")

-- Maps
Tracker:AddMaps("maps/maps.json")

-- Layout
Tracker:AddLayouts("layouts/settings_popup.json")
Tracker:AddLayouts("layouts/items.json")
Tracker:AddLayouts("layouts/tabs.json")
Tracker:AddLayouts("layouts/tracker.json")
Tracker:AddLayouts("layouts/broadcast.json")

-- Locations
Tracker:AddLocations("locations/Death Mountain.json")
Tracker:AddLocations("locations/Eastern Hyrule.json")
Tracker:AddLocations("locations/Great Palace.json")
Tracker:AddLocations("locations/Island Palace.json")
Tracker:AddLocations("locations/Maze Palace.json")
Tracker:AddLocations("locations/Midoro Palace.json")
Tracker:AddLocations("locations/Northeastern Hyrule.json")
Tracker:AddLocations("locations/Northwestern Hyrule.json")
Tracker:AddLocations("locations/Palace on the Sea.json")
Tracker:AddLocations("locations/Parapa Palace.json")
Tracker:AddLocations("locations/Southeastern Hyrule.json")
Tracker:AddLocations("locations/Three-Eye Rock Palace.json")
Tracker:AddLocations("locations/Western Coast.json")
Tracker:AddLocations("locations/Western Hyrule.json")
Tracker:AddLocations("locations/Overworld.json")

-- AutoTracking for Poptracker
ScriptHost:LoadScript("scripts/autotracking.lua")