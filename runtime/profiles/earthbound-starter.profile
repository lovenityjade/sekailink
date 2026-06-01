# EarthBound starter profile for the Sekaiemu libretro spike.
# This is intentionally small: it proves the profile model without hardcoding
# EarthBound knowledge into the generic host.

game|EarthBound
watch_region|system_ram|0x9C00|0x88

# Receive slots mirroring the original AP client behavior.
receive_slot|item|system_ram|0xB570|1
receive_slot|special|system_ram|0xB572|1
receive_slot|money|system_ram|0xB5F1|2

# A few starter aliases for local bridge injection tests.
item_alias|item|0x58|Cookie
item_alias|item|0x5A|Hamburger
item_alias|item|0x67|Bread Roll
item_alias|item|0x9A|Toothbrush

# Early Onett / Tracy checks
check|0xEB0000|17|3|Onett - Tracy Gift
check|0xEB0001|108|4|Onett - Tracy's Room Present
check|0xEB0003|17|6|Onett - Meteor Item
check|0xEB0008|108|0|Onett - Burger Shop Trashcan
check|0xEB0009|17|4|Onett - Treehouse Guy
check|0xEB000A|108|1|Onett - South Road Present
check|0xEB000B|108|2|Onett - Hotel Trashcan
check|0xEB000C|108|3|Onett - Arcade Trashcan
check|0xEB000D|17|7|Onett - Mayor Pirkle
check|0xEB000E|17|5|Onett - Traveling Entertainer
