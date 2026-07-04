# Link Chat Daemon

Date: 2026-04-17

`link` now hosts the first SekaiLink chat daemon foundation.

## Runtime

- Unit: `sekailink-chat-daemon.service`
- Runtime root: `/opt/sekailink/link/chat-daemon`
- Config: `/opt/sekailink/link/chat-daemon/conf/inspircd.conf`
- Listener: `127.0.0.1:16667`
- User/group: `sekailink:sekailink`

## Gateway Runtime

- Unit: `sekailink-chat-gateway.service`
- Runtime root: `/opt/sekailink/link/chat-gateway`
- Binary: `/opt/sekailink/link/chat-gateway/bin/sekailink_chat_gateway_service`
- Config: `/opt/sekailink/link/chat-gateway/config/chat_gateway.json`
- Listener: `127.0.0.1:19098`
- Upstream daemon: `127.0.0.1:16667`
- Public exposure: none

## Public API Adapter Runtime

- Unit: `sekailink-chat-api.service`
- Runtime root: `/opt/sekailink/link/chat-api`
- Binary: `/opt/sekailink/link/chat-api/bin/sekailink_chat_api_service`
- Config: `/opt/sekailink/link/chat-api/config/chat_api.json`
- Listener: `127.0.0.1:19099`
- Identity upstream: `149.202.61.90:19095`
- Private gateway upstream: `127.0.0.1:19098`
- Public exposure: Apache `/api/chat/` only

## Scope

This is the IRC-like realtime foundation for:

- global chat
- lobby chat
- private messages
- channel presence
- role-aware user lists
- future enriched WHOIS/profile mapping

## Security State

- The daemon is loopback-only.
- The gateway is loopback-only.
- The public chat API adapter is loopback-only and exposed only through Apache `/api/chat/`.
- Public/Core access must go through the chat API adapter.
- The gateway currently requires an internal bearer service token for non-health routes.
- The chat API adapter validates Nexus session tokens and rebuilds message identity server-side.
- Nexus remains the identity/profile source of truth.
- Link remains the lobby/room realtime policy source.

## Gateway API

- `GET /health`: local probe, no auth.
- `GET /channels`: returns the current global channel catalog, requires the service token.
- `GET /channels/<channel-id>/messages`: returns the gateway recent-message cache, requires the service token.
- `POST /channels/<channel-id>/messages`: relays a message to the private IRC daemon, requires the service token.

The private gateway also keeps a short-lived process-memory cache for low-level smoke tests. Public message history comes from the chat API adapter SQLite store.

## Public Chat API

Public clients call `/api/chat/...` on `sekailink.com`.

Supported adapter routes:

- `GET /api/chat/health`
- `GET /api/chat/channels`
- `GET /api/chat/channels/<channel-id>/messages`
- `POST /api/chat/channels/<channel-id>/messages`
- `GET /api/chat/channels/<channel-id>/presence`
- `POST /api/chat/channels/<channel-id>/presence`

For message posts, clients send only:

```json
{"content":"hello"}
```

The adapter validates the bearer session against Nexus and forwards the server-authoritative author to the private gateway.

Current public channel policy:

- `global:fr`
- `global:en`

Lobby channels and DMs intentionally return `channel_forbidden` until Link membership and friends/blocklist policy are wired.

Current chat API storage:

- SQLite: `/opt/sekailink/link/chat-api/data/chat_api.sqlite3`
- Scope: recent basic message history by channel.
- Presence: authenticated heartbeat with 90-second active window.
- Source of author identity: Nexus `/me`, never client-provided payload.

Generation handoff:

- Optional config keys:
  - `generation_handoff_root`
  - `generation_handoff_command`
- Deployment template:
  - `deploy/link/chat-api/chat_api.json.example`
- Handoff script:
  - `deploy/link/chat-api/generation-handoff/sekailink_generation_handoff.py`
- The DB config is authoritative. The handoff writes only transient backend
  generator inputs before submitting to Worlds.

Durable moderation/report storage is still staged separately.

Current built-in channels:

- `global:fr` -> `#global-fr`
- `global:en` -> `#global-en`

## Next Integration Work

- Session-token auth against Nexus.
- Role/profile mapping.
- Lobby membership channel policy.
- Friends/blocklist DM policy.
- Core UI chat adapter.
- User report/moderation persistence.
