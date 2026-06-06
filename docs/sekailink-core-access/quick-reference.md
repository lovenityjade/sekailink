# Quick Reference

## Login

```text
auth whoami
server status all
nexus services
maintenance status
```

## Logs

```text
logs list --by-server
logs tail <source>
logs filter user:<id> lobby:<id> room:<id>
logs note
logs export --format md
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

## Admin critical

```text
maintenance enable <scope> --message <msg>
release verify-cdn
release rollback <version>
server update plan <server>
cleanup plan all
```

## Panic

F12 ouvre Panic. Choisir le scope minimal. Toujours noter la raison.
