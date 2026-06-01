# SekaiLink Multi-Repo Layout

## Organization

- Forgejo org: `sekailink`
- Host: `https://projectcelio.xyz`

## Repositories

- `sekailink/sekailink-core`
- `sekailink/sekailink-connect`
- `sekailink/sekailink-emu`
- `sekailink/sekailink-soh`
- `sekailink/sekailink-worlds`
- `sekailink/sekailink-link`
- `sekailink/sekailink-nexus`
- `sekailink/sekailink-evolution`

All repositories are initialized and private.

## Clone URLs

HTTPS format:

```text
https://projectcelio.xyz/sekailink/<repo>.git
```

Examples:

```text
https://projectcelio.xyz/sekailink/sekailink-core.git
https://projectcelio.xyz/sekailink/sekailink-emu.git
https://projectcelio.xyz/sekailink/sekailink-soh.git
```

## Branch Strategy

- Default branch: `main` (protected)
- Work branches: `feature/*`, `fix/*`, `chore/*`
- No long-lived per-feature branches in `main`
