# Quick Reference

## Login

```sh
cargo run --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --shell
```

```text
auth whoami
ops doctor
server status all
server agent-health all
server agent-services all
nexus services
maintenance status
```

## Logs

```text
logs list --by-server
logs tail <source>
logs search route_not_found link:chat-api
logs filter user:<id> lobby:<id> room:<id> source:all
logs pin <source> <text>
logs note <source> <text>
logs export [query] --format md
```

## Incidents

```text
incident open <label> sev3 <summary>
incident status <label>
incident note <label> <text>
incident pin <label> <source> <text>
incident export <label> --file <label>.md
incident close <label> <resolution>
```

## Handoff

```text
ops timeline [query]
ops handoff shift-<date> --file shift-<date>.md
ops snapshot <label>
ops paths
ops exports [query]
```

## Live support

```text
user search <query>
lobby list
lobby open <lobby>
room summary <room>
room logs <room> --follow
room request-sklmi-reconnect <room> <player>
client diagnostics-request
```

## Client Diagnostics

```text
client diagnostics-request <user> <incident> <reason> --include sekaiemu,sklmi,client
client diagnostics-list [query]
client diagnostics-export [query] --file diagnostics.md
```

## Admin critical

```text
maintenance enable <scope> --message <msg>
server restart <server> <service> --confirm <server>:<service>:restart
release verify-cdn
release rollback <version>
server update plan <server>
cleanup plan all
```

## Panic

F12 ouvre Panic. Choisir le scope minimal. Toujours noter la raison.
