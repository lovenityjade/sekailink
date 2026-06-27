# SekaiLink Bug Report Payload v1

Date: 2026-06-11

All SekaiLink runtimes send bug reports to the public Link API endpoint:

```text
POST /api/client/bug-report
```

The Link API is the only component that forwards to the Discord social bot. Client
applications must not ship Discord bot tokens or `X-SekaiLink-Bot-Key`.

## Required Fields

```json
{
  "title": "Bootloader error",
  "description": "sha256_mismatch",
  "reporter_name": "SekaiLink User",
  "screenshot_base64": "",
  "logs_text": "tail of the relevant log",
  "system_info": {},
  "app_info": {
    "source": "bootloader",
    "component": "native-bootloader"
  }
}
```

## Sources

Use the same payload shape from every sphere:

- `bootloader`
- `client-core`
- `sekaiemu`
- `sklmi`

`app_info.source` is the canonical sphere name. `app_info.component` can be more
specific, such as `native-bootloader`, `sekailink-client-core`,
`sekaiemu-host`, or `sklmi-runtime`.

## Native Runtime Hooks

Sekaiemu and SKLMI submit automatic reports only for runtime-level failures, not
for ordinary CLI usage mistakes. This keeps Discord focused on actionable
incidents.

Sekaiemu reports:

- Patch materialization failures.
- Libretro core/game initialization failures.
- Non-zero runtime exits.

SKLMI reports:

- Tracker initialization failures.
- Bridge manifest load failures.
- Memory provider connection failures.
- Tracker snapshot publication failures.
- Room/session start and tick failures.

Both native runtimes use:

- `SEKAILINK_BUG_REPORT_API_BASE` to override the default
  `https://sekailink.com`.
- `SEKAILINK_BUG_REPORT_DISABLED=1` to disable automatic submission.
- `SEKAILINK_REPORTER_NAME` to override the reporter name.

## Field Limits

The current Discord bot contract accepts:

- `title`: 3 to 100 characters.
- `description`: 1 to 200 characters.
- `reporter_name`: 1 to 80 characters.
- `screenshot_base64`: optional PNG/JPEG base64 without a data URL prefix.
- `logs_text`: optional plain text log tail.
- `system_info`: JSON object.
- `app_info`: JSON object.

The Link API trims oversized screenshots to 16 MiB of base64 text and logs to
1 MiB before forwarding to the bot.

## Server Forwarding

`services/link-social` forwards normalized reports to:

```text
POST /discord/bug-report
X-SekaiLink-Bot-Key: <server secret only>
```

Configure the forwarder with either config JSON or environment variables:

```json
{
  "social_bots_host": "127.0.0.1",
  "social_bots_port": 8095,
  "social_bots_control_api_key": "redacted"
}
```

Environment fallbacks:

- `SEKAILINK_SOCIAL_BOTS_API_KEY`
- `SEKAILINK_BOT_CONTROL_API_KEY`

If the key is missing, the Link API keeps compatibility by returning
`accepted:true` and `forwarded:false`.
