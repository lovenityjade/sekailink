# SekaiLink Bootloader

The public SekaiLink bootloader is a small native C++ updater. It checks the
public `client-bundle` release endpoint, downloads the current client bundle,
verifies the pinned Ed25519 release signature and SHA-256 digest, rejects signed
rollback attempts, extracts the bundle with the embedded `miniz` ZIP reader,
installs it, then launches SekaiLink with the required bootstrap token.

Runtime dependencies are intentionally small:

- Windows: `SekaiLink Bootloader.exe` plus the bundled MinGW/MSYS2 DLL closure.
- Linux: `sekailink-bootloader` using the platform SDL2, libcurl, and OpenSSL
  runtime libraries.

Defaults:

- channel: `canonical`
- build: `release`
- Windows install dir: `%LOCALAPPDATA%\Programs\sekailink-client`
- Linux install dir: `~/.local/share/sekailink-client`
- logs: `%APPDATA%\sekailink-bootloader\logs` or
  `~/.local/state/sekailink-bootloader/logs`
- channel preference: `%APPDATA%\sekailink-bootloader\release-channel.json` or
  `~/.local/state/sekailink-bootloader/release-channel.json`

Release endpoint:

`https://sekailink.com/api/client/release-latest?channel=canonical&platform=<platform>&build=release`

Release channels:

- `canonical`: default player release. Existing `test`/`stable` requests are treated
  as aliases for Canonical during the BETA-3 transition.
- `canari`: opt-in test release. Client Core writes the channel preference from
  advanced Settings; the next bootloader launch downloads the Canari client bundle.
  A Canari build stays Canari until it is explicitly promoted to Canonical.

Client Core also checks for bootloader updates after launch. The bootloader update
endpoint is:

`https://sekailink.com/api/client/bootstrapper-latest?channel=canonical&platform=<platform>&build=release`

The static CDN fallback for bootloader manifests is:

`https://sekailink.com/downloads/client/bootstrapper/<channel>/latest/sekailink-bootstrapper-release-latest.json`

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

Build Canari archives:

```bash
cd apps/client-core
SEKAILINK_RELEASE_CHANNEL=canari npm run bootstrapper:pack
```

## Signing a release manifest

The release-signing private key is an offline deployment secret and must never
be copied to a VPS, CDN directory, update archive, log, or Git repository. The
bootloader pins the corresponding Ed25519 public key.

Every release entry must contain a positive `release_sequence`. It must increase
for each publication; unlike a display version, this value is the anti-rollback
authority. Sign a prepared manifest locally before its atomic deployment:

```bash
python3 apps/client-core/scripts/sign-release-manifest.py \
  /path/to/client_release_latest.json \
  --release-sequence YYYYMMDDNN \
  --private-key "$HOME/.local/share/sekailink-security/release-signing/ed25519-private.pem"
```

Publication gates:

- Verify every artifact SHA-256 before signing.
- Verify the signed API response using the offline public key.
- Build and test both Linux and Windows bootloaders before distributing a
  bootloader that enforces signatures.
- Never decrease or reuse `release_sequence` for different artifact content.
- Keep the previous signed manifest and artifacts as the single fallback build.
