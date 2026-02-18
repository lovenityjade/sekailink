from BaseClasses import ItemClassification
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase
from worlds.monster_sanctuary import locations as LOCATIONS


class TestSkorchEggLocation_Vanilla(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "skorch_egg_placement": 0
    }

    def test_items_are_vanilla(self):
        with self.subTest("Skorch Egg Check is Skorch Egg"):
            self.assert_item_is_at_location("Magma Chamber - Bex", "Skorch Egg")


class TestSkorchEggLocation_Filler(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "skorch_egg_placement": 2
    }

    def test_location_can_only_accept_filler(self):
        loc = self.multiworld.get_location("Magma Chamber - Bex", 1)
        self.assertIsNotNone(loc)
        self.assertFalse(loc.item_rule(self.multiworld.worlds[1].create_item("Skorch Egg")))
        self.assertTrue(loc.item_rule(self.multiworld.worlds[1].create_item("Small Potion")))