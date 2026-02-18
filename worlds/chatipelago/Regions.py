from typing import NamedTuple, Optional

from BaseClasses import Location, Region
from .names import RegionName, Chati

class ChatipelagoLoc(Location):
    game: str = Chati.name

class ChatipelagoLocationData(NamedTuple):
    region: str
    address: Optional[int] = None

class ChatipelagoRegionData(NamedTuple):
    connecting_regions: list[str] = []

region_table: dict[str, list[str]] = {
    "Menu": [],
    "Chatroom": RegionName.chatroom,
    "Prog": RegionName.prog,
}

chati_region_conn: dict[str, dict[str, str]] = {
    "Menu": {"Chatroom": "Start Game"},
    "Chatroom": {"Prog": "Fight"}
}

#Give everything an ID
location_data_table: dict[str, ChatipelagoLocationData] = {}
count = 0
for loc in region_table["Chatroom"]:
    location_data_table[loc] = ChatipelagoLocationData(
        region = "Chatroom",
        address = 100+count
    )
    count+=1
count = 0
for loc in region_table["Prog"]:
    location_data_table[loc] = ChatipelagoLocationData(
        region = "Prog",
        address = 600+count
    )
    count+=1
