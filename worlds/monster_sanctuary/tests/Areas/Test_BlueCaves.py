from worlds.monster_sanctuary.tests.Areas.TestArea import TestArea


class BlueCavesTests(TestArea):
    options = {
        "remove_locked_doors": 0
    }

    def test_switches_locked_door(self):
        self.assertNotAccessible("BlueCave_North1", "blue_cave_switches_access", [])
        self.assertAccessible("BlueCave_North1", "blue_cave_switches_access",
                              ["Blue Cave key"])

    def test_champion_door_requires_double_jump(self):
        self.assertNotAccessible("BlueCave_CentralPart", "blue_caves_story_complete",
                                 ["Blue Cave key", "Blue Cave key", "Blue Cave key"])
        self.assertAccessible("BlueCave_CentralPart", "blue_caves_story_complete",
                              ["Blue Cave key", "Blue Cave key", "Blue Cave key", "Double Jump Boots"])

    def test_champion_locked_door(self):
        self.assertNotAccessible("BlueCave_CentralPart", "blue_caves_story_complete",
                                 ["Double Jump Boots"])
        self.assertNotAccessible("BlueCave_CentralPart", "blue_caves_story_complete",
                              ["Blue Cave key", "Double Jump Boots"])
        self.assertNotAccessible("BlueCave_CentralPart", "blue_caves_story_complete",
                              ["Blue Cave key", "Blue Cave key", "Double Jump Boots"])
        self.assertAccessible("BlueCave_CentralPart", "blue_caves_story_complete",
                              ["Blue Cave key", "Blue Cave key", "Blue Cave key", "Double Jump Boots"])

    def test_south_locked_door(self):
        self.assertNotAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0", [])
        self.assertNotAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0",
                              ["Blue Cave key"])
        self.assertNotAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0",
                                 ["Blue Cave key", "Blue Cave key"])
        self.assertAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0",
                                 ["Blue Cave key", "Blue Cave key", "Blue Cave key"])

        self.assertNotAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6", ["Double Jump Boots"])
        self.assertNotAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6",
                                 ["Blue Cave key", "Double Jump Boots"])
        self.assertNotAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6",
                                 ["Blue Cave key", "Blue Cave key", "Double Jump Boots"])
        self.assertAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6",
                              ["Blue Cave key", "Blue Cave key", "Blue Cave key", "Double Jump Boots"])

    def test_mountain_path_shortcut(self):
        # Test that the shortcut can be accessed from the blue caves side
        self.assertAccessible("BlueCave_ChampionRoom2", "blue_cave_champion_room_2_west_shortcut", [])

        # Test tha the shortcut can only be used if the blue cave side has been unlocked
        self.assertNotAccessible("MountainPath_Center6_Lower", "blue_cave_champion_room_2_west_shortcut", [])
        self.assertAccessible("MountainPath_Center6_Lower", "blue_cave_champion_room_2_west_shortcut",
                              ["Blue Caves to Mountain Path Shortcut"])

    def test_sanctuary_token_check(self):
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061", [])
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                                 ["Ostanes"])
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                                 ["Stronghold Dungeon Library Access"])
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                                 ["Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token"])
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                                 ["Ostanes", "Stronghold Dungeon Library Access"])
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                                 ["Ostanes", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token"])
        self.assertAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                              ["Ostanes",
                               "Stronghold Dungeon Library Access",
                               "Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token"])


class BlueCavesMinimalLockedDoorsTests(TestArea):
    options = {
        "remove_locked_doors": 1
    }

    def test_switches_accessible_with_no_keys(self):
        self.assertAccessible("BlueCave_North1", "blue_cave_switches_access", [])

    def test_champion_accessible_with_no_keys(self):
        self.assertNotAccessible("BlueCave_CentralPart", "blue_caves_story_complete", [])
        self.assertAccessible("BlueCave_CentralPart", "blue_caves_story_complete", ["Double Jump Boots"])

    def test_south_locked_door(self):
        self.assertNotAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0", [])
        self.assertAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0",
                              ["Blue Cave key"])

        self.assertNotAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6", [])
        self.assertAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6",
                              ["Blue Cave key", "Double Jump Boots"])


class BlueCavesNoLockedDoorsTests(TestArea):
    options = {
        "remove_locked_doors": 2
    }

    def test_switches_accessible_with_no_keys(self):
        self.assertAccessible("BlueCave_North1", "blue_cave_switches_access", [])

    def test_champion_accessible_with_no_keys(self):
        self.assertNotAccessible("BlueCave_CentralPart", "blue_caves_story_complete", [])
        self.assertAccessible("BlueCave_CentralPart", "blue_caves_story_complete", ["Double Jump Boots"])

    def test_south_accessible_with_no_keys(self):
        self.assertAccessible("BlueCave_CentralPart", "BlueCave_South1_Upper_1_0", [])
        self.assertAccessible("BlueCave_South1_Upper", "BlueCave_CentralPart_6",
                                 ["Double Jump Boots"])


class BlueCavesPlotlessTests(TestArea):
    options = {
        "skip_plot": 1
    }

    def test_sanctuary_token_check(self):
        self.assertNotAccessible("BlueCave_South5", "BlueCave_South5_29300061", [])
        self.assertAccessible("BlueCave_South5", "BlueCave_South5_29300061",
                              ["Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token", "Sanctuary Token"])


class BlueCavesWithOpenShortcuts(TestArea):
    options = {
        "open_blue_caves": 1
    }

    def test_mountain_path_shortcut(self):
        self.assertAccessible(
            "MountainPath_Center6_Lower",
            "blue_cave_champion_room_2_west_shortcut",
            [])
        self.assertAccessible(
            "BlueCave_ChampionRoom2_WestAccess",
            "MountainPath_Center6_Lower_5",
            [])
