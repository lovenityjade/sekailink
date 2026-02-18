# Mod loaders et clients natifs (BepInEx, SMAPI, Fabric/Forge, r2modman)

But: documenter les mod loaders presents dans `third_party/mod_loaders/` et proposer une strategie SekaiLink pour:
- installer automatiquement les prerequis
- gerer des profils par jeu
- minimiser les fenetres
- rester robuste pour handheld et streamers

Contrainte:
- beaucoup de jeux PC exigent une copie legale via Steam/GOG/itch.
- SekaiLink ne doit pas distribuer ces jeux, mais peut detecter l'installation et guider.

## Inventaire (dans ce repo)

Chemin: `third_party/mod_loaders/`
- `BepInEx/` (LGPL-2.1)
- `SMAPI/` (modding framework Stardew Valley)
- `Fabric-Loader/` (Apache-2.0)
- `MinecraftForge/` (Forge modding API, Java)
- `r2modmanPlus/` (mod manager, Electron)
- `UnityExplorer/` (outil debug in-game, optionnel)

## Pattern general: "runtime family = mod_loader"

Un world PC modde se decrit en general par:
- `game_install`: path du jeu (Steam library ou autre)
- `loader_install`: BepInEx/SMAPI/Fabric/Forge/r2modman
- `mods_install`: mod(s) Archipelago + deps
- `launch_entrypoint`: exe/command (souvent different de l'exe original)
- `connect`: AP address + slot + pass (souvent via args, fichier, ou UI)

SekaiLink doit fournir:
- un profil par jeu (pour ne pas casser d'autres mods)
- une verification de prereqs (OK/Missing + Fix)
- une methode d'installation deterministic (source canonique par mod)

## BepInEx (Unity Mono / IL2CPP)

Docs upstream:
- `third_party/mod_loaders/BepInEx/README.md`

Ce que SekaiLink doit retenir:
- BepInEx se deploie dans le dossier du jeu (souvent un unzip)
- il existe plusieurs variantes (Mono, IL2CPP)
- les plugins vont dans `BepInEx/plugins/`
- la config va dans `BepInEx/config/`

Risques:
- "0 interaction" peut impliquer d'ecrire dans le dossier Steam du jeu
- sur Linux Steam Deck, permissions et Proton compliquent

Strategie SekaiLink (recommandee):
- demander un consentement clair "installer mods dans ce jeu"
- offrir un bouton "Uninstall mods" (cleanup)
- preferer un profil SekaiLink isole quand possible:
- approche A: installer dans le dossier du jeu (simple, commun)
- approche B: copie side-by-side du dossier du jeu (trop lourd sauf petits jeux)
- approche C: utiliser un manager (r2modman) quand le jeu le supporte

## SMAPI (Stardew Valley)

Docs upstream:
- `third_party/mod_loaders/SMAPI/docs/README.md`

Ce que SekaiLink doit retenir:
- SMAPI est concu pour etre installe "a cote" de l'exe sans modifier les fichiers jeu
- SMAPI gere des compat checks, update checks, backups
- entrypoint typique: `StardewModdingAPI` (varie selon OS)

Strategie SekaiLink:
- detecter l'installation Stardew (Steam path)
- installer SMAPI (release officielle)
- installer le mod Archipelago (source canonique)
- lancer via l'entrypoint SMAPI

## Fabric Loader et MinecraftForge (Minecraft)

Docs upstream:
- `third_party/mod_loaders/Fabric-Loader/README.md`
- `third_party/mod_loaders/MinecraftForge/docs/README.md`

Ce que SekaiLink doit retenir:
- l'ecosysteme Minecraft est fortement instance-based
- Forge et Fabric exigent une version Java compatible

Strategie SekaiLink (pragmatique):
- ne pas reimplementer un launcher
- integrer un launcher d'instances (a choisir) ou documenter un mode "managed instance"

Options possibles:
- Option A: support PrismLauncher/MultiMC (recommande pour instances)
- Option B: support official launcher (plus fragile)

Dans tous les cas:
- SekaiLink doit pouvoir declarer:
- version Minecraft
- loader (forge/fabric) + version
- mods a installer (sources)
- dossier instance (profile)

## r2modman (Thunderstore)

Docs upstream:
- `third_party/mod_loaders/r2modmanPlus/README.md`

Ce que SekaiLink doit retenir:
- r2modman est un manager multi-jeux, avec profils, updates, configs
- distribution: attention aux sites tiers, preferer Thunderstore ou GitHub release

Strategie SekaiLink:
- traiter r2modman comme runtime externe:
- installer r2modman (AppImage/zip)
- creer un profil dedie SekaiLink par jeu
- installer le mod Archipelago (Thunderstore)

Implementation future (a documenter avant code):
- existe-t-il une CLI stable ou un format config stable pour automatiser la creation de profil?
- si non, SekaiLink peut ouvrir r2modman a la premiere config, puis reprendre ensuite

## UnityExplorer (optionnel)

Docs upstream:
- `third_party/mod_loaders/UnityExplorer/README.md`

Positionnement SekaiLink:
- outil power user pour debug, pas un prerequis
- peut etre propose comme "outil" installable, pas active par defaut

## Contrat runtime pour mod loaders (manifest)

Le module manifest doit pouvoir exprimer:
- `game_install` requis (Steam app id ou patterns)
- `mod_loader`: type + version + sources
- `mods`: liste de repos/sources + checksums si possible
- `launch`: commande (souvent differente sur Linux/Windows)
- `connect`: comment passer host/slot/pass

Voir:
- `sekailink-client-plan/04-runtime-contract.md`
- `sekailink-client-plan/19-session-orchestrator.md`

