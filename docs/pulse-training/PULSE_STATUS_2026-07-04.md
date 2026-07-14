# Pulse Status - 2026-07-04

This document captures the current Pulse architecture, training state, public
support API, regression checks, and the handoff context needed for a future
admin panel prototype repository.

It intentionally excludes secrets, passwords, API keys, private tailnet auth
URLs, and raw operational credentials.

## Summary

Pulse is now split into two roles:

| Component | Role |
| --- | --- |
| Pulse VPS | Public API, console, RAG, guardrails, support routing, audit logs |
| Pulse PC | Private GPU inference worker running Ollama over Tailscale |

Pulse remains non-critical. SekaiLink gameplay, lobbies, room servers, account
login, generation, and runtime sync must continue if Pulse is unavailable.

Current production-facing support endpoint:

```text
POST https://pulse.sekailink.com/api/support-ask
GET  https://pulse.sekailink.com/api/support-health
```

Current model:

```text
sekailink-pulse:7b
```

The model is served by Ollama on Pulse PC and reached privately by the Pulse VPS
through Tailscale. Client Core and public callers do not reach the GPU worker
directly.

## What Changed Today

On 2026-07-04, Pulse was upgraded from a mostly APWorld-options assistant into a
SekaiLink support assistant with local product knowledge.

Added:

- 74 `sekailink_game_info` RAG rows from canonical runtime/module manifests:
  ROM hashes, patch extensions, trackers, wrappers, support state, compatibility
  notes.
- 395 local APWorld documentation rows for 26 active/visible BETA-3 games.
- 7 summarized Archipelago documentation rows.
- 11 targeted training examples for ROMs, YAML, tracker behavior, support
  status, vague questions, and "patch vs ROM" confusion.
- A deterministic support fast-path for high-risk support answers:
  - ROMs and hashes
  - patch files such as `.aplttp`
  - support/availability status
  - tracker and Universal Tracker answers
  - runtime wrapper/client answers
- Public `/api/support-ask`, separate from generic `/api/public-ask`.
- Public `/api/support-health`.
- A rebuild script for managed RAG imports.
- A daily systemd support regression bench.

## Pulse Public API

### `POST /api/support-ask`

Use this for Client Core support/help surfaces.

Example request:

```json
{
  "message": "SMZ3 quelles ROMs compatibles"
}
```

Example response shape:

```json
{
  "ok": true,
  "answer": "SMZ3 n'utilise pas une seule ROM de base...",
  "draft_values": {},
  "citations": [
    {
      "game_key": "smz3",
      "label": "SMZ3 SekaiLink runtime, ROM, tracker, support info"
    }
  ],
  "confidence": 1.0,
  "warnings": [],
  "fast_path": "sekailink_game_info_rom",
  "source": "pulse-rag",
  "suggested_next_step": "Compare le fichier importe avec les extensions et hashes indiques; si ca bloque, envoie le nom du fichier et le hash calcule.",
  "endpoint": "support-ask"
}
```

Important behavior:

- Web search is disabled by default for `support-ask`.
- Answers should come from local SekaiLink/RAG context.
- If Pulse does not understand the request, it must say so and ask for one short
  clarification.
- Pulse must not invent ROM names, hashes, APWorld options, trackers, server
  states, or official links.

### `GET /api/support-health`

Use this for Client Core/admin status checks.

Response includes:

- `ok`
- `service`
- `rag_status`
- nested `rag`
- `model`
- `endpoint`
- `web_search_default`

Current validated state on 2026-07-04:

```text
service: pulse-support
rag_status: 200
rag.index_rows: 2141
model: sekailink-pulse:7b
web_search_default: false
```

## Internal Services

Pulse VPS systemd services:

```text
sekailink-pulse-llm.service
sekailink-pulse-rag-api.service
sekailink-pulse-console.service
sekailink-pulse-support-bench.service
sekailink-pulse-support-bench.timer
```

