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
release current
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
user open <user>
user sessions <user>
user audit <user>
user configs <user_id> alttp
user config export <user_id> <config_id> --format yaml
lobby list
lobby open <lobby_id>
lobby chat <lobby_id>
room list 25 alttp
room summary <room_id>
room events <room_id> 50
room logs <room_id> --follow
room request-sklmi-reconnect <room_id> <player> --confirm room:<room_id>:request-sklmi-reconnect
client diagnostics-request
```

## Client Diagnostics

```text
client diagnostics-request <user> <incident> <reason> --include sekaiemu,sklmi,client
client diagnostics-list [query]
client diagnostics-export [query] --file diagnostics.md
```

## Bots

```text
discord status
discord logs --follow
discord announce <channel> <message> --confirm discord:announce:<channel>
discord incident-post <incident> <channel> <message> --confirm discord:incident-post:<incident>
twitch status
twitch logs --follow
twitch announce <channel> <message> --confirm twitch:announce:<channel>
twitch stream set-title-hint <channel> <title>
```

## Admin critical

```text
maintenance enable full "Release window" --confirm maintenance:full:enable
broadcast global "Message" --confirm broadcast:global:all
client request-sklmi-reconnect <user> <reason> --confirm client:request-sklmi-reconnect:<user>
server restart <server> <service> --confirm <server>:<service>:restart
user edit <user> disabled=false --confirm user:<user>:edit
user disable <user> --confirm user:<user>:disable
user revoke-sessions <user> --confirm user:<user>:revoke-sessions
user force-password-reset <user> --confirm user:<user>:force-password-reset
release verify-cdn
release list
release verify latest --fast
release compare 20260604 20260605
release rollback <version>
pack validate <repo-id>
pack stage <repo-id> <notes> --confirm pack:<repo-id>:stage
schedule add release-check daily "release verify latest --fast" --confirm schedule:release-check:add
cleanup plan all before-release
server update plan <server>
server update apply <server> <plan_id> --confirm server-update:<server>:apply
ssh open <server> <reason> --confirm ssh:<server>:open
```

## Panic

F12 ouvre Panic. Choisir le scope minimal. Toujours noter la raison.
