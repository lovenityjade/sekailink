# SekaiLink Release Channels and Bootstrapper Updates

## Policy

- `canonical` is the default player release.
- `canari` is opt-in from Client Core advanced Settings.
- Existing `test` and `stable` requests are compatibility aliases for `canonical`.
- A build is considered `canari` until it is explicitly approved for Canonical promotion.
- Reverting to Canonical writes the preference immediately, then the bootloader downloads Canonical on the next restart.
- Every Windows patch/update must include the full packaged runtime DLL closure.
  Do not ship "client-only" Windows bundles with DLLs omitted: the bootloader may
  install that bundle over an older install where the needed dependency is absent.
  `apps/client-core/scripts/windows-runtime-contract.mjs` is the source of truth.

## CDN Layout

Client bundles:

```text
/downloads/client/canonical/<date>/
/downloads/client/canari/<date>/
```

Bootstrapper bundles:

```text
/downloads/client/bootstrapper/canonical/<date>/
/downloads/client/bootstrapper/canonical/latest/
/downloads/client/bootstrapper/canari/<date>/
/downloads/client/bootstrapper/canari/latest/
```

The static bootstrapper fallback manifest must be named:

```text
sekailink-bootstrapper-release-latest.json
```

## API Endpoints

Client bundle:

```text
/api/client/release-latest?channel=canonical&platform=linux-x64&build=release
/api/client/release-latest?channel=canari&platform=win32-x64&build=release
```

Bootstrapper:

```text
/api/client/bootstrapper-latest?channel=canonical&platform=linux-x64&build=release
/api/client/bootstrapper-latest?channel=canari&platform=win32-x64&build=release
```

## Manifest Shape

`client_release_latest.json` can include bootstrapper releases next to client releases:

```json
{
  "releases": [],
  "bootstrapper": {
    "releases": [
      {
        "version": "1.0.0-20260702",
        "channel": "canonical",
        "build": "release",
        "implementation": "native-cpp-sdl2-miniz",
        "artifacts": [
          {
            "fileName": "SekaiLink-bootstrapper-1.0.0-20260702-windows.zip",
            "url": "https://sekailink.com/downloads/client/bootstrapper/canonical/20260702/SekaiLink-bootstrapper-1.0.0-20260702-windows.zip",
            "sha256": "<sha256>",
            "size": 0
          }
        ]
      }
    ]
  }
}
```
