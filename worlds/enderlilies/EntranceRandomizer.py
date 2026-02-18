from typing import Callable, List, Optional, Dict, Set, Tuple

from .Locations import locations
from .Names import names as el
from .Rules import get_rules
import time


def timing(f, *args, **kwargs):
    def decorator(*args, **kwargs):
        start = time.time()
        r = f(*args, **kwargs)
        end = time.time()
        print("Time:", f, end - start)
        return r

    return decorator


class Portal:
    src: str
    dst: str

    def __init__(self, src: Optional[str] = None, dst: Optional[str] = None):
        self.src = src
        self.dst = dst

    def copy(self):
        return Portal(self.src, self.dst)

    def __repr__(self):
        return f"{self.src} -> {self.dst}"


portals_locations = {
    "The Abyss 01 To Verboten Domain 18": Portal(
        src="Map.map_abyss_01.S18", dst="Map.map_swamp_18.A1"
    ),
    "The Abyss 01 To The Abyss 02": Portal(
        src="Map.map_abyss_01.2", dst="Map.map_abyss_02.1"
    ),
    "The Abyss 02 To The Abyss 03": Portal(
        src="Map.map_abyss_02.3", dst="Map.map_abyss_03.2"
    ),
    "The Abyss 02 To The Abyss 01": Portal(
        src="Map.map_abyss_02.1", dst="Map.map_abyss_01.2"
    ),
    "The Abyss 03 To The Abyss 02": Portal(
        src="Map.map_abyss_03.2", dst="Map.map_abyss_02.3"
    ),
    "The Abyss 04 To Verboten Domain 12": Portal(
        src="Map.map_abyss_04.2", dst="Map.map_swamp_12.4"
    ),
    "The Abyss 04 To The Abyss 05": Portal(
        src="Map.map_abyss_04.5", dst="Map.map_abyss_05.4"
    ),
    "The Abyss 05 To Verboten Domain 12": Portal(src=None, dst="Map.map_swamp_12.A5"),
    "The Abyss 05 To The Abyss 04": Portal(
        src="Map.map_abyss_05.4", dst="Map.map_abyss_04.5"
    ),
    "Ruined Castle 01 To Cliffside Hamlet 11": Portal(
        src="Map.map_castle_01.0", dst="Map.map_village_11.1"
    ),
    "Ruined Castle 01 To Ruined Castle 02 Lower": Portal(
        src="Map.map_castle_01.2", dst="Map.map_castle_02.1"
    ),
    "Ruined Castle 01 To Ruined Castle 02 Upper": Portal(
        src="Map.map_castle_01.2B", dst="Map.map_castle_02.1B"
    ),
    "Ruined Castle 01 To Ruined Castle 10": Portal(
        src="Map.map_castle_01.10", dst="Map.map_castle_10.1"
    ),
    "Ruined Castle 02 To Ruined Castle 05": Portal(
        src="Map.map_castle_02.5", dst="Map.map_castle_05.2"
    ),
    "Ruined Castle 02 To Ruined Castle 01 Lower": Portal(
        src="Map.map_castle_02.1", dst="Map.map_castle_01.2"
    ),
    "Ruined Castle 02 To Ruined Castle 04": Portal(
        src="Map.map_castle_02.4", dst="Map.map_castle_04.2"
    ),
    "Ruined Castle 02 To Ruined Castle 01 Upper": Portal(
        src="Map.map_castle_02.1B", dst="Map.map_castle_01.2B"
    ),
    "Ruined Castle 03 To Ruined Castle 05": Portal(
        src="Map.map_castle_03.5", dst="Map.map_castle_05.3"
    ),
    "Ruined Castle 03 To Ruined Castle 11 Left": Portal(
        src="Map.map_castle_03.11A", dst="Map.map_castle_11.3A"
    ),
    "Ruined Castle 03 To Ruined Castle 11 Right": Portal(
        src="Map.map_castle_03.11B", dst="Map.map_castle_11.3B"
    ),
    "Ruined Castle 04 To Ruined Castle 02": Portal(
        src="Map.map_castle_04.2", dst="Map.map_castle_02.4"
    ),
    "Ruined Castle 05 To Ruined Castle 02": Portal(
        src="Map.map_castle_05.2", dst="Map.map_castle_02.5"
    ),
    "Ruined Castle 05 To Ruined Castle 06": Portal(
        src="Map.map_castle_05.6", dst="Map.map_castle_06.5"
    ),
    "Ruined Castle 05 To Ruined Castle 08": Portal(
        src="Map.map_castle_05.8", dst="Map.map_castle_08.5"
    ),
    "Ruined Castle 05 To Ruined Castle 03": Portal(
        src="Map.map_castle_05.3", dst="Map.map_castle_03.5"
    ),
    "Ruined Castle 06 To Ruined Castle 12": Portal(
        src="Map.map_castle_06.12", dst="Map.map_castle_12.6"
    ),
    "Ruined Castle 06 To Ruined Castle 07": Portal(
        src="Map.map_castle_06.7", dst="Map.map_castle_07.6"
    ),
    "Ruined Castle 06 To Ruined Castle 05": Portal(
        src="Map.map_castle_06.5", dst="Map.map_castle_05.6"
    ),
    "Ruined Castle 07 To Twin Spires 01": Portal(
        src="Map.map_castle_07.F1", dst="Map.map_fort_01.C7"
    ),
    "Ruined Castle 07 To Ruined Castle 06": Portal(
        src="Map.map_castle_07.6", dst="Map.map_castle_06.7"
    ),
    "Ruined Castle 08 To Ruined Castle 05": Portal(
        src="Map.map_castle_08.5", dst="Map.map_castle_05.8"
    ),
    "Ruined Castle 08 To Ruined Castle 09": Portal(
        src="Map.map_castle_08.9", dst="Map.map_castle_09.8"
    ),
    "Ruined Castle 09 To Ruined Castle 11": Portal(
        src="Map.map_castle_09.11", dst="Map.map_castle_11.9"
    ),
    "Ruined Castle 09 To Ruined Castle 08": Portal(
        src="Map.map_castle_09.8", dst="Map.map_castle_08.9"
    ),
    "Ruined Castle 09 To Ruined Castle 10": Portal(
        src="Map.map_castle_09.10", dst="Map.map_castle_10.9"
    ),
    "Ruined Castle 10 To Ruined Castle 09": Portal(
        src="Map.map_castle_10.9", dst="Map.map_castle_09.10"
    ),
    "Ruined Castle 10 To Ruined Castle 01": Portal(
        src="Map.map_castle_10.1", dst="Map.map_castle_01.10"
    ),
    "Ruined Castle 11 To Ruined Castle 12": Portal(
        src="Map.map_castle_11.12", dst="Map.map_castle_12.11"
    ),
    "Ruined Castle 11 To Ruined Castle 09": Portal(
        src="Map.map_castle_11.9", dst="Map.map_castle_09.11"
    ),
    "Ruined Castle 11 To Ruined Castle 13": Portal(
        src="Map.map_castle_11.13", dst="Map.map_castle_13.11"
    ),
    "Ruined Castle 11 To Ruined Castle 03 Left": Portal(
        src="Map.map_castle_11.3A", dst="Map.map_castle_03.11A"
    ),
    "Ruined Castle 11 To Ruined Castle 03 Right": Portal(
        src="Map.map_castle_11.3B", dst="Map.map_castle_03.11B"
    ),
    "Ruined Castle 12 To Ruined Castle 06": Portal(
        src="Map.map_castle_12.6", dst="Map.map_castle_06.12"
    ),
    "Ruined Castle 12 To Ruined Castle 11": Portal(
        src="Map.map_castle_12.11", dst="Map.map_castle_11.12"
    ),
    "Ruined Castle 12 To Ruined Castle 21": Portal(
        src="Map.map_castle_12.21", dst="Map.map_castle_21.12"
    ),
    "Ruined Castle 13 To Ruined Castle 17": Portal(
        src="Map.map_castle_13.17", dst="Map.map_castle_17.13"
    ),
    "Ruined Castle 13 To Ruined Castle 11": Portal(
        src="Map.map_castle_13.11", dst="Map.map_castle_11.13"
    ),
    "Ruined Castle 13 To Ruined Castle 14": Portal(
        src="Map.map_castle_13.14", dst="Map.map_castle_14.13"
    ),
    "Ruined Castle 14 To Ruined Castle 13": Portal(
        src="Map.map_castle_14.13", dst="Map.map_castle_13.14"
    ),
    "Ruined Castle 14 To Ruined Castle 15": Portal(
        src="Map.map_castle_14.15", dst="Map.map_castle_15.14"
    ),
    "Ruined Castle 15 To Ruined Castle 16": Portal(
        src="Map.map_castle_15.16", dst="Map.map_castle_16.15"
    ),
    "Ruined Castle 15 To Ruined Castle 14": Portal(
        src="Map.map_castle_15.14", dst="Map.map_castle_14.15"
    ),
    "Ruined Castle 16 To Ruined Castle 18": Portal(
        src=None, dst="Map.map_castle_18.16"
    ),
    "Ruined Castle 16 To Ruined Castle 15": Portal(
        src="Map.map_castle_16.15", dst="Map.map_castle_15.16"
    ),
    "Ruined Castle 17 To Ruined Castle 13": Portal(
        src="Map.map_castle_17.13", dst="Map.map_castle_13.17"
    ),
    "Ruined Castle 17 To Ruined Castle 18": Portal(
        src="Map.map_castle_17.18", dst="Map.map_castle_18.17"
    ),
    "Ruined Castle 18 To Ruined Castle 19": Portal(
        src="Map.map_castle_18.19", dst="Map.map_castle_19.18"
    ),
    "Ruined Castle 18 To Ruined Castle 17": Portal(
        src="Map.map_castle_18.17", dst="Map.map_castle_17.18"
    ),
    "Ruined Castle 19 To Ruined Castle 20": Portal(
        src="Map.map_castle_19.20", dst="Map.map_castle_20.19"
    ),
    "Ruined Castle 19 To Ruined Castle 18": Portal(
        src="Map.map_castle_19.18", dst="Map.map_castle_18.19"
    ),
    "Ruined Castle 20 To Ruined Castle 19": Portal(
        src="Map.map_castle_20.19", dst="Map.map_castle_19.20"
    ),
    "Ruined Castle 21 To Ruined Castle 12": Portal(
        src="Map.map_castle_21.12", dst="Map.map_castle_12.21"
    ),
    "Catacombs 01 To Catacombs 02": Portal(
        src="Map.map_cave_01.2", dst="Map.map_cave_02.1"
    ),
    "Catacombs 01 To Cliffside Hamlet 12": Portal(
        src="Map.map_cave_01.V12", dst="Map.map_village_12.C1"
    ),
    "Catacombs 02 To Catacombs 05": Portal(
        src="Map.map_cave_02.5", dst="Map.map_cave_05.2"
    ),
    "Catacombs 02 To Catacombs 07": Portal(
        src="Map.map_cave_02.7", dst="Map.map_cave_07.2"
    ),
    "Catacombs 02 To Catacombs 01": Portal(
        src="Map.map_cave_02.1", dst="Map.map_cave_01.2"
    ),
    "Catacombs 03 To Catacombs 07": Portal(
        src="Map.map_cave_03.7", dst="Map.map_cave_07.3"
    ),
    "Catacombs 03 To Catacombs 08": Portal(
        src="Map.map_cave_03.8", dst="Map.map_cave_08.3"
    ),
    "Catacombs 03 To Catacombs 06": Portal(
        src="Map.map_cave_03.6", dst="Map.map_cave_06.3"
    ),
    "Catacombs 04 To Catacombs 16": Portal(
        src="Map.map_cave_04.16", dst="Map.map_cave_16.4"
    ),
    "Catacombs 04 To Catacombs 05": Portal(
        src="Map.map_cave_04.5", dst="Map.map_cave_05.4"
    ),
    "Catacombs 04 To Catacombs 12": Portal(
        src="Map.map_cave_04.12", dst="Map.map_cave_12.4"
    ),
    "Catacombs 05 To Catacombs 02": Portal(
        src="Map.map_cave_05.2", dst="Map.map_cave_02.5"
    ),
    "Catacombs 05 To Catacombs 04": Portal(
        src="Map.map_cave_05.4", dst="Map.map_cave_04.5"
    ),
    "Catacombs 05 To Catacombs 06": Portal(
        src="Map.map_cave_05.6", dst="Map.map_cave_06.5"
    ),
    "Catacombs 05 To Catacombs 10": Portal(
        src="Map.map_cave_05.10", dst="Map.map_cave_10.5"
    ),
    "Catacombs 06 To Catacombs 05": Portal(
        src="Map.map_cave_06.5", dst="Map.map_cave_05.6"
    ),
    "Catacombs 06 To Catacombs 03": Portal(
        src="Map.map_cave_06.3", dst="Map.map_cave_03.6"
    ),
    "Catacombs 07 To Catacombs 02": Portal(
        src="Map.map_cave_07.2", dst="Map.map_cave_02.7"
    ),
    "Catacombs 07 To Catacombs 03": Portal(
        src="Map.map_cave_07.3", dst="Map.map_cave_03.7"
    ),
    "Catacombs 08 To Catacombs 09": Portal(
        src="Map.map_cave_08.9", dst="Map.map_cave_09.8"
    ),
    "Catacombs 08 To Catacombs 03": Portal(
        src="Map.map_cave_08.3", dst="Map.map_cave_03.8"
    ),
    "Catacombs 08 To Catacombs 17": Portal(
        src="Map.map_cave_08.17", dst="Map.map_cave_17.8"
    ),
    "Catacombs 08 To Catacombs 11": Portal(
        src="Map.map_cave_08.11", dst="Map.map_cave_11.8"
    ),
    "Catacombs 09 To Catacombs 10": Portal(
        src="Map.map_cave_09.10", dst="Map.map_cave_10.9"
    ),
    "Catacombs 09 To Catacombs 08": Portal(
        src="Map.map_cave_09.8", dst="Map.map_cave_08.9"
    ),
    "Catacombs 09 To Catacombs 21": Portal(
        src="Map.map_cave_09.21", dst="Map.map_cave_21.9"
    ),
    "Catacombs 10 To Catacombs 23": Portal(
        src="Map.map_cave_10.23", dst="Map.map_cave_23.10"
    ),
    "Catacombs 10 To Catacombs 05": Portal(
        src="Map.map_cave_10.5", dst="Map.map_cave_05.10"
    ),
    "Catacombs 10 To Catacombs 09": Portal(
        src="Map.map_cave_10.9", dst="Map.map_cave_09.10"
    ),
    "Catacombs 10 To Catacombs 16": Portal(
        src="Map.map_cave_10.16", dst="Map.map_cave_16.10"
    ),
    "Catacombs 11 To Catacombs 08": Portal(
        src="Map.map_cave_11.8", dst="Map.map_cave_08.11"
    ),
    "Catacombs 11 To Catacombs 13": Portal(
        src="Map.map_cave_11.13", dst="Map.map_cave_13.11"
    ),
    "Catacombs 11 To Catacombs 18": Portal(
        src="Map.map_cave_11.18", dst="Map.map_cave_18.11"
    ),
    "Catacombs 12 To Catacombs 04": Portal(
        src="Map.map_cave_12.4", dst="Map.map_cave_04.12"
    ),
    "Catacombs 13 To Catacombs 11": Portal(
        src="Map.map_cave_13.11", dst="Map.map_cave_11.13"
    ),
    "Catacombs 13 To Catacombs 14": Portal(
        src="Map.map_cave_13.14", dst="Map.map_cave_14.13"
    ),
    "Catacombs 13 To Catacombs 23": Portal(
        src="Map.map_cave_13.23", dst="Map.map_cave_23.13"
    ),
    "Catacombs 13 To Catacombs 20": Portal(
        src="Map.map_cave_13.20", dst="Map.map_cave_20.13"
    ),
    "Catacombs 14 To Catacombs 13": Portal(
        src="Map.map_cave_14.13", dst="Map.map_cave_13.14"
    ),
    "Catacombs 14 To Catacombs 15": Portal(
        src="Map.map_cave_14.15", dst="Map.map_cave_15.14"
    ),
    "Catacombs 14 To Catacombs 22": Portal(
        src="Map.map_cave_14.22", dst="Map.map_cave_22.14"
    ),
    "Catacombs 15 To Catacombs 14": Portal(
        src="Map.map_cave_15.14", dst="Map.map_cave_14.15"
    ),
    "Catacombs 15 To Catacombs 16": Portal(
        src="Map.map_cave_15.16", dst="Map.map_cave_16.15"
    ),
    "Catacombs 16 To Catacombs 15": Portal(
        src="Map.map_cave_16.15", dst="Map.map_cave_15.16"
    ),
    "Catacombs 16 To Catacombs 10": Portal(
        src="Map.map_cave_16.10", dst="Map.map_cave_10.16"
    ),
    "Catacombs 16 To Catacombs 04": Portal(
        src="Map.map_cave_16.4", dst="Map.map_cave_04.16"
    ),
    "Catacombs 17 To Catacombs 08": Portal(
        src="Map.map_cave_17.8", dst="Map.map_cave_08.17"
    ),
    "Catacombs 18 To Catacombs 11 Upper": Portal(
        src="Map.map_cave_18.11", dst="Map.map_cave_11.18"
    ),
    "Catacombs 18 To Catacombs 11 Lower": Portal(src=None, dst="Map.map_cave_11.18B"),
    "Catacombs 19 To Catacombs 20": Portal(
        src="Map.map_cave_19.20", dst="Map.map_cave_20.19"
    ),
    "Catacombs 19 To Catacombs 21": Portal(
        src="Map.map_cave_19.21", dst="Map.map_cave_21.19"
    ),
    "Catacombs 20 To Catacombs 22": Portal(
        src="Map.map_cave_20.22", dst="Map.map_cave_22.20"
    ),
    "Catacombs 20 To Catacombs 19": Portal(
        src="Map.map_cave_20.19", dst="Map.map_cave_19.20"
    ),
    "Catacombs 20 To Catacombs 13": Portal(
        src="Map.map_cave_20.13", dst="Map.map_cave_13.20"
    ),
    "Catacombs 21 To Catacombs 09": Portal(
        src="Map.map_cave_21.9", dst="Map.map_cave_09.21"
    ),
    "Catacombs 21 To Catacombs 19": Portal(
        src="Map.map_cave_21.19", dst="Map.map_cave_19.21"
    ),
    "Catacombs 22 To Twin Spires 02": Portal(
        src="Map.map_cave_22.F2", dst="Map.map_fort_02.C22"
    ),
    "Catacombs 22 To Catacombs 14": Portal(
        src="Map.map_cave_22.14", dst="Map.map_cave_14.22"
    ),
    "Catacombs 22 To Catacombs 20": Portal(
        src="Map.map_cave_22.20", dst="Map.map_cave_20.22"
    ),
    "Catacombs 23 To Catacombs 13": Portal(
        src="Map.map_cave_23.13", dst="Map.map_cave_13.23"
    ),
    "Catacombs 23 To Catacombs 10": Portal(
        src="Map.map_cave_23.10", dst="Map.map_cave_10.23"
    ),
    "White Parish 01 To White Parish 02": Portal(
        src="Map.map_church_01.1", dst="Map.map_church_02.0"
    ),
    "White Parish 01 To White Parish 12": Portal(
        src="Map.map_church_01.12", dst="Map.map_church_12.1"
    ),
    "White Parish 02 To White Parish 01": Portal(
        src="Map.map_church_02.0", dst="Map.map_church_01.1"
    ),
    "White Parish 02 To White Parish 10": Portal(
        src="Map.map_church_02.10", dst="Map.map_church_10.2"
    ),
    "White Parish 03 To White Parish 05": Portal(
        src="Map.map_church_03.0", dst="Map.map_church_05.1"
    ),
    "White Parish 03 To White Parish 04": Portal(
        src="Map.map_church_03.1", dst="Map.map_church_04.0"
    ),
    "White Parish 04 To White Parish 03": Portal(
        src="Map.map_church_04.0", dst="Map.map_church_03.1"
    ),
    "White Parish 04 To White Parish 06": Portal(
        src="Map.map_church_04.1", dst="Map.map_church_06.0"
    ),
    "White Parish 05 To White Parish 11": Portal(
        src="Map.map_church_05.11", dst="Map.map_church_11.5"
    ),
    "White Parish 05 To White Parish 03": Portal(
        src="Map.map_church_05.1", dst="Map.map_church_03.0"
    ),
    "White Parish 05 To White Parish 09": Portal(
        src="Map.map_church_05.9", dst="Map.map_church_09.5"
    ),
    "White Parish 06 To White Parish 04": Portal(
        src="Map.map_church_06.0", dst="Map.map_church_04.1"
    ),
    "White Parish 06 To White Parish 07": Portal(
        src="Map.map_church_06.1", dst="Map.map_church_07.0"
    ),
    "White Parish 07 To White Parish 06": Portal(
        src="Map.map_church_07.0", dst="Map.map_church_06.1"
    ),
    "White Parish 07 To White Parish 08": Portal(
        src="Map.map_church_07.1", dst="Map.map_church_08.0"
    ),
    "White Parish 08 To Cliffside Hamlet 01": Portal(
        src="Map.map_church_08.1", dst="Map.map_village_01.0"
    ),
    "White Parish 08 To Witch's Thicket 01": Portal(
        src="Map.map_church_08.2", dst="Map.map_forest_01.C8"
    ),
    "White Parish 08 To White Parish 07": Portal(
        src="Map.map_church_08.0", dst="Map.map_church_07.1"
    ),
    "White Parish 09 To White Parish 05": Portal(
        src="Map.map_church_09.5", dst="Map.map_church_05.9"
    ),
    "White Parish 09 To White Parish 01": Portal(src=None, dst="Map.map_church_01.2"),
    "White Parish 09 To White Parish 14": Portal(
        src="Map.map_church_09.14", dst="Map.map_church_14.9"
    ),
    "White Parish 10 To White Parish 02": Portal(
        src="Map.map_church_10.2", dst="Map.map_church_02.10"
    ),
    "White Parish 10 To White Parish 11": Portal(
        src="Map.map_church_10.11", dst="Map.map_church_11.10"
    ),
    "White Parish 11 To White Parish 05": Portal(
        src="Map.map_church_11.5", dst="Map.map_church_05.11"
    ),
    "White Parish 11 To White Parish 10": Portal(
        src="Map.map_church_11.10", dst="Map.map_church_10.11"
    ),
    "White Parish 12 To White Parish 01": Portal(
        src="Map.map_church_12.1", dst="Map.map_church_01.12"
    ),
    "White Parish 12 To White Parish 13": Portal(
        src="Map.map_church_12.3", dst="Map.map_church_13.6"
    ),
    "White Parish 13 To White Parish 12": Portal(
        src="Map.map_church_13.6", dst="Map.map_church_12.3"
    ),
    "White Parish 14 To White Parish 09": Portal(
        src="Map.map_church_14.9", dst="Map.map_church_09.14"
    ),
    "Witch's Thicket 01 To White Parish 08": Portal(
        src="Map.map_forest_01.C8", dst="Map.map_church_08.2"
    ),
    "Witch's Thicket 01 To Witch's Thicket 02": Portal(
        src="Map.map_forest_01.2", dst="Map.map_forest_02.1"
    ),
    "Witch's Thicket 02 To Witch's Thicket 01": Portal(
        src="Map.map_forest_02.1", dst="Map.map_forest_01.2"
    ),
    "Witch's Thicket 02 To Witch's Thicket 04": Portal(
        src="Map.map_forest_02.4", dst="Map.map_forest_04.2"
    ),
    "Witch's Thicket 02 To Witch's Thicket 03": Portal(
        src="Map.map_forest_02.3", dst="Map.map_forest_03.2"
    ),
    "Witch's Thicket 03 To Witch's Thicket 05": Portal(
        src="Map.map_forest_03.5", dst="Map.map_forest_05.3"
    ),
    "Witch's Thicket 03 To Witch's Thicket 02": Portal(
        src="Map.map_forest_03.2", dst="Map.map_forest_02.3"
    ),
    "Witch's Thicket 04 To Witch's Thicket 05": Portal(
        src="Map.map_forest_04.6", dst="Map.map_forest_05.4"
    ),
    "Witch's Thicket 04 To Witch's Thicket 02": Portal(
        src="Map.map_forest_04.2", dst="Map.map_forest_02.4"
    ),
    "Witch's Thicket 05 To Witch's Thicket 07": Portal(
        src="Map.map_forest_05.7", dst="Map.map_forest_07.5"
    ),
    "Witch's Thicket 05 To Witch's Thicket 03": Portal(
        src="Map.map_forest_05.3", dst="Map.map_forest_03.5"
    ),
    "Witch's Thicket 05 To Witch's Thicket 04": Portal(
        src="Map.map_forest_05.4", dst="Map.map_forest_04.6"
    ),
    "Witch's Thicket 06 To Witch's Thicket 07": Portal(
        src="Map.map_forest_06.7", dst="Map.map_forest_07.6"
    ),
    "Witch's Thicket 07 To Stockade 01": Portal(
        src="Map.map_forest_07.O1", dst="Map.map_oubliette_01.F7"
    ),
    "Witch's Thicket 07 To Witch's Thicket 08": Portal(
        src="Map.map_forest_07.8", dst="Map.map_forest_08.7"
    ),
    "Witch's Thicket 07 To Witch's Thicket 05": Portal(
        src="Map.map_forest_07.5", dst="Map.map_forest_05.7"
    ),
    "Witch's Thicket 07 To Witch's Thicket 06": Portal(
        src="Map.map_forest_07.6", dst="Map.map_forest_06.7"
    ),
    "Witch's Thicket 08 To Witch's Thicket 07": Portal(
        src="Map.map_forest_08.7", dst="Map.map_forest_07.8"
    ),
    "Witch's Thicket 08 To Witch's Thicket 10": Portal(
        src="Map.map_forest_08.10", dst="Map.map_forest_10.8"
    ),
    "Witch's Thicket 09 To Witch's Thicket 10": Portal(
        src="Map.map_forest_09.10", dst="Map.map_forest_10.9"
    ),
    "Witch's Thicket 09 To Verboten Domain 02": Portal(
        src="Map.map_forest_09.S2", dst="Map.map_swamp_02.F9"
    ),
    "Witch's Thicket 10 To Witch's Thicket 11": Portal(
        src="Map.map_forest_10.11", dst="Map.map_forest_11.10"
    ),
    "Witch's Thicket 10 To Witch's Thicket 09": Portal(
        src="Map.map_forest_10.9", dst="Map.map_forest_09.10"
    ),
    "Witch's Thicket 10 To Witch's Thicket 08": Portal(
        src="Map.map_forest_10.8", dst="Map.map_forest_08.10"
    ),
    "Witch's Thicket 10 To Witch's Thicket 12": Portal(
        src="Map.map_forest_10.12", dst="Map.map_forest_12.10"
    ),
    "Witch's Thicket 11 To Witch's Thicket 10": Portal(
        src="Map.map_forest_11.10", dst="Map.map_forest_10.11"
    ),
    "Witch's Thicket 11 To Witch's Thicket 14": Portal(
        src="Map.map_forest_11.14", dst="Map.map_forest_14.11"
    ),
    "Witch's Thicket 12 To Witch's Thicket 10": Portal(
        src="Map.map_forest_12.10", dst="Map.map_forest_10.12"
    ),
    "Witch's Thicket 12 To Witch's Thicket 13": Portal(
        src="Map.map_forest_12.13", dst="Map.map_forest_13.12"
    ),
    "Witch's Thicket 12 To Witch's Thicket 17": Portal(
        src="Map.map_forest_12.17", dst="Map.map_forest_17.12"
    ),
    "Witch's Thicket 13 To Witch's Thicket 12": Portal(
        src="Map.map_forest_13.12", dst="Map.map_forest_12.13"
    ),
    "Witch's Thicket 13 To Witch's Thicket 14": Portal(
        src="Map.map_forest_13.14", dst="Map.map_forest_14.13"
    ),
    "Witch's Thicket 13 To Witch's Thicket 16": Portal(
        src="Map.map_forest_13.16", dst="Map.map_forest_16.13"
    ),
    "Witch's Thicket 14 To Witch's Thicket 15": Portal(
        src="Map.map_forest_14.15", dst="Map.map_forest_15.14"
    ),
    "Witch's Thicket 14 To Witch's Thicket 13": Portal(
        src="Map.map_forest_14.13", dst="Map.map_forest_13.14"
    ),
    "Witch's Thicket 14 To Witch's Thicket 11": Portal(
        src="Map.map_forest_14.11", dst="Map.map_forest_11.14"
    ),
    "Witch's Thicket 15 To Witch's Thicket 14": Portal(
        src="Map.map_forest_15.14", dst="Map.map_forest_14.15"
    ),
    "Witch's Thicket 16 To Witch's Thicket 13": Portal(
        src="Map.map_forest_16.13", dst="Map.map_forest_13.16"
    ),
    "Witch's Thicket 17 To Witch's Thicket 12": Portal(
        src="Map.map_forest_17.12", dst="Map.map_forest_12.17"
    ),
    "Twin Spires 01 To Twin Spires 03": Portal(
        src="Map.map_fort_01.3", dst="Map.map_fort_03.1"
    ),
    "Twin Spires 01 To Cliffside Hamlet 15": Portal(
        src="Map.map_fort_01.V15", dst="Map.map_village_15.F1"
    ),
    "Twin Spires 01 To Ruined Castle 07": Portal(
        src="Map.map_fort_01.C7", dst="Map.map_castle_07.F1"
    ),
    "Twin Spires 02 To Twin Spires 03": Portal(
        src="Map.map_fort_02.3", dst="Map.map_fort_03.2"
    ),
    "Twin Spires 02 To Catacombs 22": Portal(
        src="Map.map_fort_02.C22", dst="Map.map_cave_22.F2"
    ),
    "Twin Spires 03 To Twin Spires 04": Portal(
        src="Map.map_fort_03.4", dst="Map.map_fort_04.3"
    ),
    "Twin Spires 03 To Twin Spires 01": Portal(
        src="Map.map_fort_03.1", dst="Map.map_fort_01.3"
    ),
    "Twin Spires 03 To Twin Spires 02": Portal(
        src="Map.map_fort_03.2", dst="Map.map_fort_02.3"
    ),
    "Twin Spires 03 To Twin Spires 05": Portal(
        src="Map.map_fort_03.5", dst="Map.map_fort_05.3"
    ),
    "Twin Spires 04 To Twin Spires 05": Portal(
        src="Map.map_fort_04.5", dst="Map.map_fort_05.4"
    ),
    "Twin Spires 04 To Twin Spires 03": Portal(
        src="Map.map_fort_04.3", dst="Map.map_fort_03.4"
    ),
    "Twin Spires 05 To Twin Spires 04": Portal(
        src="Map.map_fort_05.4", dst="Map.map_fort_04.5"
    ),
    "Twin Spires 05 To Twin Spires 06": Portal(
        src="Map.map_fort_05.6", dst="Map.map_fort_06.5"
    ),
    "Twin Spires 05 To Twin Spires 03": Portal(
        src="Map.map_fort_05.3", dst="Map.map_fort_03.5"
    ),
    "Twin Spires 05 To Twin Spires 15": Portal(
        src="Map.map_fort_05.15", dst="Map.map_fort_15.5"
    ),
    "Twin Spires 06 To Twin Spires 05": Portal(
        src="Map.map_fort_06.5", dst="Map.map_fort_05.6"
    ),
    "Twin Spires 06 To Twin Spires 07": Portal(
        src="Map.map_fort_06.7", dst="Map.map_fort_07.6"
    ),
    "Twin Spires 06 To Twin Spires 10": Portal(
        src="Map.map_fort_06.10", dst="Map.map_fort_10.6"
    ),
    "Twin Spires 07 To Twin Spires 09 Left": Portal(
        src="Map.map_fort_07.9", dst="Map.map_fort_09.7"
    ),
    "Twin Spires 07 To Twin Spires 08": Portal(
        src="Map.map_fort_07.8", dst="Map.map_fort_08.7"
    ),
    "Twin Spires 07 To Twin Spires 06": Portal(
        src="Map.map_fort_07.6", dst="Map.map_fort_06.7"
    ),
    "Twin Spires 07 To Twin Spires 11": Portal(
        src="Map.map_fort_07.11", dst="Map.map_fort_11.7"
    ),
    "Twin Spires 07 To Twin Spires 09 Right": Portal(
        src="Map.map_fort_07.9B", dst="Map.map_fort_09.7B"
    ),
    "Twin Spires 08 To Twin Spires 07": Portal(
        src="Map.map_fort_08.7", dst="Map.map_fort_07.8"
    ),
    "Twin Spires 09 To Hinterlands 03": Portal(
        src="Map.map_fort_09.O3", dst="Map.map_outside_03.F9"
    ),
    "Twin Spires 09 To Twin Spires 10": Portal(
        src="Map.map_fort_09.10", dst="Map.map_fort_10.9"
    ),
    "Twin Spires 09 To Twin Spires 07 Left": Portal(
        src="Map.map_fort_09.7", dst="Map.map_fort_07.9"
    ),
    "Twin Spires 09 To Twin Spires 07 Right": Portal(
        src="Map.map_fort_09.7B", dst="Map.map_fort_07.9B"
    ),
    "Twin Spires 10 To Twin Spires 09": Portal(
        src="Map.map_fort_10.9", dst="Map.map_fort_09.10"
    ),
    "Twin Spires 10 To Twin Spires 06": Portal(
        src="Map.map_fort_10.6", dst="Map.map_fort_06.10"
    ),
    "Twin Spires 11 To Twin Spires 07": Portal(
        src="Map.map_fort_11.7", dst="Map.map_fort_07.11"
    ),
    "Twin Spires 11 To Twin Spires 12": Portal(
        src="Map.map_fort_11.12", dst="Map.map_fort_12.11"
    ),
    "Twin Spires 11 To Twin Spires 13 Left": Portal(
        src="Map.map_fort_11.13", dst="Map.map_fort_13.11"
    ),
    "Twin Spires 11 To Twin Spires 13 Right": Portal(
        src="Map.map_fort_11.53", dst="Map.map_fort_13.11B"
    ),
    "Twin Spires 12 To Twin Spires 11": Portal(
        src="Map.map_fort_12.11", dst="Map.map_fort_11.12"
    ),
    "Twin Spires 12 To Twin Spires 14": Portal(
        src="Map.map_fort_12.14", dst="Map.map_fort_14.12"
    ),
    "Twin Spires 12 To Twin Spires 16": Portal(
        src="Map.map_fort_12.16", dst="Map.map_fort_16.12"
    ),
    "Twin Spires 13 To Twin Spires 11 Left": Portal(
        src="Map.map_fort_13.11", dst="Map.map_fort_11.13"
    ),
    "Twin Spires 13 To Twin Spires 11 Right": Portal(
        src="Map.map_fort_13.11B", dst="Map.map_fort_11.53"
    ),
    "Twin Spires 13 To Twin Spires 14": Portal(
        src="Map.map_fort_13.14", dst="Map.map_fort_14.13"
    ),
    "Twin Spires 13 To Twin Spires 19": Portal(
        src="Map.map_fort_13.19", dst="Map.map_fort_19.13"
    ),
    "Twin Spires 14 To Twin Spires 15": Portal(
        src="Map.map_fort_14.15", dst="Map.map_fort_15.14"
    ),
    "Twin Spires 14 To Twin Spires 13": Portal(
        src="Map.map_fort_14.13", dst="Map.map_fort_13.14"
    ),
    "Twin Spires 14 To Twin Spires 12": Portal(
        src="Map.map_fort_14.12", dst="Map.map_fort_12.14"
    ),
    "Twin Spires 15 To Twin Spires 16 Upper": Portal(
        src="Map.map_fort_15.16", dst="Map.map_fort_16.15"
    ),
    "Twin Spires 15 To Twin Spires 14": Portal(
        src="Map.map_fort_15.14", dst="Map.map_fort_14.15"
    ),
    "Twin Spires 15 To Twin Spires 05": Portal(
        src="Map.map_fort_15.5", dst="Map.map_fort_05.15"
    ),
    "Twin Spires 15 To Twin Spires 17": Portal(
        src="Map.map_fort_15.17", dst="Map.map_fort_17.15"
    ),
    "Twin Spires 15 To Twin Spires 16 Lower": Portal(
        src="Map.map_fort_15.16B", dst="Map.map_fort_16.15B"
    ),
    "Twin Spires 16 To Twin Spires 15 Upper": Portal(
        src="Map.map_fort_16.15", dst="Map.map_fort_15.16"
    ),
    "Twin Spires 16 To Twin Spires 18": Portal(
        src="Map.map_fort_16.18", dst="Map.map_fort_18.16"
    ),
    "Twin Spires 16 To Twin Spires 15 Lower": Portal(
        src="Map.map_fort_16.15B", dst="Map.map_fort_15.16B"
    ),
    "Twin Spires 16 To Twin Spires 12": Portal(
        src="Map.map_fort_16.12", dst="Map.map_fort_12.16"
    ),
    "Twin Spires 17 To Twin Spires 15": Portal(
        src="Map.map_fort_17.15", dst="Map.map_fort_15.17"
    ),
    "Twin Spires 17 To Twin Spires 18": Portal(
        src="Map.map_fort_17.18", dst="Map.map_fort_18.17"
    ),
    "Twin Spires 18 To Twin Spires 17": Portal(
        src="Map.map_fort_18.17", dst="Map.map_fort_17.18"
    ),
    "Twin Spires 18 To Twin Spires 19": Portal(
        src="Map.map_fort_18.19", dst="Map.map_fort_19.18"
    ),
    "Twin Spires 18 To Twin Spires 16": Portal(
        src="Map.map_fort_18.16", dst="Map.map_fort_16.18"
    ),
    "Twin Spires 19 To Twin Spires 18": Portal(
        src="Map.map_fort_19.18", dst="Map.map_fort_18.19"
    ),
    "Twin Spires 19 To Twin Spires 20": Portal(
        src="Map.map_fort_19.20", dst="Map.map_fort_20.19"
    ),
    "Twin Spires 19 To Twin Spires 13": Portal(
        src="Map.map_fort_19.13", dst="Map.map_fort_13.19"
    ),
    "Twin Spires 20 To Twin Spires 19": Portal(
        src="Map.map_fort_20.19", dst="Map.map_fort_19.20"
    ),
    "Twin Spires 20 To Twin Spires 21": Portal(
        src="Map.map_fort_20.21", dst="Map.map_fort_21.20"
    ),
    "Twin Spires 21 To Twin Spires 20": Portal(
        src="Map.map_fort_21.20", dst="Map.map_fort_20.21"
    ),
    "Stockade 01 To Witch's Thicket 07": Portal(
        src="Map.map_oubliette_01.F7", dst="Map.map_forest_07.O1"
    ),
    "Stockade 01 To Stockade 02": Portal(
        src="Map.map_oubliette_01.2", dst="Map.map_oubliette_02.1"
    ),
    "Stockade 02 To Stockade 01": Portal(
        src="Map.map_oubliette_02.1", dst="Map.map_oubliette_01.2"
    ),
    "Stockade 02 To Stockade 05": Portal(
        src="Map.map_oubliette_02.5", dst="Map.map_oubliette_05.2"
    ),
    "Stockade 02 To Stockade 04": Portal(
        src="Map.map_oubliette_02.4", dst="Map.map_oubliette_04.2"
    ),
    "Stockade 03 To Stockade 04": Portal(
        src="Map.map_oubliette_03.4", dst="Map.map_oubliette_04.3"
    ),
    "Stockade 03 To Stockade 10": Portal(
        src="Map.map_oubliette_03.10", dst="Map.map_oubliette_10.3"
    ),
    "Stockade 03 To Stockade 05": Portal(
        src="Map.map_oubliette_03.5", dst="Map.map_oubliette_05.3"
    ),
    "Stockade 04 To Stockade 03": Portal(
        src="Map.map_oubliette_04.3", dst="Map.map_oubliette_03.4"
    ),
    "Stockade 04 To Stockade 02": Portal(
        src="Map.map_oubliette_04.2", dst="Map.map_oubliette_02.4"
    ),
    "Stockade 05_1 To Stockade 05": Portal(
        src="Map.map_oubliette_05_1.5", dst="Map.map_oubliette_05.5_1"
    ),
    "Stockade 05_2 To Stockade 05 Left": Portal(
        src="Map.map_oubliette_05_2.5", dst="Map.map_oubliette_05.5_2"
    ),
    "Stockade 05_2 To Stockade 05 Right": Portal(
        src="Map.map_oubliette_05_2.5B", dst="Map.map_oubliette_05.5_2B"
    ),
    "Stockade 05_3 To Stockade 05": Portal(
        src="Map.map_oubliette_05_3.5", dst="Map.map_oubliette_05.5_3"
    ),
    "Stockade 05 To Stockade 05_2 Left": Portal(
        src="Map.map_oubliette_05.5_2", dst="Map.map_oubliette_05_2.5"
    ),
    "Stockade 05 To Stockade 05_3": Portal(
        src="Map.map_oubliette_05.5_3", dst="Map.map_oubliette_05_3.5"
    ),
    "Stockade 05 To Stockade 07_2": Portal(
        src="Map.map_oubliette_05.7_2", dst="Map.map_oubliette_07_2.5"
    ),
    "Stockade 05 To Stockade 05_1": Portal(
        src="Map.map_oubliette_05.5_1", dst="Map.map_oubliette_05_1.5"
    ),
    "Stockade 05 To Stockade 07_1": Portal(
        src="Map.map_oubliette_05.7_1", dst="Map.map_oubliette_07_1.5"
    ),
    "Stockade 05 To Stockade 02": Portal(
        src="Map.map_oubliette_05.2", dst="Map.map_oubliette_02.5"
    ),
    "Stockade 05 To Stockade 06": Portal(
        src="Map.map_oubliette_05.6", dst="Map.map_oubliette_06.5"
    ),
    "Stockade 05 To Stockade 03": Portal(
        src="Map.map_oubliette_05.3", dst="Map.map_oubliette_03.5"
    ),
    "Stockade 05 To Stockade 05_2 Right": Portal(
        src="Map.map_oubliette_05.5_2B", dst="Map.map_oubliette_05_2.5B"
    ),
    "Stockade 06_1 To Stockade 07": Portal(
        src="Map.map_oubliette_06_1.7", dst="Map.map_oubliette_07.6_1"
    ),
    "Stockade 06_2 To Stockade 07": Portal(
        src="Map.map_oubliette_06_2.7", dst="Map.map_oubliette_07.6_2"
    ),
    "Stockade 06_3 To Stockade 07": Portal(
        src="Map.map_oubliette_06_3.7", dst="Map.map_oubliette_07.6_3"
    ),
    "Stockade 06_4 To Stockade 07": Portal(
        src="Map.map_oubliette_06_4.7", dst="Map.map_oubliette_07.6_4"
    ),
    "Stockade 06 To Stockade 05": Portal(
        src="Map.map_oubliette_06.5", dst="Map.map_oubliette_05.6"
    ),
    "Stockade 06 To Stockade 07": Portal(
        src="Map.map_oubliette_06.7", dst="Map.map_oubliette_07.6"
    ),
    "Stockade 06 To Stockade 10": Portal(
        src="Map.map_oubliette_06.10", dst="Map.map_oubliette_10.6"
    ),
    "Stockade 07_1 To Stockade 05": Portal(
        src="Map.map_oubliette_07_1.5", dst="Map.map_oubliette_05.7_1"
    ),
    "Stockade 07_2 To Stockade 05": Portal(
        src="Map.map_oubliette_07_2.5", dst="Map.map_oubliette_05.7_2"
    ),
    "Stockade 07 To Stockade 06_1": Portal(
        src="Map.map_oubliette_07.6_1", dst="Map.map_oubliette_06_1.7"
    ),
    "Stockade 07 To Stockade 06_3": Portal(
        src="Map.map_oubliette_07.6_3", dst="Map.map_oubliette_06_3.7"
    ),
    "Stockade 07 To Stockade 06_2": Portal(
        src="Map.map_oubliette_07.6_2", dst="Map.map_oubliette_06_2.7"
    ),
    "Stockade 07 To Stockade 06_4": Portal(
        src="Map.map_oubliette_07.6_4", dst="Map.map_oubliette_06_4.7"
    ),
    "Stockade 07 To Stockade 06": Portal(
        src="Map.map_oubliette_07.6", dst="Map.map_oubliette_06.7"
    ),
    "Stockade 07 To Stockade 13": Portal(
        src="Map.map_oubliette_07.13", dst="Map.map_oubliette_13.7"
    ),
    "Stockade 07 To Stockade 09": Portal(
        src="Map.map_oubliette_07.9", dst="Map.map_oubliette_09.7"
    ),
    "Stockade 08 To Stockade 11": Portal(
        src="Map.map_oubliette_08.11", dst="Map.map_oubliette_11.8"
    ),
    "Stockade 08 To Stockade 09": Portal(
        src="Map.map_oubliette_08.9", dst="Map.map_oubliette_09.8"
    ),
    "Stockade 08 To Stockade 13": Portal(
        src="Map.map_oubliette_08.13", dst="Map.map_oubliette_13.8"
    ),
    "Stockade 09 To Stockade 08": Portal(
        src="Map.map_oubliette_09.8", dst="Map.map_oubliette_08.9"
    ),
    "Stockade 09 To Stockade 10": Portal(
        src="Map.map_oubliette_09.10", dst="Map.map_oubliette_10.9"
    ),
    "Stockade 09 To Stockade 07": Portal(
        src="Map.map_oubliette_09.7", dst="Map.map_oubliette_07.9"
    ),
    "Stockade 10 To Stockade 09": Portal(
        src="Map.map_oubliette_10.9", dst="Map.map_oubliette_09.10"
    ),
    "Stockade 10 To Stockade 03": Portal(
        src="Map.map_oubliette_10.3", dst="Map.map_oubliette_03.10"
    ),
    "Stockade 10 To Stockade 17": Portal(
        src="Map.map_oubliette_10.17", dst="Map.map_oubliette_17.10"
    ),
    "Stockade 10 To Stockade 06": Portal(
        src="Map.map_oubliette_10.6", dst="Map.map_oubliette_06.10"
    ),
    "Stockade 11 To Stockade 12": Portal(
        src="Map.map_oubliette_11.12", dst="Map.map_oubliette_12.11"
    ),
    "Stockade 11 To Stockade 08": Portal(
        src="Map.map_oubliette_11.8", dst="Map.map_oubliette_08.11"
    ),
    "Stockade 11 To Stockade 14": Portal(
        src="Map.map_oubliette_11.14", dst="Map.map_oubliette_14.11"
    ),
    "Stockade 11 To Stockade 13": Portal(
        src="Map.map_oubliette_11.13", dst="Map.map_oubliette_13.11"
    ),
    "Stockade 11 To Stockade 13_2": Portal(
        src="Map.map_oubliette_11.13_2", dst="Map.map_oubliette_13_2.11"
    ),
    "Stockade 11 To Stockade 13_1": Portal(
        src="Map.map_oubliette_11.13_1", dst="Map.map_oubliette_13_1.11"
    ),
    "Stockade 12 To Stockade 11": Portal(
        src="Map.map_oubliette_12.11", dst="Map.map_oubliette_11.12"
    ),
    "Stockade 13_1 To Stockade 11": Portal(
        src="Map.map_oubliette_13_1.11", dst="Map.map_oubliette_11.13_1"
    ),
    "Stockade 13_2 To Stockade 11": Portal(
        src="Map.map_oubliette_13_2.11", dst="Map.map_oubliette_11.13_2"
    ),
    "Stockade 13 To Stockade 08": Portal(
        src="Map.map_oubliette_13.8", dst="Map.map_oubliette_08.13"
    ),
    "Stockade 13 To Stockade 11": Portal(
        src="Map.map_oubliette_13.11", dst="Map.map_oubliette_11.13"
    ),
    "Stockade 13 To Stockade 07": Portal(
        src="Map.map_oubliette_13.7", dst="Map.map_oubliette_07.13"
    ),
    "Stockade 14 To Stockade 11": Portal(
        src="Map.map_oubliette_14.11", dst="Map.map_oubliette_11.14"
    ),
    "Stockade 14 To Stockade 15": Portal(
        src="Map.map_oubliette_14.15", dst="Map.map_oubliette_15.14"
    ),
    "Stockade 15 To Stockade 14": Portal(
        src="Map.map_oubliette_15.14", dst="Map.map_oubliette_14.15"
    ),
    "Stockade 15 To Stockade 16": Portal(
        src="Map.map_oubliette_15.16", dst="Map.map_oubliette_16.15"
    ),
    "Stockade 16 To Stockade 15": Portal(
        src="Map.map_oubliette_16.15", dst="Map.map_oubliette_15.16"
    ),
    "Stockade 16 To Hinterlands 01": Portal(
        src="Map.map_oubliette_16.O1", dst="Map.map_outside_01.O16"
    ),
    "Stockade 17 To Stockade 10": Portal(
        src="Map.map_oubliette_17.10", dst="Map.map_oubliette_10.17"
    ),
    "Stockade 17 To Verboten Domain 06": Portal(
        src="Map.map_oubliette_17.S6", dst="Map.map_swamp_06.O17"
    ),
    "Hinterlands 01 To Hinterlands 02": Portal(
        src="Map.map_outside_01.2", dst="Map.map_outside_02.1"
    ),
    "Hinterlands 01 To Hinterlands 03": Portal(
        src="Map.map_outside_01.3", dst="Map.map_outside_03.1"
    ),
    "Hinterlands 01 To Stockade 16": Portal(
        src="Map.map_outside_01.O16", dst="Map.map_oubliette_16.O1"
    ),
    "Hinterlands 02 To Hinterlands 01": Portal(
        src="Map.map_outside_02.1", dst="Map.map_outside_01.2"
    ),
    "Hinterlands 03 To Hinterlands 01": Portal(
        src="Map.map_outside_03.1", dst="Map.map_outside_01.3"
    ),
    "Hinterlands 03 To Twin Spires 09": Portal(
        src="Map.map_outside_03.F9", dst="Map.map_fort_09.O3"
    ),
    "Verboten Domain 04 To Verboten Domain 05": Portal(
        src="Map.map_swamp_04.5", dst="Map.map_swamp_05.4"
    ),
    "Verboten Domain 04 To Verboten Domain 3R": Portal(
        src="Map.map_swamp_04.3", dst="Map.map_swamp_03.4"
    ),
    "Verboten Domain 05 To Verboten Domain 04": Portal(
        src="Map.map_swamp_05.4", dst="Map.map_swamp_04.5"
    ),
    "Verboten Domain 05 To Verboten Domain 06": Portal(
        src="Map.map_swamp_05.6", dst="Map.map_swamp_06.5"
    ),
    "Verboten Domain 05 To Verboten Domain 07": Portal(
        src="Map.map_swamp_05.7", dst="Map.map_swamp_07.5"
    ),
    "Verboten Domain 05 To Verboten Domain 09": Portal(
        src="Map.map_swamp_05.9", dst="Map.map_swamp_09.5"
    ),
    "Verboten Domain 06 To Verboten Domain 05": Portal(
        src="Map.map_swamp_06.5", dst="Map.map_swamp_05.6"
    ),
    "Verboten Domain 06 To Stockade 17": Portal(
        src="Map.map_swamp_06.O17", dst="Map.map_oubliette_17.S6"
    ),
    "Verboten Domain 07 To Verboten Domain 16": Portal(
        src="Map.map_swamp_07.16", dst="Map.map_swamp_16.7"
    ),
    "Verboten Domain 07 To Verboten Domain 05": Portal(
        src="Map.map_swamp_07.5", dst="Map.map_swamp_05.7"
    ),
    "Verboten Domain 07 To Verboten Domain 08": Portal(
        src="Map.map_swamp_07.8", dst="Map.map_swamp_08.7"
    ),
    "Verboten Domain 07 To Verboten Domain 3B": Portal(
        src="Map.map_swamp_07.3", dst="Map.map_swamp_03.7"
    ),
    "Verboten Domain 08 To Verboten Domain 07": Portal(
        src="Map.map_swamp_08.7", dst="Map.map_swamp_07.8"
    ),
    "Verboten Domain 08 To Verboten Domain 15": Portal(
        src="Map.map_swamp_08.15", dst="Map.map_swamp_15.8"
    ),
    "Verboten Domain 08 To Verboten Domain 14": Portal(
        src="Map.map_swamp_08.14", dst="Map.map_swamp_14.8"
    ),
    "Verboten Domain 09 To Verboten Domain 13 Left": Portal(
        src="Map.map_swamp_09.13", dst="Map.map_swamp_13.9"
    ),
    "Verboten Domain 09 To Verboten Domain 03": Portal(
        src="Map.map_swamp_09.3", dst="Map.map_swamp_03.9"
    ),
    "Verboten Domain 09 To Verboten Domain 13 Right": Portal(
        src="Map.map_swamp_09.13B", dst="Map.map_swamp_13.9B"
    ),
    "Verboten Domain 09 To Verboten Domain 05": Portal(
        src="Map.map_swamp_09.5", dst="Map.map_swamp_05.9"
    ),
    "Verboten Domain 01 To Verboten Domain 03": Portal(
        src="Map.map_swamp_01.3", dst="Map.map_swamp_03.1"
    ),
    "Verboten Domain 01 To Verboten Domain 02": Portal(
        src="Map.map_swamp_01.2", dst="Map.map_swamp_02.1"
    ),
    "Verboten Domain 10 To Verboten Domain 13": Portal(
        src="Map.map_swamp_10.13", dst="Map.map_swamp_13.10"
    ),
    "Verboten Domain 11 To Verboten Domain 15": Portal(
        src="Map.map_swamp_11.15", dst="Map.map_swamp_15.11"
    ),
    "Verboten Domain 11 To Verboten Domain 14": Portal(
        src="Map.map_swamp_11.14", dst="Map.map_swamp_14.11"
    ),
    "Verboten Domain 12 To Verboten Domain 15": Portal(
        src="Map.map_swamp_12.15", dst="Map.map_swamp_15.12"
    ),
    "Verboten Domain 12 To The Abyss 04": Portal(
        src="Map.map_swamp_12.4", dst="Map.map_abyss_04.2"
    ),
    "Verboten Domain 13 To Verboten Domain 10": Portal(
        src="Map.map_swamp_13.10", dst="Map.map_swamp_10.13"
    ),
    "Verboten Domain 13 To Verboten Domain 09 Left": Portal(
        src="Map.map_swamp_13.9", dst="Map.map_swamp_09.13"
    ),
    "Verboten Domain 13 To Verboten Domain 09 Right": Portal(
        src="Map.map_swamp_13.9B", dst="Map.map_swamp_09.13B"
    ),
    "Verboten Domain 13 To Verboten Domain 14": Portal(
        src="Map.map_swamp_13.14", dst="Map.map_swamp_14.13"
    ),
    "Verboten Domain 14 To Verboten Domain 13": Portal(
        src="Map.map_swamp_14.13", dst="Map.map_swamp_13.14"
    ),
    "Verboten Domain 14 To Verboten Domain 11": Portal(
        src="Map.map_swamp_14.11", dst="Map.map_swamp_11.14"
    ),
    "Verboten Domain 14 To Verboten Domain 08": Portal(
        src="Map.map_swamp_14.8", dst="Map.map_swamp_08.14"
    ),
    "Verboten Domain 15 To Verboten Domain 11": Portal(
        src="Map.map_swamp_15.11", dst="Map.map_swamp_11.15"
    ),
    "Verboten Domain 15 To Verboten Domain 08": Portal(
        src="Map.map_swamp_15.8", dst="Map.map_swamp_08.15"
    ),
    "Verboten Domain 15 To Verboten Domain 12": Portal(
        src="Map.map_swamp_15.12", dst="Map.map_swamp_12.15"
    ),
    "Verboten Domain 16 To Verboten Domain 07": Portal(
        src="Map.map_swamp_16.7", dst="Map.map_swamp_07.16"
    ),
    "Verboten Domain 16 To Verboten Domain 17": Portal(
        src="Map.map_swamp_16.17", dst="Map.map_swamp_17.16"
    ),
    "Verboten Domain 17 To Verboten Domain 18": Portal(
        src="Map.map_swamp_17.18", dst="Map.map_swamp_18.17"
    ),
    "Verboten Domain 17 To Verboten Domain 16": Portal(
        src="Map.map_swamp_17.16", dst="Map.map_swamp_16.17"
    ),
    "Verboten Domain 18 To The Abyss 01": Portal(
        src="Map.map_swamp_18.A1", dst="Map.map_abyss_01.S18"
    ),
    "Verboten Domain 18 To Verboten Domain 17": Portal(
        src="Map.map_swamp_18.17", dst="Map.map_swamp_17.18"
    ),
    "Verboten Domain 02 To Verboten Domain 01": Portal(
        src="Map.map_swamp_02.1", dst="Map.map_swamp_01.2"
    ),
    "Verboten Domain 02 To Witch's Thicket 09": Portal(
        src="Map.map_swamp_02.F9", dst="Map.map_forest_09.S2"
    ),
    "Verboten Domain 03 To Verboten Domain 09": Portal(
        src="Map.map_swamp_03.9", dst="Map.map_swamp_09.3"
    ),
    "Verboten Domain 03 To Verboten Domain 01": Portal(
        src="Map.map_swamp_03.1", dst="Map.map_swamp_01.3"
    ),
    "Verboten Domain 03 To Verboten Domain 04": Portal(
        src="Map.map_swamp_03.4", dst="Map.map_swamp_04.3"
    ),
    "Verboten Domain 03 To Verboten Domain 07": Portal(
        src="Map.map_swamp_03.7", dst="Map.map_swamp_07.3"
    ),
    "Cliffside Hamlet 01 To Cliffside Hamlet 02": Portal(
        src="Map.map_village_01.1", dst="Map.map_village_02.0"
    ),
    "Cliffside Hamlet 01 To White Parish 08": Portal(
        src="Map.map_village_01.0", dst="Map.map_church_08.1"
    ),
    "Cliffside Hamlet 02 To Cliffside Hamlet 03": Portal(
        src="Map.map_village_02.1", dst="Map.map_village_03.0"
    ),
    "Cliffside Hamlet 02 To Cliffside Hamlet 01": Portal(
        src="Map.map_village_02.0", dst="Map.map_village_01.1"
    ),
    "Cliffside Hamlet 02 To Cliffside Hamlet 13": Portal(
        src="Map.map_village_02.13", dst="Map.map_village_13.2"
    ),
    "Cliffside Hamlet 03 To Cliffside Hamlet 02": Portal(
        src="Map.map_village_03.0", dst="Map.map_village_02.1"
    ),
    "Cliffside Hamlet 03 To Cliffside Hamlet 05": Portal(
        src="Map.map_village_03.5", dst="Map.map_village_05.3"
    ),
    "Cliffside Hamlet 03 To Cliffside Hamlet 13": Portal(
        src="Map.map_village_03.2", dst="Map.map_village_13.0"
    ),
    "Cliffside Hamlet 04_1 To Cliffside Hamlet 04": Portal(
        src="Map.map_village_04_1.0", dst="Map.map_village_04.2"
    ),
    "Cliffside Hamlet 04 To Cliffside Hamlet 04": Portal(
        src="Map.map_village_04.2", dst="Map.map_village_04_1.0"
    ),
    "Cliffside Hamlet 04 To Cliffside Hamlet 05": Portal(
        src="Map.map_village_04.5", dst="Map.map_village_05.4"
    ),
    "Cliffside Hamlet 05 To Cliffside Hamlet 04": Portal(
        src="Map.map_village_05.4", dst="Map.map_village_04.5"
    ),
    "Cliffside Hamlet 05 To Cliffside Hamlet 03": Portal(
        src="Map.map_village_05.3", dst="Map.map_village_03.5"
    ),
    "Cliffside Hamlet 05 To Cliffside Hamlet 06": Portal(
        src="Map.map_village_05.1", dst="Map.map_village_06.0"
    ),
    "Cliffside Hamlet 06 To Cliffside Hamlet 05": Portal(
        src="Map.map_village_06.0", dst="Map.map_village_05.1"
    ),
    "Cliffside Hamlet 06 To Cliffside Hamlet 07": Portal(
        src="Map.map_village_06.1", dst="Map.map_village_07.0"
    ),
    "Cliffside Hamlet 06 To Cliffside Hamlet 12": Portal(
        src="Map.map_village_06.12", dst="Map.map_village_12.6"
    ),
    "Cliffside Hamlet 06 To Cliffside Hamlet 08": Portal(
        src="Map.map_village_06.8", dst="Map.map_village_08.6"
    ),
    "Cliffside Hamlet 07 To Cliffside Hamlet 06": Portal(
        src="Map.map_village_07.0", dst="Map.map_village_06.1"
    ),
    "Cliffside Hamlet 07 To Cliffside Hamlet 09": Portal(
        src="Map.map_village_07.9", dst="Map.map_village_09.7"
    ),
    "Cliffside Hamlet 07 To Cliffside Hamlet 14": Portal(
        src="Map.map_village_07.14", dst="Map.map_village_14.7"
    ),
    "Cliffside Hamlet 08 To Cliffside Hamlet 06": Portal(
        src="Map.map_village_08.6", dst="Map.map_village_06.8"
    ),
    "Cliffside Hamlet 08 To Cliffside Hamlet 09": Portal(
        src="Map.map_village_08.9", dst="Map.map_village_09.8"
    ),
    "Cliffside Hamlet 09 To Cliffside Hamlet 10": Portal(
        src="Map.map_village_09.1", dst="Map.map_village_10.0"
    ),
    "Cliffside Hamlet 09 To Cliffside Hamlet 15": Portal(
        src="Map.map_village_09.2", dst="Map.map_village_15.0"
    ),
    "Cliffside Hamlet 09 To Cliffside Hamlet 07": Portal(
        src="Map.map_village_09.7", dst="Map.map_village_07.9"
    ),
    "Cliffside Hamlet 09 To Cliffside Hamlet 08": Portal(
        src="Map.map_village_09.8", dst="Map.map_village_08.9"
    ),
    "Cliffside Hamlet 10 To Cliffside Hamlet 09": Portal(
        src="Map.map_village_10.0", dst="Map.map_village_09.1"
    ),
    "Cliffside Hamlet 10 To Cliffside Hamlet 11": Portal(
        src="Map.map_village_10.1", dst="Map.map_village_11.0"
    ),
    "Cliffside Hamlet 11_1 To Cliffside Hamlet 11": Portal(
        src="Map.map_village_11_1.0", dst="Map.map_village_11.2"
    ),
    "Cliffside Hamlet 11 To Cliffside Hamlet 11_1": Portal(
        src="Map.map_village_11.2", dst="Map.map_village_11_1.0"
    ),
    "Cliffside Hamlet 11 To Cliffside Hamlet 10": Portal(
        src="Map.map_village_11.0", dst="Map.map_village_10.1"
    ),
    "Cliffside Hamlet 11 To Ruined Castle 01": Portal(
        src="Map.map_village_11.1", dst="Map.map_castle_01.0"
    ),
    "Cliffside Hamlet 12 To Cliffside Hamlet 06": Portal(
        src="Map.map_village_12.6", dst="Map.map_village_06.12"
    ),
    "Cliffside Hamlet 12 To Cliffside Hamlet 13": Portal(
        src="Map.map_village_12.13", dst="Map.map_village_13.12"
    ),
    "Cliffside Hamlet 12 To Catacombs 01": Portal(
        src="Map.map_village_12.C1", dst="Map.map_cave_01.V12"
    ),
    "Cliffside Hamlet 12 To Cliffside Hamlet 16": Portal(
        src="Map.map_village_12.16", dst="Map.map_village_16.12"
    ),
    "Cliffside Hamlet 13 To Cliffside Hamlet 03": Portal(
        src="Map.map_village_13.0", dst="Map.map_village_03.2"
    ),
    "Cliffside Hamlet 13 To Cliffside Hamlet 02": Portal(
        src="Map.map_village_13.2", dst="Map.map_village_02.13"
    ),
    "Cliffside Hamlet 13 To Cliffside Hamlet 12": Portal(
        src="Map.map_village_13.12", dst="Map.map_village_12.13"
    ),
    "Cliffside Hamlet 14 To Cliffside Hamlet 07": Portal(
        src="Map.map_village_14.7", dst="Map.map_village_07.14"
    ),
    "Cliffside Hamlet 15 To Cliffside Hamlet 09": Portal(
        src="Map.map_village_15.0", dst="Map.map_village_09.2"
    ),
    "Cliffside Hamlet 15 To Twin Spires 01": Portal(
        src="Map.map_village_15.F1", dst="Map.map_fort_01.V15"
    ),
    "Cliffside Hamlet 16 To Cliffside Hamlet 12": Portal(
        src="Map.map_village_16.12", dst="Map.map_village_12.16"
    ),
}


