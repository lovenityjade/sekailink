# Archipelago Inspiration Directive

This directive applies to `sekailink-worlds` during BETA-3 generation work.

## Allowed Inspiration

Worlds should use Archipelago and MultiworldGG as priority behavioral
references for proven generation behavior. This is stronger than casual
comparison: when upstream has a mature invariant, failure mode, fill strategy,
accessibility replay, output shape, or runtime expectation, SekaiLink should
study it first and translate the behavior into its own native contracts before
inventing a different approach.

- option normalization and validation patterns
- generation invariants and edge cases
- item pool construction
- progression, sphere, and accessibility solving
- placement and fill strategies
- spoiler, patch, and seed package shape
- patch output and runtime handoff expectations
- room/runtime protocol semantics
- failure modes and test oracle expectations

Use those projects as behavioral references, compatibility oracles, and design
guides, not as source-code donors. The desired result is behavior that feels
compatible and battle-tested, implemented through SekaiLink-owned C++ and
LinkedWorld data.

## Hard Boundaries

- Do not copy Archipelago or MultiworldGG code 1:1.
- Do not import code with incompatible licensing or unclear provenance.
- Do not use mechanical rewrites or file shuffling as a license workaround.
- Do not add a final product dependency on Archipelago or MultiworldGG Python.
- Do not preserve upstream file structure just to make copying easier.
- Do not turn `Worlds` into an ALTTP-specific generator.
- Do not bypass Nexus config versions with loose YAML or ad-hoc settings files.
- Do not encode permanent game knowledge in generic Worlds code.

## Required Architecture

The durable path remains:

```text
Nexus config versions -> room slots -> LinkedWorld generation surfaces -> Worlds generic generation -> seed package
```

Game knowledge belongs in a `LinkedWorld` generation surface or an isolated
LinkedWorld-owned adapter. Worlds owns generic orchestration, validation,
solver/placer primitives, package emission, and service protocols.

ALTTP may be the strongest comparison target, but any ALTTP behavior accepted
into Worlds must be expressed through generic contracts or an isolated
LinkedWorld adapter seam. The same path must remain usable for EarthBound,
Pokemon, Mega Man, and future LinkedWorlds.

## Implementation Rule

When translating an Archipelago or MultiworldGG behavior, record the behavior in
SekaiLink terms:

- the Nexus config input it depends on
- the LinkedWorld surface that declares the game facts
- the generic Worlds consumer that validates or computes it
- the test that proves the behavior without relying on copied code

If a behavior cannot be described this way, keep it as reference-only until the
contract is clear.
