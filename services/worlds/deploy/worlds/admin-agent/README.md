# Worlds Admin Agent

This directory contains deployment artifacts for the native admin agent on `worlds`.

Files:

- `admin_agent.json.example`
- `sekailink-admin-agent.service`
- `sekailink-admin-agent.sudoers.example`

The `worlds` admin agent is intended to stay loopback-only and provide operational visibility into the heavy backend services hosted there.

Current live scope also includes whitelist-only `start/stop/restart` on declared `systemd` units.
