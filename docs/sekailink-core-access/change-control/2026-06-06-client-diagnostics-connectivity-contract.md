# Connectivity Contract - Client Diagnostics Bundle

## Contract name

- Name: Client diagnostics bundle upload
- Owner service: Client Core plus Link client reports endpoint
- Consumers: Core Access, Link support logs, incident workspaces
- Version: draft-2026-06-06

## Endpoint or event

- Method/channel: future authenticated client upload or realtime request/response
- Path/event name: draft `client.diagnostics.requested` and `client.diagnostics.uploaded`
- Auth: logged-in SekaiLink user session plus server-side capability check
- Capability: Service may request; user consent is required before upload

## Request payload

```json
{
  "request_id": "client-diagnostics-<id>",
  "target_user": "<username>",
  "incident": "<incident-label>",
  "reason": "<operator-visible reason>",
  "include": ["client-core", "sekaiemu", "sklmi", "configs", "system"],
  "requested_by": "<operator>",
  "consent_required": true
}
```

## Response payload

```json
{
  "request_id": "client-diagnostics-<id>",
  "state": "uploaded",
  "bundle_id": "<server-side bundle id>",
  "redacted": true,
  "files": [
    "manifest.json",
    "client-core/main.log",
    "client-core/renderer.log",
    "client-core/updater.log",
    "sekaiemu/sekaiemu.log",
    "sekaiemu/stdout.log",
    "sekaiemu/stderr.log",
    "sklmi/sklmi.log",
    "sklmi/stdout.log",
    "sklmi/stderr.log",
    "configs/client-settings.redacted.json",
    "configs/launch-context.redacted.json",
    "system/platform.txt"
  ]
}
```

## Timing

- Frequency: on demand only
- Timeout: client should finish bundle creation within 60 seconds
- Retry: user-triggered retry only
- Backoff: not applicable for MVP

## Error model

- Error codes: `consent_denied`, `bundle_failed`, `upload_failed`,
  `redaction_failed`, `missing_runtime_log`
- Recoverable errors: missing optional Sekaiemu/SKLMI log, upload timeout
- Fatal errors: redaction failure, user consent denied
- Client display: show a clear consent prompt and never upload silently

## Client Core impact

Client Core must eventually:

- receive a diagnostics request;
- show a consent prompt to the user;
- collect allowed local files;
- redact secrets before upload;
- include client version, OS, install path and app data path;
- upload the bundle or produce a clear failure status.

No Client Core implementation is included in the Core Access draft commit.

## Sekaiemu impact

Future implementation should collect existing Sekaiemu logs and captured
stdout/stderr from the launcher. It should not change emulator behavior, window
behavior, game execution, or libretro core behavior just to collect diagnostics.

## SKLMI impact

Future implementation should collect existing SKLMI companion runtime logs and
captured stdout/stderr from the launcher.

If SKLMI does not already expose the required log files, any SKLMI code change
requires explicit written approval from Jade and a separate SKLMI approval
request before implementation.

## Platform sync

- Linux: paths must cover app data, packaged runtime, Sekaiemu, and SKLMI logs.
- Windows: paths must cover `%APPDATA%`, `%LOCALAPPDATA%`, packaged runtime,
  Sekaiemu, and SKLMI logs.
- Release gate: diagnostics upload is blocked until Linux and Windows bundle
  contents are verified to be equivalent.
