  Design the “A Link to the Past Seed Config” form for SekaiLink Client Core.

  Context:
  This screen lets a player create or edit a saved Seed Config for “A Link to the Past”. A Seed
  Config is stored in the SekaiLink database. Later, when a lobby generates a Sync, the backend
  converts this database config into generator-compatible YAML.

  This is not a raw YAML editor. The user should configure the game through a polished form.

  The form has two modes:
  1. Easy Mode
     - Simple guided questions.
     - Beginner-friendly.
     - Powered by Pulse assistant later.
     - Should not expose every technical option.

  2. Advanced Mode
     - Full APWorld-derived form.
     - Group options exactly like the APWorld:
       - Game Options
       - Cosmetic Options
       - Item & Location Options
     - Tooltips should explain options.
     - Descriptions should be tooltips or expandable help, not huge text blocks in every card.
     - Remove plando options.
     - Entrance Shuffle can appear in Advanced, but mark it experimental/unsupported for Pre-BETA3
     if needed.

  Visual direction:
  - Desktop app form, not a web landing page.
  - Dark UI, teal/cyan accents, readable contrast.
  - Compact, aligned controls.
  - Stable card heights.
  - Search/filter options.
  - Sticky group navigation.
  - Clear Save / Cancel / Reset to Defaults actions.
  - Show validation errors near fields.
  - Avoid overwhelming users with giant cards.

  Control rules:
  - enum/choice: dropdown or segmented control if only a few choices.
  - boolean: toggle.
  - integer/range: slider + numeric input.
  - string/text: text input.
  - text_choice: text input plus preset dropdown.
  - array/set: searchable multi-select token list.
  - object/counter: item picker with quantity controls.
  - object/dict: structured key/value editor, not raw JSON unless expert mode.

  Top fields:
  - Seed Config Name
  - Player Display Name
  - Game: A Link to the Past
  - Status: Valid / Draft / Error
  - Source: Easy / Advanced / Pulse

  Easy Mode should expose:
  - Goal
  - Game Mode
  - Logic / Glitches Required
  - Difficulty preset
  - Keysanity toggle
  - Maps/Compasses toggle
  - Big Keys shuffle
  - Small Keys shuffle
  - Hints
  - Death Link
  - Music
  - Reduce Flashing
  - Quickswap

  Advanced Mode option schema:

  GAME OPTIONS

  Progression Balancing
  key: progression_balancing
  control: range
  default: 50

  Accessibility
  key: accessibility
  control: choice
  default: items
  choices: full, minimal, items

  Goal
  key: goal
  control: choice
  default: ganon
  choices: ganon, crystals, bosses, pedestal, ganon_pedestal, triforce_hunt, local_triforce_hunt,
  ganon_triforce_hunt, local_ganon_triforce_hunt

  Mode
  key: mode
  control: choice
  default: open
  choices: standard, open, inverted

  Glitches Required
  key: glitches_required
  control: choice
  default: no_glitches
  choices: no_glitches, minor_glitches, overworld_glitches, hybrid_major_glitches, no_logic

  Dark Room Logic
  key: dark_room_logic
  control: choice
  default: lamp
  choices: lamp, torches, none

  Open Pyramid Hole
  key: open_pyramid
  control: choice
  default: goal
  choices: closed, open, goal, auto

  Crystals for GT
  key: crystals_needed_for_gt
  control: range
  default: 7

  Crystals for Ganon
  key: crystals_needed_for_ganon
  control: range
  default: 7

  Triforce Pieces Mode
  key: triforce_pieces_mode
  control: choice
  default: available
  choices: extra, percentage, available

  Triforce Pieces Percentage
  key: triforce_pieces_percentage
  control: range
  default: 150

  Triforce Pieces Required
  key: triforce_pieces_required
  control: range
  default: 20

  Triforce Pieces Available
  key: triforce_pieces_available
  control: range
  default: 30

  Triforce Pieces Extra
  key: triforce_pieces_extra
  control: range
  default: 10

  Entrance Shuffle
  key: entrance_shuffle
  control: choice
  default: vanilla
  choices: vanilla, dungeons_simple, dungeons_full, dungeons_crossed, simple, restricted, full,
  crossed, insanity

  Entrance Shuffle Seed
  key: entrance_shuffle_seed
  control: text
  default: random

  Big Key Shuffle
  key: big_key_shuffle
  control: choice
  default: original_dungeon
  choices: original_dungeon, own_dungeons, own_world, any_world, different_world, start_with

  Small Key Shuffle
  key: small_key_shuffle
  control: choice
  default: original_dungeon
  choices: original_dungeon, own_dungeons, own_world, any_world, different_world, start_with,
  universal

  Key Drop Shuffle
  key: key_drop_shuffle
  control: toggle
  default: true

  Compass Shuffle
  key: compass_shuffle
  control: choice
  default: original_dungeon
  choices: original_dungeon, own_dungeons, own_world, any_world, different_world, start_with

  Map Shuffle
  key: map_shuffle
  control: choice
  default: original_dungeon
  choices: original_dungeon, own_dungeons, own_world, any_world, different_world, start_with

  Prevent Dungeon Item on Boss
  key: restrict_dungeon_item_on_boss
  control: toggle
  default: false

  Item Pool
  key: item_pool
  control: choice
  default: normal
  choices: easy, normal, hard, expert

  Item Functionality
  key: item_functionality
  control: choice
  default: normal
  choices: easy, normal, hard, expert

  Enemy Health
  key: enemy_health
  control: choice
  default: default
  choices: easy, default, hard, expert

  Enemy Damage
  key: enemy_damage
  control: choice
  default: default
  choices: default, shuffled, chaos

  Progressive Items
  key: progressive
  control: choice
  default: on
  choices: off, grouped_random, on

  Swordless
  key: swordless
  control: toggle
  default: false

  Dungeon Counters
  key: dungeon_counters
  control: choice
  default: pickup
  choices: on, pickup, default, off

  Retro Bow
  key: retro_bow
  control: toggle
  default: false

  Retro Caves
  key: retro_caves
  control: toggle
  default: false

  Hints
  key: hints
  control: choice
  default: on
  choices: off, on, full

  Scams
  key: scams
  control: choice
  default: off
  choices: off, king_zora, bottle_merchant, all

  Boss Shuffle
  key: boss_shuffle
  control: text_choice
  default: none
  choices: none, basic, full, chaos, singularity

  Pot Shuffle
  key: pot_shuffle
  control: toggle
  default: false

  Enemy Shuffle
  key: enemy_shuffle
  control: toggle
  default: false

  Killable Thieves
  key: killable_thieves
  control: toggle
  default: false

  Bush Shuffle
  key: bush_shuffle
  control: toggle
  default: false

  Available Shop Slots
  key: shop_item_slots
  control: range
  default: 0

  Randomize Shop Inventories
  key: randomize_shop_inventories
  control: choice
  default: default
  choices: default, randomize_by_shop_type, randomize_each

  Shuffle Shop Inventories
  key: shuffle_shop_inventories
  control: toggle
  default: false

  Include Witch’s Hut
  key: include_witch_hut
  control: toggle
  default: false

  Randomize Shop Prices
  key: randomize_shop_prices
  control: toggle
  default: false

  Randomize Cost Types
  key: randomize_cost_types
  control: toggle
  default: false

  Shop Price Modifier
  key: shop_price_modifier
  control: range
  default: 100

  Shuffle Capacity Upgrades
  key: shuffle_capacity_upgrades
  control: choice
  default: off
  choices: off, on, on_combined

  Bombless Start
  key: bombless_start
  control: toggle
  default: false

  Shuffle Prizes
  key: shuffle_prizes
  control: choice
  default: general
  choices: off, general, bonk, both

  Tile Shuffle
  key: tile_shuffle
  control: toggle
  default: false

  Misery Mire Medallion
  key: misery_mire_medallion
  control: choice
  default: random
  choices: ether, bombos, quake

  Turtle Rock Medallion
  key: turtle_rock_medallion
  control: choice
  default: random
  choices: ether, bombos, quake

  Glitched Starting Boots
  key: glitch_boots
  control: toggle
  default: true

  Beemizer Total Chance
  key: beemizer_total_chance
  control: range
  default: 0

  Beemizer Trap Chance
  key: beemizer_trap_chance
  control: range
  default: 60

  Timer
  key: timer
  control: choice
  default: none
  choices: none, timed, timed_ohko, ohko, timed_countdown, display

  Countdown Start Time
  key: countdown_start_time
  control: range
  default: 10

  Red Clock Time
  key: red_clock_time
  control: range
  default: -2

  Blue Clock Time
  key: blue_clock_time
  control: range
  default: 2

  Green Clock Time
  key: green_clock_time
  control: range
  default: 4

  Death Link
  key: death_link
  control: toggle
  default: false

  Allow Collection of checks for other players
  key: allow_collect
  control: toggle
  default: true


  COSMETIC OPTIONS

  Overworld Palette
  key: ow_palettes
  control: choice
  default: default
  choices: default, good, blackout, puke, classic, grayscale, negative, dizzy, sick

  Underworld Palette
  key: uw_palettes
  control: choice
  default: default
  choices: default, good, blackout, puke, classic, grayscale, negative, dizzy, sick

  Menu Palette
  key: hud_palettes
  control: choice
  default: default
  choices: default, good, blackout, puke, classic, grayscale, negative, dizzy, sick

  Sword Palette
  key: sword_palettes
  control: choice
  default: default
  choices: default, good, blackout, puke, classic, grayscale, negative, dizzy, sick

  Shield Palette
  key: shield_palettes
  control: choice
  default: default
  choices: default, good, blackout, puke, classic, grayscale, negative, dizzy, sick

  Heart Beep Rate
  key: heartbeep
  control: choice
  default: normal
  choices: normal, double, half, quarter, off

  Heart Color
  key: heartcolor
  control: choice
  default: red
  choices: red, blue, green, yellow

  L/R Quickswapping
  key: quickswap
  control: toggle
  default: true

  Menu Speed
  key: menuspeed
  control: choice
  default: normal
  choices: normal, instant, double, triple, quadruple, half

  Play Music
  key: music
  control: toggle
  default: true

  Reduce Screen Flashes
  key: reduceflashing
  control: toggle
  default: true

  Display Method for Triforce Hunt
  key: triforcehud
  control: choice
  default: normal
  choices: normal, hide_goal, hide_required, hide_both

  Sprite
  key: sprite
  control: dict / structured sprite selector
  default: { Link: 1 }

  Sprite Pool
  key: sprite_pool
  control: list / multi-select
  default: []

  Random Sprite on Hit
  key: random_sprite_on_hit
  control: toggle
  default: false

  Random Sprite on Enter
  key: random_sprite_on_enter
  control: toggle
  default: false

  Random Sprite on Exit
  key: random_sprite_on_exit
  control: toggle
  default: false

  Random Sprite on Slash
  key: random_sprite_on_slash
  control: toggle
  default: false

  Random Sprite on Item
  key: random_sprite_on_item
  control: toggle
  default: false

  Random Sprite on Bonk
  key: random_sprite_on_bonk
  control: toggle
  default: false

  Random Sprite on Everything
  key: random_sprite_on_everything
  control: toggle
  default: false


  ITEM & LOCATION OPTIONS

  Local Items
  key: local_items
  control: searchable item multi-select
  default: []

  Non-local Items
  key: non_local_items
  control: searchable item multi-select
  default: []

  Start Inventory
  key: start_inventory
  control: item quantity picker
  default: {}

  Start Inventory from Pool
  key: start_inventory_from_pool
  control: item quantity picker
  default: {}

  Start Hints
  key: start_hints
  control: searchable item multi-select
  default: []

  Start Location Hints
  key: start_location_hints
  control: searchable location multi-select
  default: []

  Excluded Locations
  key: exclude_locations
  control: searchable location multi-select
  default: []

  Priority Locations
  key: priority_locations
  control: searchable location multi-select
  default: []

  Item Links
  key: item_links
  control: advanced structured list editor
  default: []

  Save behavior:
  When the user clicks Create Seed Config or Save Seed Config, store the selected values in
  SekaiLink’s database. Do not save raw YAML from the UI. YAML is generated later by the backend
  when a lobby generates a Sync.

  The final design should make this huge APWorld form feel organized, approachable, and powerful
  without looking like a wall of raw settings.
