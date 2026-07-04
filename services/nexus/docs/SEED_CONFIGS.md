# Nexus Seed Configs

Date: 2026-05-10

## Role

Nexus owns the official online database for user seed configuration.

YAML is no longer the normal user-owned storage format. SekaiLink stores game
settings as validated database records and only uses YAML as an import/export
compatibility format for Archipelago-style tooling.

## Flow

1. Admin/dev imports game option definitions into Nexus.
2. Core asks Nexus for a game's option definitions.
3. Core renders a native settings form.
4. Core lists shared presets for the selected game, or lets the user start from
   defaults/custom values.
5. The user duplicates a shared preset or saves a named config per game.
6. Nexus stores canonical JSON values.
7. Worlds reads immutable config snapshots from Nexus during generation.
8. Core can synchronize a local `.sklconf` SQLite cache for solo offline use.

## Ownership

Nexus owns:

- game catalog records
- option schema versions
- option groups, definitions, and choices
- user configs
- shared game presets
- immutable config snapshots
- seed instances linked to generated runs
- audit of imports, exports, edits, snapshots, sync, and generation attachment

Core owns:

- local `.sklconf` cache creation and conflict UI
- offline solo presentation

Worlds owns:

- generation from a Nexus config snapshot
- seed package creation

## Local `.sklconf`

The `.sklconf` file is a Core-side SQLite cache, not a second source of truth.

The sync payloads must contain enough data for Core to display forms and launch
solo offline for games that were already synchronized:

- games
- option schema versions
- groups, definitions, choices
- shared game presets
- user configs
- config snapshots
- sync cursor/hash metadata

Conflict resolution remains a Core UX concern.

## Current Native Surface

The native C++ module currently provides:

- MariaDB schema SQL via `seed_config_mysql_schema_sql()`
- MariaDB persistence helpers via `SeedConfigMysqlStore`
- seed config API contract handling via `SeedConfigApiService`
- config value validation against option definitions
- strict unknown-option rejection
- shared preset import, listing, and duplication into user configs
- YAML export from canonical values
- `.sklconf` sync manifest payload building

## Imported Supported Catalog

The live Nexus seed-config database is intentionally populated with the current
Sekaiemu support target plus the extra BETA-3 targets, not the full
MultiworldGG catalog.

Current imported scope:

- NES: Final Fantasy, Mega Man 2, Mega Man 3, The Legend of Zelda, Zelda II
- SNES: A Link to the Past, Chrono Trigger Jets of Time, Donkey Kong Country,
  Donkey Kong Country 2, EarthBound, Final Fantasy IV Free Enterprise,
  Final Fantasy Mystic Quest, Final Fantasy V Career Day, Kirby's Dream Land 3,
  Lufia II Ancient Cave, Mega Man X3, Secret of Evermore, Super Mario World,
  Super Metroid, Super Metroid Map Rando, Tetris Attack, Yoshi's Island
- GB/GBC: Links Awakening DX Beta, Pokemon Crystal, Pokemon Red and Blue,
  Super Mario Land 2, Oracle of Ages, Oracle of Seasons, Wario Land
- GBA: Castlevania Circle of the Moon, Final Fantasy Tactics Advance,
  Golden Sun The Lost Age, Mario & Luigi Superstar Saga, Metroid Fusion,
  Metroid Zero Mission, Pokemon Emerald, Pokemon FireRed and LeafGreen,
  The Minish Cap, Wario Land 4, Yu-Gi-Oh! 2006,
  Yu-Gi-Oh! Dungeon Dice Monsters
- GameCube: Luigi's Mansion, Mario Kart Double Dash, Metroid Prime,
  Paper Mario The Thousand-Year Door, Sonic Adventure 2 Battle,
  Sonic Adventure DX, Sonic Heroes, Super Mario Sunshine, The Wind Waker,
  Twilight Princess
- Extra BETA-3 targets: A Link Between Worlds, Symphony of the Night,
  Ship of Harkinian for the SoH target

These records are imported from YAML compatibility templates into Nexus option
definitions. The YAML templates are treated as an import/export bridge only;
Nexus remains the source of truth for user-facing saved settings.

Ship of Harkinian is imported from the local `oot_soh.apworld` option classes,
not from the generic Ocarina of Time template. The `Jade-SoH.yaml` profile is a
user config example/source profile, while the APWorld is the schema authority.

Cycle 1 SoH server-first handoff:

- `deploy/nexus/seed-configs/examples/jade-soh.worlds-handoff.json` records the local Nexus-style handoff.
- `sekailink-linkedworld-soh/presets/jade-soh.config-snapshot.json` is the resolved immutable config snapshot used by Worlds.
- `config_version_id=7001` is reserved for the local proof fixture.
- The live Nexus database is not mutated by this fixture unless an operator imports it explicitly.

## API Contract

Current native routes are transport-neutral and return JSON payloads with
`ok/status/error` semantics:

- `POST /admin/seed-configs/games`
- `POST /admin/seed-configs/games/{game_key}/presets`
- `GET /admin/seed-configs/games/{game_key}/presets`
- `PUT /admin/seed-configs/games/{game_key}/presets/{preset_key}`
- `DELETE /admin/seed-configs/games/{game_key}/presets/{preset_key}`
- `GET /seed-configs/games`
- `GET /seed-configs/games/{game_key}/options`
- `GET /seed-configs/games/{game_key}/presets`
- `POST /users/{user_id}/seed-configs`
- `POST /users/{user_id}/seed-configs/from-preset`
- `GET /users/{user_id}/seed-configs?game_key={game_key}`
- `POST /users/{user_id}/seed-configs/{config_id}/export-yaml`
- `GET /users/{user_id}/sklconf/manifest`

The loopback HTTP daemon is available as `sekailink_seed_config_api_service`.
When `mysql.enabled` is true in config, the daemon persists game definitions,
shared presets, user configs, config versions, YAML export data, and
`.sklconf` manifests through `SeedConfigMysqlStore`.

## Shared Presets

Shared presets are the common database layer for `Game -> Preset -> Values`.
The selected game's option schema remains the form authority; a preset stores a
validated canonical `values_json` document for that schema. Core should render
the game form from `GET /seed-configs/games/{game_key}/options`, then fill it
from a preset selected through `GET /seed-configs/games/{game_key}/presets`.

When a user chooses a shared preset, Core calls
`POST /users/{user_id}/seed-configs/from-preset`. Nexus copies the preset into
`user_game_configs` and `user_game_config_versions`; from that point on, the
config is personal and editable by the user.

Admin preset deletion is a hard delete intended for the private preset authoring
tool. Do not expose it to public clients.
