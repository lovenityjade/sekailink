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
- `server status all`
- `server services [server|all]`
- `logs list`
- `logs list --by-server`
- `logs list --by-incident`
- `audit search [query]`
- `note add <target> <text>`
- `note list [query]`
- `approval request <command> <reason>`
- `approval list`
- `approval approve <id> [reason]` as Admin

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
