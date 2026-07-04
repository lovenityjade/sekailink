# Worlds Generation Server Deployment

This directory contains the first deployment artifacts for the native C++ generation server on `worlds`.

Files:
- `generation_server.json.example`: runtime configuration template
- `sekailink-generation-server.service`: `systemd` unit template

The generator itself remains external. The native service only schedules and launches it.
