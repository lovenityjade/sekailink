# BizHawk Connector Games

List extracted from `sekailink-docs/GAMES-SETUP.md` for games that explicitly use a BizHawk Lua connector.
Patch extensions are taken from `GAMES-SETUP.md`, with `worlds/*` as fallback when not specified.

## Uses `data/lua/connector_bizhawk_generic.lua`
- Kirby 64 - The Crystal Shards (k64) — Patch: `.apk64cs`
- Wario Land 4 (wl4) — Patch: `.apwl4`
- Final Fantasy Tactics Advance (ffta) — Patch: `.apffta`
- Golden Sun: The Lost Age (gstla) — Patch: `.apgstla`
- Pokemon Red/Blue (pokemon_rb) — Patch: `.apred` / `.apblue`
- Castlevania: Symphony of the Night (sotn) — Patch: `.apsotn`
- Mega Man 2 (mm2) — Patch: `.apmm2`
- Mega Man 3 (mm3) — Patch: `.apmm3`
- The Minish Cap (tmc) — Patch: `.aptmc`
- Super Mario Land 2: 6 Golden Coins (marioland2) — Patch: `.apsml2`
- Pokemon Emerald (pokemon_emerald) — Patch: `.apemerald`
- Metroid Zero Mission (mzm) — Patch: `.apmzm`
- Castlevania 64 (cv64) — Patch: `.apcv64`
- Pokemon FireRed/LeafGreen (pokemon_frlg) — Patch: `.apfirered` / `.apleafgreen`
- Pokemon Mystery Dungeon: Explorers of Sky (pmd_eos) — Patch: `.apeos`
- Castlevania: Dawn of Sorrow (cv_dos) — Patch: `.apcvdos`
- Castlevania: Circle of the Moon (cvcotm) — Patch: `.apcvcotm`
- Mario Kart 64 (mk64) — Patch: `.apmk64`
- Mario & Luigi: Superstar Saga (mlss) — Patch: `.apmlss`
- Paper Mario (papermario) — Patch: `.appm64`
- Zelda II: The Adventure of Link (zelda2) — Patch: `.apz2`
- Wario Land (wl) — Patch: `.apwl`
- Yu-Gi-Oh! 2006 (yugioh06) — Patch: `.apygo06`
- Yu-Gi-Oh! Dungeon Dice Monsters (yugiohddm) — Patch: `.apygoddm`
- Metroid Fusion (metroidfusion) — Patch: `.apmetfus`
- The Legend of Zelda: Oracle of Ages (tloz_ooa) — Patch: `.apooa`
- The Legend of Zelda: Oracle of Seasons (tloz_oos) — Patch: `.apoos`

## Uses a BizHawk-specific connector
- Link's Awakening DX Beta (ladx_beta) — Lua: `data/lua/connector_ladx_bizhawk.lua` — Patch: `.apladx`
- The Legend of Zelda (tloz) — Lua: `data/lua/connector_bizhawk.lua` — Patch: `.aptloz`

## Mixed BizHawk / mGBA
- Pokemon Crystal (pokemon_crystal) — Patch: `.apcrystal` — BizHawk: `connector_bizhawk_generic.lua`; mGBA: `connector_bizhawkclient_mgba.lua`

## Unclear / TBD
- Donkey Kong 64 (dk64) — Patch: unknown (external randomizer); Lua: listed as “probable BizHawk/Lua” but no script path specified.
