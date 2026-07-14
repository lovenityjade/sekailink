# SekaiLink Security Audit

Date: 2026-07-11  
Scope: Evolution, Link, Worlds, Nexus, Pulse, Pulse PC architecture, Client Core, native bootloader, Sekaiemu/SKLMI, Discord social bot, admin panel, generation pipeline, CDN and inter-service flows.  
Method: non-destructive configuration inspection, listener/firewall review, service and authentication logs, public HTTP header probes, permissions checks, targeted static source review and architecture-document review.

No password, token, private key, user data or exploitable secret is reproduced in this report.

## Executive summary

Overall posture: **at risk but recoverable**. The application has several good security foundations, but host-level access and release trust need immediate hardening.

| Priority | Count | Meaning |
| --- | ---: | --- |
| Critical / immediate | 3 | Plausible path to server or client code execution; mitigate first |
| High | 8 | Material compromise, data-loss or account-takeover risk |
| Medium | 10 | Defense-in-depth or operational exposure |
| Strong controls | 9 | Existing controls worth preserving |

No evidence of a successful intrusion was found in the sampled logs. Automated SSH attacks are active and continuous.

## Critical findings

### SEC-001 - Password and direct root SSH remain enabled

**Affected:** Evolution, Link, Worlds, Nexus; Pulse requires a separate effective-config confirmation.  
**Evidence:** effective SSH configuration allows password authentication; Evolution and Nexus allow direct root login. Nexus logs show repeated root-password attacks from multiple internet addresses.  
**Impact:** credential reuse, theft or brute force can become full server compromise. Local operations documentation also contains reusable privileged passwords, increasing the blast radius of any dev-machine compromise.

**Remediation:**

1. Install and test two independent administrator SSH keys on every host.
2. Create named sudo-capable operator accounts and verify recovery access through the provider console.
3. Set `PasswordAuthentication no`, `PermitRootLogin no`, `KbdInteractiveAuthentication no`, `MaxAuthTries 3`, `X11Forwarding no`.
4. Restrict `AllowTcpForwarding` to accounts that require tunnels, or disable globally and override per user.
5. Rotate every documented server password and administrative token after key-only access is confirmed.
6. Move secrets from Markdown into a password manager or encrypted secret store.

### SEC-002 - Administrative services are publicly reachable

**Affected:** Evolution, Link, Worlds, Nexus.  
**Evidence:** public listeners include 1Panel on ports `19101`-`19104`, Webmin on `10000`, an Evolution service on `19101`, a Link Python service on `8095`, a Worlds Python service on `8097`, and Nexus identity/evolution services bound directly to its public address. Evolution's host firewall is effectively managed by raw nftables while UFW reports inactive; Worlds exposes no active host firewall manager and no fail2ban.

**Impact:** any authentication or framework flaw on these surfaces is directly internet exploitable. Management panels are especially valuable targets.

**Remediation:** bind administrative APIs to loopback or Tailscale; allow only the admin gateway/VPN source in host and OVH firewalls; remove public Webmin/1Panel unless strictly needed; document every intentionally public port. Perform an external port scan after changes.

### SEC-003 - Unsigned release trust chain

**Affected:** native bootloader, Client Core updates, all users.  
**Evidence:** the bootloader correctly verifies a SHA-256 value, but both artifact and expected hash come from the same HTTPS/CDN control plane. No asymmetric signature verification or pinned release key was found.

**Impact:** compromise of Link/CDN, DNS/TLS termination or release-manifest write access can distribute arbitrary code with a matching attacker-controlled hash.

**Remediation:** sign canonical and canari manifests with an offline Ed25519 release key; embed only the public key in the bootloader; verify signature before trusting URL, version or SHA-256; require monotonic versions and explicit rollback metadata; protect release publishing with separate credentials and an auditable two-step promotion.

## High findings

### SEC-004 - Unsafe Python pickle deserialization in generation handoff

The Link generation handoff calls `pickle.loads` on decompressed Archipelago multidata. Current provenance appears to be the controlled Worlds generation output, which reduces exposure, but pickle is executable serialization. A compromised generator, substituted artifact or future upload path becomes code execution on Link. Replace this mutation with an upstream-safe parser or isolate it in a low-privilege, network-restricted sandbox with strict artifact provenance and size limits.

### SEC-005 - Security updates pending across all sampled hosts

