# Archipelago Client Wrappers

Date: 2026-06-17

This document describes the BETA-3 Archipelago client wrapper layer used by
SekaiLink. The goal is compatibility first: upstream Archipelago client logic
stays responsible for game memory, checks, received items, DeathLink, and
game-specific quirks.

## Runtime Contract

SekaiLink launches a small Python wrapper with its portable Python runtime.
The wrapper receives one JSON command per line on stdin and emits one JSON event
per line on stdout.

Release rule: packaged client builds must be zero-friction. They must use the
bundled Python runtime and bundled Python dependencies only. They must not use a
user-installed Python, must not download packages, and must not run a first-use
dependency installer. If a dependency is missing, that is a release packaging
failure, not something the player should fix.

Known supported-world extras are part of that bundled dependency contract:
`docutils`, `dolphin_memory_engine`, and `pyevermizer` are required by some
custom APWorlds/clients and must be present in the prepared runtime before a
Windows build is published.

Important commands:

- `{"cmd":"connect","address":"host:port","slot":"Player","password":"..."}`
- `{"cmd":"disconnect"}`
- `{"cmd":"say","text":"hello"}`
- `{"cmd":"command","text":"!missing"}`
- `{"cmd":"shutdown"}`

Important events:

- `ready`
- `log`
- `messagebox`
- `headless_blocked`
- `print`
- `print_json`
- `package`
- `room_info`
- `connected`
- `disconnected`
- `fatal`

The wrapper forces upstream Archipelago clients into headless mode. Native file
dialogs, message boxes, browser opens, and file-open requests are blocked and
reported as JSON events instead of opening windows.

Client Core also keeps a short in-memory trace of Archipelago wrapper events.
Bug reports append this trace to `logs_text`, after the normal log tail, so
crashes before or during client launch still carry useful context to the
Discord bug-report pipeline.

## Wrapper Families

### BizHawk

Used for NES, GB, GBC, and GBA clients that derive from Archipelago's generic
BizHawk client layer.

Entry points:

- `runtime/bizhawkclient_wrapper.py`
- `runtime/apclient_common.py --kind bizhawk`

The Archipelago runtime imports all bundled worlds and auto-registers handlers
through `AutoBizHawkClientRegister`. The active handler is selected by the ROM
or patch loaded in BizHawk.

### SNI

Used for SNES clients that derive from Archipelago's SNI client layer.

Entry points:

- `runtime/sniclient_wrapper.py`
- `runtime/apclient_common.py --kind sni`

SekaiLink starts the local SNI bridge before launching this client family. The
active handler is selected through `AutoSNIClientRegister` after the SNES ROM is
validated.

### Ocarina of Time / N64

Used for Ocarina of Time's N64 Lua connector.

Entry points:

- `runtime/ootclient_wrapper.py`
- `runtime/apclient_common.py --kind oot`

This is not the generic BizHawk/SNI model. The upstream OoT client owns its N64
sync task and expects the OoT connector flow.

### Module Clients

Used for upstream clients that expose a game-specific `main()` but do not fit
the generic BizHawk, SNI, or OoT wrappers.

Entry points:

- `runtime/moduleclient_wrapper.py`
- `runtime/apclient_common.py --kind module --module worlds.albw.Client`

This family is used for clients such as ALBW, LADX, and DK64. The wrapper still
blocks native dialogs and translates stdout/stderr into SekaiLink events, but
the game-specific module owns its own connector details.

### Dolphin / GameCube

Used for GameCube and Wii clients that use `dolphin_memory_engine`.

Entry points:

- `runtime/dolphinclient_wrapper.py`
- `runtime/apclient_common.py --kind dolphin --module worlds.tww.TWWClient`

The wrapper supervises Dolphin-style clients as long as their APWorld/client
module exists in `runtime/ap/worlds` or another active AP root.

## Registry

The client matrix lives at:

`runtime/game-registry/archipelago-clients.json`

SekaiLink may start clients by `gameKey`, `world`, `game`, `wrapper`, or direct
module. When a game key is supplied, Electron resolves it through the registry
and starts the correct wrapper family.

Current matrix after the 2026-06-17 catalog import:

- SNES via SNI: ALttP, Chrono Trigger Jets of Time, DKC, DKC2, DKC3,
  EarthBound, FF4FE, FFMQ, FFV Career Day, KDL3, Lufia II Ancient Cave,
  Mega Man X3, Super Mario World, Super Metroid, SMZ3.
- NES via BizHawk: Final Fantasy, The Legend of Zelda, Zelda II, Mega Man 2,
  Mega Man 3.
- GB/GBC via BizHawk: Pokemon Red/Blue, Pokemon Crystal, Wario Land, Super
  Mario Land 2, Oracle of Ages, Oracle of Seasons.
- GBA via BizHawk: Pokemon Emerald, Pokemon FRLG, Metroid Fusion,
  Metroid Zero Mission, Minish Cap, Mario & Luigi Superstar Saga, Wario Land 4.
- GBA via BizHawk, temporarily unavailable: FFTA. It connects, but mission
  reward based item/check transactions are unable to test manually and need a
  dedicated FFTA tester before public exposure.
- Module clients: A Link Between Worlds, Link's Awakening DX, Donkey Kong 64.
- N64 via OoT client: Ocarina of Time.
- Dolphin clients: Mario Kart Double Dash, Metroid Prime, Skyward Sword,
  Super Mario Sunshine, Wind Waker, TTYD, Twilight Princess.
- Generator-only/text entries currently present without a memory wrapper:
  Mega Man Battle Network 3, Secret of Evermore, Super Mario 64.

## Current Limits

- The SekaiLink Client Core game catalog has 55 entries. This import pass found
  local APWorld sources for 48.
- Missing local APWorld sources: Final Fantasy VI, Luigi's Mansion, Majora's
  Mask, Paper Mario, Secret of Mana, Star Fox 64, Tetris Attack.
- FF4FE is present, but still marked `special_setup_required` because its setup
  is mod-specific and heavier than the generic wrapper families.
- `generator_only` entries have APWorld generation logic but no direct memory
  client wrapper in this pass.
- These wrappers do not replace game logic. They only launch, supervise, and
  translate upstream client output into SekaiLink events.

## SKLMI Manifest Metadata

SKLMI bridge manifests may now carry a passive `archipelago_client_wrapper`
object. This lets a manifest describe which Archipelago client family is
expected for the game while keeping the actual wrapper process supervised by
Client Core/Sekaiemu for this first BETA-3 layer.

Example:

```json
{
  "archipelago_client_wrapper": {
    "enabled": true,
    "game_key": "alttp",
    "game": "A Link to the Past",
    "platform": "SNES",
    "world": "worlds.alttp",
    "wrapper": "sni",
    "client_file": "worlds/alttp/__init__.py",
    "status": "enabled",
    "requires": ["sni", "portable_python"]
  }
}
```

Accepted `wrapper` values are `text`, `bizhawk`, `sni`, `oot`, `dolphin`, and
`module`. `module` and `dolphin` entries must declare either `module` or
`client_file` unless the entry is explicitly marked `generator_only`.

When SKLMI loads a manifest with this metadata, it writes a structured
`archipelago_client_wrapper` trace event into its JSONL runtime log and includes
the same summary in bug-report `app_info`. This is deliberately verbose:
operators should be able to see the intended AP client family, world module,
client file, and dependency hints in the bug report without reproducing the
launch locally.
