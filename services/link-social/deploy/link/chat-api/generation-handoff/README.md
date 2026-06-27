# Link Chat API Generation Handoff

This is the Pre-BETA3 bridge between Link lobby state and the live Worlds
generation queue.

The database config remains authoritative. The handoff script only writes
transient generator input files under the configured spool directory.

## Runtime Shape

`POST /api/lobbies/:id/generate` in Chat API writes a JSON request and runs the
configured command:

```json
{
  "generation_handoff_root": "/opt/sekailink/link/chat-api/data/generation-handoff",
  "generation_handoff_command": [
    "/opt/sekailink/link/chat-api/generation-handoff/sekailink_generation_handoff.py",
    "--request",
    "{request_path}",
    "--response",
    "{response_path}",
    "--spool-root",
    "/opt/sekailink/link/chat-api/data/generation-spool"
  ]
}
```

The script writes:

- `Players/*.yaml`: transient backend input exported from DB config snapshots
- `manifest.json`: hash and source metadata for the exported config files
- `generation_state.json`: local status cache for status polls
- `output/`: output directory passed to Worlds

## Worlds Connection

Set these environment variables on the `sekailink-chat-api.service` runtime if
the Worlds generation TCP service is reachable from Link:

```bash
SEKAILINK_GENERATION_HOST=127.0.0.1
SEKAILINK_GENERATION_PORT=19193
SEKAILINK_GENERATION_TOKEN=replace-generation-token
SEKAILINK_GENERATION_TIMEOUT_SECONDS=8
```

The host may be a local SSH tunnel or another private-only route. On the live
Link node, `19193` is the local tunnel to the private Worlds generation service.
Do not expose the Worlds generation TCP service publicly.

Because Worlds runs the generator on a different machine, the transient
`Players/` directory must be copied to a Worlds-local spool before `submit_job`
is called. Configure:

```bash
SEKAILINK_GENERATION_REMOTE_HOST=sekailink-generation@worlds.sekailink.com
SEKAILINK_GENERATION_REMOTE_SPOOL_ROOT=/srv/sekailink-generation-handoff/chroot/link-handoff
SEKAILINK_GENERATION_REMOTE_SFTP_ROOT=/link-handoff
SEKAILINK_GENERATION_SFTP_COMMAND='sftp -i /opt/sekailink/link/chat-api/config/worlds-handoff.key -o BatchMode=yes -o StrictHostKeyChecking=accept-new'
SEKAILINK_GENERATION_REMOTE_TIMEOUT_SECONDS=20
```

The script still keeps an audit copy under Link's local spool. The remote copy is
only the backend compatibility input for the generator. The live setup should use
a dedicated chrooted SFTP account on Worlds. If a generation service is
configured but no remote spool is configured, the script returns
`generation_remote_spool_not_configured` instead of submitting paths that Worlds
cannot read.

For the BETA-3 backend generator path, the Worlds `command_template` should use
the transient players directory as its input. The generation service still calls
the field `yaml_path`, but the value may be a directory:

```json
[
  "/opt/sekailink/worlds/generation-server/runtime/python/bin/python3",
  "/opt/sekailink/worlds/generation-server/runtime/Generate.py",
  "--player_files_path",
  "{yaml_path}",
  "--outputpath",
  "{output_dir}"
]
```

The handoff is game-generic. New Pre-BETA3 games should be introduced through
database-backed config snapshots, prepared upstream world support, ROM/runtime
setup when a game needs it, and optional tracker/runtime metadata. Link should
not need a new server route or game-specific generation branch for each game.

## Room Runtime Admin Password

When the handoff starts an Archipelago `MultiServer`, it now supplies
`--server_password` so admin tools can use `!admin login ...` followed by server
commands such as `!admin /send <slot> <item>`.

Default behavior:

- a unique password is generated per generation state;
- the normal `/api/room_status/<room_id>` response does not expose it;
- `/api/room_admin_secrets/<room_id>` can retrieve it only when the chat API is
  configured with `room_admin_tool_token` or `SEKAILINK_ROOM_ADMIN_TOOL_TOKEN`.

For lab runs that need a fixed password, set:

```bash
SEKAILINK_ROOM_RUNTIME_SERVER_PASSWORD=replace-lab-password
```

If `SEKAILINK_ROOM_RUNTIME_COMMAND` overrides the default command, it can use the
`{server_password}` placeholder.

If the upstream AP/MWGG environment is too expensive or noisy to load fully, the
runtime may set `SEKAILINK_WORLD_FILTER` as a deployment allowlist. The first
showcase can scope it to `alttp`; later showcases can expand it to a
comma/space-separated set of validated upstream world IDs, or remove it entirely
when the host is ready. This filter is operational configuration, not the
SekaiLink product contract.

## Status Polls

`GET /api/lobbies/:id/generation` asks the same command for
`generation_status`. If the Worlds service is configured, the script queries
`job_status`; otherwise it returns the cached local state.

The generated `AP_*.zip` is the complete Sync package. It may contain every
player/game entry in the room, plus multidata and per-entry launch artifacts.
The Chat API response keeps `response.artifact_path` for compatibility, but the
preferred field is `response.sync_package_path` for the full zip.

The response also includes:

- `response.sync_entries`: the DB-backed user/game entries exported into
  transient generator input files.
- `response.launch_entries`: per-entry launch artifacts indexed from the Sync
  package and associated back to users/configs when possible. Launch artifacts
  are extracted into the job output tree and served later through the bounded
  Chat API `generation_artifacts` route.
- `response.players_dir` and `response.output_dir`: operational paths for Link
  Room orchestration.

Core should launch from the current user's `launch_entries`/download view, not
from the whole Sync package.

## Optional Room Runtime

When the Sync package is ready, Link can also start the background room runtime
from the package's `.archipelago` member. This keeps the Pre-BETA3 path on the
existing Archipelago/MultiworldGG room server instead of introducing a new room
protocol.

Enable it only on a runtime host that has the AP/MWGG environment prepared:

```bash
SEKAILINK_ROOM_RUNTIME_ENABLED=1
SEKAILINK_ROOM_RUNTIME_HOST=127.0.0.1
SEKAILINK_ROOM_RUNTIME_PUBLIC_HOST=link.example.internal
SEKAILINK_ROOM_RUNTIME_MULTISERVER=/opt/sekailink/link/ap-runtime/MultiServer.py
SEKAILINK_ROOM_RUNTIME_PYTHON=/opt/sekailink/link/ap-runtime/python/bin/python3
SEKAILINK_ROOM_RUNTIME_PORT_RANGE=38290-38390
```

Room runtime saves are enabled by default and stored under the generation's
`room-runtime/saves` directory. Set `SEKAILINK_ROOM_RUNTIME_DISABLE_SAVE=1`
only for disposable test rooms.

Advanced deployments can replace the default command with
`SEKAILINK_ROOM_RUNTIME_COMMAND`. Supported placeholders are:

- `{multidata_path}`
- `{host}`
- `{port}`
- `{room_id}`
- `{generation_id}`
- `{output_dir}`
- `{runtime_root}`
- `{savefile}`

The status response publishes `room_status`, `room_host`, `last_port`,
`room_runtime_log`, `room_runtime_savefile`, and `room_multidata_path`. Core uses
the room status endpoint to resolve the final `host:port` passed to
Sekaiemu/SKLMI.
