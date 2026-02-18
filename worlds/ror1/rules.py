from worlds.generic.Rules import set_rule
from BaseClasses import MultiWorld
from .locations import get_locations, map_orderedstages_table
from typing import Set, TYPE_CHECKING

if TYPE_CHECKING:
    from . import RoR1World


# Rule to see if it has access to the previous stage
def has_entrance_access_rule(multiworld: MultiWorld, stage: str, region: str, player: int) -> None:
    rule = lambda state: state.has(region, player) and state.has(stage, player)
    for entrance in multiworld.get_region(region, player).entrances:
        entrance.access_rule = rule

def has_stage_access_rule(multiworld: MultiWorld, stage: str, amount: int, region: str, player: int) -> None:
    rule = lambda state: state.has(region, player) and \
        (state.has(stage, player) or state.count("Progressive Stage", player) >= amount)
    for entrance in multiworld.get_region(region, player).entrances:
        entrance.access_rule = rule

def has_all_items(multiworld: MultiWorld, items: Set[str], region: str, player: int) -> None:
    rule = lambda state: state.has_all(items, player) and state.has(region, player)
    for entrance in multiworld.get_region(region, player).entrances:
        entrance.access_rule = rule


# Checks to see if chest/shrine are accessible
def has_location_access_rule(multiworld: MultiWorld, map: str, player: int, item_number: int, item_type: str)\
        -> None:
    if item_number == 1:
        multiworld.get_location(f"{map}: {item_type} {item_number}", player).access_rule = \
            lambda state: state.has(map, player)
    else:
        multiworld.get_location(f"{map}: {item_type} {item_number}", player).access_rule = \
            lambda state: check_location(state, map, player, item_number, item_type)

def check_location(state, map: str, player: int, item_number: int, item_name: str) -> bool:
    return state.can_reach(f"{map}: {item_name} {item_number - 1}", "Location", player)

def set_rules(self) -> None:
    player = self.player
    multiworld = self.multiworld
    ror_options = self.options
    if ror_options.grouping == "universal":
        # classic mode
        total_locations = ror_options.total_locations.value  # total locations for current player
    else:
        # explore mode
        total_locations = len(
            get_locations(
                chests=ror_options.total_locations.value,
            )
        )

    event_location_step = 25  # set an event location at these locations for "spheres"
    divisions = total_locations // event_location_step

    if ror_options.grouping == "universal":
        # universal mode
        if divisions:
            for i in range(1, divisions + 1):  # since divisions is the floor of total_locations / 25
                if i * event_location_step != total_locations:
                    event_loc = multiworld.get_location(f"Pickup{i * event_location_step}", player)
                    set_rule(event_loc,
                             lambda state, i=i: state.can_reach(f"ItemPickup{i * event_location_step - 1}",
                                                                "Location", player))
                    # we want to create a rule for each of the 25 locations per division
                for n in range(i * event_location_step, (i + 1) * event_location_step + 1):
                    if n > total_locations:
                        break
                    if n == i * event_location_step:
                        set_rule(multiworld.get_location(f"ItemPickup{n}", player),
                                 lambda state, event_item=event_loc.item.name: state.has(event_item, player))
                    else:
                        set_rule(multiworld.get_location(f"ItemPickup{n}", player),
                                 lambda state, n=n: state.can_reach(f"ItemPickup{n - 1}", "Location", player))
        set_rule(multiworld.get_location("Victory", player),
                 lambda state: state.can_reach(f"ItemPickup{total_locations}", "Location", player))

    else:
        # map mode
        chests = ror_options.total_locations

        if ror_options.grouping == "map": # Maps
            for i in range(len(map_orderedstages_table)):
                for map_name, _ in map_orderedstages_table[i].items():
                    # Make sure to go through each location
                    for chest in range(1, chests + 1):
                        has_location_access_rule(multiworld, map_name, player, chest, "Item Pickup")
                    if i > 0:
                        # has_entrance_access_rule(multiworld, f"Stage {i + 1}", map_name, player)
                        has_stage_access_rule(multiworld, f"Stage {i + 1}", i, map_name, player)

        # else ror_options.grouping == "stage": # Stages
        #     for i in range(len(map_orderedstages_table)):
        #         for _ in range(1, total_locations + 1):
        #             # Make sure to go through each location
        #             for chest in range(1, chests + 1):
        #                 has_location_access_rule(multiworld, f"Stage {i + 1}", player, chest, "Item Pickup")
        #             if i > 0:
        #                 has_entrance_access_rule(multiworld, f"Stage {i + 1}", f"OrderedStage_{i}", player)
        #                 get_stage_event(multiworld, player, i)

        has_entrance_access_rule(multiworld, "OrderedStage_6", "Risk of Rain", player)

    # Win Condition
    tele_fragments = min(self.options.required_frags, self.options.available_frags)
    completion_requirements = lambda state: state.has("Teleporter Fragment", player, tele_fragments)
    multiworld.completion_condition[player] = lambda state: completion_requirements(state) and state.has("Victory", player)