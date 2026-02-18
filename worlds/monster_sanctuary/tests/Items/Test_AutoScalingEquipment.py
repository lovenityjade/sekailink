from typing import List

from worlds.AutoWorld import call_all
from worlds.monster_sanctuary import items as ITEMS
from worlds.monster_sanctuary.tests import MonsterSanctuaryTestBase

class TestAutoScalingEquipmentHasNoLeveledEquipment(MonsterSanctuaryTestBase):
    run_default_tests = False
    options = {
        "automatically_scale_equipment": 1
    }

    def test_all_leveled_equipment_are_grouped(self):
        for item_name, item_data in ITEMS.item_data.items():
            if '+' in item_name:
                with self.subTest(f"{item_name} is in 'Leveled' group"):
                    self.assertTrue("Leveled" in item_data.groups)
            else:
                with self.subTest(f"{item_name} is not in 'Leveled' group"):
                    self.assertFalse("Leveled" in item_data.groups)

    def test_autoscaling_equipment(self):
        for item in self.multiworld.itempool:
            with self.subTest(f"{item.name} is in the 'Leveled' group"):
                self.assertFalse(ITEMS.is_item_in_group(item.name, "Leveled"))