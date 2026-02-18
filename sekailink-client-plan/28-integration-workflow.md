# Workflow d'ajustement et d'integration (SekaiLink Desktop)

But: decrire un workflow concret et extensible pour integrer progressivement "tout ce qui est supporte" par Archipelago/MWGG dans SekaiLink, sans se perdre et sans retomber dans l'analyse fichier-par-fichier.

Ce workflow part du principe qu'on ne reinvente pas les randomizers ni les clients upstream. On orchestre, on automatise ce qui est stable/legal, et on guide le reste.

## Prerequis (avant d'etendre a tous les worlds)

Ce qui doit etre vrai pour rendre l'integration scalable:
- la resolution `download -> module` ne depend pas de hardcode UI
- la sequence "Play" est une machine d'etats (reprises, timeouts, diagnostics)
- les secrets ne transitent pas via argv (streamer-first)
- les slot files ont un traitement explicite (au minimum: classification + flow manuel guide)

Docs associees:
- contrat runtime: `sekailink-client-plan/04-runtime-contract.md`
- pipeline patching: `sekailink-client-plan/05-autolaunch-patching.md`
- orchestrator: `sekailink-client-plan/19-session-orchestrator.md`
- securite: `sekailink-client-plan/24-security-privacy-and-logs.md`
- matrice worlds: `sekailink-client-plan/22-worlds-runtime-matrix.md`

## Ordre recommande (fondations)

### 1) De-hardcoder: PatchResolver + registry manifests (bloquant)

Objectif:
- remplacer `resolveModuleId()` (hardcode extensions) par une resolution basee sur les manifests.

Deliverables:
- un index genere: `patch_ext -> module_id` (registry)
- une fonction unique `PatchResolver.resolve(download_url|filename|content_type) -> {kind, module_id, game_id, patch_mode}`

Definition of Done:
- aucun switch-case d'extensions dans l'UI pour choisir le module
- ajouter un nouveau module runtime suffit a rendre un patch auto-launchable (sans toucher a `Lobby.tsx`)

### 2) Orchestrator: machine d'etats (bloquant)

Objectif:
- remplacer la boucle imperative "patch, launch, connect" par une state machine.

Deliverables:
- `Session` identifiee et loggable
- transitions explicites (prereqs, download, patch, launch, connect, tracker)
- errors codes stables + suggested fixes

Definition of Done:
- une session peut etre arretee proprement a tout moment
- un crash d'un process est detecte et remonte dans l'UI
- les logs sont visibles et correlables a une session

### 3) Securite streamer-first: pas de secrets en argv (bloquant)

Objectif:
- ne pas exposer `lobbyPassword` dans argv (PopTracker, wrapper CommonClient, etc.)

Deliverables:
- un chemin "stdin/env/tmpfile" pour secrets
- redaction logs en UI (minimum)

Definition of Done:
- en mode streamer, `ps`/task manager ne montre pas le password AP
- les logs UI n'affichent pas le password, meme en cas d'erreur

### 4) Slot files: classification + mode guide (bloquant pour couverture complete)

Objectif:
- quand le serveur renvoie `download_slot_file`, ne pas afficher "Unsupported" vague.

Deliverables:
- `download_kind` detecte (`ap_patch` vs `slot_file`)
- UI: `Manual Step` guide (doc + actions "Open folder", "Open URL", "Import file")
- pipeline automatique par famille au fur et a mesure

Definition of Done:
- chaque download a un statut comprehensible
- pour les slot files non supportes, SekaiLink guide l'utilisateur vers une resolution (pas un dead-end)

## Workflow par famille (cadre d'integration)

On integre par "families" (pas par world isole), puis on ajoute des modules world par world.

Families a prioriser (ordre general):
1. `bizhawk_lua` (deja en place)
2. `external_web_generation` et `external_patch_apply` (guidage + imports)
3. `sni` (SNES hardware/emulators via SNIClient)
4. `dolphin_memory` (TP et autres)
5. `mod_loader` (BepInEx/SMAPI/r2modman et clients natifs)

