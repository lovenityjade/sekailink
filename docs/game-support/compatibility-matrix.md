# Compatibility Matrix

Initial matrix imported from `<local-home>/deep-research-report.md`
and cross-checked against local SekaiLink paths where possible.

## Runtime Freeze - Bilateral Confirmed

Date: 2026-06-26

These games are confirmed bidirectional in current BETA-3 runtime tests:
checks travel from game to server, and received items travel from server back
to the game. Treat these paths as frozen. Do not modify their runtime,
wrapper, bridge, launch, or tracker integration paths unless a new
reproducible regression log is attached.

SNES/SNI compatibility is established for the BETA-3 runtime lane as of
2026-06-26. This means the SNES core/system path is no longer the active
blocker; future SNES failures should be treated as regression reports or
game-specific adapter debt, not as proof that the generic SNES/SNI lane is
unproven.

| Platform | Game | Runtime path | Status | Notes |
|---|---|---|---|---|
| snes | The Legend of Zelda: A Link to the Past | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | Donkey Kong Country | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | Donkey Kong Country 2 | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | EarthBound | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | Kirby's Dream Land 3 | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | Lufia II Ancient Cave | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. Roguelike-style Ancient Cave flow. |
| snes | Mega Man X2 | Sekaiemu + Snes9x + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. Confirms the MMX/Cx4-style Snes9x lane as usable for BETA-3 runtime tests. |
| snes | Secret of Mana | Sekaiemu + Snes9x + Evermizer SoM web AP client/SNI path | frozen_confirmed_with_visual_debt | Checks and items confirmed both directions. Non-blocking graphics bug remains on naming/save screen; gameplay continues after Start input and the runtime path is frozen. |
| snes | SMZ3 | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | Super Mario World | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |
| snes | Super Metroid | Sekaiemu + Archipelago/SNI path | frozen_confirmed | Checks and items confirmed both directions. |

## Deferred SNES Follow-Ups

| Platform | Game | Runtime path | Status | Notes |
|---|---|---|---|---|
| snes | Mega Man X3 | Sekaiemu + Snes9x + Archipelago/SNI path | deferred_optional_validation | bsnes-mercury loads the Cx4 firmware but produces a fully black framebuffer for both vanilla USA and AP-patched MMX3. Snes9x produces non-black frames. MMX2 already validates the MMX/Cx4-style lane for the SNES freeze; MMX3 can be validated later without reopening generic SNES/SNI. |
| snes | Secret of Evermore | Sekaiemu + Snes9x + Evermizer web AP client/SNI path | deferred_web_client_validation | Uses module-level `core_id: snes9x` to match the Evermizer browser-client runtime path. Validate separately from SoM, but do not reopen generic SNES/SNI for it. |

## Temporarily Unavailable

| Platform | Game | Runtime path | Status | Notes |
|---|---|---|---|---|
| gba | Final Fantasy Tactics Advance | Sekaiemu + GBA BizHawk wrapper path | unavailable_unable_to_test | Client connection was observed, but item/check transactions were not verified. Manual checks depend on FFTA mission reward flow, so this needs a dedicated tester familiar with the game before it is exposed again. |

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
| gba | Final Fantasy Tactics Advance | PopTracker | APWorld confirmed, setup Discord | `runtime/ap/worlds/ffta`; temporarily not available in Client Core because runtime transactions are unable to test manually |
| gba | Fire Emblem: The Sacred Stones | PopTracker | APWorld/setup confirmed | none |

## Tracker Optional Or Non-Blocking

These games were previously easy to read as "blocked" because the research
matrix focused on PopTracker availability. For SekaiLink certification, a
PopTracker pack is not always mandatory. Platformers and linear action games can
enter the pipeline with runtime/AP heartbeat proof, a web tracker, Universal
Tracker, or no tracker for the first trailer-grade milestone.

Universal Tracker is a separate future avenue, not the current packaging target.
For the pre-beta game pipeline, PopTracker remains the preferred installable
tracker target, but Universal Tracker evidence can downgrade a missing
PopTracker pack from "blocked" to "tracker acceptable for first proof" when the
game runtime itself is otherwise viable. Current reference:
`https://github.com/FarisTheAncient/Archipelago/releases/latest`.

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
