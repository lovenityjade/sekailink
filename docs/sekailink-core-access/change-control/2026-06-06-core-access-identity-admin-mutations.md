# Change Control - Core Access Identity Admin Mutations

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access can now prepare and execute gated Nexus Identity admin mutations:

- `user create`
- `user edit`
- `user disable`
- `user revoke-sessions`
- `user force-password-reset`

Each command is dry-run by default and prints the exact HTTP method, endpoint,
token env names, redacted body, and confirmation string.

## Connectivity

The implementation uses the existing Nexus Identity admin HTTP contract:

- `POST /admin/users`
- `PATCH /admin/users/{username}`
- `DELETE /admin/users/{username}`
- `POST /admin/users/{username}/sessions/revoke-others`
- `POST /admin/users/{username}/password-reset`

Requests are sent through the existing `nexus-vps` SSH transport used by the
protected Nexus wrappers. Token values are read from local environment variables
and are never printed.

## Gates

Mutation execution requires all of the following:

- Core Access role: `Admin`
- `--execute`
- `SEKAILINK_CORE_ACCESS_NEXUS_MUTATION=1`
- `SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN`
  or fallback `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN`
- exact `--confirm user:<username>:<action>` string printed by the dry-run

`user create` reads the password from `--password-env ENV`. This keeps the
password out of shell history, Core Access audit, and dry-run output.

## Risk

Primary risk is operator error on a live Identity account. The command layer
reduces this with Admin RBAC, explicit mutation env gate, exact confirmation,
dry-run default, and token redaction.

No SKLMI, Sekaiemu, Client Core, room server, or pack logic was modified.

## Rollback

Code rollback: revert this change-control entry and the Core Access changes that
added Identity mutation command planning.

Operational rollback after an accidental mutation depends on the action:

- `user edit`: restore prior fields from Nexus Identity audit/account snapshot.
- `user disable`: re-enable the user with `user edit <user> disabled=false`.
- `user revoke-sessions`: user must log in again.
- `user force-password-reset`: user follows the reset flow.
- `user create`: disable or delete according to current Identity policy.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "user edit certo role=admin disabled=false --confirm user:certo:edit"
SEKAILINK_CORE_ACCESS_TEST_PASSWORD='TestPass123!' cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "user create codexadmin codex-admin@sekailink.local admin --password-env SEKAILINK_CORE_ACCESS_TEST_PASSWORD --display-name CodexAdmin --confirm user:codexadmin:create"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "user disable certo --confirm user:certo:disable --execute"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "user disable certo --confirm user:certo:disable"
```

The third command must block unless
`SEKAILINK_CORE_ACCESS_NEXUS_MUTATION=1` is set. The fourth command must block
because Service cannot execute Admin mutations.
