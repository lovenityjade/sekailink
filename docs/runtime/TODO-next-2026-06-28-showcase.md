# TODO Next - Showcase Windows Build

Date: 2026-06-28 06:55 EDT

## Immediate Priority

Build and debug a Windows, non-dev-environment version for today's showcase.

Focus on packaging/runtime behavior first:

- Client Core starts cleanly outside Vite/dev mode.
- Sekaiemu launches from Client Core on Windows.
- Bundled/runtime paths resolve correctly for:
  - Sekaiemu executable.
  - libretro core.
  - SKLMI runtime.
  - SKLMI manifests.
  - tracker packs if present.
  - portable Python/AP runtime.
- Logs are easy to access from the installed build.
- Showcase path should be stable before continuing deeper social/backend work.

## Current Implemented Runtime Social State

Client Core now owns the full social surfaces:

- Runtime Chat floating window.
- Runtime Activity floating window.
- Runtime Hint floating window.
- Sekaiemu only has lightweight HUD buttons and compact toasts.

Sekaiemu HUD file IPC exists:

- `--client-core-hud-state <path>`
- `--client-core-hud-events <path>`

Sekaiemu now avoids the previous heavy in-frame social surfaces:

- `T` no longer opens the old Sekaiemu chat overlay.
- old chat/activity rendering is disabled.
- old chat bridge tick was made a no-op.
- tracker/state file reads were reduced or gated to avoid per-frame lag.
- HUD state polling was reduced.

Item notifications currently implemented:

- If Sekaiemu is open, Client Core can watch the SKLMI `trace.jsonl` and convert `room_item_pending` into a compact Sekaiemu HUD toast.
- Web AP client output is parsed for sent-item lines and routes through the same notification path.
- Client Core renderer listens for `runtime:item_received` and adds a toast plus bell entry.

## Important Gap: Pending Items While Client Core Was Closed

This is not complete yet.

Desired behavior:

- If someone sends items while the recipient has Client Core closed, those items remain pending server-side.
- When the recipient opens Client Core and logs in, Client Core should show the pending item inbox/notifications even if Sekaiemu is not open.
- This must be based on Room/Link pending items, not Sekaiemu logs.

Likely implementation direction:

1. Add or expose an authenticated Client Core API endpoint in `link-social` for "my pending runtime items".
2. That endpoint should identify lobbies/rooms/slots owned by the logged-in user.
3. It should query Room/Link pending items without requiring Sekaiemu to be running.
4. Deduplicate by `lobby_id + room_id + recipient_slot + delivery_id/received_index`.
5. Client Core should poll this endpoint on login/startup and periodically while open.
6. Notifications:
   - If matching Sekaiemu session is open: Sekaiemu compact toast only.
   - If no matching Sekaiemu session: Client Core toast + bell.
7. Do not acknowledge deliveries just because a notification was shown. Acknowledgement must remain tied to actual runtime/item application unless product design explicitly changes.

Relevant server-side protocol already exists:

- `issue_ticket`
- `pending_items`
- `acknowledge_delivery`

Relevant code areas:

- `services/link-room/src/room_registry.cpp`
- `services/link-room/src/room_session.cpp`
- `services/link-room/src/room_server_protocol.cpp`
- `services/link-social/src/chat_api_lobby_routes.inc`
- `services/link-social/src/chat_api_service.cpp`
- `apps/client-core/electron/lib/sekaiemu-runtime-social.cjs`
- `apps/client-core/src/redesign/App.tsx`

## Files Recently Touched For Runtime Social

- `apps/client-core/electron/lib/sekaiemu-runtime-social.cjs`
- `apps/client-core/electron/lib/sekaiemu-runtime.cjs`
- `apps/client-core/electron/lib/web-ap-client-runtime.cjs`
- `apps/client-core/electron/main.cjs`
- `apps/client-core/src/redesign/App.tsx`
- `apps/sekaiemu/src/runtime_client_core_hud.*`
- `apps/sekaiemu/src/launch_options.*`
- `apps/sekaiemu/src/runtime_loop.cpp`
- `apps/sekaiemu/src/libretro_host_runloop.cpp`
- `apps/sekaiemu/src/libretro_tracker_host_state.cpp`
- `apps/sekaiemu/src/runtime_activity_feed_imgui.cpp`

## Verification Already Done

Client Core:

- `node --check` passed for touched Electron modules.
- `npm run build` passed in `apps/client-core`.
- local smoke test confirmed:
  - open Sekaiemu session writes a HUD toast.
  - no open session emits a Client Core notification event.

Sekaiemu:

- CMake configure/build previously passed using `/tmp/sekaiemu-client-core-hud-build`.
- Existing `apps/sekaiemu/build-codex-imgui` cache is stale/broken because it points to an old path.

## Showcase Debug Checklist

- Start from a clean packaged Windows build, not dev.
- Confirm Client Core can log in.
- Confirm lobby list loads.
- Confirm joining/launching the showcase room works.
- Confirm Sekaiemu launches.
- Confirm HUD icons appear in top-right.
- Confirm no lag when items/chat/activity are active.
- Confirm item sent to open Sekaiemu shows compact toast.
- If pending item notification while closed is still missing, call it known TODO and avoid relying on it in the showcase flow.

