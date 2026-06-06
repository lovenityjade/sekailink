# Compatibility Matrix

Initial matrix imported from `/home/thelovenityjade/deep-research-report.md`
and cross-checked against local SekaiLink paths where possible.

## Priority Interpretation

Best immediate SekaiLink targets:

| Game | Why |
|---|---|
| A Link to the Past | Existing release path and local tracker bundles. |
| EarthBound | Existing local APWorld, Sekaiemu profile, and SKLMI phase1 heartbeat manifest. |
| Super Metroid | Core APWorld, PopTracker URL, high trailer value. Needs runtime adapter study. |
| Super Mario World | Core APWorld, PopTracker URL, strong SNES candidate. |
| Donkey Kong Country 3 | Core APWorld and tracker URL. Good SNES candidate. |
| Pokemon Emerald | Core APWorld and tracker URL. Good GBA candidate once GBA runtime path is validated. |
| FF4FE | Local APWorld and phase1 heartbeat profile/manifest exist, but Free Enterprise/mod path needs review. |

## Verified APWorld And PopTracker Candidates

| Platform | Game | APWorld | Tracker | Setup | AP status | SekaiLink evidence | Tier |
|---|---|---|---|---|---|---|---|
| snes | Chrono Trigger | `https://www.wiki.ctjot.com/doku.php?id=multiworld` | `https://github.com/Anguirel86/Jets-of-Time-Tracker` | GitHub setup guide | unknown | no local APWorld found in first pass | 0 |
| snes | Donkey Kong Country 2 | `https://github.com/TheLX5/Archipelago/releases?q=%22Donkey+Kong+Country+2%22&expanded=true` | `https://github.com/pwkfisher/ap-dkc2-tracker/releases` | unresolved | unknown | local APWorld: `runtime/ap/worlds/dkc2` | 1 |
| snes | Donkey Kong Country 3 | Archipelago core | `https://github.com/PoryGone/DKC3_AP_Tracker/releases/` | official guide | core_verified | local APWorld: `runtime/ap/worlds/dkc` | 1 |
| snes | EarthBound | GitHub via source | GitHub via source | unresolved | unknown | local APWorld, profile, SKLMI phase1 manifest | 3 |
| snes | Final Fantasy Mystic Quest | Archipelago core | GitHub via source | official guide | core_verified | no local APWorld found in first pass | 0 |
| snes | Kirby's Dream Land 3 | Archipelago core | GitHub via source | official guide | core_verified | local APWorld: `runtime/ap/worlds/kdl3` | 1 |
| snes | Secret of Evermore | Archipelago core | GitHub via source | official guide | core_verified | no local APWorld found in first pass | 0 |
| snes | Super Mario World | Archipelago core | `https://github.com/PoryGone/SMW_AP_Tracker` | official guide | core_verified | local APWorld: `runtime/ap/worlds/smw` | 1 |
| snes | Super Metroid | Archipelago core | `https://github.com/Cyb3RGER/sm_ap_tracker` | official guide | core_verified | local APWorld: `runtime/ap/worlds/sm` | 1 |
| snes | The Legend of Zelda: A Link to the Past | Archipelago core | `https://github.com/StripesOO7/alttp-ap-poptracker-pack` | official guide | core_verified | local APWorld, profile, tracker bundles, SKLMI manifest | 5 |
| nes | Crystalis | GitHub via source | GitHub via source | GitHub via source | unknown | no local APWorld found in first pass | 0 |
| nes | Final Fantasy | Archipelago core | GitHub via source | official guide | core_verified | no local APWorld found in first pass | 0 |
| nes | Mega Man 2 | Archipelago core | GitHub via source | official guide | core_verified | local APWorld: `runtime/ap/worlds/mm2` | 1 |
| nes | The Legend of Zelda | Archipelago core | GitHub via source | official guide | core_verified | local APWorld, profile | 1 |
| nes | Zelda II: The Adventure of Link | GitHub via source | GitHub via source | unresolved | unknown | local APWorld: `runtime/ap/worlds/zelda2` | 1 |
| gb | Pokemon Red and Blue | Archipelago core | GitHub via source | official guide | core_verified | local APWorld: `runtime/ap/worlds/pokemon_rb` | 1 |
| gb | Wario Land: Super Mario Land 3 | GitHub via source | GitHub via source | GitHub via source | unknown | local APWorld: `runtime/ap/worlds/wl` | 1 |
| gbc | Pokemon Crystal | GitHub via source | GitHub via source | setup gist | unknown | local APWorld: `runtime/ap/worlds/pokemon_crystal` | 1 |
| gbc | Oracle of Ages | GitHub via source | GitHub via source | GitHub via source | unknown | local APWorld family: `runtime/ap/worlds/tloz_oos` | 1 |
| gbc | Oracle of Seasons | GitHub via source | GitHub via source | GitHub via source | unknown | local APWorld family: `runtime/ap/worlds/tloz_oos` | 1 |
| gba | Castlevania: Circle of the Moon | `https://github.com/LiquidCat64/LiquidCatipelago/releases` | `https://github.com/sassyvania/Circle-of-the-Moon-Rando-AP-Map-Tracker-/releases` | GitHub setup guide | unknown | no local APWorld found in first pass | 0 |
| gba | Mario & Luigi: Superstar Saga | Archipelago core | GitHub via source | official guide | core_verified | no local APWorld found in first pass | 0 |
| gba | Mega Man Battle Network 3 Blue | Archipelago core | GitHub via source | official guide | core_verified | no local APWorld found in first pass | 0 |
| gba | Metroid: Zero Mission | GitHub via source | GitHub via source | unresolved | unknown | local APWorld: `runtime/ap/worlds/mzm` | 1 |
| gba | Pokemon Emerald | Archipelago core | `https://github.com/seto10987/Archipelago-Emerald-AP-Tracker` | official guide | core_verified | local APWorld: `runtime/ap/worlds/pokemon_emerald` | 1 |
| gba | Pokemon FireRed and LeafGreen | GitHub via source | GitHub via source | GitHub via source | unknown | local APWorld, profile | 1 |
| gba | Wario Land 4 | GitHub via source | GitHub via source | GitHub via source | unknown | local APWorld: `runtime/ap/worlds/wl4` | 1 |

