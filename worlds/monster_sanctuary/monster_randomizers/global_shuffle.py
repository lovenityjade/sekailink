from typing import List, Optional, Dict

from worlds.AutoWorld import World
from .. import encounters as ENCOUNTERS, EncounterData, MonsterData

class GlobalShuffleRandomizer:
    world: World
    available_monsters: List[MonsterData]
    species_swap: Dict[str, MonsterData]

    def __init__(self):
        pass

    # TODO: Currently this doesn't account for the improved_mobility_limit option when hard-placing abilities/monsters in areas
    def randomize(self, world: World) -> None:
        self.world = world
        self.species_swap = {}
        self.reset_available_monsters()

        # These monsters should never be shuffled (except maybe they can be with game options)
        self.species_swap["Spectral Wolf"] = ENCOUNTERS.get_monster("Spectral Wolf")
        self.species_swap["Spectral Toad"] = ENCOUNTERS.get_monster("Spectral Toad")
        self.species_swap["Spectral Eagle"] = ENCOUNTERS.get_monster("Spectral Eagle")
        self.species_swap["Spectral Lion"] = ENCOUNTERS.get_monster("Spectral Lion")
        self.species_swap["Bard"] = ENCOUNTERS.get_monster("Bard")

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
        # is in the item pool, we can get improved flying via evolution
        self.place_monster_in_area("Vaero", ["MountainPath", "BlueCave", "StrongholdDungeon",
                                             "SnowyPeaks", "SunPalace", "AncientWoods"])

        # Placing Fungi in the early game will guarantee that, as long as Druid Soul
        # is in the item pool, we can get spore shroud via evolution
        self.place_monster_in_area("Fungi", ["MountainPath", "BlueCave", "StrongholdDungeon",
                                             "SnowyPeaks", "SunPalace", "AncientWoods"])

        # We want to guarantee that at least the basic form of swimming is available before horizon beach
        self.place_monster_in_area("Koi", ["MountainPath", "BlueCave", "StrongholdDungeon",
                                           "SnowyPeaks", "SunPalace", "AncientWoods", "Magma Chamber"])

        # Handle encounters with exclusions first, so we're more likely to accommodate those exclusions.
        for name, enc in world.encounters.items():
            if len(enc.monster_exclusions) > 0:  # Ignore encounters with no exclusions
                self.place_monsters_in_encounter_with_exclusions(enc)

        # Shuffle the remainder of available monsters
        keys = [monster.name for monster in ENCOUNTERS.get_monster_pool(self.world) if monster.name not in self.species_swap]
        values = self.available_monsters
        self.world.random.shuffle(values)
        for i in range(len(keys)):
            self.species_swap[keys[i]] = values[i]

        if len(self.species_swap) != len(ENCOUNTERS.monster_data):
            for monster in [name for name in ENCOUNTERS.monster_data if name not in self.species_swap]:
                print(f"WARNING: {monster} was not swapped")

        # Finally, modify the world's encounter data
        for encounter in self.world.encounters.values():
            encounter.monsters = [self.species_swap[monster.name] for monster in encounter.monsters]

        self.world.species_swap = self.species_swap

    def place_monsters_in_encounter_with_exclusions(self, encounter: EncounterData):
        for original_monster in encounter.monsters:
            # If we've already swapped this monster, then ignore it and move on
            if original_monster.name in self.species_swap:
                continue

            new_monster = self.world.random.choice(self.get_possible_monsters_for_encounter(encounter))
            self.species_swap[original_monster.name] = new_monster
            self.remove_monster_from_available(new_monster)

    def reset_available_monsters(self):
        self.available_monsters = ENCOUNTERS.get_monster_pool(self.world)

    def place_monster_in_area(self, monster: str, areas: List[str]):
        if self.has_monster_been_placed(monster):
            return

        original_monster = self.get_random_non_swapped_monster_in_area(*areas)
        new_monster = ENCOUNTERS.get_monster(monster)

        self.species_swap[original_monster.name] = new_monster
        self.remove_monster_from_available(new_monster)

    def place_ability_in_area(self, abilities: List[str], areas: List[str]):
        # Pick a monster that hasn't already been shuffled, and is in the right areas
        # then swap it with a new monster that has the correct ability
        original_monster = self.get_random_non_swapped_monster_in_area(*areas)
        new_monster = self.world.random.choice(
            [mon for mon in self.available_monsters if set(abilities) & set(mon.groups)])

        self.species_swap[original_monster.name] = new_monster
        self.remove_monster_from_available(new_monster)

    def get_random_non_swapped_monster_in_area(self, *areas: str):
        return self.world.random.choice(
            [mon for mon in ENCOUNTERS.get_monsters_in_area(self.world, *areas)
             if mon.name not in self.species_swap])

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
        before_count = len(self.available_monsters)
        if to_remove in self.available_monsters:
            self.available_monsters.remove(to_remove)
        else:
            self.available_monsters = [monster for monster in self.available_monsters
                                       if monster.name != to_remove.name]

        if len(self.available_monsters) == before_count:
            raise KeyError(f"{to_remove.name} could not be removed from the list of available monsters")

    def has_monster_been_placed(self, monster_name: str) -> bool:
        return monster_name not in [monster.name for monster in self.available_monsters]