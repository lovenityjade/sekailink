from . import RoRTestBase

class MapLockTest(RoRTestBase):
    options = {
        "grouping": "map"
    }

    def test_providence(self) -> None:
        self.collect_all_but(["Risk of Rain", "Victory"])
        self.assertFalse(self.can_reach_entrance("Risk of Rain"))
        self.assertBeatable(False)
        self.collect_by_name("Risk of Rain")
        self.assertTrue(self.can_reach_entrance("Risk of Rain"))
        self.assertBeatable(True)