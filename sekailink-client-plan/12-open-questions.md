# Questions ouvertes et risques

## 1) "Integrer tous les clients" veut dire quoi exactement?
Interpretation technique:
- couvrir les familles de connexion et de lancement qui existent dans Archipelago/MWGG.
- pas litteralement "porter toutes les UIs Python".

A confirmer:
- veut-on supporter TextClient (chat only)
- veut-on supporter BizHawkClient (bridge)
- veut-on supporter SNIClient (SNES)
- veut-on supporter UT tracker
- veut-on supporter des clients par world (SC2, Factorio, etc.)

## 2) Slot file downloads
- beaucoup de worlds ne supportent pas apdeltapatch.
- `room_status.downloads` peut etre un slot file.

A faire:
- definir un standard de packaging cote serveur pour que le client puisse auto-router.

## 3) Divergence connecteurs BizHawk
- AP BizHawkClient pointe sur `data/lua/connector_bizhawk_generic.lua`.
- SekaiLink runtime utilise `client/runtime/.../connector.lua`.

Risque:
- mismatch version/protocole.

## 4) "Universal window" pour emulateurs
- techniquement difficile cross-platform.
- sur Linux, gamescope est une option solide.
- sur Windows, plus complique.

Decision a prendre:
- accepter plusieurs fenetres au debut, mais les rendre console-friendly.

## 5) Distrobox vs host
- dev peut se faire en distrobox.
- mais le produit final est un binaire host.

Risque:
- faux positifs en tests si container masque des problemes (portals, permissions, GPU).

## 6) Legal/licensing
- vendoring emulators et outils: verifier licenses (GPL, etc.)
- PopTracker: maintenir le patch CLI.

## 7) Robustesse: supervision processus
- kill tree (Windows)
- zombies (Linux)
- relaunch policy

## 8) UX controller-first
- navigation gamepad
- focus management entre SekaiLink et emulateur
