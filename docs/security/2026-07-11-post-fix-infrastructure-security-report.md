# SekaiLink Infrastructure Security Post-Fix Report

**Assessment date:** 2026-07-11 05:00-12:51 EDT  
**Scope:** Evolution, Link, Worlds, Nexus, Pulse, Client Core, native bootloader,
server source, service credentials, update distribution and backups.  
**Maintenance state at report time:** active; public login and Client Core API
access remain blocked with HTTP 503.

## Executive result

The directly exploitable infrastructure findings from the pre-fix audit were
closed or materially contained. SSH is key-only, root login is disabled, public
administration panels were removed, internal Nexus APIs moved behind SSH
tunnels, host firewalls and fail2ban are active, systems were patched and
rebooted, privileged/interservice credentials were rotated, dangerous pickle
deserialization was replaced, and release manifests are now signed.

No SekaiLink user password was reset or rewritten. The Nexus identity SQLite
file had a last-modified time of 06:30 EDT, before the 09:06 EDT credential
rotation, providing direct evidence that the user credential store was not
changed by the rotation.

Two security actions remain before maintenance should be considered fully
closed:

1. Rebuild and test the Windows native bootloader with signed-release
   enforcement when the Windows build PC returns online.
2. Rotate provider-issued Discord, Twitch and Patreon credentials in their
   provider consoles, then update the protected service configuration.

## Critical fixes

### Maintenance isolation

- `sekailink.com`, `www`, login and API routes return HTTP 503.
- `Retry-After`, `Cache-Control: no-store`, CSP, frame denial and MIME sniffing
  protection are set.
- The original landing and Apache configuration are preserved under
  `/var/backups/sekailink-maintenance/20260711T085948Z/` on Link.
- CDN and private administration paths needed for recovery were kept separate.

### SSH and privileged access

- All five VPS enforce `PermitRootLogin no`, `PasswordAuthentication no`,
  `KbdInteractiveAuthentication no`, `MaxAuthTries 3` and `X11Forwarding no`.
- Root key login was tested externally and rejected on every VPS.
- Named operator key access remains available for recovery.
- Root and operator account passwords were rotated, but password SSH remains
  disabled.
- Operator accounts currently have passwordless sudo. This is accepted for
  emergency operations but remains a medium-risk trust concentration.

### Administrative exposure

- Webmin and 1Panel were removed from every host where present.
- Services, packages, configuration and listeners were checked on all five VPS.
- Ports 10000 and 19101-19104 are no longer listening or externally reachable.
- Nexus identity and room-query APIs bind to loopback and are reached from Link
  through dedicated operator SSH tunnels.
- Public probes confirmed Nexus internal ports 19094 and 19095 are closed.

### Application code

- Generation handoff no longer uses unrestricted `pickle.loads`.
- A bounded 128 MiB decompressor and restricted Archipelago class allowlist are
  enforced.
- Eleven generation-handoff tests pass, including rejection of an arbitrary
  global/eval payload.
- Evolution's empty Postfix queue snapshot no longer fails under `pipefail`.

### Update trust chain

- Release entries are signed using an offline Ed25519 private key.
- The native bootloader pins the corresponding public key and rejects missing,
  malformed or invalid signatures before download.
- The signed payload covers version, channel, platform, build, artifact type,
  download URL, SHA-256 and monotonic release sequence.
- Installed release sequence is persisted; lower signed sequences are rejected.
- Live canonical manifest entries were signed with sequence `2026071101`.
- A live API response was verified independently with OpenSSL.
- Signature tampering and sequence tampering tests pass.
- The Linux bootloader and chat API compile successfully; chat API smoke tests
  pass.
- An isolated Linux end-to-end test passed signed API retrieval, artifact
  download, SHA-256 verification, ZIP extraction, installation and persistent
  release-sequence state. A lower but validly signed sequence was rejected.
- Existing bootloaders remain compatible because they ignore the added fields.
  Signed-release enforcement becomes universal only after new Linux and Windows
  bootloaders are distributed.

## Credential rotation

Rotated credentials include:

- Linux privileged/operator passwords on all VPS;
- per-host admin agent tokens and Link consumers;
- identity, lobby admin, lobby runtime, chat gateway, room server admin/runtime,
  client report, generation and seed-config tokens;
- social-bot control and admin panel/web/approval tokens;
- administrator TOTP seed;
- MariaDB projection, seed-config and room-server service accounts.

