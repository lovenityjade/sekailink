# EarthBound Manual SNES Heartbeat

This is a narrow second-SNES-game heartbeat for the Beta-3 path:

`Sekaiemu -> SKLMI -> Archipelago-compatible MultiServer`

It intentionally does not claim full EarthBound support. It uses the current
clean-room `earthbound.phase1.json` manifest with:

- `Onett - Tracy Gift`
- `Onett - Tracy's Room Present`
- room-controlled `Toothbrush` injection

The default session keeps the patched ROM and server seed paired on
`AP_11164262353628825690`. For that seed, the two early checks are expected to
be:

- `Onett - Tracy Gift`: `Magicant Bat`
- `Onett - Tracy's Room Present`: `Gelato de Resort`

Run:

```bash
tests/e2e/earthbound/run_earthbound_manual_session.sh
```

Then connect the admin TUI:

```bash
cd <local-home>/DevSSD/sekailink-beta-3-final/clean-room
scripts/sekailink_ap_admin_tui.sh \
  --host 127.0.0.1 \
  --port 38291 \
  --slot Jade-Earthbound \
  --game EarthBound
```

Useful TUI commands:

```text
/admin login sekailink-admin
/items tooth
/send Jade-Earthbound Toothbrush
/sync
```

Proof files:

- `/tmp/sekaiemu-earthbound-manual-session/live/saves/sklmi/earthbound-phase1/trace.jsonl`
- `/tmp/sekaiemu-earthbound-manual-session/live/multiserver.log`
