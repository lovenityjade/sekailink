# SekaiLink Core Access Web

Admin web cockpit for SekaiLink moderation and operations.

## Run locally

```sh
npm run dev
```

The UI stores endpoint settings in local browser storage. Defaults route every
service through the Admin Gateway:

- Admin Gateway: `/api/admin`
- Identity: `/api/admin`
- Lobby Admin: `/api/admin`
- Chat Gateway: `/api/admin`
- Admin Agent: `/api/admin`
- Room Server: `/api/admin`

Older local browser settings using `/identity`, `/lobby-admin`,
`/chat-gateway`, `/admin-agent`, or `/room-server` are migrated automatically.
Those Vite proxy paths still exist for direct lab testing, but production should
keep the browser on `/api/admin` only.

Use an admin bearer token in the panel when testing against protected services.

## Build

```sh
npm run build
```

## Serve admin.sekailink.com locally

```sh
SEKAILINK_ADMIN_WEB_TOKEN=change-me npm run build
SEKAILINK_ADMIN_WEB_TOKEN=change-me npm run serve:admin
```

The gateway serves `dist/` and exposes `/api/admin/*`. It keeps internal service
tokens server-side:

- `SEKAILINK_IDENTITY_ADMIN_TOKEN`
- `SEKAILINK_LOBBY_ADMIN_TOKEN`
- `SEKAILINK_CHAT_GATEWAY_TOKEN`
- `SEKAILINK_ADMIN_AGENT_TOKEN`
- `SEKAILINK_ROOM_SERVER_ADMIN_TOKEN`

For local-only testing without a token:

```sh
SEKAILINK_ADMIN_ALLOW_NO_TOKEN=1 npm run serve:admin
```

Do not expose `SEKAILINK_ADMIN_ALLOW_NO_TOKEN=1` publicly.

## Production target

Deployment templates live in `deploy/`:

- `admin-gateway.env.example`
- `sekailink-admin-gateway.service`
- `nginx-admin.sekailink.com.conf`

Production should expose only nginx on `admin.sekailink.com`. Keep Identity,
Lobby Admin, Chat Gateway, Admin Agent, and Room Server on loopback/private
interfaces. The browser talks only to `/api/admin/*`; the gateway owns internal
service tokens.

Install on the server after `npm run build`:

```sh
sudo bash deploy/install-admin-gateway.sh
```

## First connected areas

- Dashboard with server status, users, lobbies, chat presence, and reports.
- User search/detail with sessions and audit.
- Lobby list/detail with close action.
- Chat channel viewer with messages and presence.
- Service inventory, logs, and start/stop/restart actions.
- Diagnostic report viewer.

The preferred production path is an Admin Gateway at `/api/admin` that holds
private service tokens and enforces Nexus RBAC. The UI also falls back to direct
local service endpoints for lab testing.
