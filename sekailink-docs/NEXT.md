# NEXT.md

## Priorités proposées (à confirmer)
1) Tracker UI: petits bugs d’affichage à corriger (spacing, tables, scrollbars, sticky header, etc.).
2) Lobby UX: affiner le menu contextuel (icônes, accessibilité, fermeture auto, positions), et uniformiser le wording Game.
3) Social/Chat: vérifier que le spam est bien résolu en conditions réelles (socket + polling) et ajouter logs si besoin.
4) Release Windows: préparer installateur (NSIS) + auto‑update pipeline.

## Workflow (3 étapes)
1) Intégrer le client Archipelago dans SekaiLink.
   - Objectif: connexion au WebSocket Archipelago + gestion des événements.
2) MVP avec tutoriels/setup par jeu.
   - Objectif: onboarding + flux de jeu guidé.
   - Source: jeux documentés dans `GAMES-SUPPORTED.md`.
3) Major update: API/contrôle des émulateurs.
   - Objectif: API interne pour piloter BizHawk/snes9x‑rr (fenêtres, scripts, overlays, mémoire).
   - Phase longue, après MVP stable.

## Notes
- Tracker React utilise `/api/tracker_view` + `/api/tracker_player` + `/api/sphere_tracker`.
- URL ZIP Windows: https://sekailink.com/static/downloads/SekaiLink-client-alpha-0.0.1.zip

## Reminders
- Ne pas toucher au serveur sans demander si c’est une itération client.
- Toujours attendre les instructions avant de coder.
