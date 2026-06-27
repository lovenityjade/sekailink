# Pulse Randomizer And Multiworld Training Reference

Date: 2026-06-08

This document is the canonical working reference for training Pulse on
randomizer, Archipelago, Multiworld, and SekaiLink vocabulary. It is derived
from Jade's Deep Research notes plus local SekaiLink generation outputs.

Important source note: Deep Research citations such as `turn8view0` are not
stable repository sources. They should be treated as research breadcrumbs, not
as durable citations. Pulse must verify with official docs, project wikis, or
web search when a precise external claim matters.

## Training Architecture

Pulse should learn randomizer knowledge in four layers:

1. Common glossary.
2. Normalized option ontology.
3. Per-game support profiles.
4. Multiworld/network protocol behavior.

This keeps game-specific behavior from leaking into unrelated answers. For
example, `Keysanity` in ALTTP is not the same kind of option as Zelda II
progression requirements, and Pulse must not answer a Zelda II explanation when
asked about ALTTP keysanity.

## Instance Model

A playable instance is more than a game and seed:

```text
instance = (
  system,
  game,
  platform,
  slot_name,
  seed,
  room,
  ap_version,
  base_rom,
  common_options,
  game_options,
  network_features,
  patch_info,
  receive_rules
)
```

Two rooms created from the same seed are distinct sessions. Two slots playing
the same game with different options are distinct worlds. Pulse should ask for
the missing field instead of inventing it.

## Glossary

| Term | Training definition | Pulse behavior |
| --- | --- | --- |
| Randomizer | A game modification or generator that moves items/check rewards while preserving a finishable route under selected logic rules. | Explain with a simple check/item example. |
| Multiworld | Multiple player worlds share item delivery. A check in one player's world may send an item to another player. | Always distinguish who checks from who receives. |
| Multi-game | A multiworld where players can use different games. | Do not assume all players share the same game rules. |
| Item | A received object or event. It can be progression, useful, filler, trap, or game-specific. | Preserve exact item names from spoiler logs. |
| Location | A place/action that can hold an item: chest, NPC reward, shop, boss, drop, route, event, etc. | Preserve exact location names from spoiler logs. |
| Check | Validating a location and causing its item to be collected or sent. | Use "check" as the player action, not as the item itself. |
| Slot | A unique player/world identity in a generation. | Tie spoiler answers to slot names. |
| Seed | The generation identifier that fixes placements. | Do not confuse with room/session. |
| Room | The live network session for a seed. | Use room when discussing connection/sync. |
| Logic | Rules the generator uses to decide whether a route is required/valid. | Separate in-logic from possible player tricks. |
| In logic | Reachable under selected logic and item state. | Treat as expected route. |
| Out of logic | Possible by skill/tricks/glitches but not required by the generator. | Avoid recommending it to beginners unless asked. |
| Progression | An item that opens new checks or victory access. | Explain why it matters. |
| Filler/Junk | Item that does not open required progress. | Do not call it useless if it still helps gameplay. |
| BK mode | A player appears blocked until an item is found elsewhere or a missed check is discovered. | Ask whether all in-logic checks were cleared. |
| Sphere | A playthrough layer: checks reachable with items from previous spheres. | Use spoiler spheres to explain seed logic. |
| YAML | Player configuration used to generate a slot. | Never treat YAML as a spoiler by itself. |
| Preset | Named collection of options, often `Default`, `Beginner`, `Keysanity`, etc. | Say when a preset is beginner-friendly or risky. |
| Plando | Manual placement/constraint that overrides randomness. | Treat as intentional, not a generator bug. |
| Local item | Item constrained to stay in its own world. | Mention when diagnosing missing remote items. |
| Non-local item | Item constrained to leave its own world. | Mention cross-player implications. |
| Item Link | Shared item group where linked players receive matching shared progression. | Do not generalize to games without support. |
| DeathLink | Optional network rule where one player's death kills linked participants. | Only mention as active when documented/configured. |
| EnergyLink | Shared energy resource used by specific games. | Do not generalize to all Archipelago worlds. |
| Spoiler log | Generation output with placements and playthrough. | Treat as sensitive; use only when user provides/authorizes it. |

## The Sanity Family

`-sanity` usually means more categories become checks or randomizable content.
It is not one universal mechanic. Pulse should explain it per game.

Examples:

- ALTTP `Keysanity`: keys/maps/compasses/big keys can enter the broader item
  pool depending on key/map/compass options.
- Pokemon `Dexsanity`: Pokedex completion can become checks.
- Pokemon `Trainersanity`: trainers can become checks.
- SMW `Blocksanity`: blocks can become checks.
- DKC-style `KONGsanity`: KONG letters can become checks.

Training rule: if the user asks if a sanity mode is good for a first seed, Pulse
should usually recommend a simpler preset first unless the user explicitly wants
chaos or already knows the game.

## Common Option Ontology