Pending updates include OpenSSH, OpenSSL, nginx, MariaDB, kernels, Python and container tooling. Establish weekly patching, automatic security updates where safe, reboot tracking and a staged Pulse -> Worlds -> Link -> Nexus maintenance sequence.

### SEC-006 - Backups and restore testing are not evident

Only package-manager backup timers were visible. No verified application backup schedule was found for Nexus MariaDB, Link SQLite/runtime state, Discord bug bundles, configs or release manifests. Implement encrypted off-host backups, daily database dumps plus point-in-time strategy, retention, immutable/offline copies, and quarterly restore drills. A backup that has not been restored is not a confirmed backup.

### SEC-007 - Host intrusion detection is inconsistent

Fail2ban is absent/inactive on Link, Worlds and Pulse; auditd is absent on Debian/Ubuntu hosts. Enable SSH jails everywhere, persistent audit logging for privileged/config changes, and centralized alerting. Do not treat fail2ban as a replacement for key-only SSH.

### SEC-008 - Evolution carries legacy and unrelated attack surface

Evolution exposes Webmin, 1Panel, Apache, mail, Docker and legacy services; failed units include FTP and an unrelated game server. Remove unused packages/units, close legacy ports, repair Certbot monitoring and separate mail/web workloads from privileged SekaiLink administration where feasible.

### SEC-009 - Nexus core APIs bind to the public interface

Identity and evolution-query listeners use the public Nexus address. The firewall may currently restrict them, but binding to loopback/Tailscale is the safer invariant. Confirm OVH and firewalld rules, then bind internal services privately.

### SEC-010 - Secrets lifecycle is manual

Multiple long-lived shared tokens connect Link, Nexus, Worlds, room runtimes, admin agents and Discord. Define owner, scope, creation date, expiry and rotation for each secret. Use unique per-service credentials, `0600` root/service-owned env files or systemd credentials, and never pass secrets in process arguments or logs.

### SEC-011 - Client diagnostic bundles contain sensitive operational data

The new bug-report flow has consent, redaction, private storage, opaque IDs, size limits and 30-day deletion. Remaining risk is imperfect pattern-based redaction and broad session-log collection. Encrypt bundles at rest, restrict export to audited admin identities, record every read/export, support immediate deletion, and test redaction against cookies, Discord IDs, room passwords, ROM paths, IPv6 addresses and multiline secrets.

## Medium findings

### SEC-012 - Public HTTP hardening is inconsistent

Link/admin responses include strong CSP, frame denial, MIME protection, referrer and permissions policies. The root site, Worlds, Pulse and Evolution did not consistently expose the same set during probes. Standardize HSTS, CSP, `nosniff`, frame protection, referrer policy and minimal server banners. Test actual GET responses as Pulse rejects HEAD.

### SEC-013 - TLS renewal monitoring has failures

Certbot is failed on Evolution and Link. Certificates were serving during the audit, but renewal failure can become an outage or invite unsafe workarounds. Repair units and alert at 30/14/7 days before expiration.

### SEC-014 - Mail surface is broader than necessary on Link

Link publicly exposes SMTP/submission/IMAP/Sieve services. Confirm which server is authoritative, disable plaintext or obsolete protocols, enforce modern TLS, rate limits, SPF/DKIM/DMARC and separate mailbox credentials from SekaiLink identities.

### SEC-015 - Admin gateway path containment needs canonical-path hardening

The gateway checks a normalized path with string `startsWith`. Prefix checks can be fragile across sibling paths and platform rules. Use `path.resolve`, append a separator to the allowed root, reject NUL/encoded traversal, and test double encoding and symlinks.

### SEC-016 - Archive processing needs common limits

Generation, updater and diagnostics code processes ZIP files. Apply one shared policy: canonical destination checks, entry-count limits, compressed/uncompressed byte limits, compression-ratio limits, no symlinks/device files, and extraction into a fresh non-executable directory.

### SEC-017 - Service sandboxing is not standardized

Review every systemd unit for dedicated users and `NoNewPrivileges`, `PrivateTmp`, `ProtectSystem`, `ProtectHome`, `RestrictAddressFamilies`, capability bounding and explicit writable paths. Generation and room processes should not run as root.

### SEC-018 - Rate limiting and abuse quotas need end-to-end proof

Public identity, chat, lobby creation, generation, Pulse, bug upload and screenshot endpoints require per-IP and per-account limits, body/time limits and concurrency caps. Validate at both reverse proxy and application layers.

