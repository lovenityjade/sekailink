# PlayerOptions i18n Expansion (Client + Server)

Date: 2026-02-18

## Scope done
- Continued and expanded player options localization for integrated games.
- Applied on both client and server layers.

## Client
- File: `client/app/src/pages/playerOptionsWorldI18n.ts`
- Added multi-language generic fallback improvements for:
  - option names
  - choice labels
  - doc text fallback behavior
- Locales covered: `fr`, `es`, `ja`, `zh-CN`, `zh-TW`.

## Server (VPS)
- File: `/opt/multiworldgg/WebHostLib/options.py`
- Added/extended:
  - generic i18n maps for groups/options/choices
  - token-based fallback for option display names
  - cache-backed text translation path for docs/labels
- Active cache file:
  - `/opt/multiworldgg/WebHostLib/generated/player_options_mt_cache.json`

## Cache population
- Rebuilt and uploaded translation cache for integrated runtime games available on server player-options API.
- Final cache size: `18150` entries.
- Service restarted and verified active:
  - `multiworldgg-webhost.service`

## Verified examples
- Tested API responses (`/api/player-options/<game>?lang=<locale>`) for:
  - A Link to the Past
  - Ocarina of Time
  - Pokemon Emerald
  - Pokemon FireRed and LeafGreen
  - EarthBound
  - Donkey Kong Country 2
  - The Minish Cap
- Verified translated group labels, option labels, and docs across `fr/es/ja/zh-CN/zh-TW`.
