from BaseClasses import Item, ItemClassification
from typing import NamedTuple, Optional, Dict
from .locations import map_table

class RoR1Item(Item):
    game: str = "Risk of Rain"

class RoR1ItemData(NamedTuple):
    category: str
    code: Optional[int] = None
    item_type: ItemClassification = ItemClassification.filler
    weight: Optional[int] = None

offset: int = 250000
filler_offset: int = offset + 100
trap_offset: int = offset + 200
stage_offset: int = offset + 300
map_offset: int = offset + 400

# Upgrade item ids 250001 - 250005
upgrade_table: Dict[str, RoR1ItemData] = {
    "Common Item":          RoR1ItemData("Upgrade", 1 + offset, ItemClassification.filler, 64),
    "Uncommon Item":        RoR1ItemData("Upgrade", 2 + offset, ItemClassification.filler, 64),
    "Legendary Item":       RoR1ItemData("Upgrade", 3 + offset, ItemClassification.filler, 64),
    "Boss Item":            RoR1ItemData("Upgrade", 4 + offset, ItemClassification.filler, 64),
    "Equipment":            RoR1ItemData("Upgrade", 5 + offset, ItemClassification.filler, 64),
    "Teleporter Fragment":  RoR1ItemData("Upgrade", 6 + offset, ItemClassification.progression, 64),
}
# Filler item ids  250101 - 250102
filler_table: Dict[str, RoR1ItemData] = {
    "Money":                RoR1ItemData("Filler", 1 + filler_offset, ItemClassification.filler, 64),
    "1000 Exp":             RoR1ItemData("Filler", 2 + filler_offset, ItemClassification.filler, 40),
}
# Trap item ids 250201 - 250203
trap_table: Dict[str, RoR1ItemData] = {
    "Time Warp Trap":       RoR1ItemData("Trap", 1 + trap_offset, ItemClassification.trap, 20),
    "Combat Trap":          RoR1ItemData("Trap", 2 + trap_offset, ItemClassification.trap, 20),
    "Meteor Trap":          RoR1ItemData("Trap", 3 + trap_offset, ItemClassification.trap, 10),
}
# Stage item ids 250301 - 250304
stage_table: Dict[str, RoR1ItemData] = {
    "Stage 1":              RoR1ItemData("Stage", 1 + stage_offset, ItemClassification.progression),
    "Stage 2":              RoR1ItemData("Stage", 2 + stage_offset, ItemClassification.progression),
    "Stage 3":              RoR1ItemData("Stage", 3 + stage_offset, ItemClassification.progression),
    "Stage 4":              RoR1ItemData("Stage", 4 + stage_offset, ItemClassification.progression),
    "Stage 5":              RoR1ItemData("Stage", 5 + stage_offset, ItemClassification.progression),
    "Progressive Stage":    RoR1ItemData("Stage", 6 + stage_offset, ItemClassification.progression),
}

item_table = {**upgrade_table, **filler_table, **trap_table, **stage_table}

def create_map_table(name: str, map_id: int, map_classification: ItemClassification) -> Dict[str, RoR1ItemData]:
    return {name: RoR1ItemData("Map", map_offset + map_id, map_classification)}

map_pool: Dict[str, RoR1ItemData] = {}
for data, key in map_table.items():
    classification = ItemClassification.progression
    map_pool.update(create_map_table(data, key, classification))

item_table.update(map_pool)

default_weights: Dict[str, int] = {
    "Common Item":          64,
    "Uncommon Item":        32,
    "Legendary Item":       8,
    "Boss Item":            4,
    "Equipment":            32,
}