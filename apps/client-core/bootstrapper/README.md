# SekaiLink Bootstrapper MVP

These scripts install or update SekaiLink from the public `client-bundle`
release endpoint before launching the client.

- Windows: run `SekaiLink-bootstrapper.cmd`
- Linux: run `sekailink-bootstrapper.sh`

Defaults:

- channel: `test`
- build: `release`
- Windows install dir: `%LOCALAPPDATA%\Programs\sekailink-client`
- Linux install dir: `~/.local/share/sekailink-client`

Both scripts read:

`https://sekailink.com/api/client/release-latest?channel=test&platform=<platform>&build=release`

They download the `artifact_type=client-bundle` zip, verify SHA-256, unpack it,
write `.sekailink/install-state.json`, then launch the updated client.
