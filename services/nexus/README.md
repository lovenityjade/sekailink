# sekailink-nexus

Role:

- banque centrale
- verite officielle
- configs de seed utilisateur et definitions d'options de jeu

Surface active extraite:

- `server/native/`
- `deploy/`
- `docs/`
- `scripts/`

Palier Nexus Configs:

- config templates: `deploy/nexus/**/*.json.example`
- live config root: `/opt/sekailink/nexus/*/config/`
- durable data root: `/srv/nexus-data/` for host-level data, service `data/` dirs for service state
- build integration rule: add native config `.cpp` and smoke executables to CMake only when the files exist

Docs:

- `docs/nexus-configs.md`

Reference:

- `../../../docs/repo-contracts/sekailink-nexus.md`
- `docs/SEED_CONFIGS.md`
