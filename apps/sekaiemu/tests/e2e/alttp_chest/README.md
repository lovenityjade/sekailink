# ALTTP Chest E2E Heartbeat

This is the official Beta-3 heartbeat for the current Sekaiemu/SKLMI/AP path.
It proves a real gameplay check, not a manual memory injection:

1. Sekaiemu boots the patched ALTTP ROM.
2. An input script reaches Link's House and creates a savestate.
3. MultiServer starts from the AP seed zip.
4. Sekaiemu reloads the savestate with SKLMI and the LinkedWorld
   `default.bundle` tracker enabled.
5. A second input script opens the Link's House chest.
6. The test verifies:
   - SKLMI trace contains `location_checked` with canonical id `59836`.
   - MultiServer receives `LocationChecks [59836]`.
   - SKLMI room sync persists `reported|59836|Link's House`.
   - Sekaiemu tracker shutdown reports `local_checked=1` and `recent=1`.

Run:

```bash
tests/e2e/alttp_chest/run_alttp_chest_e2e.sh
```

Useful overrides:

```bash
SEKAIEMU_E2E_BIN=/path/to/sekaiemu_libretro_spike \
SEKAIEMU_E2E_CORE=/path/to/bsnes_mercury_performance_libretro.so \
SEKAIEMU_E2E_GAME_ROM=/path/to/patched_alttp.sfc \
SEKAIEMU_E2E_MULTISERVER=/path/to/MultiworldGGServer \
SEKAIEMU_E2E_SEED_ZIP=/path/to/AP_seed.zip \
SEKAIEMU_E2E_SKLMI_RUNTIME=/path/to/sekailink_sklmi_runtime \
SEKAIEMU_E2E_SKLMI_MANIFEST_DIR=/path/to/sklmi/manifests \
SEKAIEMU_E2E_RUN_ROOT=/tmp/sekaiemu-alttp-chest-e2e \
SEKAIEMU_E2E_AP_PORT=38290 \
tests/e2e/alttp_chest/run_alttp_chest_e2e.sh
```

The default paths match the current local Beta-3 development workstation. The
test intentionally stays outside regular `ctest` because it depends on ROM,
seed, MultiServer, and SKLMI runtime artifacts that are not portable fixtures.

Current caveat: the fully automated clean boot path still depends on a calibrated
ALTTP file/menu input flow. When that calibration drifts, use the manual check
session below with a known start state and validate the SKLMI/server/tracker
proof files after human gameplay.

## Manual Check Session

When a real person should take checks instead of an input script, use:

```bash
tests/e2e/alttp_chest/run_alttp_manual_check_session.sh
```

This starts MultiServer, keeps it alive while Sekaiemu is open, loads the known
Link's House start state, and launches Sekaiemu with SKLMI plus the LinkedWorld
tracker attached. After closing Sekaiemu, inspect:

- `live/saves/sklmi/alttp-phase1/trace.jsonl`
- `live/multiserver.log`
- `live/saves/tracker/default.bundle/state.json`

The helper defaults to `/tmp/sekaiemu-links-house-state` for the start state.
Override it with `SEKAIEMU_E2E_START_STATE_DIR=/path/to/state-root` when needed.

For admin item-injection tests, start the helper with a local admin password:

```bash
SEKAIEMU_E2E_SERVER_PASSWORD=sekailink-admin \
tests/e2e/alttp_chest/run_alttp_manual_check_session.sh
```

Then connect an Archipelago admin client to the same port, run
`/admin login sekailink-admin`, and use `/send <player> <item>`.
