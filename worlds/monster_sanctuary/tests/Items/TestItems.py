from typing import List

from BaseClasses import ItemClassification
from worlds.AutoWorld import call_all
from worlds.monster_sanctuary import items, locations, MonsterSanctuaryItem, MonsterSanctuaryItemCategory
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase


class TestItems(MonsterSanctuaryTestBase):
    def test_all_default_items_exist(self):
        for location_name in locations.location_data:
            data = locations.location_data[location_name]
            item = items.item_data.get(data.default_item)
            if item is None:
                print(f"{data.default_item} was not found")

            with self.subTest(f"Item exists", item=data.default_item):
                self.assertIsNotNone(item)

    def test_get_item_type(self):
        self.assertTrue(items.is_item_type("Champion Defeated", MonsterSanctuaryItemCategory.RANK))
        self.assertFalse(items.is_item_type("Feather",
                                            MonsterSanctuaryItemCategory.RANK))

    def test_multi_items_are_tagged_properly(self):
        for key, item_data in items.item_data.items():
            multiplier = item_data.name.split(' ')[0]
            if multiplier in ["2x", "3x", "4x", "5x"]:
                with self.subTest(f"{item_data.name} has the Multiple tag"):
                    self.assertIn("Multiple", item_data.groups)

    def test_key_items_appear_correct_number_of_times(self):
        if not self.options.__contains__("include_looters_handbook"):
            self.options["include_looters_handbook"] = 1
        key_items = [items.item_data[item_name] for item_name in items.item_data
                     if items.item_data[item_name].category == MonsterSanctuaryItemCategory.KEYITEM
                     and (self.options["include_looters_handbook"] == 1 and item_name == "Looter's Handbook")]

        for key_item in key_items:
            exptected_count = key_item.count
            # if we're removing key doors, then ignore area key items
            if "Area Key" in key_item.groups:
                if self.multiworld.worlds[1].options.remove_locked_doors == "minimal":
                    if key_item.name == "Ancient Woods key":
                        exptected_count = 2
                    elif key_item.name == "Mystical Workshop key":
                        exptected_count = 3
                    else:
                        exptected_count = 1
                elif self.multiworld.worlds[1].options.remove_locked_doors == "all":
                    continue

            item_pool_items = [item for item in self.multiworld.itempool
                               if item.name == key_item.name]
            with self.subTest(f"{key_item.name} appears {exptected_count} time(s)"):
                self.assertEqual(exptected_count, len(item_pool_items), key_item.name)


    def test_items_have_correct_groups(self):
        def get_category_string(cat: MonsterSanctuaryItemCategory) -> str:
            if cat == 0:
                return "Key Item"
            elif cat == 1:
                return "Crafting Material"
            elif cat == 2:
                return "Consumable"
            elif cat == 3:
                return "Food"
            elif cat == 4:
                return "Catalyst"
            elif cat == 5:
                return "Weapon"
            elif cat == 6:
                return "Accessory"
            elif cat == 7:
                return "Currency"
            elif cat == 8:
                return "Egg"
            elif cat == 9:
                return "Costume"
            elif cat == 10:
                return "Rank"
            elif cat == 11:
                return "Explore Ability"
            return ""

        def get_classification_string(classification: ItemClassification):
            if classification == 0b0000:
                return "Filler"
            elif classification == 0b0001:
                return "Progression"
            elif classification == 0b0010:
                return "Useful"
            elif classification == 0b0100:
                return "Trap"
            return ""

        for item_name, item in items.item_data.items():
            category_text = get_category_string(item.category)
            with self.subTest(f"{item_name} has the group '{category_text}'"):
                self.assertIn(category_text, item.groups)

            classification_text = get_classification_string(item.classification)
            with self.subTest(f"{item_name} has the classification '{classification_text}'"):
                self.assertIn(classification_text, item.groups)



class TestLockedDoors(MonsterSanctuaryTestBase):
    def test_no_items_placed_where_they_should_not_be(self):
        from Fill import distribute_items_restrictive

        distribute_items_restrictive(self.multiworld)
        call_all(self.multiworld, "post_fill")

        for location in self.multiworld.get_filled_locations(1):
            item_data = items.get_item_by_name(location.item.name)

            # This ignores events
            if item_data is None:
                continue

            # Only care about items with placement limitations
            if len(item_data.illegal_locations) == 0:
                continue

            with self.subTest("This item is allowed to be at this location", item=item_data.name, location=location.name):
                for illegal_location in item_data.illegal_locations:
                    self.assertFalse(location.name.startswith(illegal_location))


class TestRemoveLockedDoors_Minimal(TestItems):
    options = {
        "remove_locked_doors": 1
    }


class TestRemoveLockedDoors_All(TestItems):
    options = {
        "remove_locked_doors": 2
    }


class TestDefaultItemProbability(MonsterSanctuaryTestBase):
    def test_default_probability(self):
        self.assertEqual(len(items.item_drop_probabilities), 354)

    def test_no_key_items_generate(self):
        itempool: List[MonsterSanctuaryItem] = []
        item_exclusions = ["Multiple"]
        for i in range(1000):
            item_name = items.get_random_item_name(self.multiworld.worlds[1], itempool, group_exclude=item_exclusions)
            item = items.MonsterSanctuaryItem(
                self.player, items.item_data[item_name].id, item_name, items.item_data[item_name].classification)
            itempool.append(item)

        key_items = [item for item in itempool if items.item_data[item.name] == MonsterSanctuaryItemCategory.KEYITEM]
        self.assertEqual(0, len(key_items))
        rank_items = [item for item in itempool if items.item_data[item.name] == MonsterSanctuaryItemCategory.RANK]
        self.assertEqual(0, len(rank_items))


class TestMinimumItemProbability(MonsterSanctuaryTestBase):
    options = {
        "drop_chance_craftingmaterial": 1,
        "drop_chance_consumable": 1,
        "drop_chance_food": 1,
        "drop_chance_catalyst": 1,
        "drop_chance_weapon": 1,
        "drop_chance_accessory": 1,
        "drop_chance_currency": 1,
    }

    def test_minimum_probability(self):
        self.assertEqual(71, len(items.item_drop_probabilities))


class TestMaximumItemProbability(MonsterSanctuaryTestBase):
    options = {
        "drop_chance_craftingmaterial": 100,
        "drop_chance_consumable": 100,
        "drop_chance_food": 100,
        "drop_chance_catalyst": 100,
        "drop_chance_weapon": 100,
        "drop_chance_accessory": 100,
        "drop_chance_currency": 100,
    }

    def test_maximum_probability(self):
        self.assertEqual(707, len(items.item_drop_probabilities))


class TestLootersHandbook_Enabled(TestItems):
    options = {
        "include_looters_handbook": 1
    }

    def test_handbook_exists(self):
        self.assertTrue(any(item.name == "Looter's Handbook" for item in self.multiworld.itempool))


class TestLootersHandbook_Disabled(TestItems):
    options = {
        "include_looters_handbook": 0
    }

    def test_handbook_exists(self):
        self.assertFalse(any(item.name == "Looter's Handbook" for item in self.multiworld.itempool))

