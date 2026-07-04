# Nexus Room Query Service

Deployment artifacts for the private native room-query service on `Nexus`.

Target runtime:

- config: `/opt/sekailink/nexus/api-gateway/config/nexus_room_query.json`
- data/state: `/opt/sekailink/nexus/api-gateway/data/`
- binary: `/opt/sekailink/nexus/api-gateway/bin/sekailink_evolution_room_query_service`
- systemd: `sekailink-nexus-room-query.service`

This service is intended to replace the transitional DB/API role previously hosted on `evolution`.
