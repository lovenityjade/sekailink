# SekaiLink Project Closure

Closure date: 2026-07-14 (America/Toronto)

## Status

SekaiLink is archived and is no longer under active development or supported
operation by its original maintainer. This repository preserves the final
canonical source state so another person or team can audit it, replace the
AI-assisted implementation where appropriate, and continue it independently.

The final published Client Core update was
`0.3.1-prebeta3.20260714.1`, release sequence `2026071401`, for Linux x64 and
Windows x64. Canonical and Canari were synchronized for that release. The
release updated Client Core bundles and the signed manifest; it did not replace
the NSIS installer or native bootstrapper.

## Important Warnings

- The project is provided as-is, with no support or security-maintenance
  commitment.
- The codebase was developed with substantial AI assistance. Treat all code as
  requiring human review, especially authentication, updates, native memory
  integration, server administration, and data handling.
- Do not reuse historical production credentials. Rotate signing keys, database
  credentials, SMTP credentials, API tokens, Discord credentials, SSH keys, and
  internal service tokens before any redeployment.
- Do not distribute copyrighted ROMs or ROM-derived native game data. Users must
  provide their own lawful game data.
- Rebuild Linux and Windows artifacts from source. Do not treat historical
  binaries as a trusted supply-chain root.
- Review third-party licenses and attribution for Archipelago, libretro,
  PopTracker, APWorlds, tracker packs, native ports, and bundled libraries.

## Where To Start

1. Read `docs/SEKAILINK_TECHNICAL_CONTEXT_FOR_AI_2026-07-13.md` for the complete
   component and server overview.
2. Read `docs/SOURCE_OF_TRUTH.md`, `docs/NO_LEGACY_POLICY.md`, and the current
   game-support matrix.
3. Read `docs/security/` and resolve every remaining finding before exposing a
   service publicly.
4. Review the dated files under `docs/debug-sessions/` for recent changes,
   verification, and known bugs.
5. Recreate development and production environments with newly generated
   credentials. Never publish the private operations backup or its recovery
   keys.

## Known Final Follow-up

- Zelda 1 tracker/check presentation may be delayed in some sessions. A similar
  delayed presentation was reported in a large A Link to the Past room. The
  shared tracker/runtime event pipeline requires further investigation.
- Four Client Core runtime-manifest tests still assert implementation locations
  in the former monolithic Electron main file. The runtime moved into focused
  modules; those tests need maintenance.
- Native PC integrations and broad game support require independent per-platform
  validation. Availability in a catalog is not a substitute for a live
  bidirectional room test.

## Private Recovery Material

The closure process produced encrypted private backups of the development tree,
operations material, server application data, databases, configuration,
service definitions, and per-server restoration notes. Those archives are not
part of this public repository. Anyone receiving them must protect personal
data and secrets, follow applicable privacy obligations, and rotate all secrets
before use.
