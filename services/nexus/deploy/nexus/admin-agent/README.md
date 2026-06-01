# Nexus Admin Agent

Deployment artifacts for the private native admin-agent on `Nexus`.

Target runtime:

- config: `/opt/sekailink/nexus/admin-agent/config/admin_agent.json`
- binary: `/opt/sekailink/nexus/admin-agent/bin/sekailink_admin_agent_service`
- systemd: `sekailink-nexus-admin-agent.service`
- sudoers template: `sekailink-admin-agent.sudoers.example`

The first service inventory is expected to cover:

- `identity`
- `room-query`
- `mariadb`

Current live scope also includes whitelist-only `start/stop/restart` on declared `systemd` units.
