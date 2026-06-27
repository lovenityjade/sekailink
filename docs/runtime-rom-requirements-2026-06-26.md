# Runtime ROM Requirements Audit

Date: 2026-06-26

Purpose: track the clean base ROM versions and MD5 hashes used by SekaiLink
runtime import validation. Do not commit ROM files.

## Resolved Locally

| Game | Expected base | MD5 |
|---|---|---|
| Final Fantasy Tactics Advance | Final Fantasy Tactics Advance (USA, Australia).gba | `cd99cdde3d45554c1b36fbeb8863b7bd` |
| Pokemon Crystal | Pokemon - Crystal Version (UE) v1.1 | `301899b8087289a6436b0a241fbbb474` |
| Pokemon Emerald | Pokemon - Emerald Version (USA, Europe).gba | `605b89b67018abcea91e693a4dd25be3` |
| Pokemon FireRed | Pokemon - FireRed Version (USA, Europe).gba | `e26ee0d44e809351c8ce2d73c7400cdd` |
| Pokemon LeafGreen | Pokemon - LeafGreen Version (USA, Europe).gba | `612ca9473451fa42b51d1711031ed5f6` |
| Pokemon Red | Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb | `3d45c1ee9abd5738df46d2bdda8b57dc` |
| Pokemon Blue | Pokemon - Blue Version (USA, Europe) (SGB Enhanced).gb | `50927e843568814f7ed45ec4f944bd8b` |
| Final Fantasy V Career Day | Final Fantasy V (J).sfc | `d69b2115e17d1cf2cb3590d3f75febb9` |
| Wario Land | Wario Land - Super Mario Land 3 (World).gb | `d9d957771484ef846d4e8d241f6f2815` |

## Known Accepted Hashes Not Fully Cached

No remaining ROM from this audit is missing from the local cache.

## Multi-ROM Flow

SMZ3 is not a single base ROM. It requires:

| Component | Expected base | MD5 |
|---|---|---|
| ALttP | Zelda no Densetsu - Kamigami no Triforce (Japan) 1.0 | `03a63945398191337e896e5771f77173` |
| Super Metroid | Super Metroid (Japan, USA) | `21f3e98df4780ee1c667b84e57d88675` |

Do not add a fake `smz3` base ROM hash. Runtime validation should resolve SMZ3
through its two component base ROMs.

## OOT Product Decision

Do not support Ocarina of Time twice. The intended SekaiLink path is Ship of
Harkinian only; original-feeling OOT behavior should be exposed later through
SoH settings.
