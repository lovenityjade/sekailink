# Scope et objectifs

## Objectif produit
One-button "Play".
Apres generation cote serveur (webhost), le client SekaiLink doit pouvoir:
1. detecter le(s) download(s) du joueur (patch/slot file)
2. telecharger ce qu'il faut
3. appliquer patch si applicable
4. verifier les prerequisites (ROM/ISO/BIOS/keys/firmware/mods)
5. installer/activer emulateur ou modloader manquant (si on a une source vendorable)
6. lancer le jeu
7. lancer le tracking (PopTracker et/ou Universal Tracker)
8. se connecter au serveur Archipelago (slot + password)

"0 interaction" est l'ideal, mais on doit accepter des exceptions inevitables:
- l'utilisateur doit fournir les assets proprietaires (ROM/ISO/BIOS/keys)
- certains jeux exigent une action in-game (creation d'une save, entrer le nom, etc.)

## Contraintes plateforme
- Compatibilite: Linux + Windows (macOS plus tard)
- Dev environnement: Linux Bazzite, souvent dans une distrobox
- Affichage Linux: supporter Wayland et X11 (X11 peut rester primaire via XWayland)
- Ne pas casser Wayland (Steam Deck et autres)

## Success criteria (mesurables)
Un nouveau joueur peut:
1. installer SekaiLink
2. se login, rejoindre un lobby
3. importer ses ROMs/ISOs une seule fois
4. cliquer "Launch" ou "Play" et jouer

Temps cible (aspirationnel):
- joueur deja pret (ROM + packs): < 30s entre "Launch" et jeu ouvert
- nouveau joueur (import ROM): < 5 min sans guide externe

## Non-objectifs (pour garder le scope sain)
- Distribuer des ROMs/ISOs/BIOS/keys/firmware (non)
- Implementer un emulateur maison (non)
- Supporter tous les worlds "day 1" avec le meme niveau de zero-interaction

## Axes d'architecture (decisions)
- Electron UI: le joueur ne tape pas de commandes (tout cliquable)
- "Orchestrator" dans Electron main process
- wrappers Python headless pour reutiliser le code Archipelago existant
- modules runtime declaratifs par jeu (manifest + scripts)
- logs et output visibles dans l'app (client + serveur)
