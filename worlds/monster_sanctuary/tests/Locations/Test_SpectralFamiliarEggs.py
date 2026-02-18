import unittest

from BaseClasses import ItemClassification
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase
from worlds.monster_sanctuary import locations as LOCATIONS


class TestFamiliarEggLocation_Vanilla(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "spectral_familiar_egg_placement": 0
    }

    def test_items_are_vanilla(self):
        with self.subTest("Spectral Wolf Egg Check is Vanilla"):
            self.assert_item_is_at_location("Eternity's End - Spectral Wolf", "Spectral Wolf Egg")

        with self.subTest("Spectral Eagle Egg Check is Vanilla"):
            self.assert_item_is_at_location("Eternity's End - Spectral Eagle", "Spectral Eagle Egg")

        with self.subTest("Spectral Toad Egg Check is Vanilla"):
            self.assert_item_is_at_location("Eternity's End - Spectral Toad", "Spectral Toad Egg")

        with self.subTest("Spectral Lion Egg Check is Vanilla"):
            self.assert_item_is_at_location("Eternity's End - Spectral Lion", "Spectral Lion Egg")


class TestFamiliarEggLocation_Filler(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "spectral_familiar_egg_placement": 2
    }

    def test_location_can_only_accept_filler(self):
        with self.subTest("Spectral Wolf Egg Check is Vanilla"):
            loc = self.multiworld.get_location("Eternity's End - Spectral Wolf", 1)
            self.assertFalse(loc.item_rule(self.multiworld.worlds[1].create_item("Spectral Wolf Egg")))
            self.assertTrue(loc.item_rule(self.multiworld.worlds[1].create_item("Small Potion")))

        with self.subTest("Spectral Eagle Egg Check is Vanilla"):
            loc = self.multiworld.get_location("Eternity's End - Spectral Eagle", 1)
            self.assertFalse(loc.item_rule(self.multiworld.worlds[1].create_item("Spectral Eagle Egg")))
            self.assertTrue(loc.item_rule(self.multiworld.worlds[1].create_item("Small Potion")))

        with self.subTest("Spectral Toad Egg Check is Vanilla"):
            loc = self.multiworld.get_location("Eternity's End - Spectral Toad", 1)
            self.assertFalse(loc.item_rule(self.multiworld.worlds[1].create_item("Spectral Toad Egg")))
            self.assertTrue(loc.item_rule(self.multiworld.worlds[1].create_item("Small Potion")))

        with self.subTest("Spectral Lion Egg Check is Vanilla"):
            loc = self.multiworld.get_location("Eternity's End - Spectral Lion", 1)
            self.assertFalse(loc.item_rule(self.multiworld.worlds[1].create_item("Spectral Lion Egg")))
            self.assertTrue(loc.item_rule(self.multiworld.worlds[1].create_item("Small Potion")))
