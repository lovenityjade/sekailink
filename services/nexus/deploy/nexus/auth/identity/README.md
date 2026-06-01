# Nexus Native Identity Service

Deployment artifacts for the private native identity service on `Nexus`.

Target runtime:

- config: `/opt/sekailink/nexus/auth/config/identity_service.json`
- data: `/opt/sekailink/nexus/auth/data/identity.sqlite3`
- binary: `/opt/sekailink/nexus/auth/bin/sekailink_identity_service`
- systemd: `sekailink-nexus-identity.service`

The service is intended to run privately on `Nexus` as the future central auth/account backend.
