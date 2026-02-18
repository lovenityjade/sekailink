from typing import List, Optional, Dict

from worlds.AutoWorld import World
from .freeform import FreeFormRandomizer
from .. import encounters as ENCOUNTERS, EncounterData, MonsterData

class LocalShuffleRandomizer(FreeFormRandomizer):
    def randomize(self, world: World) -> None:
        super().randomize(world)

    def randomize_monsters_in_encounter(
            self,
            encounter: EncounterData,
            forced_monster: Optional[MonsterData] = None):
        monster_swap: Dict[str, Optional[MonsterData]] = { monster.name: None for monster in encounter.monsters }

        # If this encounter must include a specific monster, then add it to the swap list
        if forced_monster is not None:
            old_monster = self.world.random.choice(encounter.monsters)
            monster_swap[old_monster.name] = forced_monster
            self.remove_monster_from_available(forced_monster)

        # For every remaining monster to swap, we pick a new, valid, monster
        for old_monster, monster in monster_swap.items():
            # Skip monsters that we've already replaced
            if monster is not None:
                continue
            monster_swap[old_monster] = self.pick_random_monster(encounter)
            self.remove_monster_from_available(monster_swap[old_monster])

        # Finally, swap out the old monsters with the new ones.
        encounter.monsters = [monster_swap[monster.name] for monster in encounter.monsters]