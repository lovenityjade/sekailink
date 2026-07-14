from dataclasses import dataclass

from Options import FreeText, PerGameCommonOptions, Range


class CompensationMode(FreeText):
    default = "locations"


class CompensationDeficits(FreeText):
    default = "{}"


class CompensationLocationCount(Range):
    range_start = 0
    range_end = 2048
    default = 0


class OriginalPlayerCount(Range):
    range_start = 1
    range_end = 255
    default = 1


@dataclass
class CompensatorOptions(PerGameCommonOptions):
    compensation_mode: CompensationMode
    compensation_deficits: CompensationDeficits
    compensation_location_count: CompensationLocationCount
    original_player_count: OriginalPlayerCount
