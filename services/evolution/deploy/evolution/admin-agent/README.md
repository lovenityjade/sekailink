# Evolution Admin Agent

This directory contains deployment artifacts for the native admin agent on `evolution`.

Files:

- `admin_agent.json.example`
- `sekailink-admin-agent.service`
- `sekailink-admin-agent.sudoers.example`
- `update-postfix-tail.sh`
- `sekailink-postfix-tail.service`
- `sekailink-postfix-tail.timer`

This service is intended to stay loopback-only and provide operational visibility into the native `evolution` services.

Current live scope also includes whitelist-only `start/stop/restart` on declared `systemd` units.

The postfix snapshot helper exists so the admin agent can expose recent mail activity without re-reading the full live mail log on every request.
