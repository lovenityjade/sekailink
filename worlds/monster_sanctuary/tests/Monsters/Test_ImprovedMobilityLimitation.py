from worlds.monster_sanctuary import encounters as ENCOUNTERS
from worlds.monster_sanctuary.tests.Monsters.Test_Freeform import TestMonsterRandomizerFreeform
from worlds.monster_sanctuary.tests.Monsters.Test_GlobalShuffle import TestMonsterRandomizerShuffle
from worlds.monster_sanctuary.tests.Monsters.Test_LocalShuffle import TestMonsterRandomizerEncounter
from worlds.monster_sanctuary.tests.Monsters.Test_MonsterRandomizerBase import TestMonsterRandomizerBase


class ImprovedMobilityLimitationTestBase(TestMonsterRandomizerBase):
    def test_improved_mobility_does_not_appear_where_its_not_supposed_to(self):
        # Get the list of areas that these abilities should not show up in
        areas = []
        if self.multiworld.worlds[1].options.improved_mobility_limit:
            areas = ENCOUNTERS.early_game_areas

        illegal_monsters = [monster.name for monster in ENCOUNTERS.get_monsters_with_abilities(
            self.multiworld.worlds[1],
            "Improved Flying", "Lofty Mount", "Improved Swimming", "Dual Mobility")]

        for encounter_name, encounter in self.multiworld.worlds[1].encounters.items():
            # Only check encounters in areas that have the placement limitation
            if encounter.area not in areas:
                continue

            for i in range(len(encounter.monsters)):
                location_name = f"{encounter.name}_{i}"
                monster_name = encounter.monsters[i].name
                with self.subTest("Monster does not have improved mobility ability",
                                  monster=monster_name, location=location_name):
                    self.assertNotIn(monster_name, illegal_monsters)


class TestImprovedMobilityLimitation_Disabled_Any(ImprovedMobilityLimitationTestBase, TestMonsterRandomizerFreeform):
    options = {
        "randomize_monsters": 1,
        "improved_mobility_limit": 0
    }


class ImprovedMobilityLimitationTests_Disabled_Shuffle(ImprovedMobilityLimitationTestBase, TestMonsterRandomizerShuffle):
    options = {
        "randomize_monsters": 2,
        "improved_mobility_limit": 0
    }


class ImprovedMobilityLimitationTests_Disabled_Encounter(ImprovedMobilityLimitationTestBase, TestMonsterRandomizerEncounter):
    options = {
        "randomize_monsters": 3,
        "improved_mobility_limit": 0
    }
