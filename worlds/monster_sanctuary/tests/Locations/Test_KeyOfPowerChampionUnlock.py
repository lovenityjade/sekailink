from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase


class TestKeyOfPowerUnlock_Disabled(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "key_of_power_champion_unlock": 0
    }

    def test_key_of_power_location_does_not_exist(self):
        self.assert_location_does_not_exist("Key of Power - Defeat X Champions")

    def test_key_of_power_is_in_item_pool(self):
        self.assertIn("Key of Power", [item.name for item in self.multiworld.itempool])


class TestKeyOfPowerUnlock_OneChampion(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "key_of_power_champion_unlock": 1
    }

    def test_key_of_power_location_does_exist(self):
        self.assert_location_exists("Key of Power - Defeat 1 Champions")

    def test_key_of_power_is_at_location(self):
        self.assert_item_is_at_location("Key of Power - Defeat 1 Champions", "Key of Power")

    def test_key_of_power_is_not_in_item_pool(self):
        self.assertNotIn("Key of Power", [item.name for item in self.multiworld.itempool])