class EntranceRandomizerState:
    locations: List[str]
    rules: Dict[str, Callable[[object], bool]]
    dirty: bool
    destinations: Set[str]
    can_reach_locations: Set[str]

    def __init__(
        self, portals: Dict[str, Portal], rules: Dict[str, Callable[[object], bool]]
    ):
        self.locations = [l for l, data in locations.items() if not data.address]
        self.portals = portals
        self.can_reach_locations = set()
        self.rules = rules
        self.destinations = {p.dst for _, p in portals.items()}

    def has(self, key: str, player) -> bool:
        return True

    def can_reach(self, key: str, type: str, player) -> bool:
        if key in self.can_reach_locations:
            return True
        if key in self.rules:
            return self.rules[key](self)
        return key not in self.destinations

    def count(self, key: str, player) -> int:
        return 1000

    def add_location(self, location: str) -> None:
        self.can_reach_locations.add(location)
        if location in self.locations:
            self.dirty = True
            self.locations.remove(location)
        if location in self.portals:
            self.can_reach_locations.add(self.portals[location].dst)
            self.dirty = True

    def can_complete(self) -> bool:
        self.dirty = True
        while self.dirty:
            full = True
            self.dirty = False
            iterator = reversed(range(len(self.locations)))
            for i in iterator:
                location = self.locations[i]
                if self.rules[location](self):
                    self.add_location(location)
                else:
                    full = False
        return full


