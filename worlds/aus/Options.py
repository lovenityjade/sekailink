from dataclasses import dataclass
from Options import Choice, Option, Toggle, Range, PerGameCommonOptions

class IsCool(Toggle):
    """Determines whether the user is cool and checked this option. [Testing only, no gameplay effect.]"""
    display_name = "Is Cool"

@dataclass
class AUSOptions(PerGameCommonOptions):
  is_cool: IsCool