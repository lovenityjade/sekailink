# Game Intake Schema

This schema is the bridge from research to implementation.

## Required Fields

| Field | Meaning |
|---|---|
| `game_key` | Stable lowercase key used by SekaiLink. |
| `display_name` | Human-facing game title. |
| `platform` | `snes`, `nes`, `gb`, `gbc`, or `gba` for this first pass. |
| `archipelago_status` | `core_verified`, `unknown`, `unknown_beta`, or `missing_github`. |
| `apworld_url` | Canonical APWorld repo/page, or `null` if unresolved. |
| `apworld_local_path` | Local APWorld folder if present in `runtime/ap/worlds`. |
| `tracker_status` | `github_verified`, `github_via_source`, `web_only`, `universal_tracker`, `missing`, or `discord_only`. |
| `poptracker_url` | PopTracker repository URL, or `null`. |
| `web_tracker_url` | Web tracker or patcher URL, or `null`. |
| `setup_url` | Setup documentation URL, or `null`. |
| `sklmi_manifest` | Local SKLMI manifest path, or `null`. |
| `sekaiemu_profile` | Local Sekaiemu profile path, or `null`. |
| `tier` | Integer 0-5 from [tiers.md](tiers.md). |
| `notes` | Short operational notes. |

## Interpretation Rules

- `Core-Verified` from Deep Research maps to `core_verified`.
- `Unknown / Beta` maps to `unknown_beta`.
- `GitHub via source` is preserved as unresolved evidence and maps to
  `github_via_source`.
- Discord-only resources are not enough for automated install without a later
  manual retrieval step.
- A local APWorld does not prove SekaiLink runtime support. It only proves that
  generation/client code is available locally.

## Future Commands

The registry is shaped for future Core Access commands:

```text
game list
game open <game_key>
game install <game_key>
game validate <game_key>
game certify <game_key>
game publish <game_key>
```

