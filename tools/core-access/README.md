# SekaiLink Core Access

Rust MVP for the SekaiLink Core Access bastion cockpit.

This first executable slice is intentionally local-only:

- no production service mutation;
- no Client Core change;
- no Sekaiemu change;
- no SKLMI change;
- audit and notes are written under `~/.sekailink/core-access/`.

Implemented MVP commands include:

- `dashboard`
- `auth whoami`
- `commands list`
- `commands search <query>`
- `server status [server|all] [--execute]`
- `server services [server|all]`
- `server logs <server> <service> [--follow] [--execute]`
- `health probe [server|all] [--execute]`
- `logs list`
- `logs list --by-server`
- `logs list --by-incident`
- `logs tail <source> [--follow] [--execute]`
- `audit search [query]`
- `audit export [query] [file-name]`
- `note add <target> <text>`
- `note list [query]`
- `note export [query] [file-name]`
- `approval request <command> <reason>`
- `approval list`
- `approval approve <id> [reason]` as Admin
- `ops snapshot [label]`
- `client-banner list`
- `client-banner preview <1|2|3>`
- `client-banner edit <1|2|3> <text>` as Admin, local draft only
- `maintenance status`
- `maintenance schedule <scope> <start> <end> <message>` as Admin, local draft only
- `maintenance history`
- `schedule list`
- `schedule calendar`
- `schedule add <name> <when> <command>` as Admin, local draft only
- `schedule history`
- `pack repo list`
- `pack repo add <id> <url> <game> [notes]` as Admin, local draft only
- `pack schedule-check <repo-id> <when-or-interval>` as Admin, local draft only

## Build

```sh
cargo build --manifest-path tools/core-access/Cargo.toml
```

## Run

```sh
cargo run --manifest-path tools/core-access/Cargo.toml
```

Run one command:

```sh
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "server status all"
```

Bootstrap identity for MVP:

```sh
SEKAILINK_CORE_ACCESS_USER=jade SEKAILINK_CORE_ACCESS_ROLE=admin \
  cargo run --manifest-path tools/core-access/Cargo.toml
```

Production auth must be replaced by Nexus login/capabilities before this tool is
trusted for real server mutation.

Read-only remote commands are blocked unless both conditions are true:

- the command includes `--execute`;
- `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1` is set in the environment.

Remote write/mutation commands remain unimplemented.
