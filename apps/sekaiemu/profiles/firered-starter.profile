# Minimal FireRed profile for queue-based AP item injection through the
# game's Archipelago receive buffer.

game|Pokemon FireRed
watch_region|gba_system_bus|0x0203F714|0x10

flag_check|740|740|Player's PC - PC Item
flag_check|560|560|Route 1 - Free Sample

receive_slot|item|gba_system_bus|0x0203F714|2
receive_slot|progress|gba_system_bus|0x0203F716|2
receive_slot|pending|gba_system_bus|0x0203F718|1
receive_slot|display|gba_system_bus|0x0203F719|1

item_alias|item|0x000D|Potion
item_alias|item|0x0004|Poke Ball
item_alias|item|0x000E|Antidote
item_alias|item|0x0001|Master Ball
item_alias|item|0x015F|HM01
item_alias|item|0x015F|Cut
item_alias|item|0x016E|Cascade Badge
