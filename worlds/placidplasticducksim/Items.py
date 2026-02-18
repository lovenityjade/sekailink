import math

from BaseClasses import Item, ItemClassification, Optional
from typing import NamedTuple, Dict

class PPDSItem(Item):
    game = "Placid Plastic Duck Simulator"

class PPDSItemData(NamedTuple):
    type: ItemClassification
    id_offset: Optional[int]

item_table: Dict[str, PPDSItemData] = {
    "Progressive Column Unlock": PPDSItemData(ItemClassification.progression, 1),
    "Progressive Spawn Speed Upgrade": PPDSItemData(ItemClassification.useful, 2),
    "Random Duck": PPDSItemData(ItemClassification.filler, 3),
}

def create_items(world):
    for i in range(9):
        world.multiworld.itempool.append(world.create_item("Progressive Column Unlock"))
        world.multiworld.itempool.append(world.create_item("Progressive Spawn Speed Upgrade"))

    for i in range(world.locations_to_fill - 18):
        world.multiworld.itempool.append(world.create_item("Random Duck"))