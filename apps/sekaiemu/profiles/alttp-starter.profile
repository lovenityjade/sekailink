# A Link to the Past starter profile for the Sekaiemu libretro spike.
# This profile intentionally focuses on early underworld checks whose flags
# live in the ALTTP savedata block mirrored in WRAM.
#
# Reference client:
#   worlds/alttp/Client.py
# Relevant constants:
#   SAVEDATA_START = WRAM_START + 0xF000
#   RECV_PROGRESS_ADDR = SAVEDATA_START + 0x4D0
#   RECV_ITEM_ADDR = SAVEDATA_START + 0x4D2
#   RECV_ITEM_PLAYER_ADDR = SAVEDATA_START + 0x4D3
#
# Note:
# The current local bridge can watch these checks immediately.
# ALTTP receive-item behavior is more stateful than EarthBound: the game expects
# progress + item + player bytes together, so these receive slots are defined
# now for future bridge work, but are not yet sufficient on their own to fully
# emulate Archipelago item delivery.

game|A Link to the Past
watch_region|system_ram|0xF000|0x240

# Receive slots mirroring the original AP client behavior.
receive_slot|progress|system_ram|0xF4D0|2
receive_slot|item|system_ram|0xF4D2|1
receive_slot|player|system_ram|0xF4D3|1

# Useful local aliases for starter bridge injection tests.
item_alias|item|0x0C|Blue Boomerang
item_alias|item|0x44|Arrows (10)
item_alias|item|0x31|Bombs (10)
item_alias|item|0x34|Rupee (1)
item_alias|item|0x35|Rupees (5)
item_alias|item|0x36|Rupees (20)

# Early open/standard checks backed by underworld room flags.
# byte_index is relative to watch_region start.
check|0xEA79|0x24|4|Sanctuary
check|0xE971|0xAA|4|Secret Passage
check|0xE9BC|0x208|4|Link's House
check|0xE974|0xE2|4|Hyrule Castle - Boomerang Chest
check|0xEB0C|0xE4|4|Hyrule Castle - Map Chest
check|0xEB5D|0x22|4|Sewers - Secret Room - Left
check|0xEB60|0x22|5|Sewers - Secret Room - Middle
check|0xEB63|0x22|6|Sewers - Secret Room - Right
