from typing import List

from worlds.AutoWorld import call_all
from worlds.monster_sanctuary import items as ITEMS
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase


class TestEggsFollowMobilityLimitationRules(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "randomize_monsters": 1,
        "improved_mobility_limit": 1
    }

    def test_eggs_can_be_placed_correctly(self):
        self.assertFalse(ITEMS.can_item_be_placed(
            self.multiworld.worlds[1],
            self.multiworld.worlds[1].create_item("Krakaturtle Egg"),
            "Mountain Path - East Corridor 1"))
        self.assertTrue(ITEMS.can_item_be_placed(
            self.multiworld.worlds[1],
            self.multiworld.worlds[1].create_item("Krakaturtle Egg"),
            "Blob Burg - King Blob Room"))

    def test_eggs_are_placed_correctly(self):
        illegal_locations = ["Menu", "Mountain Path", "Blue Cave", "Keepers Stronghold", "Keepers Tower",
                             "Stronghold Dungeon", "Snowy Peaks", "Sun Palace", "Ancient Woods"]
        for location in self.multiworld.get_filled_locations(1):
            item_data = ITEMS.get_item_by_name(location.item.name)

            # This ignores events
            if item_data is None:
                continue

            # Only care about eggs with the right abilities
            if not ITEMS.is_item_in_group(item_data.name, "Improved Flying", "Lofty Mount", "Improved Swimming", "Dual Mobility"):
                continue

            with self.subTest(f"{item_data.name} is allowed to be at {location.name}"):
                area = location.name.split(' - ')[0]
                self.assertNotIn(area, illegal_locations)
