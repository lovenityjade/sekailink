# Changelog

## 2026-05-30 - Shared seed config presets

- Added a generic Nexus shared preset layer for `Game -> Preset -> Values`,
  backed by `common_game_presets` and `common_game_preset_versions`.
- Added API routes for admin preset import, user preset listing, and copying a
  shared preset into a user's editable game config.
- Updated `.sklconf` manifests to include shared preset entries for offline
  Core sync.

## 2026-05-18 - SoH server-first handoff fixture

- Added a Nexus-to-Worlds handoff fixture for the cycle 1 SoH server-first vertical.
- Pinned `Jade-SoH` as config version `7001` for the local package-generation proof.
- Documented that Worlds receives resolved immutable `config_snapshot` JSON, while the fixture only records the source handoff.

## 2026-05-10 - Nexus seed configs foundation

- Added the MariaDB schema contract for game option definitions, named user configs, immutable config versions, generated seed instances, and audit events.
- Added `SeedConfigMysqlStore` for schema initialization and core MariaDB persistence operations.
- Added native validation for canonical seed config values, including strict unknown-option rejection and default handling.
- Added a transport-neutral seed config API contract for game imports, option reads, user config saves, YAML export, and `.sklconf` manifest reads.
- Added the `sekailink_seed_config_api_service` HTTP daemon and a loopback HTTP smoke test.
- Wired the API daemon to MariaDB-backed persistence when `mysql.enabled` is configured.
- Deployed `sekailink-nexus-seed-config-api.service` on Nexus and validated live HTTP+MariaDB persistence across restart.
- Added YAML compatibility export so SekaiLink configs can still round-trip with Archipelago-style tooling.
- Added `.sklconf` sync manifest payload construction for the future Core-side offline SQLite cache.
- Added CMake smoke targets for schema, service, and API validation.
- Added a schema dump utility for producing deployable MariaDB SQL.
- Documented Nexus ownership of online seed configs and Core ownership of the local `.sklconf` cache.
- Populated live Nexus seed configs with the 54 current BETA-3 supported games and documented the supported catalog scope.
- Added platform normalization SQL for the imported supported catalog.
- Replaced the temporary SoH `ocarina_of_time` fallback with the real `ship_of_harkinian` catalog entry imported from `oot_soh.apworld`.
