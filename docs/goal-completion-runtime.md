# Goal Completion Runtime

Status: first Sekaiemu integration pass.

Sekaiemu owns the immediate player-facing celebration:

- the core pauses as soon as goal completion is triggered;
- the last emulation frame remains behind a dark/bright overlay;
- the goal completion screen is rendered over the emulator;
- the local run timer is stored in `goal-completion-timer.json` under the game save directory;
- `F4` triggers a local test completion;
- `--test-goal-completion-at-frame <frame>` triggers deterministic test completion.

The timer starts when emulation frames start running, persists every few seconds, saves on shutdown, resumes from the same save directory on next launch, and stops once completion is triggered.

The SekaiPieces source assets copied into Sekaiemu are:

- `apps/sekaiemu/assets/goal-completion/goal_completed_transparent.png`
- `apps/sekaiemu/assets/goal-completion/goalcompleted.mp3`
- `apps/sekaiemu/assets/goal-completion/aftergoal.mp3`
- `apps/sekaiemu/assets/goal-completion/goalcompleted.wav`
- `apps/sekaiemu/assets/goal-completion/aftergoal.wav`
- `apps/sekaiemu/assets/goal-completion/goalcompleted_timeline_v1.json`

Sekaiemu keeps the MP3 source files and plays the generated WAV files through a separate SDL queued-audio device. The sequence is `goalcompleted.wav`, then `aftergoal.wav`. The libretro audio device is paused during the completion screen so the music is not mixed with the game.

## Nexus Storage Shape

Nexus should store completion records separately from lobbies so times survive room cleanup:

```json
{
  "user_id": "sekailink user id",
  "display_name": "SekaiLink name",
  "game_id": "alttp",
  "game_name": "A Link to the Past",
  "config_id": "stable hash of world/config/options",
  "seed_id": "seed or lobby seed id",
  "lobby_id": "optional lobby id",
  "room_id": "optional room id",
  "slot": 1,
  "elapsed_seconds": 12345,
  "checked_count": 216,
  "total_locations": 216,
  "completion_percent": 100,
  "completed_at": "UTC timestamp",
  "source": "archipelago_status_update"
}
```

Recommended key: `user_id + game_id + config_id + seed_id + slot`.
