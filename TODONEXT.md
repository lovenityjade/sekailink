# TODO Next (GitHub)

## Player Options i18n (prochaine etape)
- Finaliser la traduction **world-by-world** pour tous les jeux actuellement actives (27 jeux serveur).
- Supprimer progressivement le fallback generique quand un world map specifique est complet.
- Etendre les mappings a:
  - labels d'options
  - tooltips/docs d'options
  - labels de groupes
  - labels de choices
- Verifier que chaque world conserve les **value YAML canoniques** (jamais traduites) pour eviter les erreurs de generation.

## Locales a couvrir
- FR (priorite, completion 100%)
- ES
- JA
- ZH-CN
- ZH-TW

## Server-side
- Completer `WebHostLib/options.py` avec tables world-specific par jeu.
- Conserver la resolution locale via `?lang=` + `X-SekaiLink-Locale` / `Accept-Language`.
- Ajouter tests de non-regression: payload traduit + valeurs YAML non traduites.

## Client-side
- Completer `client/app/src/pages/playerOptionsWorldI18n.ts` par world.
- Garder mode `Simple / Advanced` et ajuster le set simple par jeu.
- Traduire les derniers textes restants (placeholders, contextuels, dropdowns specifiques).

## QA
- Matrice de verification par jeu:
  - affichage UI traduit
  - save YAML OK
  - generation seed OK
  - reload/import YAML OK
- Smoke tests sur Linux + Windows.

## Packaging / release
- Build client: `npm run build`
- Package Linux: `npm run electron:pack`
- (optionnel) Package Windows selon pipeline habituel.
- Tag/versionner proprement avant push GitHub.

## Priority Hardening Roadmap

### P0 — must-have avant release officielle
1. [x] Verif hash + extraction safe pour tout download (portable python, updates, zips).
2. [x] Durcir Electron (contextIsolation, nodeIntegration off, IPC validation stricte).
3. [x] Logger no-secrets: audit JSONL + scrub tokens/passwords/URLs sensibles.
4. [x] Ajouter `LICENSE` + `THIRD_PARTY_NOTICES.md`.

### P1 — stabilite & maintenance
5. [x] Split `electron/main.cjs` en modules (reduire dette technique).
6. [x] Ajouter lint/format (`ESLint` + `Prettier`) + hook `pre-commit`.
7. [x] Ajouter smoke tests headless en CI (baser sur script `headless:smoke`).

### P2 — polish pro
8. [x] Crash reporting opt-in (Sentry ou equivalent) + upload logs facile.
9. [x] Auto-update signe + mecanisme rollback.
10. [x] Documenter `Dev Setup` + `Release Process` (1 page chacun).