class EntranceRandomizer:
    starting_location: str
    exit_to_portal: Dict[str, Portal]
    portals_locations: Dict[str, Portal]
    portals: List[Portal]
    rules: Dict[str, Callable[[object], bool]]

    def __init__(self, starting_location: str):
        self.starting_location = starting_location
        self.rules = get_rules(0)[0]
        self.portals_locations = {
            loc: portal.copy() for loc, portal in portals_locations.items()
        }

    def get_portals(self) -> List[Portal]:
        keys = sorted(list(self.portals_locations.keys()))
        return [self.portals_locations[location] for location in keys]

    def can_complete(self) -> bool:
        s = EntranceRandomizerState(self.portals_locations, self.rules)
        s.add_location(self.starting_location)
        return s.can_complete()

    @timing
    def Randomize(self, portals: List[Portal]) -> Dict[str, str]:
        def swap(pair1: Tuple[Portal, Portal], pair2: Tuple[Portal, Portal]) -> bool:
            if pair1[0] == pair2[1] or pair1[1] == pair2[0]:
                return False
            pair1[0].dst = pair2[1].src
            pair2[0].dst = pair1[1].src
            pair1[1].dst = pair2[0].src
            pair2[1].dst = pair1[0].src
            return True

        def reverse(pair1: Tuple[Portal, Portal], pair2: Tuple[Portal, Portal]) -> bool:
            pair1[0].dst = pair1[1].src
            pair2[0].dst = pair2[1].src
            pair1[1].dst = pair1[0].src
            pair2[1].dst = pair2[0].src

        def dst(portal):
            return exit_to_portal[portal.dst]

        portals = [portal for portal in portals if portal.src]
        exit_to_portal = {portal.src: portal for portal in portals}

        #assert self.can_complete()
        count = len(portals)
        for i in range(count):
            for j in range(i + 1, count):
                first = (portals[i], dst(portals[i]))
                second = (portals[j], dst(portals[j]))
                if swap(first, second):
                    if self.can_complete():
                        break
                    else:
                        reverse(first, second)
        #assert self.can_complete()
        return {
            location: portal.dst for location, portal in self.portals_locations.items()
        }


def test():
    e = EntranceRandomizer(el["Ossuary"])
    print(e.Randomize(e.get_portals()))
