from BaseClasses import Location
from .options import TotalLocations
from typing import Dict, List, TypeVar, NamedTuple, Optional

class RoR1Location(Location):
    game: str = "Risk of Rain"

class RoR1LocationData(NamedTuple):
    category: str
    code: Optional[int] = None

ror_locations_start_id = 250000

def get_universal_item_pickups(n: int) -> Dict[str, int]:
    n = max(n, 0)
    n = min(n, TotalLocations.range_end)
    return {f"ItemPickup{i + 1}": ror_locations_start_id + i for i in range(n)}

item_pickups = get_universal_item_pickups(TotalLocations.range_end)
location_table = item_pickups

ror_locations_start_ordered_stage = ror_locations_start_id + TotalLocations.range_end

offset_chests = 0


map_orderedstage_1_table: Dict[str, int] = {
    "Desolate Forest":          0,
    "Dried Lake":               1,
}
map_orderedstage_2_table: Dict[str, int] = {
    "Damp Caverns":             2,
    "Sky Meadow":               3,
}
map_orderedstage_3_table: Dict[str, int] = {
    "Ancient Valley":           4,
    "Sunken Tombs":             5,
}
map_orderedstage_4_table: Dict[str, int] = {
    "Magma Barracks":           6,
    "Hive Cluster":             7,
}
map_orderedstage_5_table: Dict[str, int] = {
    "Temple of the Elders":     8,
}
map_special_table: Dict[str, int] = {
    # "Boar Beach":               9,
    "Risk of Rain":            10,
}

X = TypeVar("X")
Y = TypeVar("Y")

#Taken from ror2environments.py
def compress_dict_list_horizontal(list_of_dict: List[Dict[X, Y]]) -> Dict[X, Y]:
    """Combine all dictionaries in a list together into one dictionary."""
    compressed: Dict[X, Y] = {}
    for individual in list_of_dict:
        compressed.update(individual)
    return compressed

map_orderedstages_table = \
    [map_orderedstage_1_table, map_orderedstage_2_table, map_orderedstage_3_table,
     map_orderedstage_4_table, map_orderedstage_5_table]

map_table = \
    {**compress_dict_list_horizontal(map_orderedstages_table),
     **map_special_table}

def shift_by_offset(dictionary: Dict[str, int], offset: int) -> Dict[str, int]:
    """Shift all indexes in a dictionary by an offset"""
    return {name: index+offset for name, index in dictionary.items()}

def get_map_locations(chests: int, map_name: str, map_index: int) -> Dict[str, int]:
    locations = {}

    map_start_id = map_index * TotalLocations.range_end + ror_locations_start_ordered_stage
    for n in range(chests):
        locations.update({f"{map_name}: Item Pickup {n + 1}": n + offset_chests + map_start_id})
    return locations

def get_locations(chests: int) -> Dict[str, int]:
    locations = {}
    orderedstages = compress_dict_list_horizontal(map_orderedstages_table)
    for map_name, map_index in orderedstages.items():
        locations.update(get_map_locations(
            chests = chests,
            map_name = map_name,
            map_index = map_index
        ),)
    return locations

location_table.update(get_locations(
    chests=TotalLocations.range_end
))