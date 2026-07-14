# SekaiLink Solo Crossworld Runtime

Status: idea and architecture discussion

Date: 2026-07-12

## Concept

Create a solo campaign containing multiple games where reaching a configured location in one game automatically transitions the player into another game. The transition should not require visible interaction with Client Core.

Example:

```text
A Link to the Past: Lost Woods location
  -> SekaiLink transition
  -> Super Mario 64: Castle Lobby
```

Client Core may install and prepare the campaign, but a lightweight background runtime director owns transitions while the campaign is running.

## Existing SekaiLink Capabilities

SekaiLink already controls or observes the core pieces required for a prototype:

- active game and runtime;
- Archipelago location list and newly completed checks;
- game memory through Sekaiemu and SKLMI;
- game processes and emulator lifecycle;
- normal saves and libretro savestates;
- room connection and received-item index;
- native PC integrations such as SM64EX, Ship of Harkinian and 2Ship2Harkinian.

This means the first implementation does not require additional ROM patches merely to detect a transition. An existing Archipelago location can be declared as a Crossworld trigger.

## Campaign Manifest

```yaml
schema: sekailink.crossworld/v1
campaign: Castle Between Worlds

games:
  - game: a_link_to_the_past
    runtime: sekaiemu
  - game: super_mario_64
    runtime: sm64ex

transitions:
  - id: alttp_lost_woods_to_sm64_castle
    source:
      game: a_link_to_the_past
      location: Lost Woods - Mushroom
    destination:
      game: super_mario_64
      entry: castle_lobby
```

The first visit can be triggered by `onLocationChecked(locationId)`. Because an Archipelago check is normally sent only once, repeatable portals need an additional lightweight memory condition, a dedicated return action, or a native game event.

## Runtime Director

The Crossworld Runtime Director runs independently of the visible Client Core interface.

Transition transaction:

1. Receive `crossworld.transition_requested` from SKLMI, Sekaiemu, or a native game adapter.
2. Pause the source runtime.
3. Persist the source game's logical campaign state.
4. Start or reactivate the destination runtime.
5. Load or construct the destination entry state.
6. Apply the destination game's persistent logical state.
7. Verify game identity, room, position, item index, and memory readiness.
8. Commit the transition and resume the destination.
9. Roll back to the source runtime if preparation fails.

Only one game needs to be visible. Frequently used runtimes may remain suspended in memory for fast transitions; less-used runtimes can close after their state is persisted.

## Entry Strategies

Destination adapters use the strongest available strategy in this order:

1. **Native entrance command**
   - Preferred for controllable PC ports.
   - The game loads a declared room or entrance through an internal API.

2. **Hydrated savestate template**
   - Preferred for compatible emulated games.
   - A known-good state already contains a correctly initialized scene, camera, audio, and object system.

3. **Normal save plus memory transition**
   - Load the campaign's normal save, wait for gameplay readiness, then write a stable entrance or room identifier.

4. **State-aware input automation**
   - Traverse title and file-selection screens with inputs gated by memory signatures rather than fixed delays.

5. **Resume last position**
   - Minimal generic fallback where precise destination placement is unavailable.

## Hydrated Savestate Templates

Each Crossworld destination may ship as a clean savestate captured at the correct location. Sekaiemu must not patch arbitrary bytes in a compressed savestate file.

Safe hydration process:

```text
load clean template through the core
  -> apply campaign state through the memory provider
  -> run a small number of stabilization frames
  -> serialize a hydrated state through the core API
  -> resume gameplay
```

Example template contract:

```json
{
  "schema": "sekailink.crossworld-state-template/v1",
  "gameId": "a_link_to_the_past",
  "entryId": "lost_woods",
  "romSha256": "...",
  "coreId": "snes9x",
  "coreBuild": "...",
  "coreOptionsHash": "...",
  "stateFormat": 1
}
```

Templates are tied to the exact ROM revision, core build, state format, and relevant core options. A mismatch must reject the template rather than risk corrupting progress.

## Persistent Logical State

The campaign stores state per game rather than copying RAM between unrelated games.

```json
{
  "game": "a_link_to_the_past",
  "inventory": {},
  "completedLocations": [],
  "receivedIndex": 42,
  "storyFlags": {},
  "health": 8,
  "currency": 120,
  "saveDataVersion": 1
}
```

Each game adapter translates this logical state into its own memory and save structures. The received-item index, inventory, checks, progression flags, and normal save must remain consistent so reconnecting does not duplicate or lose items.

## Game Adapter Contract

```text
is_ready()
prepare_transition()
export_campaign_state()
load_entry(entry_id)
apply_campaign_state(state)
verify_transition(entry_id, state)
commit_transition()
rollback_transition()
```

An adapter may additionally expose memory signatures for title screen, file selection, gameplay readiness, safe item delivery, and repeatable portal activation.

## Visual Experience

The technical boot sequence should remain hidden behind a full-screen Crossworld transition:

1. Source game freezes or pauses.
2. SekaiLink transition appears.
3. Destination runtime starts or resumes behind it.
4. Menus are traversed or a template is hydrated.
5. The director verifies the destination.
6. Transition clears only after gameplay is ready.

No Client Core loading screen or manual connection screen should appear.

## Recommended Prototype

Start with two games and two reciprocal transitions.

- One native PC port, preferably SM64EX or Ship of Harkinian.
- One emulated game with stable memory mapping and savestate support, preferably A Link to the Past.
- First iteration resumes a known destination state.
- Second iteration adds precise native entrance and hydrated savestate adapters.
- Persist each game's received-item index and normal save.
- Include timeout, verification, rollback, and a manual test-transition command.

Suggested first route:

```text
A Link to the Past location
  <->
SM64EX Castle Lobby
```

## Risks and Open Questions

- Savestate compatibility across core versions, platforms, and options.
- Safe synchronization between savestate memory and normal SRAM.
- Duplicate item delivery after changing runtimes.
- Repeatable portal detection after the original location is checked.
- Games whose room, camera, scripts, and object state cannot be safely changed through simple memory writes.
- Resource use when multiple native or emulated runtimes remain suspended.
- Transition rollback after destination startup or verification failure.
- Campaign recovery following a power loss during a transition transaction.
- Legal and distribution review for any state template that may contain copyrighted game-derived data.

## Scope Boundary

This design does not require patching a ROM merely to trigger transitions. Existing location checks and memory observation are sufficient for the first prototype. Game-specific ROM modifications remain optional and should only be considered where a native event or reliable memory/state adapter cannot provide the intended experience.