### SEC-019 - Logs need retention and centralized integrity

Define maximum size and retention per service; redact bearer tokens and query secrets; centralize security events read-only; alert on authentication bursts, admin actions, unexpected service restarts and release publication.

### SEC-020 - Pulse PC is a shared-machine trust boundary

The architecture correctly limits Ollama to the Pulse VPS through Tailscale and keeps the public API on the VPS. Because the PC is shared, use a dedicated non-admin Windows account, BitLocker, Defender/firewall, unattended patching, ACL-restricted model/log directories and no SekaiLink production secrets. Pulse must remain non-critical and treat model output as untrusted.

### SEC-021 - Dependency scanning is not enforced in CI

Add lockfile audits, Dependabot/Renovate, secret scanning, CodeQL/SAST and SBOM generation for Electron/Node, Python, C++ and bundled native libraries. Fail releases on known exploitable critical issues, with documented temporary exceptions.

## Strong controls observed

- Electron windows use context isolation, sandboxing and disable renderer Node integration.
- PopTracker/runtime windows disable developer tools in production paths reviewed.
- Downloaded client artifacts are checked with SHA-256 before installation.
- ROM/core system files support checksum validation.
- Link admin UI uses a restrictive CSP with nonce and denies framing.
- Nexus and Pulse use SELinux enforcing; Nexus has firewalld, fail2ban and auditd.
- Internal databases and many application APIs bind to loopback.
- Pulse's GPU worker is behind Tailscale and restricted to the Pulse VPS by Windows Firewall.
- Bug reports require explicit consent, redact common secrets, use opaque IDs, private `0600` storage, size caps and 30-day retention.

## Server posture

| Host | Rating | Main concern |
| --- | --- | --- |
| Evolution | Critical | public management panels, password/root SSH, legacy surface, failed security/renewal units |
| Link | High | broad public mail/runtime surface, password SSH, no fail2ban/auditd, failed Certbot |
| Worlds | High | no confirmed firewall/fail2ban, public generation listener, password SSH, high-value execution workload |
| Nexus | High | public root/password SSH under active attack, public-address internal listeners, pending DB/crypto updates |
| Pulse VPS | Medium-High | no fail2ban, pending crypto/SSH updates; application services otherwise mostly loopback |
| Pulse PC | Medium | shared Windows endpoint; network design is appropriately private but live inspection was not performed |

## Remediation sequence

### First 24 hours

1. Verify redundant SSH keys and provider-console recovery on every VPS.
2. Disable password and direct-root SSH, beginning with Pulse/Worlds and ending with Link/Nexus after validation.
3. Firewall Webmin, 1Panel, ports 8095/8097 and Nexus internal APIs to loopback/Tailscale/admin sources.
4. Rotate privileged passwords and tokens present in local documentation.
5. Repair certificate renewal monitoring and snapshot critical databases before patching.

### First 7 days

1. Apply OS/security updates and controlled reboots.
2. Deploy fail2ban/auditd and centralized alerts consistently.
3. Establish encrypted off-host backups and execute one full restore test.
4. Add release-manifest signing design and protect CDN publication credentials.
5. Inventory every public route and port; remove unused Evolution/Link legacy services.

### First 30 days

1. Ship Ed25519-signed updates and rollback protection.
2. Remove or sandbox pickle deserialization.
3. Harden systemd units and archive extraction centrally.
4. Add CI dependency/SAST/secret scanning and SBOMs.
5. Complete privacy policy, retention/deletion workflow and auditable bug-report export access.
6. Run a scoped external penetration test after remediation.

## Validation checklist

- External scans show only documented public ports.
- Password/root SSH attempts are rejected before authentication.
- Internal APIs cannot be reached from the public internet.
- A tampered or unsigned update is rejected even with a matching attacker-controlled hash field.
- Backups restore Nexus identities/configs and Link lobby/runtime state in an isolated environment.
- Diagnostic export access is authenticated, logged and subject to deletion/retention.
- Security updates and certificate expiry produce actionable alerts.

## Limitations

This was a defensive, non-destructive audit, not an exploit test. Pulse PC and the offline Windows build PC were not inspected live. Source review was targeted at high-risk boundaries rather than a formal line-by-line third-party audit. Database contents were not read, passwords were not tested, and no production configuration was changed. These limitations should be closed with authenticated external scanning, backup restoration and a focused penetration test after the first remediation pass.
