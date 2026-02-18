from typing import Optional, Dict

from BaseClasses import ItemClassification
from .rules import AccessCondition


class FlagData:
    location_name: str  # This is the unique flag name, which is marked as "id" in world.json
    item_id: int  # item id that's placed at this location
    item_name: str  # item name that's placed at this location
    item_classification: ItemClassification
    region: str
    access_condition: Optional[AccessCondition]

    def __init__(self,
                 location_name: str,
                 item_name: str,
                 region: str,
                 access_condition: AccessCondition):
        self.location_name = location_name
        self.item_name = item_name
        self.region = region
        self.access_condition = access_condition

    def set_item_id(self, id: int):
        self.item_id = id

    def set_item_classification(self, classification: ItemClassification):
        self.item_classification = classification


flag_data: Dict[str, FlagData] = {}


def get_flag_by_item_name(item_name: str) -> Optional[FlagData]:
    for flag_name, flag in flag_data.items():
        if flag.item_name == item_name:
            return flag

    return None


def add_flag(flag: FlagData):
    if flag_data.get(flag.location_name) is not None:
        raise KeyError(f"{flag.location_name} already exists in flag_data")

    flag_data[flag.location_name] = flag


def set_flag_item_id(item_name: str, item_id: int, item_classification: ItemClassification):
    for k, v in flag_data.items():
        if v.item_name == item_name:
            v.set_item_id(item_id)
            v.set_item_classification(item_classification)
