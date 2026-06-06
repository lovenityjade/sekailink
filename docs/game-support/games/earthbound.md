# EarthBound

## Status

- Platform: SNES
- Deep Research status: Unknown
- SekaiLink tier: 3, Runtime Heartbeat
- Public wording: "EarthBound compatibility preview" until broader playability is proven.

## Local Evidence

- APWorld: `runtime/ap/worlds/earthbound`
- Sekaiemu profile: `runtime/profiles/earthbound-starter.profile`
- SKLMI manifest: `runtime/sklmi/manifests/earthbound.phase1.json`
- Manual session: `apps/sekaiemu/tests/e2e/earthbound/run_earthbound_manual_session.sh`

The current manifest is phase1. It proves a narrow heartbeat:

- `Onett - Tracy Gift`
- `Onett - Tracy's Room Present`
- room-controlled `Toothbrush` receive path

## Current Blocker

The manual session expects a patched ROM and AP seed zip under
`/tmp/sekailink-earthbound-manual-bundle`. Those artifacts were found only under
`_sekailink_quarantine`, which must not be used without explicit approval.

## Next Steps

1. Regenerate or approve migration of the EarthBound manual bundle.
2. Run the manual session with canonical Sekaiemu/SKLMI binaries.
3. Capture `trace.jsonl` and `multiserver.log`.
4. Expand SKLMI mapping only after confirming which APWorld/client/SNI data can
   be translated instead of hand-maintained.

