from worlds.monster_sanctuary.tests.Areas.TestArea import TestArea


class MountainPathTests(TestArea):
    options = {
        "remove_locked_doors": 0
    }

    def test_champion_is_accessible_with_no_items(self):
        self.assertAccessible("MountainPath_North1", "MountainPath_West6_Champion", [])

    def test_champion_required_to_access_stronghold(self):
        self.assertNotAccessible("MountainPath_North1", "KeeperStronghold_WestStairwell_5", [])
        self.assertAccessible("MountainPath_North1", "KeeperStronghold_WestStairwell_5", ["Champion Defeated"])

    def test_buran_blob_key(self):
        self.assertNotAccessible("MountainPath_West6", "MountainPath_West6_2100040", [])
        self.assertAccessible("MountainPath_West6", "MountainPath_West6_2100040", ["Blob Key Accessible"])

    def test_keys_to_get_to_champion(self):
        self.assertNotAccessible("MountainPath_Center3", "MountainPath_Center7_Champion", [])
        self.assertAccessible("MountainPath_Center3", "MountainPath_Center7_Champion", ["Mountain Path key"])


class MountainPathMinimumLockedDoorsTests(TestArea):
    options = {
        "remove_locked_doors": 1
    }

    def test_keys_to_get_to_champion(self):
        self.assertNotAccessible("MountainPath_Center3", "MountainPath_Center7_Champion", [])
        self.assertAccessible("MountainPath_Center3", "MountainPath_Center7_Champion", ["Mountain Path key"])


class MountainPathNoLockedDoorsTests(TestArea):
    options = {
        "remove_locked_doors": 2
    }

    def test_second_champion_accessible_with_no_keys(self):
        self.assertAccessible("MountainPath_Center3", "MountainPath_Center7_Champion", [])


class MountainPathPlotlessTests(TestArea):
    options = {
        "skip_plot": 1
    }

    def test_champion_is_not_required_to_access_stronghold(self):
        self.assertAccessible("MountainPath_North1", "KeeperStronghold_WestStairwell_5", [])