import copy
from enum import IntEnum
from random import Random
from typing import Dict, List, Optional, Tuple

from .rules import AccessCondition
from BaseClasses import MultiWorld
from ..AutoWorld import World


class GameStage(IntEnum):
    EARLY = 0
    LATE = 1


class EvolutionData:
    monster: str
    catalyst: str


class MonsterData:
    id: int
    name: str
    groups: List[str]
    stage: Optional[GameStage] = None
    pre_evolutions: List[EvolutionData]
    evolutions: List[EvolutionData]
    species_explore_item: str
    ability_explore_item: str
    type_explore_item: str
    progressive_explore_item: Tuple[str, int]
    combo_explore_item: Dict[str, int]

    def __init__(self, id: int, name: str, groups: List[str]):
        # This needs to exist alongside normal item ids, because monsters will ultimately be classified as items
        # Because event locations need to hold items with ids
        self.id = id
        self.name = name
        self.groups = groups
        self.pre_evolutions = []
        self.evolutions = []
        self.combo_explore_item = {}

    def __str__(self):
        return self.name

    def add_evolution(self, monster_name: str, catalyst: str):
        data = EvolutionData()
        data.monster = monster_name
        data.catalyst = catalyst
        self.evolutions.append(data)

    def add_pre_evolution(self, monster_name: str, catalyst: str):
        data = EvolutionData()
        data.monster = monster_name
        data.catalyst = catalyst
        self.pre_evolutions.append(data)

    def is_evolved(self):
        return any(self.pre_evolutions)

    def is_pre_evolved(self):
        return any(self.evolutions)

    def get_evolution(self, evo_name) -> Optional[EvolutionData]:
        for evo in self.evolutions:
            if evo.monster == evo_name:
                return evo

    def get_pre_evolution(self, evo_name) -> Optional[EvolutionData]:
        for evo in self.pre_evolutions:
            if evo.monster == evo_name:
                return evo

    def get_evolution_catalyst(self, evo_name) -> Optional[str]:
        evo_data = self.get_evolution(evo_name)
        if evo_data is not None:
            return evo_data.catalyst

    def get_pre_evolution_catalyst(self, evo_name) -> Optional[str]:
        evo_data = self.get_pre_evolution(evo_name)
        if evo_data is not None:
            return evo_data.catalyst

    def egg_name(self, ignore_evolution: bool = False):
        if self.name == "Plague Egg":
            return "??? Egg"

        # Since King Blob is the only mon with multiple pre-evos, and it never drops its own egg
        # we can safely use the first (and usually only) entry in the pre-evo list.
        if any(self.pre_evolutions) and not ignore_evolution:
            return f"{self.pre_evolutions[0].monster} Egg"

        return f"{self.name} Egg"


class EncounterData:
    encounter_id: int
    name: str
    monsters: List[MonsterData]
    champion: bool = False
    region: str
    area: str
    access_condition = Optional[AccessCondition]
    monster_exclusions: List[str]

    def __init__(self,
                 encounter_id: int,
                 encounter_name: str,
                 is_champion: bool,
                 region: str,
                 access_condition: AccessCondition):
        self.encounter_id = encounter_id
        self.name = encounter_name
        self.champion = is_champion
        self.region = region
        self.area = self.region.split("_")[0]
        self.access_condition = access_condition
        self.monsters = []
        self.monster_exclusions = []

    def __str__(self):
        return f"{self.name}: {', '.join([monster.name for monster in self.monsters])}"

    def add_monster(self, monster: MonsterData):
        self.monsters.append(monster)

    def add_exclusion(self, *monsters: str):
        for monster in monsters:
            if monster not in self.monster_exclusions:
                self.monster_exclusions.append(monster)

    def replace_monsters(self, *new_monsters: MonsterData):
        if len(new_monsters) != len(self.monsters):
            raise ValueError(
                f"Could not replace monsters for encounter {self.name}. Number of monsters does not match.")

        self.monsters = list(new_monsters)


monster_data: Dict[str, MonsterData] = {}
encounter_data: Dict[str, EncounterData] = {}


early_game_areas: List[str] = ["Menu", "MountainPath", "BlueCave", "KeepersStronghold", "KeepersTower",
                               "StrongholdDungeon", "SnowyPeaks", "SunPalace", "AncientWoods"]
late_game_areas: List[str] = ["HorizonBeach", "MagmaChamber", "BlobBurg", "ForgottenWorld", "MysticalWorkshop",
                              "Underworld", "AbandonedTower", "EternitysEnd"]


