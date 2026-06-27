# SekaiLink Bootloader

The public SekaiLink bootloader is a small native C++ updater. It checks the
public `client-bundle` release endpoint, downloads the current client bundle,
verifies SHA-256, extracts it with the embedded `miniz` ZIP reader, installs it,
then launches SekaiLink with the required bootstrap token.

Runtime dependencies are intentionally small:

- Windows: `SekaiLink Bootloader.exe` plus the bundled MinGW/MSYS2 DLL closure.
- Linux: `sekailink-bootloader` using the platform SDL2, libcurl, and OpenSSL
  runtime libraries.

Defaults:

- channel: `test`
- build: `release`
- Windows install dir: `%LOCALAPPDATA%\Programs\sekailink-client`
- Linux install dir: `~/.local/share/sekailink-client`
- logs: `%APPDATA%\sekailink-bootloader\logs` or
  `~/.local/state/sekailink-bootloader/logs`

Release endpoint:

`https://sekailink.com/api/client/release-latest?channel=test&platform=<platform>&build=release`

The bootloader writes both install-state copies expected by Client Core:

- `.sekailink/install-state.json`
- bootloader state `install-state.json`

It also creates/refreshes `.sekailink/launcher-secret.key`, signs the launch
payload with HMAC-SHA256, and starts the client with:

- `SEKAILINK_BOOTSTRAP_INSTALL_DIR`
- `SEKAILINK_BOOTSTRAP_STATE_DIR`
- `SEKAILINK_BOOTSTRAP_TOKEN`
- `SEKAILINK_REQUIRE_BOOTSTRAP=1`

Developer smoke helpers:

```bash
cmake -S apps/client-core/native-bootloader -B /tmp/sekailink-native-bootloader-build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/sekailink-native-bootloader-build -j4
/tmp/sekailink-native-bootloader-build/sekailink-bootloader --no-ui --no-launch --force --install-dir /tmp/sekailink-native-install-test
```

Package both public archives:

```bash
cd apps/client-core
npm run bootstrapper:pack
```
