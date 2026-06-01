# APTest MultiworldGG Runtime

APTest is a private Archipelago/MultiworldGG test runtime hosted on `worlds`.
It exists to pretest APWorlds and randomizers without touching SekaiLink production services.

## Endpoint

- Public WebHost: `https://aptest.sekailink.com`
- Room management: `https://aptest.sekailink.com/aptest/rooms`
- Systemd service: `aptest-webhost.service`
- Runtime root: `/opt/aptest`
- Source tree: `/opt/aptest/source`
- Python venv: `/opt/aptest/venv`
- SQLite database: `/opt/aptest/data/aptest.db3`
- Nginx vhost: `/etc/nginx/sites-available/aptest.sekailink.com`

## Source Decision

The deployed runtime uses the full MultiworldGG source tree.

The original target was “official Archipelago core plus MultiworldGG worlds”, but that hybrid does not load the full world set reliably. Several MultiworldGG worlds depend on MultiworldGG-specific core symbols and modules, so the hybrid tree only partially imports worlds.

Official Archipelago is kept locally as a reference source, not as the active APTest runtime.

## Runtime Behavior

- WebHost binds to `127.0.0.1:19080`.
- Nginx exposes it through HTTPS at `aptest.sekailink.com`.
- Generated rooms are persistent for test usage.
- Empty rooms do not immediately auto-shutdown.
- Room state remains handled by normal Archipelago/MultiworldGG multisave behavior.
- The management page can list active rooms and request a room close.

## Isolation Rules

APTest must stay isolated from SekaiLink production:

- Do not reuse SekaiLink Nexus, Link, Evolution, or Worlds databases.
- Do not import APTest code into SekaiLink Core.
- Do not share the APTest Python venv with SekaiLink services.
- Keep APTest service management under `aptest-webhost.service`.

## Operational Commands

Check service:

```bash
systemctl status aptest-webhost.service --no-pager
```

View logs:

```bash
journalctl -u aptest-webhost.service -n 200 --no-pager
```

Restart runtime:

```bash
sudo systemctl restart aptest-webhost.service
```

Smoke check:

```bash
curl -I https://aptest.sekailink.com/
curl -I https://aptest.sekailink.com/aptest/rooms
```

## Notes

The runtime is intentionally not optimized as a public production service. It is a durable test bench for Jade, dev collaborators, and trusted players who need persistent rooms for APWorld evaluation.