Doc reference:
- families et heterogeneite: `sekailink-client-plan/06-archipelago-clients.md`
- generation externe: `sekailink-client-plan/18-external-web-generation.md`
- modloaders: `sekailink-client-plan/23-modloaders-and-native-clients.md`

## Workflow par world (procedure standard)

Pour integrer un nouveau world, suivre exactement cette procedure.

### Etape A: Classifier (matrice)

Remplir une entree dans la matrice (schema de `sekailink-client-plan/22-worlds-runtime-matrix.md`):
- `generation_type` (AP_INTERNAL, LOCAL_ONLY, EXTERNAL_*)
- `download_kind` (ap_patch vs slot_file)
- `runtime_family`
- emulateur(s) candidats + contraintes de versions (ex: BizHawk range)
- connecteur(s) (Lua/JS/IPC/mod)
- tracker (PopTracker pack repo + variants, ou UT supported)
- prereqs utilisateur (ROM, BIOS, keys, game install)

### Etape B: Creer/mettre a jour un module runtime

Creer `client/runtime/modules/<module_id>/manifest.json` et assets associes.

Ce que le module doit declarer au minimum:
- `game_id`, `display_name`, `driver.family`
- `patch_mode` et/ou `patch_exts` (selon evolution schema)
- `rom_requirements` (hashes) si ROM-based
- `connector` si applicable (Lua path, protocol)
- `tracking` (pack repo/uid + variants)
- contraintes runtime (version ranges) quand necessaire

### Etape C: Valider prereqs et fixes

Implementer (ou declarer) les checks prereqs:
- ROM presente (hash match)
- BIOS/keys/firmware presents si necessaire
- emulateur/runtime installe
- tracker pack installe et valide

Exigence UX:
- pour chaque manque: un bouton `Fix` (import file, pick folder, installer runtime, open guide)

### Etape D: Integrer au PatchResolver

Regle:
- pas de hardcode dans l'UI.

Action:
- l'entree manifest doit etre suffisante pour resoudre une extension ou un artefact.

### Etape E: Ajouter/valider le pipeline de launch

Selon `runtime_family`:
- `bizhawk_lua`: patch -> lancer BizHawk -> bridge -> tracker
- `external_web_generation`: guider "Open website" + "Import YAML" + "Select patched ROM" -> launch
- `slot_file`: extract/copy -> launch -> connect
- `mod_loader`: detect install -> installer loader/mods -> launch entrypoint -> connect

### Etape F: Tests et acceptance

Definition of Done par world:
- Play fonctionne en X11 (Linux)
- Play ne casse pas en Wayland (ok via XWayland ou gamescope)
- erreurs claires + fixes quand prereqs manquent
- aucun secret en argv en streamer mode
- logs visibles (server + client + tracker)

Doc reference:
- tests: `sekailink-client-plan/25-contract-checks-and-testing.md`

## Workflow "server contract" (SekaiLink VPS vs repo local)

Contrainte:
- ce repo serveur n'est pas forcement identique au VPS.

Regle:
- quand une integration depend d'un endpoint ou d'un champ, on ecrit une note de contrat et un check runtime, plutot que d'assumer.

Exemples:
- `room_status.downloads[].download` a-t-il besoin d'auth?
- le server fournit-il `content_type` ou `filename`?

Doc reference:
- APIs: `sekailink-client-plan/10-server-apis-and-logs.md`
- checks: `sekailink-client-plan/25-contract-checks-and-testing.md`

## Gate "release" (quand on dit qu'un jeu est supporte)

Un jeu est "supporte" si:
- il a une entree matrice complete
- il a un module runtime
- il passe la DoD ci-dessus (X11, Wayland best-effort, secrets, logs)
- il ne depend pas d'un hardcode UI

## Notes specifiques handheld/streamers (toujours actives)

Handheld:
- viser gamescope en premier, fallback best-effort sinon
- UI controller-first, focus stable

Streamers:
- fenetres avec titres stables
- pas de secrets en argv
- logs redacted

Docs reference:
- `sekailink-client-plan/15-layout-and-streaming.md`
- `sekailink-client-plan/24-security-privacy-and-logs.md`

