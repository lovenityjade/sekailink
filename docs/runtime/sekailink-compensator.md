# SekaiLink Compensator

The Compensator is a hidden Archipelago world used only when a generation has a recognized item/location capacity mismatch. It preserves SekaiLink's zero-friction generation flow without hiding unrelated configuration, ROM, logic, or APWorld failures.

## Generation contract

- `item_location_deficit`: the existing worlds produced more items than usable locations. SekaiLink retries once with synthetic Compensator locations.
- `filler_item_deficit`: the existing worlds produced more locations than items. SekaiLink retries once after asking each affected real APWorld to create the missing filler items.
- Every other failure is returned unchanged.
- The retry uses the original options and the same seed. It never retries more than once.
- The Compensator is a real slot with a deterministic, collision-safe human-readable name.
- More than 512 compensations produces an operator warning. More than 2048 is rejected.

The resulting sync ZIP contains `sekailink_compensator.json`. Link stores and exposes this metadata with generation status and posts one human-readable system message to the lobby after generation succeeds.

## Runtime contract

Compensator locations are released inside the existing room-server process. No extra client, WebSocket, subprocess, or polling service remains connected for the Compensator.

- Progression items are distributed across the first 60% of real checks.
- Remaining items are distributed between 60% and 95% of real checks.
- When at least one human is connected and no real check arrives for 20 minutes, one pending Compensator location is released. The fallback is limited to one release per 10 minutes.
- Empty rooms do not advance the fallback timer.
- Runtime progress and the selected release mode are saved with the room.

The Compensator is excluded from Link's human player, Ready, and lobby counts. Its events remain visible in room activity and chat.

## Host controls

The host-only Link endpoint is:

```text
POST /lobbies/{lobby_id}/compensator/release
{"mode":"all|progression|accelerate"}
```

The same controls are available to operators:

```text
!admin /compensator status
!admin /compensator release all
!admin /compensator release progression
!admin /compensator release accelerate
```

`skl-room` exposes these as `compensator status` and `compensator release <mode>`. The standard AP `/release <slot name>` remains available when the host wants to release the Compensator exactly like another slot.
