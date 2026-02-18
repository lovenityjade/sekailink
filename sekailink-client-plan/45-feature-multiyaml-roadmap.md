# Feature Roadmap: MultiYAML (Multi-Game per Player)

Date: 2026-02-18  
Status: Planned (future feature)

## Summary
Support players who want to use multiple YAML profiles (multi-game / multi-slot) under one SekaiLink identity.

## Scope Options

### A) Sequential MultiYAML (recommended first)
- One active game session at a time.
- Player can switch between configured YAMLs quickly.
- Lower risk and faster delivery.

### B) Simultaneous MultiYAML (advanced)
- Multiple active game sessions in parallel.
- Multiple AP clients/runtimes at once (emu/tracker/connectors).
- Higher complexity and operational risk.

## Difficulty
- Sequential mode: medium.
- Simultaneous mode: high.

## Why Sequential First
- Reuses existing launch/runtime path.
- Avoids immediate multi-process orchestration complexity.
- Delivers user value quickly while preparing architecture for parallel mode later.

## Proposed Implementation Plan

### Phase 1 — Data Model
- Add a `multiyaml_bundle` concept:
  - bundle id
  - player/account id
  - entries: game, slot, yaml id, module id, runtime profile
  - preferred launch order / default entry

### Phase 2 — UI/UX
- Game Manager:
  - create/edit bundle
  - add/remove YAML entries
  - set default active entry
- Lobby/Home:
  - show active entry status
  - quick switch action between bundle entries

### Phase 3 — Runtime Orchestrator
- Introduce a session manager with explicit `sessionId`.
- Start with **single-active lock**:
  - prevent double launch collisions
  - require stop/switch behavior
- Standardize per-session logs/events.

### Phase 4 — Parallel Mode (optional)
- Support multiple active sessions:
  - per-session process tracking
  - port allocation strategy
  - conflict-safe connector/tracker startup
- UI: multi-session panel (status, stop/restart, errors).

## Core Technical Requirements
- Deterministic state machine per session (`idle -> launching -> running -> error -> stopping`).
- Resource locking (ports/files/runtime folders).
- Process tree management per session.
- Namespaced logs/events by `sessionId`.

## Risks
- Port conflicts and connector collisions.
- Increased crash surface with parallel runtimes.
- UI complexity for debugging and support.

## MVP Recommendation
- Deliver Sequential MultiYAML first:
  1. bundle CRUD
  2. launch selected entry
  3. quick switch with clean stop/start
  4. session-specific status/log panel

## Success Criteria
- Player can configure 2+ YAMLs in one bundle.
- Launch/switch flow works without manual runtime cleanup.
- No cross-session log/process confusion.
- Clear path to parallel sessions without redesign.
