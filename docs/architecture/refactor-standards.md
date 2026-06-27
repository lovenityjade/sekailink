# SekaiLink Refactor Standards

Date: 2026-06-25

## Goal

SekaiLink code should be readable, debuggable, and economical to inspect by a
human or AI agent. Large files increase regression risk, cognitive load, and
token/credit cost.

## File Size Rule

Maintained SekaiLink source files should stay under **800 lines**.

Exceptions:

- third-party/vendor code;
- generated files;
- release/build artifacts;
- data tables that are intentionally mechanical;
- temporary migration files with a documented removal target.

If a maintained file must exceed 800 lines temporarily, add a TODO near the top
that names the responsibility to extract next.

## Refactor Method

Use a strangler refactor:

1. Extract one coherent responsibility.
2. Keep public behavior and function names stable where possible.
3. Run a targeted syntax/build/test check.
4. Continue with the next responsibility only after the check passes.

Do not combine refactor work with feature changes unless the feature is blocked
by the extraction.

## Current Priority

1. Client Core Electron main process.
2. Runtime wrapper layer.
3. Sekaiemu oversized runtime/debug files.
4. SKLMI oversized bridge/API files.

## First Completed Extractions

- Client Core Electron:
  - runtime path resolution;
  - process/port utilities;
  - bootstrapper control;
  - diagnostics and bug-report artifact collection.
- Runtime wrapper:
  - AP client event formatting and patch metadata;
  - AP/bootstrap/headless hooks.