# region Data Loading
def add_encounter(encounter: EncounterData, monsters: List[str]) -> None:
    if encounter_data.get(encounter.name) is not None:
        raise KeyError(f"{encounter.name} already exists in encounters_data")

    for monster_name in monsters:
        encounter.add_monster(get_monster(monster_name))

    encounter_data[encounter.name] = encounter


def assign_game_stage_to_monsters():
    for encounter_name, encounter in encounter_data.items():
        stage: GameStage

        if encounter.area in early_game_areas:
            stage = GameStage.EARLY
        elif encounter.area in late_game_areas:
            stage = GameStage.LATE

        for monster in encounter.monsters:
            if monster.stage is None or stage < monster.stage:
                monster.stage = stage
# endregion


# region Randomizer
def randomize(world: World):
    set_encounter_monster_exclusions(world)

    # This holds the data for all encounter locations used by the world.
    # We store it here so that encounter_data can remain immutable while randomizing
    world.encounters = copy.deepcopy(encounter_data)

    randomizer = None
    if world.options.randomize_monsters == 1:
        from .monster_randomizers.freeform import FreeFormRandomizer
        randomizer = FreeFormRandomizer()

    elif world.options.randomize_monsters == 2:
        from .monster_randomizers.global_shuffle import  GlobalShuffleRandomizer
        randomizer = GlobalShuffleRandomizer()

    elif world.options.randomize_monsters == 3:
        from .monster_randomizers.local_shuffle import LocalShuffleRandomizer
        randomizer = LocalShuffleRandomizer()

    if randomizer is not None:
        randomizer.randomize(world)
# endregion


# UNUSED
def assign_familiar(world: World) -> None:
    if world.options.spectral_familiar == "wolf":
        world.encounters["Menu_0"].add_monster(get_monster("Spectral Wolf"))
    elif world.options.spectral_familiar == "eagle":
        world.encounters["Menu_0"].add_monster(get_monster("Spectral Eagle"))
    elif world.options.spectral_familiar == "toad":
        world.encounters["Menu_0"].add_monster(get_monster("Spectral Toad"))
    elif world.options.spectral_familiar == "lion":
        world.encounters["Menu_0"].add_monster(get_monster("Spectral Lion"))


def set_encounter_monster_exclusions(world: World):
    """Sets up rules for which monsters cannot appear at certain locations"""
    if world.options.goal == "defeat_mad_lord":
        mad_lord = encounter_data["AbandonedTower_Final_1"]
        mad_lord.add_exclusion("Akhlut")
        mad_lord.add_exclusion("Krakaturtle")
        mad_lord.add_exclusion("Gryphonix")
        mad_lord.add_exclusion("Bard")
        mad_lord.add_exclusion("Tar Blob")

        # if the cryomancer locations are in the pool then Dodo can't be on the mad lord slot
        if world.options.monster_shift_rule != "never":
            mad_lord.add_exclusion("Dodo")

    # if there's regions where improved mobility are illegal, set it up.
    if world.options.improved_mobility_limit:
        encounters = [encounter for encounter in encounter_data.values()
                      if encounter.area in early_game_areas]
        monsters = [monster.name for monster in get_monsters_with_abilities(
            world,
            "Improved Flying", "Lofty Mount", "Improved Swimming", "Dual Mobility")]
        for enc in encounters:
            enc.add_exclusion(*monsters)


def get_monster_pool(world: World, *monster_exclusions: str) -> List[MonsterData]:
    exclude = list(monster_exclusions)
    if not world.options.include_spectral_familiars_in_pool:
        exclude = exclude + ["Spectral Wolf", "Spectral Toad", "Spectral Eagle", "Spectral Lion"]

    if not world.options.include_bard_in_pool:
        exclude = exclude + ["Bard"]

    return [monster for monster in monster_data.values() if monster.name not in exclude]


def get_monster(monster_name: str) -> Optional[MonsterData]:
    if monster_data.get(monster_name) is not None:
        return monster_data[monster_name]

    raise KeyError(f"'{monster_name}' was not found in monsters_data")


def get_monsters_with_abilities(world: World, *abilities) -> List[MonsterData]:
    return [monster for monster in get_monster_pool(world) if set(abilities) & set(monster.groups)]


def get_monsters_in_area(world: World, *areas: str) -> List[MonsterData]:
    monsters: Dict[str, MonsterData] = {}

    for (name, encounter) in world.encounters.items():
        if encounter.area not in areas:
            continue

        for monster in encounter.monsters:
            if monster.name not in monsters:
                monsters[monster.name] = monster

    return list(monsters.values())
