from BaseClasses import ItemClassification
from worlds.monster_sanctuary import locations as LOCATIONS
from worlds.monster_sanctuary import items as ITEMS, MonsterSanctuaryItem, MonsterSanctuaryLocation
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase


class AreaKeyPlacementTest(MonsterSanctuaryTestBase):
    run_default_tests = False

    def assert_item_can_be_placed(self, item_name, location_name):
        location_data = LOCATIONS.location_data[location_name]
        with self.subTest(f"{item_name} can be placed at {location_name}"):
            world = self.multiworld.worlds[1]
            item = world.create_item(item_name)
            location = self.multiworld.get_location(location_data.name, 1)

            self.assertTrue(ITEMS.can_item_be_placed(world, item, location.name))

    def assert_item_can_not_be_placed(self, item_name, location_name):
        location_data = LOCATIONS.location_data[location_name]
        with self.subTest(f"{item_name} can't be placed at {location_name}"):
            world = self.multiworld.worlds[1]
            item = world.create_item(item_name)
            location = self.multiworld.get_location(location_data.name, 1)

            self.assertFalse(ITEMS.can_item_be_placed(world, item, location.name))

area_keys = [
    "Mountain Path key",
    "Blue Cave key",
    "Stronghold Dungeon key",
    "Ancient Woods key",
    "Magma Chamber key",
    "Mystical Workshop key",
    "Underworld key"
]

locations = [
    "MountainPath_North3_4",
    "BlueCave_NorthFork_Upper_10",
    "KeeperStronghold_WestStairwell_5",
    "StrongholdDungeon_Jail_5",
    "SnowyPeaks_East2_Lower_4",
    "SnowyPeaks_West2_1",
    "AncientWoods_West1_4",
    "HorizonBeach_West1_0",
    "MagmaChamber_West1_East_41",
    "BlobBurg_East2_2",
    "MysticalWorkshop_South4_18",
    "ForgottenWorld_FallHidden_0",
    "Underworld_East3_3",
    "AbandonedTower_South2_1"
]

expected = {
    "Mountain Path key": "MountainPath_North3_4",
    "Blue Cave key": "BlueCave_NorthFork_Upper_10",
    "Stronghold Dungeon key": "StrongholdDungeon_Jail_5",
    "Ancient Woods key": "AncientWoods_West1_4",
    "Magma Chamber key": "MagmaChamber_West1_East_41",
    "Mystical Workshop key": "MysticalWorkshop_South4_18",
    "Underworld key": "Underworld_East3_3"
}


class AreaKeyLocalPlacementTests(AreaKeyPlacementTest):
    options = {
        "local_area_keys": 1
    }

    def test_keys_can_be_placed_in_their_own_areas(self):
        for key in area_keys:
            for location in locations:
                if expected[key] == location:
                    self.assert_item_can_be_placed(key, location)

    def test_keys_cannot_be_placed_outside_their_areas(self):
        for key in area_keys:
            for location in locations:
                if expected[key] != location:
                    self.assert_item_can_not_be_placed(key, location)


class AreaKeyGlobalPlacementTests(AreaKeyPlacementTest):
    options = {
        "local_area_keys": 0
    }

    def test_keys_are_placed_locally(self):
        for key in area_keys:
            for location in locations:
                self.assert_item_can_be_placed(key, location)



class AreaKeyLocalPlacementTests_WithNoProgressionInUnderworld(AreaKeyPlacementTest):
    run_default_tests = True
    options = {
        "local_area_keys": 1,
        "no_progression_in_underworld": 1
    }


class AreaKeyLocalPlacementTests_WithNoProgressionInForgottenWorld(AreaKeyPlacementTest):
    run_default_tests = True
    options = {
        "local_area_keys": 1,
        "no_progression_in_forgotten_world": 1
    }
