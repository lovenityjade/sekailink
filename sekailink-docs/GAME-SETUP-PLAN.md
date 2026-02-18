# Game Setup Plan (MVP)

## Goal
Un **Game Setup Wizard** ultra-guide, **0 interaction externe**, **Windows + Linux**, **verification hash obligatoire**, **telechargements automatiques** de sources fiables.

## Regles
- Stockage **local** (pas serveur) pour "Mes Jeux".
- Validation ROM **par hash (MD5/SHA1)**; **blocage** si mismatch.
- Telechargements **uniquement sources fiables** (ex. GitHub releases). Sinon, jeu non supporte.
- Si site web externe requis, utiliser un **WebView integre** controle (pas de navigation libre).
- Emu setup **automatise** (BizHawk prioritaire, puis snes9x-rr/Dolphin/Project64 si besoin).

## MVP Jeux
- **Pokemon FireRed / LeafGreen**
- **Pokemon Emerald**
- **A Link to the Past (ALttP)**
- **Ocarina of Time (OOT)**

## Storage local
- Fichier: `~/.sekailink/games.json`
- Contenu:
  - `installed_games[]` avec `game_id`, `status`, `setup_step`, `assets`

## Flow UX (Game Setup Wizard)
1. **Add a Game**: ajoute dans "Mes Jeux" et retire de la liste.
2. **Scan ROM folder**: copie ROMs vers `userData/roms`.
3. **Hash check**: compare MD5/SHA1.
4. **Auto-download tools**: client/mod/patcher si requis.
5. **Auto-download tracker packs** (PopTracker) depuis GitHub releases.
6. **Auto-config**: emu + scripts + connecteurs.
7. **Ready**: jeu utilisable sans action externe.

## Verification ROM
- Hash obligatoire par jeu.
- Si mismatch:
  - blocage de la progression
  - message clair ("ROM incorrecte")

## Sources de verite
- `GAMES-SETUP.md`
- `SUPPORTED-GAMES.md`
- docs par jeu dans `worlds/<game>/docs/*`
- registry auto-genere (`games.generated.json`) si utile

## Tech Notes
- BizHawk est le **socle** pour le maximum de jeux.
- Autres emus seulement si BizHawk non viable.

## Livrables MVP
1. Store local `games.json` + UI "Mes Jeux".
2. Wizard FR/LG + Emerald + ALttP.
3. Auto-scan + hash + import ROM.
