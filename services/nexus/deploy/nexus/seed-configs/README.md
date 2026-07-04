# Nexus Seed Configs Deployment

This palier is MariaDB-only. Do not extend the legacy SQLite identity store for
seed configuration data.

## Schema

Build the native utility and dump the schema:

```sh
cmake --build server/native/sekailink_server_core/build --target sekailink_seed_config_schema_dump
server/native/sekailink_server_core/build/sekailink_seed_config_schema_dump > seed-configs.sql
```

Apply the SQL to the Nexus MariaDB database with the production credentials from
the local server notes.

## API Service

Paths:

- binary: `/opt/sekailink/nexus/seed-configs/bin/sekailink_seed_config_api_service`
- config: `/opt/sekailink/nexus/seed-configs/config/seed_config_api.json`
- state: `/opt/sekailink/nexus/seed-configs/state/`
- unit: `sekailink-nexus-seed-config-api.service`

Routes:

- `GET /seed-configs/health`
- `POST /admin/seed-configs/games`
- `POST /admin/seed-configs/games/{game_key}/presets`
- `GET /seed-configs/games`
- `GET /seed-configs/games/{game_key}/options`
- `GET /seed-configs/games/{game_key}/presets`
- `POST /users/{user_id}/seed-configs`
- `POST /users/{user_id}/seed-configs/from-preset`
- `GET /users/{user_id}/seed-configs?game_key={game_key}`
- `POST /users/{user_id}/seed-configs/{config_id}/export-yaml`
- `GET /users/{user_id}/sklconf/manifest`

The service is loopback/private by default. Public access should go through the
SekaiLink gateway/auth layer, not by exposing this port directly.

## Live Status

Deployed on Nexus:

- service: `sekailink-nexus-seed-config-api.service`
- loopback health: `http://127.0.0.1:19106/seed-configs/health`
- database: `sekailink_seed_configs`
- persistence: enabled through MariaDB
- imported catalog: 54 supported BETA-3 games with platform-normalized
  `system_key` values
- SoH catalog entry: `ship_of_harkinian`, imported from `oot_soh.apworld`
- cycle 1 import example: `examples/ship_of_harkinian.import.json`
- cycle 1 Worlds handoff fixture:
  `examples/jade-soh.worlds-handoff.json`

Validated with a temporary HTTP import/save/restart/read cycle, then the smoke
records were removed from MariaDB.

The platform normalization SQL is kept in
`deploy/nexus/seed-configs/supported_catalog_platforms.sql`.
The old `ocarina_of_time` SoH fallback cleanup is kept in
`deploy/nexus/seed-configs/remove_oot_soh_fallback.sql`.

## Ownership

Nexus is the online source of truth for:

- game records
- option schema versions
- shared game presets
- user seed configs
- immutable config versions used by Worlds
- generated seed instances
- audit events

Core owns the offline `.sklconf` SQLite cache and synchronizes it from Nexus.

## Cycle 1 SoH Handoff

For the local server-first proof, Nexus is represented by an immutable config
snapshot fixture instead of mutating the live database again.

Handoff chain:

- Nexus fixture: `examples/jade-soh.worlds-handoff.json`
- resolved snapshot: `clean-room/repos/sekailink-linkedworld-soh/presets/jade-soh.config-snapshot.json`
- Worlds slot input: `slot_id=1`, `user_id=1001`, `config_version_id=7001`
- Worlds output: `link_room_seed_contract.json`

This keeps the cycle honest: the live DB model is still the target, but the
validated local proof does not depend on live Nexus state.
