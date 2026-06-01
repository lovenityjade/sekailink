# Link's Awakening DX starter profile for the Sekaiemu libretro spike.
# This first pass keeps the check watcher generic and only adds a dedicated
# receive queue through the usual AP multiworld command block.

game|Links Awakening DX Beta
watch_region|system_ram|0xD800|0x300

# Early starter checks from Tracker.py / checkMetadata.py.
# LADX tracker addresses are WRAM mirrors at 0xD800 + room-id unless overridden.
# Tarin/Well/Dream Hut all live in the 0x2xx room range, so their byte indexes
# are the full room ids relative to 0xD800.
check|10000675|0x2A3|5|Tarin's Gift
check|10000676|0x2A4|4|Well Heart Piece
check|10000702|0x2BE|4|Dream Hut West
check|10000703|0x2BF|4|Dream Hut East

# Link's Awakening DX AP receive queue:
# recv index at DDF6/DDF7, command block at DDF8..DDFF
receive_slot|recv_index_hi|system_ram|0xDDF6|1
receive_slot|recv_index_lo|system_ram|0xDDF7|1
receive_slot|command|system_ram|0xDDF8|1
receive_slot|item|system_ram|0xDDF9|1
receive_slot|sender_hi|system_ram|0xDDFA|1
receive_slot|sender_lo|system_ram|0xDDFB|1
receive_slot|mp_c|system_ram|0xDDFC|1
receive_slot|mp_d|system_ram|0xDDFD|1
receive_slot|mp_e|system_ram|0xDDFE|1
receive_slot|mp_f|system_ram|0xDDFF|1

item_alias|item|0x0B|Progressive Sword
item_alias|item|0x01|Progressive Shield
item_alias|item|0x0E|Boomerang
item_alias|item|0x09|Magic Powder
item_alias|item|0x0A|Bomb
item_alias|item|0x1C|20 Rupees
item_alias|item|0x1B|50 Rupees
