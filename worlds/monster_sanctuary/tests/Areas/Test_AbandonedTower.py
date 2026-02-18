from worlds.monster_sanctuary.tests.Areas.TestArea import TestArea


class AbandonedTowerTests(TestArea):
    def test_south_shortcut(self):
        self.assertNotAccessible("AbandonedTower_South8", "abandoned_tower_south_shortcut", [])
        self.assertAccessible("AbandonedTower_South8", "abandoned_tower_south_shortcut",
                              ["Abandoned Tower South Shortcut"])

    def test_center_shortcut(self):
        self.assertNotAccessible("AbandonedTower_Center10", "abandoned_tower_center_shortcut", [])
        self.assertAccessible("AbandonedTower_Center10", "abandoned_tower_center_shortcut",
                              ["Abandoned Tower Center Shortcut"])

    def test_progression(self):
        self.assertNotAccessible("AbandonedTower_Entrance", "mad_lord_defeated", [])
        self.assertNotAccessible("AbandonedTower_Entrance", "mad_lord_defeated",
                                 ["Key of Power"])
        self.assertNotAccessible("AbandonedTower_Entrance", "mad_lord_defeated",
                                 ["Double Jump Boots"])
        self.assertAccessible("AbandonedTower_Entrance", "mad_lord_defeated",
                              ["Key of Power", "Double Jump Boots"])


class AbandonedTowerWithOpenEntrances(TestArea):
    options = {
        "open_abandoned_tower": "entrances"
    }

    def test_abandoned_tower_is_accessible(self):
        self.assertAccessible(
            "MysticalWorkshop_West3_Access",
            "AbandonedTower_South2_1",
            ["Blob"]  # Only needed to get the chest, not for navigation
        )


class AbandonedTowerWithOpenShortcuts(TestArea):
    options = {
        "open_abandoned_tower": "shortcuts"
    }

    def test_south_shortcut(self):
        self.assertAccessible("AbandonedTower_South8", "abandoned_tower_south_shortcut", [])

    def test_center_shortcut(self):
        self.assertAccessible("AbandonedTower_Center10", "abandoned_tower_center_shortcut", [])