The recovery vault and release private key are outside Git and mode 600 under
`~/.local/share/sekailink-security/`. No operational secret marker was found in
tracked first-party source. Provider-issued Discord, Twitch and Patreon secrets
remain pending provider-console rotation.

A post-rotation permission audit found and corrected world-readable legacy
configurations on Evolution and Link. Active service configurations now use
either `0600` or `0640` with the exact service group; pre-rotation files,
SQLite snapshots and historical Twitch configurations are `0600`.

## Network and host validation

### External ports observed

- Evolution: 22, 80, 443.
- Link: 22, 80, 443, 465, 587, 993.
- Worlds: 22, 80, 443.
- Nexus: 22 only.
- Pulse: 22, 80, 443.

The probe set also covered legacy mail, Webmin/1Panel, Link 8095, Worlds 8097
and Nexus 19094/19095. No unintended tested port was reachable.

### Host controls

- Link and Worlds use active UFW default-deny policies.
- Nexus and Pulse use active firewalld policies.
- Evolution uses a persistent nftables ruleset with explicit input accepts and
  a final drop.
- fail2ban and auditd are active on all five VPS.
- Evolution's stale `banned_db` fail2ban action was removed; nftables is now the
  only configured SSH ban action and its configuration test passes.
- fail2ban was observed actively banning current SSH scans.

### Patch and service state

- All five VPS were updated and rebooted where required.
- Package checks report zero pending updates on every VPS.
- Systemd reports zero failed units after Certbot repair.
- Evolution Certbot dry-run succeeds for `evolution.sekailink.com`; Link dry-run
  succeeds for both active `sekailink.com` certificate lineages.
- Obsolete renewal definitions whose DNS moved elsewhere were archived, not
  deleted. The removed Webmin renewal was also archived.
- All expected SekaiLink services are active. Health checks return 200, or 401
  where health is intentionally authenticated.
- Social Bots and Nexus Lobby Admin were hardened with dedicated non-root
  service accounts, `NoNewPrivileges`, strict filesystem protection, empty
  capability sets and restricted address families/namespaces. Their systemd
  exposure ratings improved from `9.2/9.6 UNSAFE` to `4.2 OK`.
- Social Bots now binds `127.0.0.1:8095`; its host firewall is no longer the
  only barrier protecting that API.
- Nexus MariaDB reports alive; Link-to-Nexus identity, room-query, seed-config
  and database tunnels are healthy.
- The earlier room-server handshake failures occurred during tunnel correction;
  no current credential-chain errors were found.

## Backups and recovery

- MariaDB pre-rotation dump:
  `/srv/nexus-data/security-backups/20260711T090633Z/all-databases.sql`.
- SQLite identity/lobby backups:
  `/srv/nexus-data/security-backups/20260711T162749Z/sqlite/`.
- SQLite `PRAGMA integrity_check` returned `ok`.
- Link security backups remain under `/opt/sekailink/security-backups/`.
- Encrypted off-host copies are stored under
  `~/.local/share/sekailink-security/offsite-backups/2026-07-11/`.
- Off-host archives use GPG AES-256, mode 600, SHA-256 manifests and were tested
  by decrypting and listing their contents without persisting plaintext.

These copies are off-server but still on the administrator devbox. A scheduled,
versioned and immutable backup destination remains recommended.

## Remaining risk register

### High: Windows security audit deferred

The Windows build PC is offline. No Windows build or release is considered
validated by this maintenance. Do not claim universal update-signature
enforcement until the Windows binary is rebuilt, dependency-closure tested,
executed and audited on that PC.

### High: external provider credentials

Discord, Twitch and Patreon credentials require rotation in their respective
provider consoles. Internal social-bot control credentials are already rotated.

### Medium: passwordless sudo

Operator keys grant passwordless sudo. Consider hardware-backed SSH keys,
restricted sudo commands and a separate break-glass path after stabilization.

### Medium: immutable backup automation

The encrypted off-host snapshot is manual and stored on one administrator
workstation. Add scheduled encrypted backups with retention, restore drills and
immutable/object-lock storage.

### Low: legacy Evolution configuration

Several unrelated historical Apache vhosts and certificates remain archived or
enabled on the shared Evolution host. They do not expose the removed admin
panels, but workload separation and a deliberate vhost retirement pass would
reduce operational noise.

## Release gate

Keep maintenance active until:

- provider credentials are rotated or explicitly accepted as a deferred risk;
- Windows signed bootloader build and tests pass;
- the administrator enrolls the rotated TOTP seed;
- a final public login, Client Core login, lobby creation, generation and game
  synchronization smoke test passes after maintenance is lifted.
