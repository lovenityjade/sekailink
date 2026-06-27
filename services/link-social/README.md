# sekailink-link-social

Role:

- presence
- contacts
- DM
- salons
- room chat

Surface active extraite:

- `include/`
- `src/`
- `docs/`

Surface snapshot:

- `source-snapshots/monolith-core/`

Reference:

- `../../../docs/repo-contracts/sekailink-link-social.md`

Presence contract:

- `/api/social/friends` and `/api/social/users/search` return `status`,
  `presence_status`, `is_online`, and `last_seen` for social profiles.
- `status`/`presence_status` are visible presence values. A user is reported
  online only when their `social_profiles.updated_at` heartbeat is recent and
  their saved social setting is not offline/appear-offline.
- Unknown, stale, or missing presence is reported as offline so Client Core does
  not show closed clients as online.
