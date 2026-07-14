# SekaiLink PC Game Package Contract

Status: draft v1

SekaiLink distributes integrations and open-source binaries only. Packages must not contain ROMs,
Nintendo assets, generated O2R archives, or other copyrighted game data supplied by the player.

## Launch contract

Client Core writes a temporary JSON file readable only by the current user and launches the game with:

```text
soh-sekailink --sekailink-launch <temporary-json-path>
```

The file is deleted after the child process has read it or exited. Secrets must not be passed directly
on the command line because process arguments can be visible to other local processes.

```json
{
  "schema": "sekailink.pc-launch/v1",
  "gameId": "ship-of-harkinian",
  "sessionId": "local-opaque-id",
  "connection": {
    "server": "room.example.test:38281",
    "slot": "Player",
    "password": "",
    "autoConnect": true
  },
  "paths": {
    "data": "/user-selected/data/path",
    "logs": "/sekailink/session/log/path"
  }
}
```

When a launch manifest is active, the port must display the connection but prevent editing the server,
slot, and password. Standalone launches retain the upstream editable connection screen.

## Download package manifest

Each CDN package entry must include:

- stable package ID and game ID;
- platform and architecture;
- semantic package version and release channel;
- archive URL, byte size, and SHA-256;
- executable relative path and launch-contract version;
- required user-owned files and accepted checksums;
- license and upstream attribution URLs;
- dependencies and optional components;
- minimum compatible Client Core version.

Client Core installs packages into a versioned staging directory, verifies every declared hash, and only
then atomically activates the new version. One previously working version is retained for repair or rollback.
