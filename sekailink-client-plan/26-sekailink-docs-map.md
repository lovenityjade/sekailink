# Relation: `sekailink-docs/` vs `sekailink-client-plan/`

But: eviter de se perdre entre deux corpus de documentation.

Observation:
- `sekailink-docs/` contient des notes historiques et des plans initiaux (souvent plus longs, parfois redondants).
- `sekailink-client-plan/` vise a devenir la reference "IA-friendly" et actionnable (contrats, flux, decisions, risques).

Regle proposee:
- `sekailink-client-plan/` est la reference pour les decisions actuelles et les contrats.
- `sekailink-docs/` reste utile pour:
- historique
- inventaires bruts
- notes UI
- planning initial

## Cartographie rapide des docs existantes

Dans `sekailink-docs/` (high-signal):
- `sekailink-docs/CLIENT_AUTOLAUNCH_PLAN.md`
- `sekailink-docs/CLIENT_AUTOLAUNCH_FAMILIES.md`
- `sekailink-docs/CLIENT_EXTERNAL_PATCHERS.md`
- `sekailink-docs/CLIENT-EMULATOR-INTEGRATION.md`
- `sekailink-docs/BIZHAWK-CONNECTORS.md`
- `sekailink-docs/GAMES-SETUP.md`
- `sekailink-docs/POPTRACKER.md`
- `sekailink-docs/WINDOWS-BUILD.md`

Equivalent ou successeur dans `sekailink-client-plan/`:
- runtime contract: `sekailink-client-plan/04-runtime-contract.md`
- autolaunch pipeline: `sekailink-client-plan/05-autolaunch-patching.md`
- clients AP: `sekailink-client-plan/06-archipelago-clients.md`
- fenetres + gamescope: `sekailink-client-plan/07-emulators-windowing.md`
- trackers: `sekailink-client-plan/08-trackers.md`
- connecteurs Lua: `sekailink-client-plan/09-lua-connectors.md`
- APIs + logs: `sekailink-client-plan/10-server-apis-and-logs.md`
- layout/streaming: `sekailink-client-plan/15-layout-and-streaming.md`
- emulateurs: `sekailink-client-plan/17-third-party-emulators.md`
- generation externe: `sekailink-client-plan/18-external-web-generation.md`
- orchestrator: `sekailink-client-plan/19-session-orchestrator.md`
- settings: `sekailink-client-plan/20-settings-and-profiles.md`
- installers: `sekailink-client-plan/21-runtime-installers.md`
- matrice worlds: `sekailink-client-plan/22-worlds-runtime-matrix.md`
- mod loaders: `sekailink-client-plan/23-modloaders-and-native-clients.md`
- securite logs: `sekailink-client-plan/24-security-privacy-and-logs.md`
- tests: `sekailink-client-plan/25-contract-checks-and-testing.md`

## Plan de consolidation (sans perte)

Court terme:
- maintenir les deux, mais ajouter des liens croises
- quand une info est "decision", elle doit etre dans `sekailink-client-plan/`

Moyen terme:
- deplacer dans `sekailink-client-plan/`:
- les tableaux de families autolaunch
- les liens patchers externes
- les notes BizHawk connectors

Long terme:
- `sekailink-docs/` devient un dossier "archive" et "notes historiques"

