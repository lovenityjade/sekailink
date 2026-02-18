from BaseClasses import Location
from worlds.monster_sanctuary import hints as HINTS
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase


class TestHints(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "hints": 1
    }

    def assert_readable_name(self, room_name: str, expected: str):
        location = self.multiworld.get_location(room_name, self.player)
        area_name = HINTS.get_area_name_for_location(self.multiworld.worlds[1], location, self.player)
        with self.subTest(f"Hinted area should be '{expected}'"):
            self.assertEqual(expected, area_name)

    def assert_hint_region_in_another_world(self):
        self.multiworld.player_name[2] = "Some Other Player"
        location = Location(2, "Some Other Room", 123456)
        text = HINTS.get_area_name_for_location(self.multiworld.worlds[1], location, 1)
        with self.subTest(f"Hinted area should be 'in Some Other Player's world'"):
            self.assertEqual("in Some Other Player's world", text)

    def test_hints_have_correct_area_names(self):
        self.assert_readable_name("Mountain Path - Manticorb Room", "on the {Mountain Path}")
        self.assert_readable_name("Blue Cave - North Fork", "in the {Blue Caves}")
        self.assert_readable_name("Keeper Stronghold - West Towers", "in the {Keeper Stronghold}")
        self.assert_readable_name("Stronghold Dungeon - Jail Hidden Area", "in the {Stronghold Dungeon}")
        self.assert_readable_name("Snowy Peaks - East Spikes", "on {Snowy Peaks}")
        self.assert_readable_name("Sun Palace - Bex", "in {Sun Palace}")
        self.assert_readable_name("Ancient Woods - West Cliffs", "in the {Ancient Woods}")
        self.assert_readable_name("Horizon Beach - Old Man by the Sea", "at {Horizon Beach}")
        self.assert_readable_name("Magma Chamber - Bex", "in {Magma Chamber}")
        self.assert_readable_name("Blob Burg - Center Vertical", "in {Blob Burg}")
        self.assert_readable_name("Underworld - Crystal Room", "in the {Underworld}")
        self.assert_readable_name("Mystical Workshop - Bex", "in the {Mystical Workshop}")
        self.assert_readable_name("Forgotten World - North West Tar Pits", "in the {Forgotten World}")
        self.assert_readable_name("Abandoned Tower - South East Climb", "in the {Abandoned Tower}")
        self.assert_hint_region_in_another_world()


