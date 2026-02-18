from BaseClasses import MultiWorld, Region
from typing import List, Optional, Dict
from .rules import AccessCondition


area_names: Dict[str, str] = {
    "MountainPath": "Mountain Path",
    "BlueCave": "Blue Caves",
    "KeeperStronghold": "Keeper Stronghold",
    "KeepersTower": "Keepers' Tower",
    "StrongholdDungeon": "Stronghold Dungeon",
    "SnowyPeaks": "Snowy Peaks",
    "SunPalace": "Sun Palace",
    "AncientWoods": "Ancient Woods",
    "HorizonBeach": "Horizon Beach",
    "MagmaChamber": "Magma Chamber",
    "BlobBurg": "Blob Burg",
    "ForgottenWorld": "Forgotten World",
    "MysticalWorkshop": "Mystical Workshop",
    "Underworld": "Underworld",
    "AbandonedTower": "Abandoned Tower",
    "EternitysEnd": "Eternity's End"
}


# Gets a human-readable name from a region's name
def get_area_name(region: str) -> Optional[str]:
    segments = region.split("_")
    return area_names.get(segments[0])


class MonsterSanctuaryConnection:
    region: str
    access_rules: Optional[AccessCondition]

    def __init__(self, region: str, access_rules: Optional[AccessCondition]):
        self.region = region
        self.access_rules = access_rules

    def get_access_func(self, player: int):
        return lambda state: self.access_rules is None or self.access_rules.has_access(state, player)


class RegionData:
    name: str
    connections: List[MonsterSanctuaryConnection]

    def __init__(self, name: str):
        self.name = name
        self.connections: List[MonsterSanctuaryConnection] = []

    def __str__(self):
        return self.name

    def get_connection(self, region: str) -> Optional[MonsterSanctuaryConnection]:
        for connection in self.connections:
            if connection.region == region:
                return connection


class MonsterSanctuaryRegion(Region):
    game: str = "Monster Sanctuary"
    area: str  # Human-readable name for the area that this region is in

    def __init__(self, world: MultiWorld, player: int, region_name: str):
        super().__init__(region_name, player, world)
        self.area = get_area_name(region_name)


# This holds all the region data that is parsed from world.json file
region_data: Dict[str, RegionData] = {}
