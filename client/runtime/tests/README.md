# Headless Runtime Smoke

`headless_ap_smoke.py` is an MVP smoke test for the AP runtime wrappers.

## What it checks

- `integration` mode:
  - starts a local mock AP websocket server
  - launches a wrapper subprocess (`common` or `bizhawk`)
  - runs a connect flow
  - verifies `RoomInfo -> GetDataPackage` path does not hit the old websocket `.open` crash
- `compat` mode:
  - static guard check in `CommonClient.send_msgs` for websocket compatibility (`closed` + optional `open`)

## Commands

From repo root:

```bash
scripts/headless-smoke.sh --wrapper common --mode auto
scripts/headless-smoke.sh --wrapper bizhawk --mode integration
```

From `client/app`:

```bash
npm run headless:smoke
```

## Notes

- `auto` tries integration first, then falls back to compat if local sockets cannot bind in the current environment.
- Integration mode requires a Python environment with `websockets` available.
