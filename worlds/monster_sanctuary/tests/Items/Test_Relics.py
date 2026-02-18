import re
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase
from worlds.monster_sanctuary import items as ITEMS


class TestRelicsAreNotInPool(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "include_chaos_relics": 0
    }

    def test_no_relics_are_in_pool(self):
        relics = ITEMS.get_items_in_group("Relic", True)
        pool = [item.name for item in self.multiworld.itempool]
        for relic in relics:
            with self.subTest(f"{relic.name} is not in the item pool"):
                self.assertNotIn(relic.name, pool)


class TestSomeRelicsAreInPool(MonsterSanctuaryTestBase):
    options = {
        "include_chaos_relics": 2
    }

    def test_at_least_5_relics_are_in_pool(self):
        relics = ITEMS.get_items_in_group("Relic", True)
        pool = [item.name for item in self.multiworld.itempool]

        relics_in_pool = [relic.name for relic in relics if relic.name in pool]

        self.assertGreater(len(relics_in_pool), 5)


class TestAllRelicsAreInPool(MonsterSanctuaryTestBase):
    options = {
        "include_chaos_relics": 3
    }

    def test_all_relics_are_in_pool(self):
        relics = [relic.name for relic in ITEMS.get_items_in_group("Relic", False)]
        pool = [re.sub(r"\+\d", "", item.name) for item in self.multiworld.itempool]

        for relic in relics:
            with self.subTest(f"{relic} is in item pool"):
                self.assertIn(relic, pool)
