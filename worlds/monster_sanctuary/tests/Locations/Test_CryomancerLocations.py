from BaseClasses import ItemClassification
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase
from worlds.monster_sanctuary import locations as LOCATIONS


class TestCryomancer_Vanilla(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "cryomancer_check_restrictions": 0
    }

    def test_items_are_vanilla(self):
        with self.subTest("Cryomancer Egg Reward 1 is Shockhopper Egg"):
            self.assert_item_is_at_location("Snowy Peaks - Cryomancer - Egg Reward 1", "Shockhopper Egg")
        with self.subTest("Cryomancer Egg Reward 2 is Reward Box Lvl 2"):
            self.assert_item_is_at_location("Snowy Peaks - Cryomancer - Egg Reward 2", "Reward Box Lvl 2")
        with self.subTest("Cryomancer Light Egg Reward is Shockhopper Egg"):
            self.assert_item_is_at_location("Snowy Peaks - Cryomancer - Light Egg Reward", "Shockhopper Egg")
        with self.subTest("Cryomancer Dark Egg Reward is Shockhopper Egg"):
            self.assert_item_is_at_location("Snowy Peaks - Cryomancer - Dark Egg Reward", "Shockhopper Egg")


class TestCryomancer_NoShifts(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "monster_shift_rule": 0
    }

    def test_cryomancer_locations_do_not_exist(self):
        with self.subTest("Snowy Peaks - Cryomancer - Light Egg Reward"):
            self.assertNotIn("Snowy Peaks - Cryomancer - Light Egg Reward", self.multiworld.regions.location_cache[self.player])
        with self.subTest("Snowy Peaks - Cryomancer - Dark Egg Reward"):
            self.assertNotIn("Snowy Peaks - Cryomancer - Dark Egg Reward", self.multiworld.regions.location_cache[self.player])


class TestCryomancer_WithShifts(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "monster_shift_rule": 1
    }

    def test_cryomancer_locations_exist(self):
        with self.subTest("Snowy Peaks - Cryomancer - Light Egg Reward"):
            self.assertIn("Snowy Peaks - Cryomancer - Light Egg Reward", self.multiworld.regions.location_cache[self.player])
        with self.subTest("Snowy Peaks - Cryomancer - Dark Egg Reward"):
            self.assertIn("Snowy Peaks - Cryomancer - Dark Egg Reward", self.multiworld.regions.location_cache[self.player])

