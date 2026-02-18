# HTPC + Android Remote Control Plan (Future)

Date: 2026-02-18  
Status: Planned (future scope)

## Goal
Make SekaiLink fully "sofa-ready" for HTPC by using an Android app as a smart controller, while the HTPC keeps all runtime-heavy work (patching, emulator, tracker, connectors).

## Product Direction
- Android app handles: login, room flow, game selection, launch/stop requests, live status, basic logs.
- HTPC receiver handles: patch/download/runtime orchestration, emulator/tracker launch, connectors, session lifecycle.
- No emulator/tracker execution on Android.

## Why This Is Strong
- Removes TV/remote UX friction (no touchscreen required on HTPC).
- Preserves existing proven launch pipeline on Linux/Windows.
- Creates a "console-like" 2-screen experience (control + status on phone).

## Security Requirements (Critical)
- LAN-only by default (no internet exposure by default).
- Pairing flow with short code or QR.
- Persisted device token after pairing.
- Every command includes: token + timestamp + nonce.
- Add HMAC signature (derived/shared pairing secret) for anti-replay and command integrity.

## Architecture (Recommended)

### 1) HTPC Receiver Daemon
- Linux: `systemd` service.
- Windows: service (or tray app initially, then service).
- Exposes local control API (`WebSocket` preferred, or HTTP + SSE).

Responsibilities:
- Own the launch state machine.
- Execute the same logic as current `Launch` button pipeline.
- Maintain single active session lock.
- Publish live structured events and logs.
- Stop/cleanup process tree safely.

### 2) Android Controller App
- UI-first orchestrator, no runtime-heavy execution.
- Sends high-level commands only:
  - `LAUNCH(profile_id | room_id | game_id)`
  - `STOP(session_id)`
  - `GET_STATUS`
  - `GET_LOG_TAIL`
  - `PAIR_REQUEST` / `PAIR_CONFIRM`
- Avoid low-level imperative commands (daemon decides exact launch sequence).

### 3) Discovery
- Primary: mDNS/Bonjour discovery on LAN.
- Fallback: QR or manual host entry.

## State Model (Daemon)
- `idle`
- `downloading`
- `patching`
- `launching_emulator`
- `launching_tracker`
- `connecting`
- `running`
- `error`
- `stopping`

## Event Model (Structured JSON)
Examples:
- `PATCH_STARTED`
- `PATCH_FINISHED`
- `EMU_STARTED`
- `TRACKER_STARTED`
- `AP_CONNECTED`
- `SNI_CONNECTED`
- `SESSION_READY`
- `SESSION_ERROR`
- `SESSION_STOPPED`

## Operational Rules
- Phone is optional after launch (session must continue if phone disconnects).
- Enforce session lock to avoid double-launch conflicts.
- Keep logs structured and consumable by both HTPC UI and Android UI.

## MVP Scope
1. HTPC receiver daemon (LAN-only)
2. Pairing (6-digit code or QR)
3. Remote `Launch` (reuses existing launch pipeline)
4. Live status/progress feed
5. Remote `Stop`
6. Last error retrieval

## Nice-to-Have After MVP
- "Now Playing" card on Android (game, room, state, quick controls)
- Quick-copy room info and deep links
- Optional advanced remote WAN mode later (separate security model)

## Risks / Watchouts
- Security surface if command API is weak.
- Divergence if Android and desktop each become separate sources of truth.
- Overly verbose unstructured logs hurting UX and debuggability.

## Next Implementation Step (when started)
- Define protocol schema (`commands`, `events`, `errors`) in a dedicated contract doc.
- Implement daemon skeleton with `idle -> running -> stop` flow and mocked launch first.
