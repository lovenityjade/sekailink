import unittest
from argparse import Namespace
from typing import List

from BaseClasses import MultiWorld, CollectionState, ItemClassification
from worlds import AutoWorldRegister
from worlds.monster_sanctuary import locations as LOCATIONS


class TestArea(unittest.TestCase):
    options = {}

    # Test structure originally copied from ALttP dungeon tests
    def world_setup(self):
        self.multiworld = MultiWorld(1)
        self.multiworld.game = {1: "Monster Sanctuary"}
        self.multiworld.set_seed(None)

        args = Namespace()
        for name, option in AutoWorldRegister.world_types["Monster Sanctuary"].options_dataclass.type_hints.items():
            value = option.from_any(getattr(option, "default"))
            if name in self.options:
                value = self.options[name]

            setattr(args, name, {1: value})
        self.multiworld.set_options(args)

    def setUp(self):
        self.world_setup()
        self.starting_regions = []  # Where to start exploring

        self.multiworld.worlds[1].generate_early()
        self.multiworld.worlds[1].create_regions()
        self.multiworld.worlds[1].generate_basic()

        self.multiworld.get_region('Menu', 1).exits = []

        self.multiworld.worlds[1].set_rules()
        self.multiworld.worlds[1].create_items()

        self.multiworld.state = CollectionState(self.multiworld)


    def set_option(self, option: str, value):
        opt = self.multiworld.worlds[1].options
        setattr(opt, option, value)

    def can_access(self, start: str, end: str, items: List[str]) -> bool:
        for i in range(len(items)):
            items[i] = self.multiworld.create_item(items[i], 1)

        state = CollectionState(self.multiworld)
        state.reachable_regions[1].add(
            self.multiworld.get_region('Menu', 1)
        )

        region = self.multiworld.get_region(start, 1)
        state.reachable_regions[1].add(region)
        for region_exit in region.exits:
            if region_exit.connected_region is not None:
                state.blocked_connections[1].add(region_exit)

        for item in items:
            item.classification = ItemClassification.progression
            # Don't want to use state.collect() because that sweeps for all other checks
            # that follows, and we don't want to do that. We only want to use the items we
            # define the test to use
            state.prog_items[1][item.name] += 1

        location_name = end
        if location_name in LOCATIONS.location_data:
            location_name = LOCATIONS.location_data[end].name

        location = self.multiworld.get_location(location_name, 1)
        return location.can_reach(state)

    def assertAccessible(self, start: str, end: str, items: List[str]):
        with self.subTest("Can access location from region",
                          starting_region=start, end_location=end, items=', '.join(items)):
            self.assertEqual(self.can_access(start, end, items), True)

    def assertNotAccessible(self, start: str, end: str, items: List[str]):
        with self.subTest("Can't access location from region",
                          starting_region=start, end_location=end, items=', '.join(items)):
            self.assertEqual(self.can_access(start, end, items), False)
