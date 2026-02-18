from typing import Dict

from worlds.monster_sanctuary import encounters as ENCOUNTERS
from worlds.monster_sanctuary.tests.Monsters.Test_MonsterRandomizerBase import TestMonsterRandomizerBase


class TestMonsterRandomizerEncounter(TestMonsterRandomizerBase):
    options = {
        "randomize_monsters": 3
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
