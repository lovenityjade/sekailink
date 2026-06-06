# The Legend of Zelda: A Link to the Past

## Status

- Platform: SNES
- Deep Research status: Core-Verified
- SekaiLink tier: 5, Release Certified candidate
- Public wording: "SekaiLink compatible" once the current release build remains verified.

## Local Evidence

- APWorld: `runtime/ap/worlds/alttp`
- Sekaiemu profile: `runtime/profiles/alttp-starter.profile`
- SKLMI manifest: `runtime/sklmi/manifests/alttp.phase1.json`
- Tracker bundles:
  - `runtime/tracker-bundles/alttp-default`
  - `runtime/tracker-bundles/alttp-linkedworld-default`
  - `runtime/tracker-bundles/alttp-native`

## Notes

ALTTP is the release fixture, but it must not become a hardcoded rule for other
games. New game integrations should reuse generic APWorld, PopTracker, SNI/Lua,
and SKLMI adapter paths wherever possible.

