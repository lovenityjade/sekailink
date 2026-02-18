from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase
from worlds.monster_sanctuary import locations as LOCATIONS


class TestLocations_MadLord(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "goal": 0,
    }

    def test_keeper_master_locations_do_not_exist(self):
        for location in LOCATIONS.keeper_master_locations:
            self.assert_location_does_not_exist(location)

    def test_post_game_locations_do_not_exist(self):
        for location in LOCATIONS.postgame_locations:
            self.assert_location_does_not_exist(location)

    # def test_keeper_master_shop_locations_do_not_exist(self):
    #     for location in LOCATIONS.shopsanity_keeper_master_locations:
    #         self.assert_location_does_not_exist(location)

    # def test_velvet_melody_locations_exist(self):
    #     for location in LOCATIONS.velvet_melody_locations:
    #         self.assert_location_exists(location)


# class TestLocations_MadLord_ShopsIgnoreRank(MonsterSanctuaryTestBase):
#     options = {
#         "goal": 0,
#         "shopsanity": 1,
#         "shops_ignore_rank": 1,
#         "monster_army": 1
#     }
#     run_default_tests = False
#
#     def test_keeper_master_shop_locations_exist(self):
#         for location in LOCATIONS.shopsanity_keeper_master_locations:
#             self.assert_location_exists(location)
