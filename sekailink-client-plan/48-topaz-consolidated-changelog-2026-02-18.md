# Topaz Consolidated Changelog (2026-02-18)

This file consolidates the client/server work completed during the Topaz sprint window.
For release notes, use `CHANGELOG.md` at repo root.

## Delivered
- UI redesign rollout (teal/cyan theme, modalized settings/profile flows, lobby tab redesign).
- Runtime/update flow hardening (incremental sync/self-patching behavior).
- SNES/SNI + PopTracker integration stabilization.
- SteamGridDB boxart integration in client and server APIs.
- Lobby moderation/reporting/profile actions and social wiring.
- Friend request flow in lobby context/profile modals.
- Translation updates for newly added UI/feature strings.
- Landing page copy refresh aligned with current SekaiLink capabilities.
- Security cleanup of secret-tracking docs (`XX-*` redacted templates).

## Ops / VPS notes
- Production services expected active:
  - `multiworldgg-webhost.service`
  - `multiworldgg-workers.service`
  - `sekailink-social-bots.service`
  - `sekailink-llama.service`
  - `webmin.service`
- Server path: `/opt/multiworldgg`

## Remaining follow-up
- Complete world-by-world Player Options i18n.
- Continue server API hardening and permission audits.
- QA sweep on all locale paths and modal/context menus.
- Final pre-release regression pass (lobby, generation, launch, reconnect, slow release).