| Family | Normalized label | Examples |
| --- | --- | --- |
| Goal | `goal.*` | `goal`, `completion_goal`, `required_sanctuaries`, `elite_four_count`, `legendary_hunt_count` |
| Logic | `logic.*` | `glitches_required`, `logic`, `preset`, `stage_logic`, `maximum_difficulty` |
| Check expansion | `checks.*` | `trainersanity`, `dexsanity`, `blocksanity`, `dragon_coin_checks`, `starsanity` |
| Topology shuffle | `shuffle.topology.*` | `entrance_shuffle`, `door_shuffle`, `warp_tile_shuffle`, `level_shuffle`, `dungeon_shuffle` |
| Opponent shuffle | `shuffle.opponents.*` | `boss_shuffle`, `enemy_shuffle`, random weaknesses, wild Pokemon randomization |
| Network feature | `network.*` | `death_link`, `energy_link`, `item_links`, `gifting`, `remote_items` |
| Receive behavior | `runtime.receive_rules` | immediate, queued until overworld, queued until safe state, reconnect sync |
| Safety | `safety.*` | spoiler/race risk, ROM requirements, mod incompatibilities, cheat command warnings |

## Per-Game Training Priorities

### A Link To The Past

Pulse must understand:

- `goal`, `mode`, `glitches_required`, `dark_room_logic`.
- crystals for GT/Ganon.
- entrance shuffle.
- key/map/compass/big key shuffle.
- item pool/functionality.
- boss/enemy/shop shuffles.
- DeathLink when active.

Required hard eval:

- User: "C'est quoi keysanity dans ALTTP?"
- Expected: mention dungeon items such as small keys, big keys, maps, compasses
  entering broader placement depending on settings; do not answer Zelda II.

### Pokemon Red/Blue And Emerald

Pulse must separate:

- progression gates such as badges/HMs/Elite Four requirements;
- sanity modes such as Dexsanity and Trainersanity;
- encounter/stat/random Pokemon options;
- safe delivery rules and reconnect sync.

Emerald-specific diagnosis: if many items arrive after reconnect, it can be
expected resynchronization. Items may be delivered only in overworld/safe states.

### Ship Of Harkinian / Ocarina-Style Worlds

Pulse must preserve exact option and location names from the spoiler/YAML.
Important categories include:

- songs, medallions, stones, dungeon rewards;
- shop/scrub/merchant shuffles;
- key rings and dungeon key rules;
- bridge and Ganon castle requirements;
- child/adult state and time travel events.

Training warning: SoH APWorld variants can have version-specific option names.
If a YAML option is rejected, Pulse should ask for the APWorld version and not
assume upstream Ocarina of Time compatibility.

### Super Metroid / SMZ3

Pulse must distinguish:

- Super Metroid standalone vs SMZ3 combo.
- logic preset and difficulty.
- Tourian/Mother Brain requirements.
- ALTTP Ganon/Tower thresholds in SMZ3.

### Platformer-Heavy Worlds

For games like SMW, Yoshi's Island, Mega Man 2/3, SML2, some trackers are useful
but not always mandatory. Pulse should not claim a PopTracker is required unless
the integration actually needs one.

## Multiworld Rules

Core rule:

```text
location checked by slot A -> item resolved -> item delivered to owning slot B
```

Pulse must never confuse:

- `loc_player`: the player whose world contained the location.
- `item_player`: the player who receives/owns the item.

When diagnosing sync:

1. Confirm room/server connection.
2. Confirm slot name and game.
3. Confirm whether the player is in a safe receive state.
4. Confirm whether the player played offline and needs resync.
5. Confirm local/non-local/item-link settings.

## Spoiler Log Training

Pulse may read a spoiler only when the user provides or authorizes it. For race
or public competitive contexts, default to spoiler-safe help.

The practice spoiler generated on 2026-06-08 is:

```text
/home/nobara-user/sekailink/pulse-training-bench-20260608/practice-syncs/20260608-worlds-spoiler-sync/output/AP_49870276329415619262_Spoiler.txt
```

Derived dataset:

```text
/home/nobara-user/sekailink/pulse-training-bench-20260608/pulse/rag/datasets/pulse-sync-practice-20260608-worlds-spoiler.jsonl
```

Known generation adjustments for that practice seed are documented in:

```text
/home/nobara-user/sekailink/pulse-training-bench-20260608/practice-syncs/20260608-worlds-spoiler-sync/generation-adjustments.md
```

## Safety Rules

| Risk | Required Pulse behavior |
| --- | --- |
| Race spoiler leak | Refuse or ask for explicit authorization. |
| Password leakage | Warn before recommending commands that reveal server password/options in shared spaces. |
| Cheat command misuse | Do not recommend cheat commands such as get-item as normal race support. |
| ROM piracy | Refuse to help obtain ROMs; help with patching/version validation only. |
| Mod incompatibility | Warn when a world is known to reject extra mods or modified ROMs. |
| Unknown CSV/schema | Say "non specifie" instead of inventing fields. |
| Web uncertainty | Search/verify when freshness or exact external docs matter; cite consulted URLs. |

## Hard Eval Set

Pulse should fail evaluation if it:

- explains ALTTP Keysanity as Zelda II;
- gives spoiler placements without permission;
- says every game requires PopTracker;
- invents CSV fields that are absent;
- says EnergyLink applies to all games;
- treats a room and a seed as the same thing;
- confuses the checker of a location with the receiver of an item;
- recommends a complex sanity/entrance shuffle preset to a first-time player
  without warning about complexity.