## Incomplete Or Blocked Research

| Platform | Game | Missing | Notes | Local evidence |
|---|---|---|---|---|
| snes | ActRaiser | GitHub APWorld and GitHub PopTracker | resources found on Discord only | none |
| snes | Final Fantasy IV | GitHub APWorld | PopTracker confirmed by source; APWorld Discord-only | none |
| snes | Final Fantasy VI | PopTracker | APWorld Discord-only; tracker listed as EmoTracker | none |
| snes | Lufia II: Rise of the Sinistrals | PopTracker | core/setup official, no PopTracker found | `runtime/ap/worlds/lufia2ac` |
| snes | SMZ3 | PopTracker | core/setup official, no PopTracker found | `runtime/ap/worlds/smz3` |
| snes | Yoshi's Island | GitHub PopTracker | pack on Discord, web tracker alternative | none |
| snes | Mario is Missing! | GitHub APWorld and GitHub PopTracker | Discord-only | none |
| nes | Faxanadu | PopTracker | APWorld/client/setup exist | none |
| nes | The Guardian Legend | PopTracker | APWorld/setup exist | none |
| nes | Mega Man 3 | PopTracker | Universal Tracker only | `runtime/ap/worlds/mm3` |
| nes | Spelunker | GitHub APWorld | PopTracker confirmed by source | none |
| gb/gbc | Super Mario Land 2 | GitHub APWorld and PopTracker | APWorld Discord, setup GitHub | `runtime/ap/worlds/marioland2` |
| gb/gbc | Link's Awakening DX | PopTracker | core + beta APWorld + Magpie web tracker | local profile only |
| gba | Final Fantasy Tactics Advance | PopTracker | APWorld confirmed, setup Discord | `runtime/ap/worlds/ffta` |
| gba | Fire Emblem: The Sacred Stones | PopTracker | APWorld/setup confirmed | none |

## Tracker Optional Or Non-Blocking

These games were previously easy to read as "blocked" because the research
matrix focused on PopTracker availability. For SekaiLink certification, a
PopTracker pack is not always mandatory. Platformers and linear action games can
enter the pipeline with runtime/AP heartbeat proof, a web tracker, Universal
Tracker, or no tracker for the first trailer-grade milestone.

| Platform | Game | Tracker interpretation | Notes | Local evidence |
|---|---|---|---|---|
| snes | ActRaiser | optional for first heartbeat | Platformer/action structure; Discord resources still need retrieval before install automation. | none |
| snes | Super Mario World 2: Yoshi's Island | optional/web alternative | Pack reported on Discord and web tracker alternative exists; PopTracker GitHub is not a hard blocker. | none |
| nes | Mega Man 3 | optional/Universal Tracker | Platformer; Universal Tracker is enough for initial support classification. | `runtime/ap/worlds/mm3` |
| gb/gbc | Super Mario Land 2 | optional | Platformer; APWorld/setup evidence exists but GitHub links still need resolution. | `runtime/ap/worlds/marioland2` |
| gb/gbc | Link's Awakening DX | web tracker confirmed | MagpieTracker web tracker covers initial tracking need; missing PopTracker is not a hard blocker. | `runtime/profiles/ladx-starter.profile` |

## Next Research Tasks

1. Resolve every `GitHub via source` URL from the community index or upstream
   repos before automated download is enabled.
2. Separate local APWorld presence from true SekaiLink runtime readiness.
3. Classify tracker requirement per game: mandatory, optional, web-only, or
   Universal Tracker acceptable.
4. For SNES games, inspect official clients and SNI/Lua connectors first.
5. For each candidate, produce or generate a SKLMI runtime adapter manifest.
6. Add one trailer checklist per game once it reaches Tier 3.
