# Live Seed Generation Boundary

Status: BETA-3 live wiring, 2026-05-30.

## Source of Truth

- Nexus owns user identity and seed configuration records.
- Product identity is `user_id`, not Archipelago-style `slot`.
- Game configuration identity is `config_id` plus `version_id`.
- Game identity is generic `game_key`; ALTTP is only the current showcase driver.
- YAML is compatibility output for the background generator, not the normal server storage format.

## Generate Flow

1. Core selects one or more Nexus configs for each user in a Sync.
2. Link Social stores the lobby selection as `config_id`, `version_id`, `game_key`, and display metadata.
3. When the host clicks Generate, Link Social exports each selected config from Nexus as generator-compatible YAML.
4. The generation handoff writes only under `spool/<generation_id>/`.
5. `Players/`, `output/`, `manifest.json`, and `generation_state.json` are per-generation; no shared `Players`, `output`, `latest`, or `current` paths are allowed.
6. Worlds receives the isolated `Players` directory and returns the seed package.

## Compatibility Names

The generator still needs short YAML `name:` values. SekaiLink therefore creates temporary compatibility names in the handoff layer only.

- Human UI/logs use usernames and game names.
- Internal server data keeps `user_id`, `config_id`, `version_id`, and `game_key`.
- If a user has more than one config for the same game in the same Sync, display names receive `{1}`, `{2}`, and so on.
- Example display line: `Jade (A Link to the Past {1}) sends Boomerang to Raifu (Twilight Princess)`.
- The `{n}` suffix is not a slot. It is only the per-user, per-game instance number inside that Sync.
