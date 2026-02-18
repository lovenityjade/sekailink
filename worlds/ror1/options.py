from dataclasses import dataclass
from Options import Toggle, DefaultOnToggle, DeathLink, Range, Choice, PerGameCommonOptions

class Grouping(Choice):
    """
    Universal: Every Item pickup increases fills a progress bar which gives location checks.

    Stages (NYI): Each stage will have location checks within each map variant on a given stage.
    Stages will be locked in the item pool until received.

    Maps: Each map will have location checks within each map on said stage.
    Both maps and stages will be locked in the item pool until received. asdkfjas;dlkf
    """
    display_name = "Location Grouping"
    option_universal = 0
    # option_stage = 1
    option_map = 2
    default = 2

class TotalLocations(Range):
    """
    Number of location checks which are added to the Risk of Rain playthrough.
    On stage or map grouping, will determine how many locations checks are added per stage/map.
    """
    display_name = "Total Locations"
    range_start = 10
    range_end = 200
    default = 20

class ProgressiveStage(DefaultOnToggle):
    """
    Changes stage items to be progressive.
    """
    display_name = "Progressive Stages"

class RequireStage(DefaultOnToggle):
    """
    Add Stage items to the pool that blocks maps of the type
    Does nothing unless on map grouping
    """
    display_name = "Require Stage"

class StrictStageProg(Toggle):
    """
    Require both the prior stage and any corresponding map before having access to later stages
    Ex. You won't be able to access Stage 3 until you have Stage 2 and either Stage 2 map
    """
    display_name = "Strict Map Requirements"

class StageFiveTP(Toggle):
    """
    Only allow access to teleport to final stage if on stage 5, like RoR2
    """
    display_name = "Divine Teleporter on Stage 5"

class RequiredFrags(Range):
    """
    Percentage of teleporter fragments required to access the final stage
    """
    display_name = "Teleporter Fragments Required"
    range_start = 0
    range_end = 100
    default = 0

class AvailableFrags(Range):
    """
    Percentage of filler items to be replaced with teleporter fragments
    """
    display_name = "Teleporter Fragments Available"
    range_start = 0
    range_end = 100
    default = 0

class ItemPickupStep(Range):
    """
    Number of items to pick up before an AP Check is completed.
    Setting to 1 means every other pickup.
    Setting to 2 means every third pickup. So on...
    """
    display_name = "Item Pickup Step"
    range_start = 0
    range_end = 5
    default = 1

class AllowTrapItems(Toggle):
    """Allows Trap items in the item pool."""
    display_name = "Enable Trap Items"

class CommonItem(Range):
    """Weight of common items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Common Items"
    range_start = 0
    range_end = 100
    default = 64

class UncommonItem(Range):
    """Weight of uncommon items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Uncommon Items"
    range_start = 0
    range_end = 100
    default = 32

class LegendaryItem(Range):
    """Weight of legendary items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Legendary Items"
    range_start = 0
    range_end = 100
    default = 8

class BossItem(Range):
    """Weight of boss items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Boss Items"
    range_start = 0
    range_end = 100
    default = 4

class Equipment(Range):
    """Weight of equipment items in the item pool.

     (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Equipment"
    range_start = 0
    range_end = 100
    default = 32

class Money(Range):
    """Weight of money items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Money"
    range_start = 0
    range_end = 100
    default = 64

class Experience(Range):
    """Weight of 1000 exp items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "1000 Exp"
    range_start = 0
    range_end = 100
    default = 40

class TimeWarpTrap(Range):
    """Weight of time warp trap items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Time Warp Trap"
    range_start = 0
    range_end = 100
    default = 20

class CombatTrap(Range):
    """Weight of combat trap items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Combat Trap"
    range_start = 0
    range_end = 100
    default = 20

class MeteorTrap(Range):
    """Weight of meteor trap items in the item pool.

    (Ignored unless Item Weight Presets is 'No')"""
    display_name = "Meteor Trap"
    range_start = 0
    range_end = 100
    default = 20

@dataclass
class ROROptions(PerGameCommonOptions):
    grouping: Grouping
    total_locations: TotalLocations
    progressive_stages: ProgressiveStage
    require_stage: RequireStage
    stage_five_tp: StageFiveTP
    strict_stage_prog: StrictStageProg
    required_frags: RequiredFrags
    available_frags: AvailableFrags
    item_pickup_step: ItemPickupStep
    enable_trap: AllowTrapItems
    common_item: CommonItem
    uncommon_item: UncommonItem
    legendary_item: LegendaryItem
    boss_item: BossItem
    equipment: Equipment
    money: Money
    experience: Experience
    time_warp_trap: TimeWarpTrap
    combat_trap: CombatTrap
    meteor_trap: MeteorTrap