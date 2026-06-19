# Active Directive

Date: 2026-06-20

Primary objective:

**Stabilize the project state before making product decisions.**

SekaiLink is currently on hiatus. Do not treat any release as imminent. Do not
push builds, deploy updates, delete work, move directories, or restart large
architecture changes unless Jade explicitly asks for that specific action.

Current focus:

- Reduce cognitive load around the repository.
- Document what exists, what is experimental, and what is safe to ignore.
- Preserve work without moving or deleting it.
- Separate product decisions from emotional urgency.
- Keep the public site in hiatus mode.

Frozen until explicitly reopened:

- Client Core release work.
- Bootloader release work.
- Server/CDN update pushes.
- Native tracker replacement.
- New game compatibility expansion.

Allowed cleanup:

- Add documentation that clarifies current state.
- Add non-destructive inventories.
- Run read-only checks and smoke tests.
- Propose cleanup steps before executing them.

Rule:

If a task feels like it could reshape the product, pause and write the decision
down first. SekaiLink should be resumed as a small, understandable system, not
as an emergency.
