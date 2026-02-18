from typing import Dict

from worlds.monster_sanctuary import encounters as ENCOUNTERS
from worlds.monster_sanctuary.tests.Monsters.Test_MonsterRandomizerBase import TestMonsterRandomizerBase


class TestMonsterRandomizerFreeform(TestMonsterRandomizerBase):
    options = {
        "randomize_monsters": 1
    }

    def test_all_monsters_available(self):
        monster_counts: Dict[str, int] = {name: 0 for name in ENCOUNTERS.monster_data}

        for name, encounter in self.multiworld.worlds[1].encounters.items():
            for monster in encounter.monsters:
                monster_counts[monster.name] += 1

        for name, count in monster_counts.items():
            # We don't randomize these
            if name in ["Spectral Wolf", "Spectral Toad", "Spectral Eagle", "Spectral Lion", "Bard"]:
                continue

            with self.subTest("Monster has been placed", name=name, count=count):
                self.assertGreaterEqual(count, 1)

    def test_monster_eggs_in_item_pool(self):
        item_names = [item.name for item in self.multiworld.itempool]

        with self.subTest("Koi Egg is in item pool"):
            self.assertIn("Koi Egg", item_names)

        with self.subTest("Bard Egg is in item pool"):
            self.assertIn("Bard Egg", item_names)
