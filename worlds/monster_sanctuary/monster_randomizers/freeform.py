from typing import List, Optional

from worlds.AutoWorld import World
from .. import encounters as ENCOUNTERS, EncounterData, MonsterData


class FreeFormRandomizer:
    world: World
    encounters_to_randomize: List[EncounterData]
    available_monsters: List[MonsterData]

    def __init__(self):
        pass

    def randomize(self, world: World) -> None:
        self.world = world
        self.encounters_to_randomize = [encounter for name, encounter in world.encounters.items()]
        self.reset_available_monsters()

        # Some monsters and abilities need to be available early to minimize the chance of having an unbeatable seed
        self.place_ability_in_area(["Breakable Walls"], ["MountainPath", "BlueCave"])
        self.place_ability_in_area(["Flying"], ["MountainPath", "BlueCave"])
        self.place_ability_in_area(
            ["Mount", "Charging Mount", "Tar Mount", "Sonar Mount"],
            ["MountainPath", "BlueCave", "StrongholdDungeon", "AncientWoods", "SnowyPeaks", "SunPalace"])

        # These are to make sure the four elemental orbs before Horizon Beach are always possible
        self.place_ability_in_area(["Water Orbs"],
                              ["MountainPath", "BlueCave", "StrongholdDungeon", "SnowyPeaks", "SunPalace",
                               "AncientWoods"])
        self.place_ability_in_area(["Fire Orbs"],
                              ["MountainPath", "BlueCave", "StrongholdDungeon", "SnowyPeaks", "SunPalace",
                               "AncientWoods"])
        self.place_ability_in_area(["Lightning Orbs"],
                              ["MountainPath", "BlueCave", "StrongholdDungeon", "SnowyPeaks", "SunPalace",
                               "AncientWoods"])
        self.place_ability_in_area(["Earth Orbs"],
                              ["MountainPath", "BlueCave", "StrongholdDungeon", "SnowyPeaks", "SunPalace",
                               "AncientWoods"])

        # Make sure that some form of improved swimming is available outside of horizon beach.
        # It's forced to be in later areas (regardless of world options) to make things simpler
        self.place_ability_in_area(["Improved Swimming"], ["Underworld", "Magma Chamber",
                                                           "Mystical Workshop", "Blob Burg"])

        # Placing Vaero in the early game will guarantee that, as long as Silver Feather
        # is in the item pool, we can get improved flying via evolution (more important for global shuffle)
        self.place_monster_in_area("Vaero", ["MountainPath", "BlueCave", "StrongholdDungeon",
                                             "SnowyPeaks", "SunPalace", "AncientWoods"])

        # Placing Fungi in the early game will guarantee that, as long as Druid Soul
        # is in the item pool, we can get spore shroud via evolution (more important for global shuffle)
        self.place_monster_in_area("Fungi", ["MountainPath", "BlueCave", "StrongholdDungeon",
                                             "SnowyPeaks", "SunPalace", "AncientWoods"])

        # We want to guarantee that at least the basic form of swimming is available before horizon beach
        self.place_monster_in_area("Koi", ["MountainPath", "BlueCave", "StrongholdDungeon",
                                           "SnowyPeaks", "SunPalace", "AncientWoods", "Magma Chamber"])

        # Bard is another that might require being careful about, but not in the freeform shuffler.
        # Because in this randomizer the Bard egg is either accessible without its own ability (vanilla)
        # Or it's in the item pool and AP can make sure that it's not locked behind itself.

        # Now we just go through each encounter left and randomize them.
        for encounter in self.encounters_to_randomize:
            self.randomize_monsters_in_encounter(encounter)

    def reset_available_monsters(self):
        self.available_monsters = ENCOUNTERS.get_monster_pool(self.world)

    def place_monster_in_area(self, monster: str, areas: List[str]):
        encounter = self.get_random_encounter_in_area(areas)
        new_monster = ENCOUNTERS.get_monster(monster)

        self.randomize_monsters_in_encounter(encounter, new_monster)
        self.encounters_to_randomize.remove(encounter)

    def place_ability_in_area(self, abilities: List[str], areas: List[str]):
        encounter = self.get_random_encounter_in_area(areas)
        new_monster = self.world.random.choice(
            [mon for mon in self.available_monsters if set(abilities) & set(mon.groups)])

        self.randomize_monsters_in_encounter(encounter, new_monster)
        self.encounters_to_randomize.remove(encounter)

    def get_random_encounter_in_area(self, areas: List[str]) -> EncounterData:
        return self.world.random.choice(
            [encounter for encounter in self.encounters_to_randomize if encounter.area in areas])

    def randomize_monsters_in_encounter(
            self,
            encounter: EncounterData,
            forced_monster: Optional[MonsterData] = None):
        number_of_monsters = len(encounter.monsters)
        encounter.monsters.clear()

        # If this encounter must include a specific monster, then add it
        if forced_monster is not None:
            self.add_monster_to_encounter(encounter, forced_monster, number_of_monsters)
            self.remove_monster_from_available(forced_monster)

        # Fill out the rest of the monster slots with random monsters
        while len(encounter.monsters) < number_of_monsters:
            self.add_monster_to_encounter(
                encounter,
                self.pick_random_monster(encounter),
                number_of_monsters)

        # Shuffle the monsters for good measure
        self.world.random.shuffle(encounter.monsters)

        # Once we're done randomizing, we remove the monsters we added from the list of available mons
        for monster in set(encounter.monsters):
            self.remove_monster_from_available(monster)

    def pick_random_monster(self, encounter: EncounterData):
        picks = self.get_possible_monsters_for_encounter(encounter)

        if len(picks) == 0:
            self.reset_available_monsters()
            picks = self.get_possible_monsters_for_encounter(encounter)

        return self.world.random.choice(picks)

    def get_possible_monsters_for_encounter(self, encounter) -> List[MonsterData]:
        return [monster for monster in self.available_monsters if monster.name not in encounter.monster_exclusions]

    def add_monster_to_encounter(self, encounter: EncounterData, monster: MonsterData, max_monsters: int) -> None:
        if len(encounter.monsters) >= 3:
            return

        encounter.add_monster(monster)

        # there's a 1 in 3 chance that we duplicate the monster we just added (assuming there's room)
        # this will normalize the monsters you encounter slightly
        # by reducing the number of unique monsters you see by ~40%
        if len(encounter.monsters) < max_monsters and self.world.random.randint(0,2) == 0:
            self.add_monster_to_encounter(encounter, monster, max_monsters)

    def remove_monster_from_available(self, to_remove: MonsterData):
        if to_remove in self.available_monsters:
            self.available_monsters.remove(to_remove)
        else:
            self.available_monsters = [monster for monster in self.available_monsters
                                       if monster.name != to_remove.name]
