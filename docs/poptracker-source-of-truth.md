# PopTracker Source Of Truth

Last updated: 2026-06-21

## Canonical Reference

Absolute source of truth for whether an Archipelago game has a PopTracker pack:

https://archipelago.miraheze.org/wiki/Category:Games_with_PopTracker

Use this page before changing tracker availability claims in Client Core,
runtime module manifests, or release notes.

## Local Downloads Staging

The remaining tracker packs downloaded for review/import are currently staged
in:

`<local-home>/Downloads`

Observed downloads on 2026-06-21:

- `supermetroid.zip`
- `KDL3.Poptracker.0.1.3.zip`
- `MLSS-PopTracker-v.2.0.3.zip`
- `Metroid.Fusion.pack.zip`
- `seasons_ap.zip`
- `crystal-ap-tracker-11.0.5.zip`
- `pokemon-frlg-tracker.zip`
- `StableTmcrTrackerDeoxis.zip`
- `MetroidZeroMission_PopTrackerPack.zip`
- `megaman2-ap-poptracker.zip`
- `megamanx3-ap-poptracker.zip`
- `MMBN3_AP_Tracker.zip`
- `ooa_brooty.zip`
- `rb_tracker-3.1.1.zip`
- `SuperMarioLand2TrackerAP.zip`
- `tloz_brooty.zip`
- `wl4_jth.zip`
- `zelda-2-ap-tracker-0.0.4.zip`
- `SoM-Open-Mode-Tracker-v1_7_0_3.zip`
- `smz3-ap-tracker-1.5.1.zip`

Imported into runtime staging on 2026-06-21:

- Secret of Mana: `runtime/downloaded-resources/sekailink-supported/poptracker/secret_of_mana/SoM-Open-Mode-Tracker-v1_7_0_3.zip`
- SMZ3: `runtime/downloaded-resources/sekailink-supported/poptracker/smz3/smz3-ap-tracker-1.5.1.zip`

## Bundled Runtime Packs

Integrated into `runtime/poptracker/packs` on 2026-06-26 for currently
available Client Core games:

- A Link to the Past: `runtime/poptracker/packs/alttp/pack`
- Kirby's Dream Land 3: `runtime/poptracker/packs/kirbys_dream_land_3/pack/Kirby_Dream_Land_3_AP_Poptracker-main`
- Mega Man 2: `runtime/poptracker/packs/mega_man_2/pack`
- Mega Man X3: `runtime/poptracker/packs/mega_man_x3/pack`
- Metroid Fusion: `runtime/poptracker/packs/metroid_fusion/pack`
- Metroid: Zero Mission: `runtime/poptracker/packs/metroid_zero_mission/pack`
- Secret of Mana: `runtime/poptracker/packs/secret_of_mana/pack`
- SMZ3: `runtime/poptracker/packs/smz3/pack`
- The Legend of Zelda: `runtime/poptracker/packs/the_legend_of_zelda/pack`
- The Minish Cap: `runtime/poptracker/packs/the_minish_cap/pack`
- Wario Land 4: `runtime/poptracker/packs/wario_land_4/pack/wl4_jth-main`
- Zelda II: `runtime/poptracker/packs/zelda_ii/pack/zelda-2-ap-tracker-0.0.4`

The Electron PopTracker launcher must consider `tracker_pack_path` a valid
external tracker source. Local pack manifests may use PopTracker-style JSON
comments, so validation must remain tolerant rather than strict `JSON.parse`.

## Trackerless Supported Exceptions

These games are considered supported in SekaiLink even without a PopTracker
pack for the current compatibility tier:

- Final Fantasy Tactics Advance
- Lufia II Ancient Cave
- Mega Man 3
- Wario Land

Do not block these games solely because `tracker_pack_uid` is empty.

## Unstable / Not Available

- Final Fantasy V Career Day is marked unavailable/unstable until runtime and
  client behavior are revalidated.
