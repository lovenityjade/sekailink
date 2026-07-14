import json
from pathlib import Path

from BaseClasses import Item, ItemClassification, Location, MultiWorld, Region
from worlds.AutoWorld import WebWorld, World

from .Options import CompensatorOptions


LOCATION_ID_BASE = 9_950_000
MAX_COMPENSATION = 2048


class CompensatorWeb(WebWorld):
    options_page = False


class SekaiLinkCompensatorWorld(World):
    game = "SekaiLink Compensator"
    author = "SekaiLink"
    hidden = True
    topology_present = False
    options_dataclass = CompensatorOptions
    options: CompensatorOptions
    web = CompensatorWeb()

    item_name_to_id = {}
    location_name_to_id = {
        f"Compensation {index:04d}": LOCATION_ID_BASE + index
        for index in range(1, MAX_COMPENSATION + 1)
    }

    def create_regions(self) -> None:
        menu = Region("Compensator", self.player, self.multiworld)
        count = int(self.options.compensation_location_count.value)
        for index in range(1, count + 1):
            name = f"Compensation {index:04d}"
            menu.locations.append(Location(self.player, name, self.location_name_to_id[name], menu))
        self.multiworld.regions.append(menu)

    def create_item(self, name: str) -> Item:
        return Item(name, ItemClassification.filler, None, self.player)

    def create_items(self) -> None:
        # Compensation items belong to real game slots and are added by stage_create_items.
        return

    @classmethod
    def stage_create_items(cls, multiworld: MultiWorld) -> None:
        players = multiworld.get_game_players(cls.game)
        if not players:
            return
        world = multiworld.worlds[players[0]]
        if str(world.options.compensation_mode.value) != "fillers":
            return
        try:
            deficits = json.loads(str(world.options.compensation_deficits.value))
        except (TypeError, ValueError, json.JSONDecodeError) as exc:
            raise ValueError("invalid_compensator_deficits") from exc
        for raw_player, raw_count in deficits.items():
            player = int(raw_player)
            count = int(raw_count)
            if player == world.player or player not in multiworld.worlds or count < 0:
                raise ValueError("invalid_compensator_target")
            if count > MAX_COMPENSATION:
                raise ValueError("compensator_limit_exceeded")
            target_world = multiworld.worlds[player]
            multiworld.itempool.extend(target_world.create_filler() for _ in range(count))

    def set_rules(self) -> None:
        self.multiworld.completion_condition[self.player] = lambda _state: True

    def pre_fill(self) -> None:
        # Locality rules run after set_rules. Compensator locations intentionally accept
        # every real-game item because their purpose is to repair a capacity deficit.
        for location in self.multiworld.get_locations(self.player):
            location.item_rule = lambda _item: True

    def fill_slot_data(self) -> dict:
        return {
            "sekailink_compensator": True,
            "mode": str(self.options.compensation_mode.value),
            "original_player_count": int(self.options.original_player_count.value),
            "location_count": int(self.options.compensation_location_count.value),
        }

    def generate_output(self, output_directory: str) -> None:
        mode = str(self.options.compensation_mode.value)
        deficits = json.loads(str(self.options.compensation_deficits.value))
        amount = (int(self.options.compensation_location_count.value)
                  if mode == "locations" else sum(int(value) for value in deficits.values()))
        payload = {
            "schema_version": "sekailink-compensator-v1",
            "active": True,
            "slot": self.player,
            "slot_name": self.multiworld.player_name[self.player],
            "display_name": self.multiworld.player_name[self.player],
            "reason": "item_location_deficit" if mode == "locations" else "filler_item_deficit",
            "mode": mode,
            "amount": amount,
            "deficits": deficits,
            "attempts": 2,
            "original_player_count": int(self.options.original_player_count.value),
            "release_state": "scheduled",
        }
        Path(output_directory, "sekailink_compensator.json").write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