The legacy local CPU llama.cpp service still exists as a local component, but
the active RAG/console LLM URL points to the Pulse PC Ollama endpoint through
systemd drop-ins.

Relevant drop-ins:

```text
/etc/systemd/system/sekailink-pulse-rag-api.service.d/20-pulse-pc.conf
/etc/systemd/system/sekailink-pulse-console.service.d/20-pulse-pc.conf
```

## RAG State

Main index:

```text
/opt/sekailink/pulse/rag/indexes/apworld-options.jsonl
```

Current row counts:

```text
2141 total rows
1665 base APWorld option rows
74 SekaiLink game info rows
395 local APWorld doc rows
7 Archipelago doc summary rows
```

Auditable import files:

```text
/opt/sekailink/pulse/rag/imports/sekailink-game-info-20260704.jsonl
/opt/sekailink/pulse/rag/imports/sekailink-apworld-docs-20260704.jsonl
/opt/sekailink/pulse/rag/imports/archipelago-docs-20260704.jsonl
/opt/sekailink/pulse/rag/imports/support-rag-import-manifest.json
```

Targeted support datasets:

```text
/opt/sekailink/pulse/rag/datasets/pulse-royal-treatment-20260704.jsonl
/opt/sekailink/pulse/rag/datasets/pulse-royal-treatment-extra-20260704.jsonl
```

Rebuild script:

```text
/opt/sekailink/pulse/bin/rebuild_pulse_support_rag.py
```

Rebuild behavior:

- Keeps unmanaged/base rows.
- Removes managed `sekailink_game_info`, `apworld_doc`, and `archipelago_doc`
  rows.
- Reimports the dated support JSONL files from `rag/imports`.
- Writes a manifest with row counts and a backup path.

## Regression Bench

Support bench:

```text
/opt/sekailink/pulse/tests/run_pulse_support_bench.py
```

Result log:

```text
/opt/sekailink/pulse/tests/pulse-support-bench-results.jsonl
```

Timer:

```text
sekailink-pulse-support-bench.timer
```

Current bench coverage:

- Secret of Evermore ROM/hash
- SMZ3 two-ROM requirements
- Pokemon Crystal unavailable/force unavailable
- `.aplttp` patch vs ROM
- Wario Land Universal Tracker
- vague request clarification
- beginner YAML explanation
- Pokemon Emerald badges values
- Link's Awakening DX canari availability
- Oracle of Seasons canari availability
- Secret of Mana `web_ap_client`
- Mega Man 3 Universal Tracker
- Pokemon Red/Blue ROM hashes
- Super Metroid MD5/SHA256
- Donkey Kong Country force unavailable
- no default web search for support endpoint

Current result:

```text
16/16 PASS
```

## Current Known Correct Answers

These are important examples the support endpoint must preserve.

### SMZ3 ROMs

SMZ3 requires two separate base ROMs, not a fake `SMZ3` ROM:

- ALttP: `Zelda no Densetsu - Kamigami no Triforce (Japan) 1.0`
  - MD5 `03a63945398191337e896e5771f77173`
- Super Metroid: `Super Metroid (Japan, USA)`
  - MD5 `21f3e98df4780ee1c667b84e57d88675`
  - SHA256 `12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72`

### Secret of Evermore ROM

SekaiLink expects:

- extensions `.sfc` or `.smc`
- MD5 `6e9c94511d04fac6e0a1e582c170be3a`

Windows validation still requires caution if the issue is runtime-specific.

### Pokemon Crystal

Pokemon Crystal must currently be treated as unavailable/force unavailable.

Reason:

- Crystal reaches gameplay in forced GBC mode, then freezes or corrupts
  graphics/audio.
- Oracle of Ages and Pokemon Red/Blue confirmed the general GB/GBC path works.
- Therefore the remaining issue is Crystal-specific.

### Link's Awakening DX

Current support status:

- Client Core availability: available
- support status: `canari_test`
- Linux bilateral send/reception confirmed.
- Windows validation still required before Canonical promotion.

