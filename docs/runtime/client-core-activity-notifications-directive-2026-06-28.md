# Client Core Activity Notifications Directive - 2026-06-28

## Decision

Sekaiemu must behave like an emulator first.

For BETA-3, runtime-social surfaces move to Client Core:

- activity feed;
- queued / pending received item notifications;
- notification bell history;
- social wording for item transfers and hints.

Sekaiemu keeps only emulator responsibilities:

- run cores and ROMs;
- expose memory;
- receive/write runtime bridge data required by the active game;
- reset, save state, load state, and switch game.

## Immediate Rule

Do not send item notification bursts through Sekaiemu's chat/activity bridge.

Client Core receives item events directly from the runtime/client/server layer and decides whether to show:

- a toaster;
- a notification bell entry;
- an activity feed entry.

## Notification Eligibility

A player receives a Client Core item notification when:

- the user is a member of an active or async lobby;
- the user has at least one game/config in that lobby;
- an item is received by one of that user's slots;
- Sekaiemu is not currently open for that lobby/slot/game.

If Sekaiemu is open for the exact receiving game, the game receives the item normally and Client Core should avoid duplicate noise.

## Preferred Copy

Toaster:

`Alice sent you Hookshot in Core Test - SNES - SMZ3`

Bell/activity detail:

`Alice sent you Hookshot from Blind's Hideout - Right in Core Test - SNES - SMZ3`

Hint-driven copy:

`Alice found your hinted Hookshot at Blind's Hideout - Right in Core Test - SNES - SMZ3`

## Event Shape

Preferred server/client event shape:

```json
{
  "type": "item_received",
  "lobby_id": "core-test-snes",
  "lobby_title": "Core Test - SNES",
  "room_id": "38292",
  "recipient_account_id": "user_123",
  "recipient_display_name": "Jade",
  "recipient_slot": "thelov-smz3-7d62",
  "recipient_game": "SMZ3",
  "sender_account_id": "user_456",
  "sender_display_name": "Alice",
  "sender_game": "A Link to the Past",
  "item_name": "Hookshot",
  "location_name": "Blind's Hideout - Right",
  "received_index": 24,
  "was_hint_requested": false,
  "hint_requested_by_account_id": "",
  "created_at": "2026-06-28T00:00:00Z"
}
```

## Deduplication Key

Use a stable key:

`lobby_id + room_id + recipient_slot + received_index`

Fallback when `received_index` is unavailable:

`lobby_id + room_id + recipient_slot + item_name + sender_display_name + location_name + created_at`

## Tracker Direction

Do not restart native tracker work now.

Future update direction:

- PopTracker becomes a tracking engine / server process.
- Client Core draws the SekaiLink tracker UI from structured PopTracker state.
- Sekaiemu remains independent from tracker UI and tracker logic.

