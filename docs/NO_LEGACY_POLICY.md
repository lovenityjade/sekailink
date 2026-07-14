# No Legacy Policy

SekaiLink BETA-3 now uses the canonical repository as the active source tree.

## Forbidden As Active Sources

- `<local-home>/Projects/Sekaiemu-Libretro-Spike-Codex`
- `<local-home>/DevSSD/sekailink-beta-3-final`
- `<local-home>/SekaiLinkDev`

These paths may be used for historical comparison only. They must not be used by build,
launch, or packaging scripts after canonical migration.

## Runtime Boundary

- SKLMI owns tracker logic, PopTracker compatibility, Lua, AP/room events, items, pins,
  maps, callbacks, and snapshot generation.
- Sekaiemu owns emulation, audio, input, memory exposure, snapshot reading, and drawing.
- Sekaiemu must not run PopTracker Lua, `RunFrameHandlers`, `CanReach`, or tracker object
  logic during normal BETA-3 runtime.

## Local Safety

Old directories are not deleted during this cleanup. They remain available until the canonical
repo has passed visual and runtime validation.