### Patch vs ROM

Example:

`.aplttp` is a patch file, not the playable ROM. SekaiLink uses the patch plus a
compatible base ROM to produce/launch the patched ROM.

## Boundaries

Pulse should answer support/configuration questions. It must not become an admin
command runner.

Allowed:

- explain game support status
- explain ROM requirements and hashes from local context
- explain patch vs ROM
- explain trackers and Universal Tracker
- explain APWorld options and beginner YAML concepts
- suggest next diagnostic steps
- admit when a request is unclear

Not allowed:

- execute server commands
- expose SSH, passwords, tokens, private endpoint credentials, or DB secrets
- tell users to run destructive commands
- invent missing hashes or ROM filenames
- claim a game is supported if Client Core marks it force unavailable
- rely on web search for normal support answers

## Admin Panel Handoff Context

A future admin panel prototype repo should receive:

- This document.
- `docs/admin-panel-server-architecture.md`.
- Public API contracts for Pulse:
  - `GET /api/support-health`
  - `POST /api/support-ask`
- Read-only service concepts:
  - Pulse status
  - RAG index row counts
  - model name
  - support bench status
  - last support bench run
  - latest support warnings
- A backend-only plan for privileged admin actions. Secrets must not be sent to
  a browser or committed into the prototype repo.

Do not include in the prototype repo:

- `XX-server-infos.local.md`
- SSH passwords
- Tailscale auth URLs or keys
- API key files
- raw DB credentials
- Discord/Twitch/webhook tokens
- private room server admin tokens

Recommended first admin panel pages:

1. Overview
   - health of Link, Nexus, Worlds, Pulse, Evolution
   - public endpoint reachability
   - current release channel status

2. Pulse
   - support health
   - model name
   - RAG row counts
   - bench status
   - quick ask/support tester
   - recent warnings/errors

3. Lobbies/Rooms
   - read-only list of live lobbies
   - room summaries
   - stale/expired room detection
   - destructive actions hidden behind backend confirmations

4. Client Reports
   - bug reports by source/component/platform
   - bootloader/update failures
   - runtime crashes

5. Releases/CDN
   - Canonical/Canari manifests
   - installer/update artifacts
   - latest/fallback build inventory

For Pulse specifically, the admin panel can start safely with public endpoints
only. Any deeper Pulse controls, such as rebuilding RAG or running the bench on
demand, must go through a private backend/admin-agent and require authorization.

## Useful Operator Commands

Run support health locally on Pulse VPS:

```bash
curl -fsS http://127.0.0.1:18183/api/support-health | python3 -m json.tool
```

Run support health publicly:

```bash
curl -fsS https://pulse.sekailink.com/api/support-health | python3 -m json.tool
```

Ask support:

```bash
curl -fsS https://pulse.sekailink.com/api/support-ask \
  -H 'Content-Type: application/json' \
  --data '{"message":"SMZ3 quelles ROMs compatibles"}' \
  | python3 -m json.tool
```

Run the support bench:

```bash
/opt/sekailink/pulse/tests/run_pulse_support_bench.py
```

Rebuild managed support RAG:

```bash
sudo /opt/sekailink/pulse/bin/rebuild_pulse_support_rag.py
sudo systemctl restart sekailink-pulse-rag-api.service
```

Check services:

```bash
systemctl is-active \
  sekailink-pulse-rag-api.service \
  sekailink-pulse-console.service \
  sekailink-pulse-support-bench.timer
```

## Next Steps

Recommended next work:

- Add Client Core integration for `support-ask`.
- Add a small support UI that shows `answer`, `warnings`, `citations`, and
  `suggested_next_step`.
- Build the admin panel prototype repo from sanitized docs and public/private
  API contracts.
- Add a private admin endpoint or agent command to trigger support bench and RAG
  rebuild on demand.
- Expand support bench when new games become active or when community reports
  repeat the same confusion.

